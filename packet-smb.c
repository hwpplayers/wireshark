/* packet-smb.c
 * Routines for smb packet dissection
 * Copyright 1999, Richard Sharpe <rsharpe@ns.aus.com>
 *
 * $Id: packet-smb.c,v 1.137 2001/11/08 08:21:12 guy Exp $
 *
 * Ethereal - Network traffic analyzer
 * By Gerald Combs <gerald@ethereal.com>
 * Copyright 1998 Gerald Combs
 *
 * Copied from packet-pop.c
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#include <time.h>
#include <string.h>
#include <glib.h>
#include <ctype.h>
#include "packet.h"
#include "conversation.h"
#include "smb.h"
#include "alignment.h"
#include "strutil.h"

#include "packet-smb-mailslot.h"
#include "packet-smb-pipe.h"

/*
 * Various specifications and documents about SMB can be found in
 *
 *	ftp://ftp.microsoft.com/developr/drg/CIFS/
 *
 * and a CIFS draft from the Storage Networking Industry Association
 * can be found on a link from the page at
 *
 *	http://www.snia.org/English/Work_Groups/NAS/CIFS/WG_CIFS_Docs.html
 *
 * (it supercedes the document at
 *
 *	ftp://ftp.microsoft.com/developr/drg/CIFS/draft-leach-cifs-v1-spec-01.txt
 *
 * ).
 *
 * There are also some Open Group publications documenting CIFS for sale;
 * catalog entries for them are at:
 *
 *	http://www.opengroup.org/products/publications/catalog/c209.htm
 *
 *	http://www.opengroup.org/products/publications/catalog/c195.htm
 *
 * Beware - these specs may have errors.
 */
static int proto_smb = -1;
static int hf_smb_cmd = -1;
static int hf_smb_pid = -1;
static int hf_smb_tid = -1;
static int hf_smb_uid = -1;
static int hf_smb_mid = -1;
static int hf_smb_response_to = -1;
static int hf_smb_response_in = -1;
static int hf_smb_nt_status = -1;
static int hf_smb_error_class = -1;
static int hf_smb_error_code = -1;
static int hf_smb_reserved = -1;
static int hf_smb_flags_lock = -1;
static int hf_smb_flags_receive_buffer = -1;
static int hf_smb_flags_caseless = -1;
static int hf_smb_flags_canon = -1;
static int hf_smb_flags_oplock = -1;
static int hf_smb_flags_notify = -1;
static int hf_smb_flags_response = -1;
static int hf_smb_flags2_long_names_allowed = -1;
static int hf_smb_flags2_ea = -1;
static int hf_smb_flags2_sec_sig = -1;
static int hf_smb_flags2_long_names_used = -1;
static int hf_smb_flags2_esn = -1;
static int hf_smb_flags2_dfs = -1;
static int hf_smb_flags2_roe = -1;
static int hf_smb_flags2_nt_error = -1;
static int hf_smb_flags2_string = -1;
static int hf_smb_word_count = -1;
static int hf_smb_byte_count = -1;
static int hf_smb_buffer_format = -1;
static int hf_smb_dialect_name = -1;
static int hf_smb_dialect_index = -1;
static int hf_smb_max_trans_buf_size = -1;
static int hf_smb_max_mpx_count = -1;
static int hf_smb_max_vcs_num = -1;
static int hf_smb_session_key = -1;
static int hf_smb_server_timezone = -1;
static int hf_smb_encryption_key_length = -1;
static int hf_smb_encryption_key = -1;
static int hf_smb_primary_domain = -1;
static int hf_smb_max_raw_buf_size = -1;
static int hf_smb_server_guid = -1;
static int hf_smb_security_blob_len = -1;
static int hf_smb_security_blob = -1;
static int hf_smb_sm_mode16 = -1;
static int hf_smb_sm_password16 = -1;
static int hf_smb_sm_mode = -1;
static int hf_smb_sm_password = -1;
static int hf_smb_sm_signatures = -1;
static int hf_smb_sm_sig_required = -1;
static int hf_smb_rm_read = -1;
static int hf_smb_rm_write = -1;
static int hf_smb_server_date_time = -1;
static int hf_smb_server_smb_date = -1;
static int hf_smb_server_smb_time = -1;
static int hf_smb_server_cap_raw_mode = -1;
static int hf_smb_server_cap_mpx_mode = -1;
static int hf_smb_server_cap_unicode = -1;
static int hf_smb_server_cap_large_files = -1;
static int hf_smb_server_cap_nt_smbs = -1;
static int hf_smb_server_cap_rpc_remote_apis = -1;
static int hf_smb_server_cap_nt_status = -1;
static int hf_smb_server_cap_level_ii_oplocks = -1;
static int hf_smb_server_cap_lock_and_read = -1;
static int hf_smb_server_cap_nt_find = -1;
static int hf_smb_server_cap_dfs = -1;
static int hf_smb_server_cap_infolevel_passthru = -1;
static int hf_smb_server_cap_large_readx = -1;
static int hf_smb_server_cap_large_writex = -1;
static int hf_smb_server_cap_unix = -1;
static int hf_smb_server_cap_reserved = -1;
static int hf_smb_server_cap_bulk_transfer = -1;
static int hf_smb_server_cap_compressed_data = -1;
static int hf_smb_server_cap_extended_security = -1;
static int hf_smb_system_time = -1;
static int hf_smb_unknown = -1;
static int hf_smb_dir_name = -1;
static int hf_smb_echo_count = -1;
static int hf_smb_echo_data = -1;
static int hf_smb_echo_seq_num = -1;
static int hf_smb_max_buf_size = -1;
static int hf_smb_password = -1;
static int hf_smb_password_len = -1;
static int hf_smb_ansi_password = -1;
static int hf_smb_ansi_password_len = -1;
static int hf_smb_unicode_password = -1;
static int hf_smb_unicode_password_len = -1;
static int hf_smb_path = -1;
static int hf_smb_service = -1;
static int hf_smb_move_flags_file = -1;
static int hf_smb_move_flags_dir = -1;
static int hf_smb_move_flags_verify = -1;
static int hf_smb_count = -1;
static int hf_smb_file_name = -1;
static int hf_smb_open_function_open = -1;
static int hf_smb_open_function_create = -1;
static int hf_smb_fid = -1;
static int hf_smb_file_attr_read_only_16bit = -1;
static int hf_smb_file_attr_read_only_8bit = -1;
static int hf_smb_file_attr_hidden_16bit = -1;
static int hf_smb_file_attr_hidden_8bit = -1;
static int hf_smb_file_attr_system_16bit = -1;
static int hf_smb_file_attr_system_8bit = -1;
static int hf_smb_file_attr_volume_16bit = -1;
static int hf_smb_file_attr_volume_8bit = -1;
static int hf_smb_file_attr_directory_16bit = -1;
static int hf_smb_file_attr_directory_8bit = -1;
static int hf_smb_file_attr_archive_16bit = -1;
static int hf_smb_file_attr_archive_8bit = -1;
static int hf_smb_file_attr_device = -1;
static int hf_smb_file_attr_normal = -1;
static int hf_smb_file_attr_temporary = -1;
static int hf_smb_file_attr_sparse = -1;
static int hf_smb_file_attr_reparse = -1;
static int hf_smb_file_attr_compressed = -1;
static int hf_smb_file_attr_offline = -1;
static int hf_smb_file_attr_not_content_indexed = -1;
static int hf_smb_file_attr_encrypted = -1;
static int hf_smb_file_size = -1;
static int hf_smb_search_attribute_read_only = -1;
static int hf_smb_search_attribute_hidden = -1;
static int hf_smb_search_attribute_system = -1;
static int hf_smb_search_attribute_volume = -1;
static int hf_smb_search_attribute_directory = -1;
static int hf_smb_search_attribute_archive = -1;
static int hf_smb_access_mode = -1;
static int hf_smb_access_sharing = -1;
static int hf_smb_access_locality = -1;
static int hf_smb_access_caching = -1;
static int hf_smb_access_writetru = -1;
static int hf_smb_create_time = -1;
static int hf_smb_create_dos_date = -1;
static int hf_smb_create_dos_time = -1;
static int hf_smb_last_write_time = -1;
static int hf_smb_last_write_dos_date = -1;
static int hf_smb_last_write_dos_time = -1;
static int hf_smb_access_time = -1;
static int hf_smb_access_dos_date = -1;
static int hf_smb_access_dos_time = -1;
static int hf_smb_old_file_name = -1;
static int hf_smb_offset = -1;
static int hf_smb_remaining = -1;
static int hf_smb_padding = -1;
static int hf_smb_file_data = -1;
static int hf_smb_data_len = -1;
static int hf_smb_seek_mode = -1;
static int hf_smb_data_size = -1;
static int hf_smb_alloc_size = -1;
static int hf_smb_alloc_size64 = -1;
static int hf_smb_max_count = -1;
static int hf_smb_min_count = -1;
static int hf_smb_timeout = -1;
static int hf_smb_high_offset = -1;
static int hf_smb_units = -1;
static int hf_smb_bpu = -1;
static int hf_smb_blocksize = -1;
static int hf_smb_freeunits = -1;
static int hf_smb_data_offset = -1;
static int hf_smb_dcm = -1;
static int hf_smb_request_mask = -1;
static int hf_smb_response_mask = -1;
static int hf_smb_sid = -1;
static int hf_smb_write_raw_mode_write_through = -1;
static int hf_smb_write_raw_mode_return_remaining = -1;
static int hf_smb_write_raw_mode_connectionless = -1;
static int hf_smb_resume_key_len = -1;
static int hf_smb_resume_server_cookie = -1;
static int hf_smb_resume_client_cookie = -1;
static int hf_smb_andxoffset = -1;
static int hf_smb_lock_type_large = -1;
static int hf_smb_lock_type_cancel = -1;
static int hf_smb_lock_type_change = -1;
static int hf_smb_lock_type_oplock = -1;
static int hf_smb_lock_type_shared = -1;
static int hf_smb_locking_ol = -1;
static int hf_smb_number_of_locks = -1;
static int hf_smb_number_of_unlocks = -1;
static int hf_smb_lock_long_offset = -1;
static int hf_smb_lock_long_length = -1;
static int hf_smb_file_type = -1;
static int hf_smb_device_state = -1;
static int hf_smb_server_fid = -1;
static int hf_smb_open_flags_add_info = -1;
static int hf_smb_open_flags_ex_oplock = -1;
static int hf_smb_open_flags_batch_oplock = -1;
static int hf_smb_open_flags_ealen = -1;
static int hf_smb_open_action_open = -1;
static int hf_smb_open_action_lock = -1;
static int hf_smb_vc_num = -1;
static int hf_smb_account = -1;
static int hf_smb_os = -1;
static int hf_smb_lanman = -1;
static int hf_smb_setup_action_guest = -1;
static int hf_smb_fs = -1;
static int hf_smb_connect_flags_dtid = -1;
static int hf_smb_connect_support_search = -1;
static int hf_smb_connect_support_in_dfs = -1;
static int hf_smb_max_setup_count = -1;
static int hf_smb_total_param_count = -1;
static int hf_smb_total_data_count = -1;
static int hf_smb_max_param_count = -1;
static int hf_smb_max_data_count = -1;
static int hf_smb_param_disp32 = -1;
static int hf_smb_param_count32 = -1;
static int hf_smb_param_offset32 = -1;
static int hf_smb_data_disp32 = -1;
static int hf_smb_data_count32 = -1;
static int hf_smb_data_offset32 = -1;
static int hf_smb_setup_count = -1;
static int hf_smb_nt_trans_subcmd = -1;
static int hf_smb_nt_ioctl_function_code = -1;
static int hf_smb_nt_ioctl_isfsctl = -1;
static int hf_smb_nt_ioctl_flags_root_handle = -1;
static int hf_smb_nt_ioctl_data = -1;
static int hf_smb_nt_security_information = -1;
static int hf_smb_nt_notify_action = -1;
static int hf_smb_nt_notify_watch_tree = -1;
static int hf_smb_nt_notify_stream_write = -1;
static int hf_smb_nt_notify_stream_size = -1;
static int hf_smb_nt_notify_stream_name = -1;
static int hf_smb_nt_notify_security = -1;
static int hf_smb_nt_notify_ea = -1;
static int hf_smb_nt_notify_creation = -1;
static int hf_smb_nt_notify_last_access = -1;
static int hf_smb_nt_notify_last_write = -1;
static int hf_smb_nt_notify_size = -1;
static int hf_smb_nt_notify_attributes = -1;
static int hf_smb_nt_notify_dir_name = -1;
static int hf_smb_nt_notify_file_name = -1;
static int hf_smb_root_dir_fid = -1;
static int hf_smb_nt_create_disposition = -1;
static int hf_smb_nt_create_options = -1;
static int hf_smb_sd_length = -1;
static int hf_smb_ea_length = -1;
static int hf_smb_file_name_len = -1;
static int hf_smb_nt_impersonation_level = -1;
static int hf_smb_nt_security_flags_context_tracking = -1;
static int hf_smb_nt_security_flags_effective_only = -1;
static int hf_smb_nt_create_bits_oplock = -1;
static int hf_smb_nt_create_bits_boplock = -1;
static int hf_smb_nt_create_bits_dir = -1;
static int hf_smb_nt_access_mask_generic_read = -1;
static int hf_smb_nt_access_mask_generic_write = -1;
static int hf_smb_nt_access_mask_generic_execute = -1;
static int hf_smb_nt_access_mask_generic_all = -1;
static int hf_smb_nt_access_mask_maximum_allowed = -1;
static int hf_smb_nt_access_mask_system_security = -1;
static int hf_smb_nt_access_mask_synchronize = -1;
static int hf_smb_nt_access_mask_write_owner = -1;
static int hf_smb_nt_access_mask_write_dac = -1;
static int hf_smb_nt_access_mask_read_control = -1;
static int hf_smb_nt_access_mask_delete = -1;
static int hf_smb_nt_share_access_read = -1;
static int hf_smb_nt_share_access_write = -1;
static int hf_smb_nt_share_access_delete = -1;
static int hf_smb_file_eattr_read_only = -1;
static int hf_smb_file_eattr_hidden = -1;
static int hf_smb_file_eattr_system = -1;
static int hf_smb_file_eattr_volume = -1;
static int hf_smb_file_eattr_directory = -1;
static int hf_smb_file_eattr_archive = -1;
static int hf_smb_file_eattr_device = -1;
static int hf_smb_file_eattr_normal = -1;
static int hf_smb_file_eattr_temporary = -1;
static int hf_smb_file_eattr_sparse = -1;
static int hf_smb_file_eattr_reparse = -1;
static int hf_smb_file_eattr_compressed = -1;
static int hf_smb_file_eattr_offline = -1;
static int hf_smb_file_eattr_not_content_indexed = -1;
static int hf_smb_file_eattr_encrypted = -1;
static int hf_smb_file_eattr_write_through = -1;
static int hf_smb_file_eattr_no_buffering = -1;
static int hf_smb_file_eattr_random_access = -1;
static int hf_smb_file_eattr_sequential_scan = -1;
static int hf_smb_file_eattr_delete_on_close = -1;
static int hf_smb_file_eattr_backup_semantics = -1;
static int hf_smb_file_eattr_posix_semantics = -1;
static int hf_smb_security_descriptor_len = -1;
static int hf_smb_security_descriptor = -1;
static int hf_smb_nt_qsd_owner = -1;
static int hf_smb_nt_qsd_group = -1;
static int hf_smb_nt_qsd_dacl = -1;
static int hf_smb_nt_qsd_sacl = -1;
static int hf_smb_extended_attributes = -1;
static int hf_smb_oplock_level = -1;
static int hf_smb_create_action = -1;
static int hf_smb_ea_error_offset = -1;
static int hf_smb_end_of_file = -1;
static int hf_smb_device_type = -1;
static int hf_smb_is_directory = -1;
static int hf_smb_next_entry_offset = -1;
static int hf_smb_change_time = -1;

static gint ett_smb = -1;
static gint ett_smb_hdr = -1;
static gint ett_smb_command = -1;
static gint ett_smb_fileattributes = -1;
static gint ett_smb_capabilities = -1;
static gint ett_smb_aflags = -1;
static gint ett_smb_dialect = -1;
static gint ett_smb_dialects = -1;
static gint ett_smb_mode = -1;
static gint ett_smb_rawmode = -1;
static gint ett_smb_flags = -1;
static gint ett_smb_flags2 = -1;
static gint ett_smb_desiredaccess = -1;
static gint ett_smb_search = -1;
static gint ett_smb_file = -1;
static gint ett_smb_openfunction = -1;
static gint ett_smb_filetype = -1;
static gint ett_smb_openaction = -1;
static gint ett_smb_writemode = -1;
static gint ett_smb_lock_type = -1;
static gint ett_smb_ssetupandxaction = -1;
static gint ett_smb_optionsup = -1;
static gint ett_smb_time_date = -1;
static gint ett_smb_64bit_time = -1;
static gint ett_smb_move_flags = -1;
static gint ett_smb_file_attributes = -1;
static gint ett_smb_search_resume_key = -1;
static gint ett_smb_search_dir_info = -1;
static gint ett_smb_unlocks = -1;
static gint ett_smb_unlock = -1;
static gint ett_smb_locks = -1;
static gint ett_smb_lock = -1;
static gint ett_smb_open_flags = -1;
static gint ett_smb_open_action = -1;
static gint ett_smb_setup_action = -1;
static gint ett_smb_connect_flags = -1;
static gint ett_smb_connect_support_bits = -1;
static gint ett_smb_nt_create_bits = -1;
static gint ett_smb_nt_access_mask = -1;
static gint ett_smb_nt_share_access = -1;
static gint ett_smb_nt_security_flags = -1;
static gint ett_smb_nt_trans_setup = -1;
static gint ett_smb_nt_notify_completion_filter = -1;
static gint ett_smb_nt_ioctl_flags = -1;
static gint ett_smb_security_information_mask = -1;


static char *decode_smb_name(unsigned char);
static int dissect_smb_command(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree, guint8 cmd);
static const gchar *get_unicode_or_ascii_string_tvb(tvbuff_t *tvb,
    int *offsetp, packet_info *pinfo, int *len, gboolean nopad,
    gboolean exactlen, guint16 *bc);


#define WORD_COUNT	\
	/* Word Count */				\
	wc = tvb_get_guint8(tvb, offset);		\
	proto_tree_add_uint(tree, hf_smb_word_count,	\
		tvb, offset, 1, wc);			\
	offset += 1;					\
	if(wc==0) goto bytecount;

#define BYTE_COUNT	\
	bytecount:					\
	bc = tvb_get_letohs(tvb, offset);		\
	proto_tree_add_uint(tree, hf_smb_byte_count,	\
			tvb, offset, 2, bc);		\
	offset += 2;					\
	if(bc==0) goto endofcommand;

#define CHECK_BYTE_COUNT(len)	\
	if (bc < len) goto endofcommand;

#define COUNT_BYTES(len)	\
	offset += len;		\
	bc -= len;

#define END_OF_SMB	\
	if (bc != 0) { \
		proto_tree_add_text(tree, tvb, offset, bc, \
		    "Extra byte parameters");		\
		offset += bc;				\
	}						\
	endofcommand:


/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
   These variables and functions are used to match
   responses with calls
   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
static GMemChunk *smb_info_chunk = NULL;
static int smb_info_init_count = 200;
static GHashTable *smb_info_table = NULL;

static gint
smb_info_equal(gconstpointer k1, gconstpointer k2)
{
	smb_info_t *key1 = (smb_info_t *)k1;
	smb_info_t *key2 = (smb_info_t *)k2;
	gint res;

	/* make sure to always compare mid first since this is most
	   likely to differ ==> shortcircuiting the expression */
	res= (	(key1->mid==key2->mid)
		&&	(key1->pid==key2->pid)
		&&	(key1->uid==key2->uid)
		&&	(key1->cmd==key2->cmd)
		&&	(key1->tid==key2->tid)
		&&	(ADDRESSES_EQUAL(key1->src, key2->src))
		&&	(ADDRESSES_EQUAL(key1->dst, key2->dst)) );

	return res;
}

static guint
smb_info_hash(gconstpointer k)
{
	smb_info_t *key = (smb_info_t *)k;

	/* multiplex id is very likely to differ between calls
	   it should be sufficient for a good distribution of hash
	   values.
	*/
	return key->mid;
}
 
static gboolean
free_all_smb_info(gpointer key_arg, gpointer value, gpointer user_data)
{
	smb_info_t *key = (smb_info_t *)key_arg;

	if((key->src)&&(key->src->data)){
		g_free((gpointer)key->src->data);
		key->src->data = NULL;
		g_free((gpointer)key->src);
		key->src = NULL;
	}

	if((key->dst)&&(key->dst->data)){
		g_free((gpointer)key->dst->data);
		key->dst->data = NULL;
		g_free((gpointer)key->dst);
		key->dst = NULL;
	}
 
	return TRUE;
}

void
smb_info_init(void)
{
	if(smb_info_table){
		g_hash_table_foreach_remove(smb_info_table,
			free_all_smb_info, NULL);
	} else {
		smb_info_table = g_hash_table_new(smb_info_hash,
			smb_info_equal);
	}

	if(smb_info_chunk){
		g_mem_chunk_destroy(smb_info_chunk);
	}
	smb_info_chunk = g_mem_chunk_new("smb_info_chunk",
		sizeof(smb_info_t),
		smb_info_init_count*sizeof(smb_info_t),
		G_ALLOC_ONLY);
}
/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
   End of request/response matching functions
   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */

static const value_string buffer_format_vals[] = {
	{1,     "Data Block"},
	{2,     "Dialect"},
	{3,     "Pathname"},
	{4,     "ASCII"},
	{5,     "Variable Block"},
	{0,     NULL}
};

/*
 * UTIME - this is *almost* like a UNIX time stamp, except that it's
 * in seconds since January 1, 1970, 00:00:00 *local* time, not since
 * January 1, 1970, 00:00:00 GMT.
 *
 * This means we have to do some extra work to convert it.  This code is
 * based on the Samba code:
 *
 *	Unix SMB/Netbios implementation.
 *	Version 1.9.
 *	time handling functions
 *	Copyright (C) Andrew Tridgell 1992-1998
 */

/*
 * Yield the difference between *A and *B, in seconds, ignoring leap
 * seconds.
 */
#define TM_YEAR_BASE 1900

static int
tm_diff(struct tm *a, struct tm *b)
{
	int ay = a->tm_year + (TM_YEAR_BASE - 1);
	int by = b->tm_year + (TM_YEAR_BASE - 1);
	int intervening_leap_days =
	    (ay/4 - by/4) - (ay/100 - by/100) + (ay/400 - by/400);
	int years = ay - by;
	int days =
	    365*years + intervening_leap_days + (a->tm_yday - b->tm_yday);
	int hours = 24*days + (a->tm_hour - b->tm_hour);
	int minutes = 60*hours + (a->tm_min - b->tm_min);
	int seconds = 60*minutes + (a->tm_sec - b->tm_sec);

	return seconds;
}

/*
 * Return the UTC offset in seconds west of UTC, or 0 if it cannot be
 * determined.
 */
static int
TimeZone(time_t t)
{
	struct tm *tm = gmtime(&t);
	struct tm tm_utc;

	if (tm == NULL)
		return 0;
	tm_utc = *tm;
	tm = localtime(&t);
	if (tm == NULL)
		return 0;
	return tm_diff(&tm_utc,tm);
}

/*
 * Return the same value as TimeZone, but it should be more efficient.
 *
 * We keep a table of DST offsets to prevent calling localtime() on each 
 * call of this function. This saves a LOT of time on many unixes.
 *
 * Updated by Paul Eggert <eggert@twinsun.com>
 */
#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

#ifndef TIME_T_MIN
#define TIME_T_MIN ((time_t)0 < (time_t) -1 ? (time_t) 0 \
		    : ~ (time_t) 0 << (sizeof (time_t) * CHAR_BIT - 1))
#endif
#ifndef TIME_T_MAX
#define TIME_T_MAX (~ (time_t) 0 - TIME_T_MIN)
#endif

static int
TimeZoneFaster(time_t t)
{
	static struct dst_table {time_t start,end; int zone;} *tdt;
	static struct dst_table *dst_table = NULL;
	static int table_size = 0;
	int i;
	int zone = 0;

	if (t == 0)
		t = time(NULL);

	/* Tunis has a 8 day DST region, we need to be careful ... */
#define MAX_DST_WIDTH (365*24*60*60)
#define MAX_DST_SKIP (7*24*60*60)

	for (i = 0; i < table_size; i++) {
		if (t >= dst_table[i].start && t <= dst_table[i].end)
			break;
	}

	if (i < table_size) {
		zone = dst_table[i].zone;
	} else {
		time_t low,high;

		zone = TimeZone(t);
		if (dst_table == NULL)
			tdt = g_malloc(sizeof(dst_table[0])*(i+1));
		else
			tdt = g_realloc(dst_table, sizeof(dst_table[0])*(i+1));
		if (tdt == NULL) {
			if (dst_table)
				free(dst_table);
			table_size = 0;
		} else {
			dst_table = tdt;
			table_size++;
    
			dst_table[i].zone = zone; 
			dst_table[i].start = dst_table[i].end = t;
    
			/* no entry will cover more than 6 months */
			low = t - MAX_DST_WIDTH/2;
			if (t < low)
				low = TIME_T_MIN;
      
			high = t + MAX_DST_WIDTH/2;
			if (high < t)
				high = TIME_T_MAX;
      
			/*
			 * Widen the new entry using two bisection searches.
			 */
			while (low+60*60 < dst_table[i].start) {
				if (dst_table[i].start - low > MAX_DST_SKIP*2)
					t = dst_table[i].start - MAX_DST_SKIP;
				else
					t = low + (dst_table[i].start-low)/2;
				if (TimeZone(t) == zone)
					dst_table[i].start = t;
				else
					low = t;
			}

			while (high-60*60 > dst_table[i].end) {
				if (high - dst_table[i].end > MAX_DST_SKIP*2)
					t = dst_table[i].end + MAX_DST_SKIP;
				else
					t = high - (high-dst_table[i].end)/2;
				if (TimeZone(t) == zone)
					dst_table[i].end = t;
				else
					high = t;
			}
		}
	}
	return zone;
}

/*
 * Return the UTC offset in seconds west of UTC, adjusted for extra time
 * offset, for a local time value.  If ut = lt + LocTimeDiff(lt), then
 * lt = ut - TimeDiff(ut), but the converse does not necessarily hold near
 * daylight savings transitions because some local times are ambiguous.
 * LocTimeDiff(t) equals TimeDiff(t) except near daylight savings transitions.
 */
static int
LocTimeDiff(time_t lt)
{
	int d = TimeZoneFaster(lt);
	time_t t = lt + d;

	/* if overflow occurred, ignore all the adjustments so far */
	if (((t < lt) ^ (d < 0)))
		t = lt;

	/*
	 * Now t should be close enough to the true UTC to yield the
	 * right answer.
	 */
	return TimeZoneFaster(t);
}

static int
dissect_smb_UTIME(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, int hf_date)
{
	guint32 timeval;
	nstime_t ts;
 
	timeval = tvb_get_letohl(tvb, offset);
	if (timeval == 0xffffffff) {
		proto_tree_add_text(tree, tvb, offset, 4,
		    "%s: No time specified (0xffffffff)",
		    proto_registrar_get_name(hf_date));
		offset += 4;
		return offset;
	}

	/*
	 * We add the local time offset.
	 */
	ts.secs = timeval + LocTimeDiff(timeval);
	ts.nsecs = 0;

	proto_tree_add_time(tree, hf_date, tvb, offset, 4, &ts);
	offset += 4;
 
	return offset;
}

static int
dissect_smb_64bit_time(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset, char *str, int hf_date)
{
	proto_item *item = NULL;
	proto_tree *tree = NULL;
	nstime_t tv;

	/* XXXX we need some way to represent this as a time
	   properly. For now we display everything as 8 bytes*/

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 8,
			str);
		tree = proto_item_add_subtree(item, ett_smb_64bit_time);
	}

	proto_tree_add_bytes_format(tree, hf_smb_unknown, tvb, offset, 8, tvb_get_ptr(tvb, offset, 8), "%s: can't decode this yet", str);

	offset += 8;
	return offset;
}

static int
dissect_smb_datetime(tvbuff_t *tvb, packet_info *pinfo,
    proto_tree *parent_tree, int offset, int hf_date, int hf_dos_date,
    int hf_dos_time, gboolean time_first)
{
	guint16 dos_time, dos_date;
	proto_item *item = NULL;
	proto_tree *tree = NULL;
	struct tm tm;
	time_t t;
	static const int mday_noleap[12] = {
		31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};
	static const int mday_leap[12] = {
		31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};
#define ISLEAP(y) (((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))
	nstime_t tv;

	if (time_first) {
		dos_time = tvb_get_letohs(tvb, offset);
		dos_date = tvb_get_letohs(tvb, offset+2);
	} else {
		dos_date = tvb_get_letohs(tvb, offset);
		dos_time = tvb_get_letohs(tvb, offset+2);
	}

	if ((dos_time == 0xffff && dos_time == 0xffff) ||
	    (dos_time == 0 && dos_time == 0)) {
		/*
		 * No date/time specified.
		 */
		if(parent_tree){
			proto_tree_add_text(parent_tree, tvb, offset, 4,
			    "%s: No time specified (0x%08x)",
			    proto_registrar_get_name(hf_date),
			    (dos_date << 16) | dos_time);
		}
		offset += 4;
		return offset;
	}

	tm.tm_sec = (dos_time&0x1f)*2;
	tm.tm_min = (dos_time>>5)&0x3f;
	tm.tm_hour = (dos_time>>11)&0x1f;
	tm.tm_mday = dos_date&0x1f;
	tm.tm_mon = ((dos_date>>5)&0x0f) - 1;
	tm.tm_year = ((dos_date>>9)&0x7f) + 1980 - 1900;
	tm.tm_isdst = -1;

	/*
	 * Do some sanity checks before calling "mktime()";
	 * "mktime()" doesn't do them, it "normalizes" out-of-range
	 * values.
	 */
	if (tm.tm_sec > 59 || tm.tm_min > 59 || tm.tm_hour > 23 ||
	   tm.tm_mon < 0 || tm.tm_mon > 11 ||
	   (ISLEAP(tm.tm_year + 1900) ?
	     tm.tm_mday > mday_leap[tm.tm_mon] :
	     tm.tm_mday > mday_noleap[tm.tm_mon]) ||
	     (t = mktime(&tm)) == -1) {
		/*
		 * Invalid date/time.
		 */
		if (parent_tree) {
			item = proto_tree_add_text(parent_tree, tvb, offset, 4,
			    "%s: Invalid time",
			    proto_registrar_get_name(hf_date));
			tree = proto_item_add_subtree(item, ett_smb_time_date);
			if (time_first) {
				proto_tree_add_uint_format(tree, hf_dos_time, tvb, offset, 2, dos_time, "DOS Time: %02d:%02d:%02d (0x%04x)", tm.tm_hour, tm.tm_min, tm.tm_sec, dos_time);
				proto_tree_add_uint_format(tree, hf_dos_date, tvb, offset+2, 2, dos_date, "DOS Date: %04d-%02d-%02d (0x%04x)", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, dos_date);
			} else {
				proto_tree_add_uint_format(tree, hf_dos_date, tvb, offset, 2, dos_date, "DOS Date: %04d-%02d-%02d (0x%04x)", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, dos_date);
				proto_tree_add_uint_format(tree, hf_dos_time, tvb, offset+2, 2, dos_time, "DOS Time: %02d:%02d:%02d (0x%04x)", tm.tm_hour, tm.tm_min, tm.tm_sec, dos_time);
			}
		}
		offset += 4;
		return offset;
	}

	tv.secs = t;
	tv.nsecs = 0;

	if(parent_tree){
		item = proto_tree_add_time(parent_tree, hf_date, tvb, offset, 4, &tv);
		tree = proto_item_add_subtree(item, ett_smb_time_date);
		if (time_first) {
			proto_tree_add_uint_format(tree, hf_dos_time, tvb, offset, 2, dos_time, "DOS Time: %02d:%02d:%02d (0x%04x)", tm.tm_hour, tm.tm_min, tm.tm_sec, dos_time);
			proto_tree_add_uint_format(tree, hf_dos_date, tvb, offset+2, 2, dos_date, "DOS Date: %04d-%02d-%02d (0x%04x)", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, dos_date);
		} else {
			proto_tree_add_uint_format(tree, hf_dos_date, tvb, offset, 2, dos_date, "DOS Date: %04d-%02d-%02d (0x%04x)", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, dos_date);
			proto_tree_add_uint_format(tree, hf_dos_time, tvb, offset+2, 2, dos_time, "DOS Time: %02d:%02d:%02d (0x%04x)", tm.tm_hour, tm.tm_min, tm.tm_sec, dos_time);
		}
	}

	offset += 4;

	return offset;
}


static const value_string da_access_vals[] = {
	{ 0,		"Open for reading"},
	{ 1,		"Open for writing"},
	{ 2,		"Open for reading and writing"},
	{ 3,		"Open for execute"},
	{0, NULL}
};
static const value_string da_sharing_vals[] = {
	{ 0,		"Compatibility mode"},
	{ 1,		"Deny read/write/execute (exclusive)"},
	{ 2,		"Deny write"},
	{ 3,		"Deny read/execute"},
	{ 4,		"Deny none"},
	{0, NULL}
};
static const value_string da_locality_vals[] = {
	{ 0,		"Locality of reference unknown"},
	{ 1,		"Mainly sequential access"},
	{ 2,		"Mainly random access"},
	{ 3,		"Random access with some locality"},
	{0, NULL}
};
static const true_false_string tfs_da_caching = {
	"Do not cache this file",
	"Caching permitted on this file"
};
static const true_false_string tfs_da_writetru = {
	"Write through enabled",
	"Write through disabled"
};
static int
dissect_access(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset, char *type)
{
	guint16 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohs(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 2,
			"%s Access: 0x%04x", type, mask);
		tree = proto_item_add_subtree(item, ett_smb_desiredaccess);
	}

	proto_tree_add_boolean(tree, hf_smb_access_writetru,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_access_caching,
		tvb, offset, 2, mask);
	proto_tree_add_uint(tree, hf_smb_access_locality,
		tvb, offset, 2, mask);
	proto_tree_add_uint(tree, hf_smb_access_sharing,
		tvb, offset, 2, mask);
	proto_tree_add_uint(tree, hf_smb_access_mode,
		tvb, offset, 2, mask);

	offset += 2;

	return offset;
}

#define FILE_ATTRIBUTE_READ_ONLY		0x00000001
#define FILE_ATTRIBUTE_HIDDEN			0x00000002
#define FILE_ATTRIBUTE_SYSTEM			0x00000004
#define FILE_ATTRIBUTE_VOLUME			0x00000008
#define FILE_ATTRIBUTE_DIRECTORY		0x00000010
#define FILE_ATTRIBUTE_ARCHIVE			0x00000020
#define FILE_ATTRIBUTE_DEVICE			0x00000040
#define FILE_ATTRIBUTE_NORMAL			0x00000080
#define FILE_ATTRIBUTE_TEMPORARY		0x00000100
#define FILE_ATTRIBUTE_SPARSE			0x00000200
#define FILE_ATTRIBUTE_REPARSE			0x00000400
#define FILE_ATTRIBUTE_COMPRESSED		0x00000800
#define FILE_ATTRIBUTE_OFFLINE			0x00001000
#define FILE_ATTRIBUTE_NOT_CONTENT_INDEXED	0x00002000
#define FILE_ATTRIBUTE_ENCRYPTED		0x00004000

/*
 * These are flags to be used in NT Create operations.
 */
#define FILE_ATTRIBUTE_WRITE_THROUGH		0x80000000
#define FILE_ATTRIBUTE_NO_BUFFERING		0x20000000
#define FILE_ATTRIBUTE_RANDOM_ACCESS		0x10000000
#define FILE_ATTRIBUTE_SEQUENTIAL_SCAN		0x08000000
#define FILE_ATTRIBUTE_DELETE_ON_CLOSE		0x04000000
#define FILE_ATTRIBUTE_BACKUP_SEMANTICS		0x02000000
#define FILE_ATTRIBUTE_POSIX_SEMANTICS		0x01000000

static const true_false_string tfs_file_attribute_write_through = {
	"This object requires WRITE THROUGH",
	"This object does NOT require write through",
};
static const true_false_string tfs_file_attribute_no_buffering = {
	"This object requires NO BUFFERING",
	"This object can be buffered",
};
static const true_false_string tfs_file_attribute_random_access = {
	"This object will be RANDOM ACCESSed",
	"Random access is NOT requested",
};
static const true_false_string tfs_file_attribute_sequential_scan = {
	"This object is optimized for SEQUENTIAL SCAN",
	"This object is NOT optimized for sequential scan",
};
static const true_false_string tfs_file_attribute_delete_on_close = {
	"This object will be DELETED ON CLOSE",
	"This object will not be deleted on close",
};
static const true_false_string tfs_file_attribute_backup_semantics = {
	"This object supports BACKUP SEMANTICS",
	"This object does NOT support backup semantics",
};
static const true_false_string tfs_file_attribute_posix_semantics = {
	"This object supports POSIX SEMANTICS",
	"This object does NOT support POSIX semantics",
};
static const true_false_string tfs_file_attribute_read_only = {
	"This file is READ ONLY",
	"This file is NOT read only",
};
static const true_false_string tfs_file_attribute_hidden = {
	"This is a HIDDEN file",
	"This is NOT a hidden file"
};
static const true_false_string tfs_file_attribute_system = {
	"This is a SYSTEM file",
	"This is NOT a system file"
};
static const true_false_string tfs_file_attribute_volume = {
	"This is a volume ID",
	"This is NOT a volume ID"
};
static const true_false_string tfs_file_attribute_directory = {
	"This is a DIRECTORY",
	"This is NOT a directory"
};
static const true_false_string tfs_file_attribute_archive = {
	"This is an ARCHIVE file",
	"This is NOT an archive file"
};
static const true_false_string tfs_file_attribute_device = {
	"This is a DEVICE",
	"This is NOT a device"
};
static const true_false_string tfs_file_attribute_normal = {
	"This file is an ordinary file",
	"This file has some attribute set"
};
static const true_false_string tfs_file_attribute_temporary = {
	"This is a TEMPORARY file",
	"This is NOT a temporary file"
};
static const true_false_string tfs_file_attribute_sparse = {
	"This is a SPARSE file",
	"This is NOT a sparse file"
};
static const true_false_string tfs_file_attribute_reparse = {
	"This file has an associated REPARSE POINT",
	"This file does NOT have an associated reparse point"
};
static const true_false_string tfs_file_attribute_compressed = {
	"This is a COMPRESSED file",
	"This is NOT a compressed file"
};
static const true_false_string tfs_file_attribute_offline = {
	"This file is OFFLINE",
	"This file is NOT offline"
};
static const true_false_string tfs_file_attribute_not_content_indexed = {
	"This file MAY NOT be indexed by the CONTENT INDEXING service",
	"This file MAY be indexed by the content indexing service"
};
static const true_false_string tfs_file_attribute_encrypted = {
	"This is an ENCRYPTED file",
	"This is NOT an encrypted file"
};

static int
dissect_file_attributes(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint16 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohs(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 2,
			"File Attributes: 0x%04x", mask);
		tree = proto_item_add_subtree(item, ett_smb_file_attributes);
	}
	proto_tree_add_boolean(tree, hf_smb_file_attr_read_only_16bit,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_hidden_16bit,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_system_16bit,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_volume_16bit,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_directory_16bit,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_archive_16bit,
		tvb, offset, 2, mask);

	offset += 2;

	return offset;
}

/* 3.11 */
static int
dissect_file_ext_attr(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint32 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohl(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 4,
			"File Attributes: 0x%08x", mask);
		tree = proto_item_add_subtree(item, ett_smb_file_attributes);
	}

	proto_tree_add_boolean(tree, hf_smb_file_eattr_write_through,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_no_buffering,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_random_access,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_sequential_scan,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_delete_on_close,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_backup_semantics,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_posix_semantics,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_encrypted,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_not_content_indexed,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_offline,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_compressed,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_reparse,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_sparse,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_temporary,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_normal,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_device,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_archive,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_directory,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_volume,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_system,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_hidden,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_file_eattr_read_only,
		tvb, offset, 4, mask);

	offset += 4;

	return offset;
}

static int
dissect_dir_info_file_attributes(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint8 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_guint8(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 1,
			"File Attributes: 0x%02x", mask);
		tree = proto_item_add_subtree(item, ett_smb_file_attributes);
	}
	proto_tree_add_boolean(tree, hf_smb_file_attr_read_only_8bit,
		tvb, offset, 1, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_hidden_8bit,
		tvb, offset, 1, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_system_8bit,
		tvb, offset, 1, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_volume_8bit,
		tvb, offset, 1, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_directory_8bit,
		tvb, offset, 1, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_archive_8bit,
		tvb, offset, 1, mask);

	offset += 1;

	return offset;
}

static int
dissect_search_attributes(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint16 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohs(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 2,
			"Search Attributes: 0x%04x", mask);
		tree = proto_item_add_subtree(item, ett_smb_search);
	}

	proto_tree_add_boolean(tree, hf_smb_search_attribute_read_only,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_search_attribute_hidden,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_search_attribute_system,
		tvb, offset, 2, mask);	
	proto_tree_add_boolean(tree, hf_smb_search_attribute_volume,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_search_attribute_directory,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_search_attribute_archive,
		tvb, offset, 2, mask);

	offset += 2;
	return offset;
}

static int
dissect_extended_file_attributes(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint32 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohl(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 2,
			"File Attributes: 0x%08x", mask);
		tree = proto_item_add_subtree(item, ett_smb_file_attributes);
	}
	proto_tree_add_boolean(tree, hf_smb_file_attr_read_only_16bit,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_hidden_16bit,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_system_16bit,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_volume_16bit,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_directory_16bit,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_archive_16bit,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_device,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_normal,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_temporary,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_sparse,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_reparse,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_compressed,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_offline,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_not_content_indexed,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_file_attr_encrypted,
		tvb, offset, 2, mask);

	offset += 2;

	return offset;
}


#define SERVER_CAP_RAW_MODE            0x00000001
#define SERVER_CAP_MPX_MODE            0x00000002
#define SERVER_CAP_UNICODE             0x00000004
#define SERVER_CAP_LARGE_FILES         0x00000008
#define SERVER_CAP_NT_SMBS             0x00000010
#define SERVER_CAP_RPC_REMOTE_APIS     0x00000020
#define SERVER_CAP_STATUS32            0x00000040
#define SERVER_CAP_LEVEL_II_OPLOCKS    0x00000080
#define SERVER_CAP_LOCK_AND_READ       0x00000100
#define SERVER_CAP_NT_FIND             0x00000200
#define SERVER_CAP_DFS                 0x00001000
#define SERVER_CAP_INFOLEVEL_PASSTHRU  0x00002000
#define SERVER_CAP_LARGE_READX         0x00004000
#define SERVER_CAP_LARGE_WRITEX        0x00008000
#define SERVER_CAP_UNIX                0x00800000
#define SERVER_CAP_RESERVED            0x02000000
#define SERVER_CAP_BULK_TRANSFER       0x20000000
#define SERVER_CAP_COMPRESSED_DATA     0x40000000
#define SERVER_CAP_EXTENDED_SECURITY   0x80000000
static const true_false_string tfs_server_cap_raw_mode = {
	"Read Raw and Write Raw are supported",
	"Read Raw and Write Raw are not supported"
};
static const true_false_string tfs_server_cap_mpx_mode = {
	"Read Mpx and Write Mpx are supported",
	"Read Mpx and Write Mpx are not supported"
};
static const true_false_string tfs_server_cap_unicode = {
	"Unicode strings are supported",
	"Unicode strings are not supported"
};
static const true_false_string tfs_server_cap_large_files = {
	"Large files are supported",
	"Large files are not supported",
};
static const true_false_string tfs_server_cap_nt_smbs = {
	"NT SMBs are supported",
	"NT SMBs are not supported"
};
static const true_false_string tfs_server_cap_rpc_remote_apis = {
	"RPC remote APIs are supported",
	"RPC remote APIs are not supported"
};
static const true_false_string tfs_server_cap_nt_status = {
	"NT status codes are supported",
	"NT status codes are not supported"
};
static const true_false_string tfs_server_cap_level_ii_oplocks = {
	"Level 2 oplocks are supported",
	"Level 2 oplocks are not supported"
};
static const true_false_string tfs_server_cap_lock_and_read = {
	"Lock and Read is supported",
	"Lock and Read is not supported"
};
static const true_false_string tfs_server_cap_nt_find = {
	"NT Find is supported",
	"NT Find is not supported"
};
static const true_false_string tfs_server_cap_dfs = {
	"Dfs is supported",
	"Dfs is not supported"
};
static const true_false_string tfs_server_cap_infolevel_passthru = {
	"NT information level request passthrough is supported",
	"NT information level request passthrough is not supported"
};
static const true_false_string tfs_server_cap_large_readx = {
	"Large Read andX is supported",
	"Large Read andX is not supported"
};
static const true_false_string tfs_server_cap_large_writex = {
	"Large Write andX is supported",
	"Large Write andX is not supported"
};
static const true_false_string tfs_server_cap_unix = {
	"UNIX extensions are supported",
	"UNIX extensions are not supported"
};
static const true_false_string tfs_server_cap_reserved = {
	"Reserved",
	"Reserved"
};
static const true_false_string tfs_server_cap_bulk_transfer = {
	"Bulk Read and Bulk Write are supported",
	"Bulk Read and Bulk Write are not supported"
};
static const true_false_string tfs_server_cap_compressed_data = {
	"Compressed data transfer is supported",
	"Compressed data transfer is not supported"
};
static const true_false_string tfs_server_cap_extended_security = {
	"Extended security exchanges are supported",
	"Extended security exchanges are not supported"
};
static int
dissect_negprot_capabilities(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint32 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohl(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 4, "Capabilities: 0x%08x", mask);
		tree = proto_item_add_subtree(item, ett_smb_capabilities);
	}

	proto_tree_add_boolean(tree, hf_smb_server_cap_raw_mode,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_server_cap_mpx_mode,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_server_cap_unicode,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_server_cap_large_files,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_server_cap_nt_smbs,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_server_cap_rpc_remote_apis,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_server_cap_nt_status,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_server_cap_level_ii_oplocks,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_server_cap_lock_and_read,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_server_cap_nt_find,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_server_cap_dfs,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_server_cap_infolevel_passthru,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_server_cap_large_readx,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_server_cap_large_writex,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_server_cap_unix,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_server_cap_reserved,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_server_cap_bulk_transfer,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_server_cap_compressed_data,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_server_cap_extended_security,
		tvb, offset, 4, mask);

	return mask;
}

#define RAWMODE_READ   0x01
#define RAWMODE_WRITE  0x02
static const true_false_string tfs_rm_read = {
	"Read Raw is supported",
	"Read Raw is not supported"
};
static const true_false_string tfs_rm_write = {
	"Write Raw is supported",
	"Write Raw is not supported"
};

static int
dissect_negprot_rawmode(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint16 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohs(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 2, "Raw Mode: 0x%04x", mask);
		tree = proto_item_add_subtree(item, ett_smb_rawmode);
	}

	proto_tree_add_boolean(tree, hf_smb_rm_read, tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_rm_write, tvb, offset, 2, mask);

	offset += 2;

	return offset;
}

#define SECURITY_MODE_MODE             0x01
#define SECURITY_MODE_PASSWORD         0x02
#define SECURITY_MODE_SIGNATURES       0x04
#define SECURITY_MODE_SIG_REQUIRED     0x08
static const true_false_string tfs_sm_mode = {
	"USER security mode",
	"SHARE security mode"
};
static const true_false_string tfs_sm_password = {
	"ENCRYPTED password. Use challenge/response",
	"PLAINTEXT password"
};
static const true_false_string tfs_sm_signatures = {
	"Security signatures ENABLED",
	"Security signatures NOT enabled"
};
static const true_false_string tfs_sm_sig_required = {
	"Security signatures REQUIRED",
	"Security signatures NOT required"
};

static int
dissect_negprot_security_mode(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset, int wc)
{
	guint16 mask = 0;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	switch(wc){
	case 13:
		mask = tvb_get_letohs(tvb, offset);
		item = proto_tree_add_text(parent_tree, tvb, offset, 2,
				"Security Mode: 0x%04x", mask);
		tree = proto_item_add_subtree(item, ett_smb_mode);
		proto_tree_add_boolean(tree, hf_smb_sm_mode16, tvb, offset, 2, mask);
		proto_tree_add_boolean(tree, hf_smb_sm_password16, tvb, offset, 2, mask);
		offset += 2;
		break;

	case 17:
		mask = tvb_get_guint8(tvb, offset);
		item = proto_tree_add_text(parent_tree, tvb, offset, 1,
				"Security Mode: 0x%02x", mask);
		tree = proto_item_add_subtree(item, ett_smb_mode);
		proto_tree_add_boolean(tree, hf_smb_sm_mode, tvb, offset, 1, mask);
		proto_tree_add_boolean(tree, hf_smb_sm_password, tvb, offset, 1, mask);
		proto_tree_add_boolean(tree, hf_smb_sm_signatures, tvb, offset, 1, mask);
		proto_tree_add_boolean(tree, hf_smb_sm_sig_required, tvb, offset, 1, mask);
		offset += 1;
		break;
	}

	return offset;
}

static int
dissect_negprot_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	proto_item *it = NULL;
	proto_tree *tr = NULL;
	guint16 bc;
	guint8 wc;

	WORD_COUNT;

	BYTE_COUNT;

	if(tree){
		it = proto_tree_add_text(tree, tvb, offset, bc,
				"Requested Dialects");
		tr = proto_item_add_subtree(it, ett_smb_dialects);
	}

	while(bc){
		int len;
		int old_offset = offset;
		const guint8 *str;
		proto_item *dit = NULL;
		proto_tree *dtr = NULL;

		/* XXX - what if this runs past bc? */
		len = tvb_strsize(tvb, offset+1);
		str = tvb_get_ptr(tvb, offset+1, len);

		if(tr){
			dit = proto_tree_add_text(tr, tvb, offset, len+1,
					"Dialect: %s", str);
			dtr = proto_item_add_subtree(dit, ett_smb_dialect);
		}

		/* Buffer Format */
		CHECK_BYTE_COUNT(1);
		proto_tree_add_item(dtr, hf_smb_buffer_format, tvb, offset, 1,
			TRUE);
		COUNT_BYTES(1);

		/*Dialect Name */
		CHECK_BYTE_COUNT(len);
		proto_tree_add_string(dtr, hf_smb_dialect_name, tvb, offset,
			len, str);
		COUNT_BYTES(len);
	}

	END_OF_SMB

	return offset;
}

static int
dissect_negprot_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 dialect;
	const char *dn;
	int dn_len;
	guint16 bc;
	guint16 ekl=0;
	guint32 caps=0;
	gint16 tz;

	WORD_COUNT;

	/* Dialect Index */
	dialect = tvb_get_letohs(tvb, offset);
	switch(wc){
	case 1:
		if(dialect==0xffff){
			proto_tree_add_uint_format(tree, hf_smb_dialect_index,
				tvb, offset, 2, dialect,
				"Selected Index: -1, PC NETWORK PROGRAM 1.0 choosen");
		} else {
			proto_tree_add_uint(tree, hf_smb_dialect_index,
				tvb, offset, 2, dialect);
		}
		break;
	case 13:
		proto_tree_add_uint_format(tree, hf_smb_dialect_index,
			tvb, offset, 2, dialect,
			"Dialect Index: %u, Greater than CORE PROTOCOL and up to LANMAN2.1", dialect);
		break;
	case 17:
		proto_tree_add_uint_format(tree, hf_smb_dialect_index,
			tvb, offset, 2, dialect,
			"Dialect Index: %u, greater than LANMAN2.1", dialect);
		break;
	default:
		proto_tree_add_text(tree, tvb, offset, wc*2,
			"Words for unknown response format");
		offset += wc*2;
		goto bytecount;
	}
	offset += 2;

	switch(wc){
	case 13:
		/* Security Mode */
		offset = dissect_negprot_security_mode(tvb, pinfo, tree, offset,
				wc);

		/* Maximum Transmit Buffer Size */
		proto_tree_add_item(tree, hf_smb_max_trans_buf_size,
			tvb, offset, 2, TRUE);
		offset += 2;

		/* Maximum Multiplex Count */
		proto_tree_add_item(tree, hf_smb_max_mpx_count,
			tvb, offset, 2, TRUE);
		offset += 2;

		/* Maximum Vcs Number */
		proto_tree_add_item(tree, hf_smb_max_vcs_num,
			tvb, offset, 2, TRUE);
		offset += 2;

		/* raw mode */
		offset = dissect_negprot_rawmode(tvb, pinfo, tree, offset);

		/* session key */
		proto_tree_add_item(tree, hf_smb_session_key,
			tvb, offset, 4, TRUE);
		offset += 4;

		/* current time and date at server */
		offset = dissect_smb_datetime(tvb, pinfo, tree, offset, hf_smb_server_date_time, hf_smb_server_smb_date, hf_smb_server_smb_time,
		    TRUE);

		/* time zone */
		tz = tvb_get_letohs(tvb, offset);
		proto_tree_add_int_format(tree, hf_smb_server_timezone, tvb, offset, 2, tz, "Server Time Zone: %d min from UTC", tz);
		offset += 2;

		/* encryption key length */
		ekl = tvb_get_letohs(tvb, offset);
		proto_tree_add_uint(tree, hf_smb_encryption_key_length, tvb, offset, 2, ekl);
		offset += 2;

		/* 2 reserved bytes */
		proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 2, TRUE);
		offset += 2;

		break;

	case 17:
		/* Security Mode */
		offset = dissect_negprot_security_mode(tvb, pinfo, tree, offset, wc);

		/* Maximum Multiplex Count */
		proto_tree_add_item(tree, hf_smb_max_mpx_count,
			tvb, offset, 2, TRUE);
		offset += 2;

		/* Maximum Vcs Number */
		proto_tree_add_item(tree, hf_smb_max_vcs_num,
			tvb, offset, 2, TRUE);
		offset += 2;

		/* Maximum Transmit Buffer Size */
		proto_tree_add_item(tree, hf_smb_max_trans_buf_size,
			tvb, offset, 4, TRUE);
		offset += 4;

		/* maximum raw buffer size */
		proto_tree_add_item(tree, hf_smb_max_raw_buf_size,
			tvb, offset, 4, TRUE);
		offset += 4;

		/* session key */
		proto_tree_add_item(tree, hf_smb_session_key,
			tvb, offset, 4, TRUE);
		offset += 4;

		/* server capabilities */
		caps = dissect_negprot_capabilities(tvb, pinfo, tree, offset);
		offset += 4;

		/* system time */
		offset = dissect_smb_64bit_time(tvb, pinfo, tree, offset,
			       	"System Time", hf_smb_system_time);

		/* time zone */
		tz = tvb_get_letohs(tvb, offset);
		proto_tree_add_int_format(tree, hf_smb_server_timezone,
			tvb, offset, 2, tz,
			"Server Time Zone: %d min from UTC", tz);
		offset += 2;

		/* encryption key length */
		ekl = tvb_get_guint8(tvb, offset);
		proto_tree_add_uint(tree, hf_smb_encryption_key_length,
			tvb, offset, 1, ekl);
		offset += 1;

		break;
	}

	BYTE_COUNT;

	switch(wc){
	case 13:
		/* challenge/response encryption key */
		if(ekl){
			CHECK_BYTE_COUNT(ekl);
			proto_tree_add_item(tree, hf_smb_encryption_key, tvb, offset, ekl, TRUE);
			COUNT_BYTES(ekl);
		}

		/* domain */
		dn = get_unicode_or_ascii_string_tvb(tvb, &offset,
			pinfo, &dn_len, FALSE, FALSE, &bc);
		if (dn == NULL)
			goto endofcommand;
		proto_tree_add_string(tree, hf_smb_primary_domain, tvb,
			offset, dn_len,dn);
		COUNT_BYTES(dn_len);
		break;

	case 17:
		if(!(caps&SERVER_CAP_EXTENDED_SECURITY)){
			smb_info_t *si;

			/* challenge/response encryption key */
			/* XXX - is this aligned on an even boundary? */
			if(ekl){
				CHECK_BYTE_COUNT(ekl);
				proto_tree_add_item(tree, hf_smb_encryption_key,
					tvb, offset, ekl, TRUE);
				COUNT_BYTES(ekl);
			}

			/* domain */
			/* this string is special, unicode is flagged in caps */
			/* This string is NOT padded to be 16bit aligned. (seen in actual capture) */
			si = pinfo->private_data;
			si->unicode = (caps&SERVER_CAP_UNICODE);
			dn = get_unicode_or_ascii_string_tvb(tvb,
				&offset, pinfo, &dn_len, TRUE, FALSE,
				&bc);
			if (dn == NULL)
				goto endofcommand;
			proto_tree_add_string(tree, hf_smb_primary_domain,
				tvb, offset, dn_len, dn);
			COUNT_BYTES(dn_len);
		} else {
			int len;

			/* guid */
			/* XXX - show it in the standard Microsoft format
			   for GUIDs? */
			CHECK_BYTE_COUNT(16);
			proto_tree_add_item(tree, hf_smb_server_guid,
				tvb, offset, 16, TRUE);
			COUNT_BYTES(16);

			/* security blob */
			/* XXX - is this ASN.1-encoded?  Is it a Kerberos
			   data structure, at least in NT 5.0-and-later
			   server replies? */
			if(bc){
				proto_tree_add_item(tree, hf_smb_security_blob,
					tvb, offset, bc, TRUE);
				COUNT_BYTES(bc);
			}
		}
		break;
	}

	END_OF_SMB

	return offset;
}


static int
dissect_old_dir_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	int dn_len;
	const char *dn;
	guint8 wc;
	guint16 bc;

	WORD_COUNT;
 
	BYTE_COUNT;

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* dir name */
	dn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &dn_len,
		FALSE, FALSE, &bc);
	if (dn == NULL)
		goto endofcommand;
	proto_tree_add_string(tree, hf_smb_dir_name, tvb, offset, dn_len,
		dn);
	COUNT_BYTES(dn_len);

	if (check_col(pinfo->fd, COL_INFO)) {
		col_append_fstr(pinfo->fd, COL_INFO, ", Directory: %s", dn);
	}

	END_OF_SMB

	return offset;
}

static int
dissect_empty(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 bc;
 
	WORD_COUNT;
 
	BYTE_COUNT;

	END_OF_SMB

	return offset;
}

static int
dissect_echo_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint16 ec, bc;
	guint8 wc;

	WORD_COUNT;

	/* echo count */
	ec = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_echo_count, tvb, offset, 2, ec);
	offset += 2;

	BYTE_COUNT;

	if (bc != 0) {
		/* echo data */
		proto_tree_add_item(tree, hf_smb_echo_data, tvb, offset, bc, TRUE);
		COUNT_BYTES(bc);
	}

	END_OF_SMB

	return offset;
}

static int
dissect_echo_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint16 bc;
	guint8 wc;

	WORD_COUNT;

	/* echo sequence number */
	proto_tree_add_item(tree, hf_smb_echo_seq_num, tvb, offset, 2, TRUE);
	offset += 2;

	BYTE_COUNT;

	if (bc != 0) {
		/* echo data */
		proto_tree_add_item(tree, hf_smb_echo_data, tvb, offset, bc, TRUE);
		COUNT_BYTES(bc);
	}

	END_OF_SMB

	return offset;
}

static int
dissect_tree_connect_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	int an_len, pwlen;
	const char *an;
	guint8 wc;
	guint16 bc;

	WORD_COUNT;
 
	BYTE_COUNT;

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* Path */
	an = get_unicode_or_ascii_string_tvb(tvb, &offset,
		pinfo, &an_len, FALSE, FALSE, &bc);
	if (an == NULL)
		goto endofcommand;
	proto_tree_add_string(tree, hf_smb_path, tvb,
		offset, an_len, an);
	COUNT_BYTES(an_len);

	if (check_col(pinfo->fd, COL_INFO)) {
		col_append_fstr(pinfo->fd, COL_INFO, ", Path: %s", an);
	}

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* password, ANSI */
	/* XXX - what if this runs past bc? */
	pwlen = tvb_strsize(tvb, offset);
	CHECK_BYTE_COUNT(pwlen);
	proto_tree_add_item(tree, hf_smb_password,
		tvb, offset, pwlen, TRUE);
	COUNT_BYTES(pwlen);

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* Service */
	an = get_unicode_or_ascii_string_tvb(tvb, &offset,
		pinfo, &an_len, FALSE, FALSE, &bc);
	if (an == NULL)
		goto endofcommand;
	proto_tree_add_string(tree, hf_smb_service, tvb,
		offset, an_len, an);
	COUNT_BYTES(an_len);

	END_OF_SMB

	return offset;
}

static int
dissect_tree_connect_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 bc;

	WORD_COUNT;
 
	/* Maximum Buffer Size */
	proto_tree_add_item(tree, hf_smb_max_buf_size, tvb, offset, 2, TRUE);
	offset += 2;

	/* tid */
	proto_tree_add_item(tree, hf_smb_tid, tvb, offset, 2, TRUE);
	offset += 2;

	BYTE_COUNT;

	END_OF_SMB

	return offset;
}
 

static const true_false_string tfs_of_create = {
	"Create file if it does not exist",
	"Fail if file does not exist"
};
static const value_string of_open[] = {
	{ 0,		"Fail if file exists"},
	{ 1,		"Open file if it exists"},
	{ 2,		"Truncate file if it exists"},
	{0, NULL}
};
static int
dissect_open_function(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint16 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohs(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 2,
			"Open Function: 0x%04x", mask);
		tree = proto_item_add_subtree(item, ett_smb_openfunction);
	}

	proto_tree_add_boolean(tree, hf_smb_open_function_create,
		tvb, offset, 2, mask);
	proto_tree_add_uint(tree, hf_smb_open_function_open,
		tvb, offset, 2, mask);

	offset += 2;

	return offset;
}


static const true_false_string tfs_mf_file = {
	"Target must be a file",
	"Target needn't be a file"
 };
static const true_false_string tfs_mf_dir = {
	"Target must be a directory",
	"Target needn't be a directory"
};
static const true_false_string tfs_mf_verify = {
	"MUST verify all writes",
	"Don't have to verify writes"
};
static int
dissect_move_flags(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint16 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohs(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 2,
			"Flags: 0x%04x", mask);
		tree = proto_item_add_subtree(item, ett_smb_move_flags);
	}
 
	proto_tree_add_boolean(tree, hf_smb_move_flags_verify,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_move_flags_dir,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_move_flags_file,
		tvb, offset, 2, mask);

	offset += 2;

	return offset;
}

static int
dissect_move_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	int fn_len;
	guint16 tid;
	guint16 bc;
	guint8 wc;
	const char *fn;

	WORD_COUNT;

	/* tid */
	tid = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint_format(tree, hf_smb_tid, tvb, offset, 2, tid,
		"TID (target): 0x%04x", tid);
	offset += 2;

	/* open function */
	offset = dissect_open_function(tvb, pinfo, tree, offset);

	/* move flags */
	offset = dissect_move_flags(tvb, pinfo, tree, offset);

	BYTE_COUNT;

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* file name */
	fn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &fn_len,
		FALSE, FALSE, &bc);
	if (fn == NULL)
		goto endofcommand;
	proto_tree_add_string_format(tree, hf_smb_file_name, tvb, offset,
		fn_len,	fn, "Old File Name: %s", fn);
	COUNT_BYTES(fn_len);

	if (check_col(pinfo->fd, COL_INFO)) {
		col_append_fstr(pinfo->fd, COL_INFO, ", Old Name: %s", fn);
	}

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* file name */
	fn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &fn_len,
		FALSE, FALSE, &bc);
	if (fn == NULL)
		goto endofcommand;
	proto_tree_add_string_format(tree, hf_smb_file_name, tvb, offset,
		fn_len,	fn, "New File Name: %s", fn);
	COUNT_BYTES(fn_len);

	if (check_col(pinfo->fd, COL_INFO)) {
		col_append_fstr(pinfo->fd, COL_INFO, ", New Name: %s", fn);
	}

	END_OF_SMB

	return offset;
}

static int
dissect_move_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	int fn_len;
	const char *fn;
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* read count */
	proto_tree_add_item(tree, hf_smb_count, tvb, offset, 2, TRUE);
	offset += 2;

	BYTE_COUNT;

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* file name */
	fn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &fn_len,
		FALSE, FALSE, &bc);
	if (fn == NULL)
		goto endofcommand;
	proto_tree_add_string(tree, hf_smb_file_name, tvb, offset, fn_len,
		fn);
	COUNT_BYTES(fn_len);

	END_OF_SMB

	return offset;
}

static int
dissect_open_file_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	int fn_len;
	const char *fn;
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* desired access */
	offset = dissect_access(tvb, pinfo, tree, offset, "Desired");

	/* Search Attributes */
	offset = dissect_search_attributes(tvb, pinfo, tree, offset);

	BYTE_COUNT;

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* file name */
	fn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &fn_len,
		FALSE, FALSE, &bc);
	if (fn == NULL)
		goto endofcommand;
	proto_tree_add_string(tree, hf_smb_file_name, tvb, offset, fn_len,
		fn);
	COUNT_BYTES(fn_len);

	if (check_col(pinfo->fd, COL_INFO)) {
		col_append_fstr(pinfo->fd, COL_INFO, ", Path: %s", fn);
	}

	END_OF_SMB

	return offset;
}

static int
dissect_open_file_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* fid */
	proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
	offset += 2;

	/* File Attributes */
	offset = dissect_file_attributes(tvb, pinfo, tree, offset);

	/* last write time */
	offset = dissect_smb_UTIME(tvb, pinfo, tree, offset, hf_smb_last_write_time);
	
	/* File Size */
	proto_tree_add_item(tree, hf_smb_file_size, tvb, offset, 4, TRUE);
	offset += 4;

	/* granted access */
	offset = dissect_access(tvb, pinfo, tree, offset, "Granted");

	BYTE_COUNT;

	END_OF_SMB

	return offset;
}

static int
dissect_fid(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* fid */
	proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
	offset += 2;

	BYTE_COUNT;

	END_OF_SMB

	return offset;
}

static int
dissect_create_file_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	int fn_len;
	const char *fn;
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* file attributes */
	offset = dissect_file_attributes(tvb, pinfo, tree, offset);

	/* creation time */
	offset = dissect_smb_UTIME(tvb, pinfo, tree, offset, hf_smb_create_time);

	BYTE_COUNT;

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* File Name */
	fn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &fn_len,
		FALSE, FALSE, &bc);
	if (fn == NULL)
		goto endofcommand;
	proto_tree_add_string(tree, hf_smb_file_name, tvb, offset, fn_len,
		fn);
	COUNT_BYTES(fn_len);

	if (check_col(pinfo->fd, COL_INFO)) {
		col_append_fstr(pinfo->fd, COL_INFO, ", Path: %s", fn);
	}

	END_OF_SMB

	return offset;
}

static int
dissect_close_file_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* fid */
	proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
	offset += 2;

	/* last write time */
	offset = dissect_smb_UTIME(tvb, pinfo, tree, offset, hf_smb_last_write_time);

	BYTE_COUNT;

	END_OF_SMB

	return offset;
}

static int
dissect_delete_file_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	int fn_len;
	const char *fn;
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* search attributes */
	offset = dissect_search_attributes(tvb, pinfo, tree, offset);

	BYTE_COUNT;

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* file name */
	fn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &fn_len,
		FALSE, FALSE, &bc);
	if (fn == NULL)
		goto endofcommand;
	proto_tree_add_string(tree, hf_smb_file_name, tvb, offset, fn_len,
		fn);
	COUNT_BYTES(fn_len);

	if (check_col(pinfo->fd, COL_INFO)) {
		col_append_fstr(pinfo->fd, COL_INFO, ", Path: %s", fn);
	}

	END_OF_SMB

	return offset;
}

static int
dissect_rename_file_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	int fn_len;
	const char *fn;
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* search attributes */
	offset = dissect_search_attributes(tvb, pinfo, tree, offset);

	BYTE_COUNT;

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* old file name */
	fn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &fn_len,
		FALSE, FALSE, &bc);
	if (fn == NULL)
		goto endofcommand;
	proto_tree_add_string(tree, hf_smb_old_file_name, tvb, offset, fn_len,
		fn);
	COUNT_BYTES(fn_len);

	if (check_col(pinfo->fd, COL_INFO)) {
		col_append_fstr(pinfo->fd, COL_INFO, ", Old Name: %s", fn);
	}

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* file name */
	fn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &fn_len,
		FALSE, FALSE, &bc);
	if (fn == NULL)
		goto endofcommand;
	proto_tree_add_string(tree, hf_smb_file_name, tvb, offset, fn_len,
		fn);
	COUNT_BYTES(fn_len);

	if (check_col(pinfo->fd, COL_INFO)) {
		col_append_fstr(pinfo->fd, COL_INFO, ", New Name: %s", fn);
	}

	END_OF_SMB

	return offset;
}

static int
dissect_query_information_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint16 bc;
	guint8 wc;
	const char *fn;
	int fn_len;

	WORD_COUNT;

	BYTE_COUNT;

	/* Buffer Format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* File Name */
	fn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &fn_len,
		FALSE, FALSE, &bc);
	if (fn == NULL)
		goto endofcommand;
	proto_tree_add_string(tree, hf_smb_file_name, tvb, offset, fn_len,
		fn);
	COUNT_BYTES(fn_len);

	if (check_col(pinfo->fd, COL_INFO)) {
		col_append_fstr(pinfo->fd, COL_INFO, ", Path: %s", fn);
	}

	END_OF_SMB

	return offset;
}
 
static int
dissect_query_information_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint16 bc;
	guint8 wc;
	nstime_t ts;

	WORD_COUNT;

	/* File Attributes */
	offset = dissect_file_attributes(tvb, pinfo, tree, offset);

	/* Last Write Time */
	offset = dissect_smb_UTIME(tvb, pinfo, tree, offset, hf_smb_last_write_time);

	/* File Size */
	proto_tree_add_item(tree, hf_smb_file_size, tvb, offset, 4, TRUE);
	offset += 4;

	/* 10 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 10, TRUE);
	offset += 10;

	BYTE_COUNT;

	END_OF_SMB

	return offset;
}

static int
dissect_set_information_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	int fn_len;
	const char *fn;
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* file attributes */
	offset = dissect_file_attributes(tvb, pinfo, tree, offset);

	/* last write time */
	offset = dissect_smb_UTIME(tvb, pinfo, tree, offset, hf_smb_last_write_time);

	/* 10 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 10, TRUE);
	offset += 10;

	BYTE_COUNT;

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* file name */
	fn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &fn_len,
		FALSE, FALSE, &bc);
	if (fn == NULL)
		goto endofcommand;
	proto_tree_add_string(tree, hf_smb_file_name, tvb, offset, fn_len,
		fn);
	COUNT_BYTES(fn_len);

	if (check_col(pinfo->fd, COL_INFO)) {
		col_append_fstr(pinfo->fd, COL_INFO, ", Path: %s", fn);
	}

	END_OF_SMB

	return offset;
}

static int
dissect_read_file_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* fid */
	proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
	offset += 2;

	/* read count */
	proto_tree_add_item(tree, hf_smb_count, tvb, offset, 2, TRUE);
	offset += 2;

	/* offset */
	proto_tree_add_item(tree, hf_smb_offset, tvb, offset, 4, TRUE);
	offset += 4;

	/* remaining */
	proto_tree_add_item(tree, hf_smb_remaining, tvb, offset, 2, TRUE);
	offset += 2;

	BYTE_COUNT;

	END_OF_SMB

	return offset;
}

static int
dissect_read_file_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint16 cnt=0, bc;
	guint8 wc;

	WORD_COUNT;

	/* read count */
	cnt = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_count, tvb, offset, 2, cnt);
	offset += 2;

	/* 8 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 8, TRUE);
	offset += 8;

	BYTE_COUNT;

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* data len */
	CHECK_BYTE_COUNT(2);
	proto_tree_add_item(tree, hf_smb_data_len, tvb, offset, 2, TRUE);
	COUNT_BYTES(2);

	if (bc != 0) {
		/* file data */
		int len = tvb_length_remaining(tvb, offset);
		if(bc>len){
			proto_tree_add_bytes_format(tree, hf_smb_file_data, tvb, offset, len, tvb_get_ptr(tvb, offset, len),"File Data: Incomplete. Only %u of %u bytes", len, bc);
			offset += len;
		} else {
			proto_tree_add_item(tree, hf_smb_file_data, tvb, offset, bc, TRUE);
			offset += bc;
		}
		bc = 0;
	}

	END_OF_SMB

	return offset;
}

static int
dissect_lock_and_read_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint16 cnt, bc;
	guint8 wc;

	WORD_COUNT;

	/* read count */
	cnt = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_count, tvb, offset, 2, cnt);
	offset += 2;

	/* 8 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 8, TRUE);
	offset += 8;

	BYTE_COUNT;

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* data len */
	CHECK_BYTE_COUNT(2);
	proto_tree_add_item(tree, hf_smb_data_len, tvb, offset, 2, TRUE);
	COUNT_BYTES(2);

	END_OF_SMB

	return offset;
}


static int
dissect_write_file_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint16 cnt=0, bc;
	guint8 wc;

	WORD_COUNT;

	/* fid */
	proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
	offset += 2;

	/* write count */
	cnt = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_count, tvb, offset, 2, cnt);
	offset += 2;

	/* offset */
	proto_tree_add_item(tree, hf_smb_offset, tvb, offset, 4, TRUE);
	offset += 4;

	/* remaining */
	proto_tree_add_item(tree, hf_smb_remaining, tvb, offset, 2, TRUE);
	offset += 2;

	BYTE_COUNT;

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* data len */
	CHECK_BYTE_COUNT(2);
	proto_tree_add_item(tree, hf_smb_data_len, tvb, offset, 2, TRUE);
	COUNT_BYTES(2);

	if (bc != 0) {
		/* file data */
		int len = tvb_length_remaining(tvb, offset);
		if(bc>len){
			proto_tree_add_bytes_format(tree, hf_smb_file_data, tvb, offset, len, tvb_get_ptr(tvb, offset, len),"File Data: Incomplete. Only %d of %d bytes", len, bc);
			offset += len;
		} else {
			proto_tree_add_item(tree, hf_smb_file_data, tvb, offset, bc, TRUE);
			offset += bc;
		}
		bc = 0;
	}

	END_OF_SMB

	return offset;
}
 
static int
dissect_write_file_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* write count */
	proto_tree_add_item(tree, hf_smb_count, tvb, offset, 2, TRUE);
	offset += 2;

	BYTE_COUNT;

	END_OF_SMB

	return offset;
}

static int
dissect_lock_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* fid */
	proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
	offset += 2;

	/* lock count */
	proto_tree_add_item(tree, hf_smb_count, tvb, offset, 4, TRUE);
	offset += 4;

	/* offset */
	proto_tree_add_item(tree, hf_smb_offset, tvb, offset, 4, TRUE);
	offset += 4;

	BYTE_COUNT;

	END_OF_SMB

	return offset;
}

static int
dissect_create_temporary_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	int fn_len;
	const char *fn;
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* 2 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 2, TRUE);
	offset += 2;

	/* Creation time */
	offset = dissect_smb_UTIME(tvb, pinfo, tree, offset, hf_smb_create_time);

	BYTE_COUNT;

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* directory name */
	fn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &fn_len,
		FALSE, FALSE, &bc);
	if (fn == NULL)
		goto endofcommand;
	proto_tree_add_string(tree, hf_smb_dir_name, tvb, offset, fn_len,
		fn);
	COUNT_BYTES(fn_len);

	if (check_col(pinfo->fd, COL_INFO)) {
		col_append_fstr(pinfo->fd, COL_INFO, ", Path: %s", fn);
	}

	END_OF_SMB

	return offset;
}

static int
dissect_create_temporary_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	int fn_len;
	const char *fn;
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* fid */
	proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
	offset += 2;

	BYTE_COUNT;

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* file name */
	fn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &fn_len,
		FALSE, FALSE, &bc);
	if (fn == NULL)
		goto endofcommand;
	proto_tree_add_string(tree, hf_smb_file_name, tvb, offset, fn_len,
		fn);
	COUNT_BYTES(fn_len);

	END_OF_SMB

	return offset;
}

static const value_string seek_mode_vals[] = {
	{0,	"From Start Of File"},
	{1,	"From Current Position"},
	{2,	"From End Of File"},
	{0,	NULL}
};

static int
dissect_seek_file_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* fid */
	proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
	offset += 2;

	/* Seek Mode */
	proto_tree_add_item(tree, hf_smb_seek_mode, tvb, offset, 2, TRUE);
	offset += 2;

	/* offset */
	proto_tree_add_item(tree, hf_smb_offset, tvb, offset, 4, TRUE);
	offset += 4;

	BYTE_COUNT;

	END_OF_SMB

	return offset;
}

static int
dissect_seek_file_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* offset */
	proto_tree_add_item(tree, hf_smb_offset, tvb, offset, 4, TRUE);
	offset += 4;

	BYTE_COUNT;

	END_OF_SMB

	return offset;
}
 
static int
dissect_set_information2_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* fid */
	proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
	offset += 2;

	/* create time */
	offset = dissect_smb_datetime(tvb, pinfo, tree, offset,
		hf_smb_create_time,
		hf_smb_create_dos_date, hf_smb_create_dos_time, FALSE);

	/* access time */
	offset = dissect_smb_datetime(tvb, pinfo, tree, offset,
		hf_smb_access_time,
		hf_smb_access_dos_date, hf_smb_access_dos_time, FALSE);

	/* last write time */
	offset = dissect_smb_datetime(tvb, pinfo, tree, offset,
		hf_smb_last_write_time,
		hf_smb_last_write_dos_date, hf_smb_last_write_dos_time, FALSE);

	BYTE_COUNT;

	END_OF_SMB

	return offset;
}

static int
dissect_query_information2_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* create time */
	offset = dissect_smb_datetime(tvb, pinfo, tree, offset,
		hf_smb_create_time,
		hf_smb_create_dos_date, hf_smb_create_dos_time, FALSE);

	/* access time */
	offset = dissect_smb_datetime(tvb, pinfo, tree, offset,
		hf_smb_access_time,
		hf_smb_access_dos_date, hf_smb_access_dos_time, FALSE);

	/* last write time */
	offset = dissect_smb_datetime(tvb, pinfo, tree, offset,
		hf_smb_last_write_time,
		hf_smb_last_write_dos_date, hf_smb_last_write_dos_time, FALSE);

	/* data size */
	proto_tree_add_item(tree, hf_smb_data_size, tvb, offset, 4, TRUE);
	offset += 4;

	/* allocation size */
	proto_tree_add_item(tree, hf_smb_alloc_size, tvb, offset, 4, TRUE);
	offset += 4;

	/* File Attributes */
	offset = dissect_file_attributes(tvb, pinfo, tree, offset);

	BYTE_COUNT;

	END_OF_SMB

	return offset;
}
 

static int
dissect_write_and_close_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 cnt=0;
	guint16 bc;
	int len;

	WORD_COUNT;

	/* fid */
	proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
	offset += 2;

	/* write count */
	cnt = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_count, tvb, offset, 2, cnt);
	offset += 2;

	/* offset */
	proto_tree_add_item(tree, hf_smb_offset, tvb, offset, 4, TRUE);
	offset += 4;

	/* last write time */
	offset = dissect_smb_UTIME(tvb, pinfo, tree, offset, hf_smb_last_write_time);
	
	if(wc==12){
		/* 12 reserved bytes */
		proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 12, TRUE);
		offset += 12;
	}

	BYTE_COUNT;

	/* 1 pad byte */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_padding, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);
	
	/*XXX Do we have to do something like in dissect_read_file_response()?
	      Must check some captures. */
	/* file data */
	len = tvb_length_remaining(tvb, offset);
	if(cnt>len){
		proto_tree_add_bytes_format(tree, hf_smb_file_data, tvb, offset, len, tvb_get_ptr(tvb, offset, len),"File Data: Incomplete. Only %d of %d bytes", len, cnt);
		offset += len;
	} else {
		proto_tree_add_item(tree, hf_smb_file_data, tvb, offset, cnt, TRUE);
		offset += cnt;
	}
	bc = 0;	/* XXX */

	END_OF_SMB

	return offset;
}
 
static int
dissect_write_and_close_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint16 cnt;
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* write count */
	cnt = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_count, tvb, offset, 2, cnt);
	offset += 2;

	BYTE_COUNT;

	END_OF_SMB

	return offset;
}

static int
dissect_read_raw_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 bc;
	guint32 to;

	WORD_COUNT;

	/* fid */
	proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
	offset += 2;

	/* offset */
	proto_tree_add_item(tree, hf_smb_offset, tvb, offset, 4, TRUE);
	offset += 4;

	/* max count */
	proto_tree_add_item(tree, hf_smb_max_count, tvb, offset, 2, TRUE);
	offset += 2;

	/* min count */
	proto_tree_add_item(tree, hf_smb_min_count, tvb, offset, 2, TRUE);
	offset += 2;

	/* timeout */
	to = tvb_get_letohl(tvb, offset);
	proto_tree_add_uint_format(tree, hf_smb_timeout, tvb, offset, 4, to, "Timeout: %s", time_msecs_to_str(to));
	offset += 4;

	/* 2 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 2, TRUE);
	offset += 2;

	if(wc==10){
		/* high offset */
		proto_tree_add_item(tree, hf_smb_high_offset, tvb, offset, 4, TRUE);
		offset += 4;
	}

	BYTE_COUNT;

	END_OF_SMB

	return offset;
}

static int
dissect_query_information_disk_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* units */
	proto_tree_add_item(tree, hf_smb_units, tvb, offset, 2, TRUE);
	offset += 2;

	/* bpu */
	proto_tree_add_item(tree, hf_smb_bpu, tvb, offset, 2, TRUE);
	offset += 2;

	/* block size */
	proto_tree_add_item(tree, hf_smb_blocksize, tvb, offset, 2, TRUE);
	offset += 2;

	/* free units */
	proto_tree_add_item(tree, hf_smb_freeunits, tvb, offset, 2, TRUE);
	offset += 2;

	/* 2 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 2, TRUE);
	offset += 2;

	BYTE_COUNT;

	END_OF_SMB

	return offset;
}

static int
dissect_read_mpx_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* fid */
	proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
	offset += 2;

	/* offset */
	proto_tree_add_item(tree, hf_smb_offset, tvb, offset, 4, TRUE);
	offset += 4;

	/* max count */
	proto_tree_add_item(tree, hf_smb_max_count, tvb, offset, 2, TRUE);
	offset += 2;

	/* min count */
	proto_tree_add_item(tree, hf_smb_min_count, tvb, offset, 2, TRUE);
	offset += 2;

	/* 6 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 6, TRUE);
	offset += 6;

	BYTE_COUNT;

	END_OF_SMB

	return offset;
}

static int
dissect_read_mpx_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint16 datalen=0, bc;
	guint8 wc;
	int tvblen;

	WORD_COUNT;

	/* offset */
	proto_tree_add_item(tree, hf_smb_offset, tvb, offset, 4, TRUE);
	offset += 4;

	/* count */
	proto_tree_add_item(tree, hf_smb_count, tvb, offset, 2, TRUE);
	offset += 2;

	/* 2 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 2, TRUE);
	offset += 2;

	/* data compaction mode */
	proto_tree_add_item(tree, hf_smb_dcm, tvb, offset, 2, TRUE);
	offset += 2;

	/* 2 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 2, TRUE);
	offset += 2;

	/* data len */
	datalen = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_data_len, tvb, offset, 2, datalen);
	offset += 2;

	/* data offset */
	proto_tree_add_item(tree, hf_smb_data_offset, tvb, offset, 2, TRUE);
	offset += 2;

	BYTE_COUNT;

	/* file data */
	if(bc>datalen){
		/* We have some initial padding bytes. */
		/* XXX - use the data offset here instead? */
		proto_tree_add_item(tree, hf_smb_padding, tvb, offset, bc-datalen,
			TRUE);
		offset += bc-datalen;
		bc = datalen;
	}
	tvblen = tvb_length_remaining(tvb, offset);
	if(bc>tvblen){
		proto_tree_add_bytes_format(tree, hf_smb_file_data, tvb, offset, tvblen, tvb_get_ptr(tvb, offset, tvblen),"File Data: Incomplete. Only %d of %d bytes", tvblen, bc);
		offset += tvblen;
	} else {
		proto_tree_add_item(tree, hf_smb_file_data, tvb, offset, bc, TRUE);
		offset += bc;
	}
	bc = 0;

	END_OF_SMB

	return offset;
}


static const true_false_string tfs_write_raw_mode_write_through = {
	"WRITE THROUGH requested",
	"Write through not requested"
};
static const true_false_string tfs_write_raw_mode_return_remaining = {
	"RETURN REMAINING (pipe/dev) requested",
	"DON'T return remaining (pipe/dev)"
};
static const true_false_string tfs_write_raw_mode_connectionless = {
	"CONNECTIONLESS mode requested",
	"Connectionless mode NOT requested"
};
static int
dissect_write_raw_mode(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset, int bm)
{
	guint16 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohs(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 2,
			"Write Mode: 0x%04x", mask);
		tree = proto_item_add_subtree(item, ett_smb_rawmode);
	}

	if(bm&0x0008){
		proto_tree_add_boolean(tree, hf_smb_write_raw_mode_connectionless,
			tvb, offset, 2, mask);
	}
	if(bm&0x0002){
		proto_tree_add_boolean(tree, hf_smb_write_raw_mode_return_remaining,
			tvb, offset, 2, mask);
	}
	if(bm&0x0001){
		proto_tree_add_boolean(tree, hf_smb_write_raw_mode_write_through,
			tvb, offset, 2, mask);
	}

	offset += 2;
	return offset;
}

static int
dissect_write_raw_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint32 to;
	guint16 datalen=0, bc;
	guint8 wc;
	int tvblen;

	WORD_COUNT;

	/* fid */
	proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
	offset += 2;

	/* count */
	proto_tree_add_item(tree, hf_smb_count, tvb, offset, 2, TRUE);
	offset += 2;

	/* 2 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 2, TRUE);
	offset += 2;

	/* offset */
	proto_tree_add_item(tree, hf_smb_offset, tvb, offset, 4, TRUE);
	offset += 4;

	/* timeout */
	to = tvb_get_letohl(tvb, offset);
	proto_tree_add_uint_format(tree, hf_smb_timeout, tvb, offset, 4, to, "Timeout: %s", time_msecs_to_str(to));
	offset += 4;

	/* mode */
	offset = dissect_write_raw_mode(tvb, pinfo, tree, offset, 0x0003);

	/* 4 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 4, TRUE);
	offset += 4;

	/* data len */
	datalen = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_data_len, tvb, offset, 2, datalen);
	offset += 2;

	/* data offset */
	proto_tree_add_item(tree, hf_smb_data_offset, tvb, offset, 2, TRUE);
	offset += 2;

	BYTE_COUNT;

	/* file data */
	if(bc>datalen){
		/* We have some initial padding bytes. */
		/* XXX - use the data offset here instead? */
		proto_tree_add_item(tree, hf_smb_padding, tvb, offset, bc-datalen,
			TRUE);
		offset += bc-datalen;
		bc = datalen;
	}
	tvblen = tvb_length_remaining(tvb, offset);
	if(bc>tvblen){
		proto_tree_add_bytes_format(tree, hf_smb_file_data, tvb, offset, tvblen, tvb_get_ptr(tvb, offset, tvblen),"File Data: Incomplete. Only %d of %d bytes", tvblen, bc);
		offset += tvblen;
	} else {
		proto_tree_add_item(tree, hf_smb_file_data, tvb, offset, bc, TRUE);
		offset += bc;
	}
	bc = 0;

	END_OF_SMB

	return offset;
}
 
static int
dissect_write_raw_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* remaining */
	proto_tree_add_item(tree, hf_smb_remaining, tvb, offset, 2, TRUE);
	offset += 2;

	BYTE_COUNT;

	END_OF_SMB

	return offset;
}
 
static int
dissect_write_mpx_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* response mask */
	proto_tree_add_item(tree, hf_smb_response_mask, tvb, offset, 4, TRUE);
	offset += 4;
	
	BYTE_COUNT;

	END_OF_SMB

	return offset;
}

static int
dissect_write_mpx_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint32 to;
	guint16 datalen=0, bc;
	guint8 wc;
	int tvblen;

	WORD_COUNT;

	/* fid */
	proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
	offset += 2;

	/* count */
	proto_tree_add_item(tree, hf_smb_count, tvb, offset, 2, TRUE);
	offset += 2;

	/* 2 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 2, TRUE);
	offset += 2;

	/* offset */
	proto_tree_add_item(tree, hf_smb_offset, tvb, offset, 4, TRUE);
	offset += 4;

	/* timeout */
	to = tvb_get_letohl(tvb, offset);
	proto_tree_add_uint_format(tree, hf_smb_timeout, tvb, offset, 4, to, "Timeout: %s", time_msecs_to_str(to));
	offset += 4;

	/* mode */
	offset = dissect_write_raw_mode(tvb, pinfo, tree, offset, 0x0083);

	/* request mask */
	proto_tree_add_item(tree, hf_smb_request_mask, tvb, offset, 4, TRUE);
	offset += 4;
	
	/* data len */
	datalen = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_data_len, tvb, offset, 2, datalen);
	offset += 2;

	/* data offset */
	proto_tree_add_item(tree, hf_smb_data_offset, tvb, offset, 2, TRUE);
	offset += 2;

	BYTE_COUNT;

	/* file data */
	if(bc>datalen){
		/* We have some initial padding bytes. */
		/* XXX - use the data offset here instead? */
		proto_tree_add_item(tree, hf_smb_padding, tvb, offset, bc-datalen,
			TRUE);
		offset += bc-datalen;
		bc = datalen;
	}
	tvblen = tvb_length_remaining(tvb, offset);
	if(bc>tvblen){
		proto_tree_add_bytes_format(tree, hf_smb_file_data, tvb, offset, tvblen, tvb_get_ptr(tvb, offset, tvblen),"File Data: Incomplete. Only %d of %d bytes", tvblen, bc);
		offset += tvblen;
	} else {
		proto_tree_add_item(tree, hf_smb_file_data, tvb, offset, bc, TRUE);
		offset += bc;
	}
	bc = 0;

	END_OF_SMB

	return offset;
}

static int
dissect_sid(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc;
	guint16 bc;

	WORD_COUNT;

	/* sid */
	proto_tree_add_item(tree, hf_smb_sid, tvb, offset, 2, TRUE);
	offset += 2;

	BYTE_COUNT;

	END_OF_SMB

	return offset;
}

static int
dissect_search_resume_key(tvbuff_t *tvb, packet_info *pinfo,
    proto_tree *parent_tree, int offset, guint16 *bc, gboolean *trunc)
{
	proto_item *item = NULL;
	proto_tree *tree = NULL;
	int fn_len;
	const char *fn;
	char fname[11+1];

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 21,
			"Resume Key");
		tree = proto_item_add_subtree(item, ett_smb_search_resume_key);
	}

	/* reserved byte */
	if (*bc < 1) {
		*trunc = TRUE;
		return offset;
	}
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 1, TRUE);
	offset += 1;
	*bc -= 1;

	/* file name */
	fn_len = 11;
	fn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &fn_len,
		TRUE, TRUE, bc);
	if (fn == NULL) {
		*trunc = TRUE;
		return offset;
	}
	/* ensure that it's null-terminated */
	strncpy(fname, fn, 11);
	fname[11] = '\0';
	proto_tree_add_string(tree, hf_smb_file_name, tvb, offset, 11,
		fname);
	offset += fn_len;
	*bc -= fn_len;

	/* server cookie */
	if (*bc < 5) {
		*trunc = TRUE;
		return offset;
	}
	proto_tree_add_item(tree, hf_smb_resume_server_cookie, tvb, offset, 5, TRUE);
	offset += 5;
	*bc -= 5;

	/* client cookie */
	if (*bc < 4) {
		*trunc = TRUE;
		return offset;
	}
	proto_tree_add_item(tree, hf_smb_resume_client_cookie, tvb, offset, 4, TRUE);
	offset += 4;
	*bc -= 4;

	*trunc = FALSE;
	return offset;
}

static int
dissect_search_dir_info(tvbuff_t *tvb, packet_info *pinfo,
    proto_tree *parent_tree, int offset, guint16 *bc, gboolean *trunc)
{
	proto_item *item = NULL;
	proto_tree *tree = NULL;
	int fn_len;
	const char *fn;
	char fname[13+1];

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 46,
			"Directory Information");
		tree = proto_item_add_subtree(item, ett_smb_search_dir_info);
	}

	/* resume key */
	offset = dissect_search_resume_key(tvb, pinfo, tree, offset, bc, trunc);
	if (*trunc)
		return offset;

	/* File Attributes */
	if (*bc < 1) {
		*trunc = TRUE;
		return offset;
	}
	offset = dissect_dir_info_file_attributes(tvb, pinfo, tree, offset);
	*bc -= 1;

	/* last write time */
	if (*bc < 4) {
		*trunc = TRUE;
		return offset;
	}
	offset = dissect_smb_datetime(tvb, pinfo, tree, offset,
		hf_smb_last_write_time,
		hf_smb_last_write_dos_date, hf_smb_last_write_dos_time,
		TRUE);
	*bc -= 4;

	/* File Size */
	if (*bc < 4) {
		*trunc = TRUE;
		return offset;
	}
	proto_tree_add_item(tree, hf_smb_file_size, tvb, offset, 4, TRUE);
	offset += 4;
	*bc -= 4;

	/* file name */
	fn_len = 13;
	fn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &fn_len,
		TRUE, TRUE, bc);
	if (fn == NULL) {
		*trunc = TRUE;
		return offset;
	}
	/* ensure that it's null-terminated */
	strncpy(fname, fn, 13);
	fname[13] = '\0';
	proto_tree_add_string(tree, hf_smb_file_name, tvb, offset, fn_len,
		fname);
	offset += fn_len;
	*bc -= fn_len;

	*trunc = FALSE;
	return offset;
}


static int
dissect_search_dir_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	int fn_len;
	const char *fn;
	guint16 rkl;
	guint8 wc;
	guint16 bc;
	gboolean trunc;

	WORD_COUNT;

	/* max count */
	proto_tree_add_item(tree, hf_smb_max_count, tvb, offset, 2, TRUE);
	offset += 2;

	/* Search Attributes */
	offset = dissect_search_attributes(tvb, pinfo, tree, offset);

	BYTE_COUNT;

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* file name */
	fn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &fn_len,
		TRUE, FALSE, &bc);
	if (fn == NULL)
		goto endofcommand;
	proto_tree_add_string(tree, hf_smb_file_name, tvb, offset, fn_len,
		fn);
	COUNT_BYTES(fn_len);

	if (check_col(pinfo->fd, COL_INFO)) {
		col_append_fstr(pinfo->fd, COL_INFO, ", File: %s", fn);
	}

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* resume key length */
	CHECK_BYTE_COUNT(2);
	rkl = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_resume_key_len, tvb, offset, 2, rkl);
	COUNT_BYTES(2);

	/* resume key */
	if(rkl){
		offset = dissect_search_resume_key(tvb, pinfo, tree, offset,
		    &bc, &trunc);
		if (trunc)
			goto endofcommand;
	}

	END_OF_SMB

	return offset;
}

static int
dissect_search_dir_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint16 count=0;
	guint8 wc;
	guint16 bc;
	gboolean trunc;

	WORD_COUNT;

	/* count */
	count = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_count, tvb, offset, 2, count);
	offset += 2;

	BYTE_COUNT;

	/* buffer format */
	CHECK_BYTE_COUNT(1);
	proto_tree_add_item(tree, hf_smb_buffer_format, tvb, offset, 1, TRUE);
	COUNT_BYTES(1);

	/* data len */
	CHECK_BYTE_COUNT(2);
	proto_tree_add_item(tree, hf_smb_data_len, tvb, offset, 2, TRUE);
	COUNT_BYTES(2);

	while(count--){
		offset = dissect_search_dir_info(tvb, pinfo, tree, offset,
		    &bc, &trunc);
		if (trunc)
			goto endofcommand;
	}

	END_OF_SMB

	return offset;
}

static const value_string locking_ol_vals[] = {
	{0,	"Client is not holding oplock on this file"},
	{1,	"Level 2 oplock currently held by client"},
	{0, NULL}
};

static const true_false_string tfs_lock_type_large = {
	"Large file locking format requested",
	"Large file locking format not requested"
};
static const true_false_string tfs_lock_type_cancel = {
	"Cancel outstanding lock request",
	"Don't cancel outstanding lock request"
};
static const true_false_string tfs_lock_type_change = {
	"Change lock type",
	"Don't change lock type"
};
static const true_false_string tfs_lock_type_oplock = {
	"This is an oplock break notification/response",
	"This is not an oplock break notification/response"
};
static const true_false_string tfs_lock_type_shared = {
	"This is a shared lock",
	"This is an exclusive lock"
};
static int
dissect_locking_andx_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8	wc, cmd=0xff, lt=0;
	guint16 andxoffset=0, un=0, ln=0, bc;
	guint32 to;
	proto_item *litem = NULL;
	proto_tree *ltree = NULL;

	WORD_COUNT;

	/* next smb command */
	cmd = tvb_get_guint8(tvb, offset);
	if(cmd!=0xff){
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: %s (0x%02x)", decode_smb_name(cmd), cmd);
	} else {
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: No further commands (0xff)");
	}
	offset += 1;

	/* reserved byte */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 1, TRUE);
	offset += 1;

	/* andxoffset */
	andxoffset = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_andxoffset, tvb, offset, 2, andxoffset);
	offset += 2;

	/* fid */
	proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
	offset += 2;

	/* lock type */
	lt = tvb_get_guint8(tvb, offset);
	if(tree){
		litem = proto_tree_add_text(tree, tvb, offset, 1,
			"Lock Type: 0x%02x", lt);
		ltree = proto_item_add_subtree(litem, ett_smb_lock_type);
	}
	proto_tree_add_boolean(ltree, hf_smb_lock_type_large,
		tvb, offset, 1, lt);
	proto_tree_add_boolean(ltree, hf_smb_lock_type_cancel,
		tvb, offset, 1, lt);
	proto_tree_add_boolean(ltree, hf_smb_lock_type_change,
		tvb, offset, 1, lt);
	proto_tree_add_boolean(ltree, hf_smb_lock_type_oplock,
		tvb, offset, 1, lt);
	proto_tree_add_boolean(ltree, hf_smb_lock_type_shared,
		tvb, offset, 1, lt);
	offset += 1;

	/* oplock level */
	proto_tree_add_item(tree, hf_smb_locking_ol, tvb, offset, 1, TRUE);
	offset += 1;

	/* timeout */
	to = tvb_get_letohl(tvb, offset);
	if (to == 0)
		proto_tree_add_uint_format(tree, hf_smb_timeout, tvb, offset, 4, to, "Timeout: Return immediately (0)");
	else if (to == 0xffffffff)
		proto_tree_add_uint_format(tree, hf_smb_timeout, tvb, offset, 4, to, "Timeout: Wait indefinitely (-1)");
	else
		proto_tree_add_uint_format(tree, hf_smb_timeout, tvb, offset, 4, to, "Timeout: %s", time_msecs_to_str(to));
	offset += 4;

	/* number of unlocks */
	un = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_number_of_unlocks, tvb, offset, 2, un);
	offset += 2;

	/* number of locks */
	ln = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_number_of_locks, tvb, offset, 2, ln);
	offset += 2;

	BYTE_COUNT;

	/* unlocks */
	if(un){
		proto_item *it = NULL;
		proto_tree *tr = NULL;
		int old_offset = offset;

		it = proto_tree_add_text(tree, tvb, offset, 0,
			"Unlocks");
		tr = proto_item_add_subtree(it, ett_smb_unlocks);
		while(un--){
			proto_item *litem = NULL;
			proto_tree *ltree = NULL;
			if(lt&0x10){
				/* large lock format */
				litem = proto_tree_add_text(tr, tvb, offset, 20,
					"Unlock");
				ltree = proto_item_add_subtree(litem, ett_smb_unlock);
				
				/* PID */
				proto_tree_add_item(ltree, hf_smb_pid, tvb, offset, 2, TRUE);
				offset += 2;

				/* 2 reserved bytes */
				proto_tree_add_item(ltree, hf_smb_reserved, tvb, offset, 2, TRUE);
				offset += 2;

				/* offset */
				proto_tree_add_item(ltree, hf_smb_lock_long_offset, tvb, offset, 8, TRUE);
				offset += 8;

				/* length */
				proto_tree_add_item(ltree, hf_smb_lock_long_length, tvb, offset, 8, TRUE);
				offset += 8;
			} else {
				/* normal lock format */
				litem = proto_tree_add_text(tr, tvb, offset, 10,
					"Unlock");
				ltree = proto_item_add_subtree(litem, ett_smb_unlock);
				
				/* PID */
				proto_tree_add_item(ltree, hf_smb_pid, tvb, offset, 2, TRUE);
				offset += 2;

				/* offset */
				proto_tree_add_item(ltree, hf_smb_offset, tvb, offset, 4, TRUE);
				offset += 4;

				/* lock count */
				proto_tree_add_item(ltree, hf_smb_count, tvb, offset, 4, TRUE);
				offset += 4;
			}
		}
		proto_item_set_len(it, offset-old_offset);
	}

	/* locks */
	if(ln){
		proto_item *it = NULL;
		proto_tree *tr = NULL;
		int old_offset = offset;

		it = proto_tree_add_text(tree, tvb, offset, 0,
			"Locks");
		tr = proto_item_add_subtree(it, ett_smb_locks);
		while(ln--){
			proto_item *litem = NULL;
			proto_tree *ltree = NULL;
			if(lt&0x10){
				/* large lock format */
				litem = proto_tree_add_text(tr, tvb, offset, 20,
					"Lock");
				ltree = proto_item_add_subtree(litem, ett_smb_lock);
				
				/* PID */
				proto_tree_add_item(ltree, hf_smb_pid, tvb, offset, 2, TRUE);
				offset += 2;

				/* 2 reserved bytes */
				proto_tree_add_item(ltree, hf_smb_reserved, tvb, offset, 2, TRUE);
				offset += 2;

				/* offset */
				proto_tree_add_item(ltree, hf_smb_lock_long_offset, tvb, offset, 8, TRUE);
				offset += 8;

				/* length */
				proto_tree_add_item(ltree, hf_smb_lock_long_length, tvb, offset, 8, TRUE);
				offset += 8;
			} else {
				/* normal lock format */
				litem = proto_tree_add_text(tr, tvb, offset, 10,
					"Unlock");
				ltree = proto_item_add_subtree(litem, ett_smb_unlock);
				
				/* PID */
				proto_tree_add_item(ltree, hf_smb_pid, tvb, offset, 2, TRUE);
				offset += 2;

				/* offset */
				proto_tree_add_item(ltree, hf_smb_offset, tvb, offset, 4, TRUE);
				offset += 4;

				/* lock count */
				proto_tree_add_item(ltree, hf_smb_count, tvb, offset, 4, TRUE);
				offset += 4;
			}
		}
		proto_item_set_len(it, offset-old_offset);
	}

	END_OF_SMB

	/* call AndXCommand (if there are any) */
	dissect_smb_command(tvb, pinfo, tree, andxoffset, smb_tree, cmd);

	return offset;
}

static int
dissect_locking_andx_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8	wc, cmd=0xff;
	guint16 andxoffset=0;
	guint16 bc;

	WORD_COUNT;

	/* next smb command */
	cmd = tvb_get_guint8(tvb, offset);
	if(cmd!=0xff){
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: %s (0x%02x)", decode_smb_name(cmd), cmd);
	} else {
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: No further commands (0xff)");
	}
	offset += 1;

	/* reserved byte */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 1, TRUE);
	offset += 1;

	/* andxoffset */
	andxoffset = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_andxoffset, tvb, offset, 2, andxoffset);
	offset += 2;

	BYTE_COUNT;

	END_OF_SMB

	/* call AndXCommand (if there are any) */
	dissect_smb_command(tvb, pinfo, tree, andxoffset, smb_tree, cmd);

	return offset;
}


static const value_string oa_open_vals[] = {
	{ 0,		"No action taken?"},
	{ 1,		"The file existed and was opened"},
	{ 2,		"The file did not exist but was created"},
	{ 3,		"The file existed and was truncated"},
	{0,	NULL}
};
static const true_false_string tfs_oa_lock = {
	"File is currently opened only by this user",
	"File is opened by another user (or mode not supported by server)"
};
static int
dissect_open_action(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint16 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohs(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 2,
			"Action: 0x%04x", mask);
		tree = proto_item_add_subtree(item, ett_smb_open_action);
	}

	proto_tree_add_boolean(tree, hf_smb_open_action_lock,
		tvb, offset, 2, mask);
	proto_tree_add_uint(tree, hf_smb_open_action_open,
		tvb, offset, 2, mask);

	offset += 2;

	return offset;
}

static const true_false_string tfs_open_flags_add_info = {
	"Additional information requested",
	"Additional information not requested"
};
static const true_false_string tfs_open_flags_ex_oplock = {
	"Exclusive oplock requested",
	"Exclusive oplock not requested"
};
static const true_false_string tfs_open_flags_batch_oplock = {
	"Batch oplock requested",
	"Batch oplock not requested"
};
static const true_false_string tfs_open_flags_ealen = {
	"Total length of EAs requested",
	"Total length of EAs not requested"
};
static int
dissect_open_flags(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset, int bm)
{
	guint16 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohs(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 2,
			"Flags: 0x%04x", mask);
		tree = proto_item_add_subtree(item, ett_smb_open_flags);
	}

	if(bm&0x0001){
		proto_tree_add_boolean(tree, hf_smb_open_flags_add_info,
			tvb, offset, 2, mask);
	}
	if(bm&0x0002){
		proto_tree_add_boolean(tree, hf_smb_open_flags_ex_oplock,
			tvb, offset, 2, mask);
	}
	if(bm&0x0004){
		proto_tree_add_boolean(tree, hf_smb_open_flags_batch_oplock,
			tvb, offset, 2, mask);
	}
	if(bm&0x0008){
		proto_tree_add_boolean(tree, hf_smb_open_flags_ealen,
			tvb, offset, 2, mask);
	}

	offset += 2;

	return offset;
}

static const value_string filetype_vals[] = {
	{ 0,		"Disk file or directory"},
	{ 1,		"Named pipe in byte mode"},
	{ 2,		"Named pipe in message mode"},
	{ 3,		"Spooled printer"},
	{0, NULL}
};
static int
dissect_open_andx_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8	wc, cmd=0xff;
	guint16 andxoffset=0, bc;
	int fn_len;
	const char *fn;

	WORD_COUNT;

	/* next smb command */
	cmd = tvb_get_guint8(tvb, offset);
	if(cmd!=0xff){
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: %s (0x%02x)", decode_smb_name(cmd), cmd);
	} else {
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: No further commands (0xff)");
	}
	offset += 1;

	/* reserved byte */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 1, TRUE);
	offset += 1;

	/* andxoffset */
	andxoffset = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_andxoffset, tvb, offset, 2, andxoffset);
	offset += 2;

	/* open flags */
	offset = dissect_open_flags(tvb, pinfo, tree, offset, 0x0007);

	/* desired access */
	offset = dissect_access(tvb, pinfo, tree, offset, "Desired");

	/* Search Attributes */
	offset = dissect_search_attributes(tvb, pinfo, tree, offset);

	/* File Attributes */
	offset = dissect_file_attributes(tvb, pinfo, tree, offset);

	/* creation time */
	offset = dissect_smb_UTIME(tvb, pinfo, tree, offset, hf_smb_create_time);
	
	/* open function */
	offset = dissect_open_function(tvb, pinfo, tree, offset);

	/* allocation size */
	proto_tree_add_item(tree, hf_smb_alloc_size, tvb, offset, 4, TRUE);
	offset += 4;

	/* 8 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 8, TRUE);
	offset += 8;

	BYTE_COUNT;

	/* file name */
	fn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &fn_len,
		FALSE, FALSE, &bc);
	if (fn == NULL)
		goto endofcommand;
	proto_tree_add_string(tree, hf_smb_file_name, tvb, offset, fn_len,
		fn);
	COUNT_BYTES(fn_len);

	if (check_col(pinfo->fd, COL_INFO)) {
		col_append_fstr(pinfo->fd, COL_INFO, ", Path: %s", fn);
	}

	END_OF_SMB

	/* call AndXCommand (if there are any) */
	dissect_smb_command(tvb, pinfo, tree, andxoffset, smb_tree, cmd);

	return offset;
}

static int
dissect_open_andx_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8	wc, cmd=0xff;
	guint16 andxoffset=0, bc;

	WORD_COUNT;

	/* next smb command */
	cmd = tvb_get_guint8(tvb, offset);
	if(cmd!=0xff){
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: %s (0x%02x)", decode_smb_name(cmd), cmd);
	} else {
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: No further commands (0xff)");
	}
	offset += 1;

	/* reserved byte */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 1, TRUE);
	offset += 1;

	/* andxoffset */
	andxoffset = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_andxoffset, tvb, offset, 2, andxoffset);
	offset += 2;

	/* fid */
	proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
	offset += 2;

	/* File Attributes */
	offset = dissect_file_attributes(tvb, pinfo, tree, offset);

	/* last write time */
	offset = dissect_smb_UTIME(tvb, pinfo, tree, offset, hf_smb_last_write_time);
	
	/* File Size */
	proto_tree_add_item(tree, hf_smb_file_size, tvb, offset, 4, TRUE);
	offset += 4;

	/* granted access */
	offset = dissect_access(tvb, pinfo, tree, offset, "Granted");

	/* File Type */
	proto_tree_add_item(tree, hf_smb_file_type, tvb, offset, 2, TRUE);
	offset += 2;

	/* Device State */
	/*
	 * XXX - dissect this according to the stuff on page 67 of
	 *
	 *	ftp://ftp.microsoft.com/developr/drg/CIFS/dosextp.txt
	 */
	proto_tree_add_item(tree, hf_smb_device_state, tvb, offset, 2, TRUE);
	offset += 2;

	/* open_action */
	offset = dissect_open_action(tvb, pinfo, tree, offset);

	/* server fid */
	proto_tree_add_item(tree, hf_smb_server_fid, tvb, offset, 4, TRUE);
	offset += 4;

	/* 2 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 2, TRUE);
	offset += 2;

	BYTE_COUNT;

	END_OF_SMB

	/* call AndXCommand (if there are any) */
	dissect_smb_command(tvb, pinfo, tree, andxoffset, smb_tree, cmd);

	return offset;
}

static int
dissect_read_andx_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8	wc, cmd=0xff;
	guint16 andxoffset=0, bc;

	WORD_COUNT;

	/* next smb command */
	cmd = tvb_get_guint8(tvb, offset);
	if(cmd!=0xff){
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: %s (0x%02x)", decode_smb_name(cmd), cmd);
	} else {
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: No further commands (0xff)");
	}
	offset += 1;

	/* reserved byte */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 1, TRUE);
	offset += 1;

	/* andxoffset */
	andxoffset = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_andxoffset, tvb, offset, 2, andxoffset);
	offset += 2;

	/* fid */
	proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
	offset += 2;

	/* offset */
	proto_tree_add_item(tree, hf_smb_offset, tvb, offset, 4, TRUE);
	offset += 4;

	/* max count */
	proto_tree_add_item(tree, hf_smb_max_count, tvb, offset, 2, TRUE);
	offset += 2;

	/* min count */
	proto_tree_add_item(tree, hf_smb_min_count, tvb, offset, 2, TRUE);
	offset += 2;

	/* 4 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 4, TRUE);
	offset += 4;

	/* remaining */
	proto_tree_add_item(tree, hf_smb_remaining, tvb, offset, 2, TRUE);
	offset += 2;

	if(wc==12){
		/* high offset */
		proto_tree_add_item(tree, hf_smb_high_offset, tvb, offset, 4, TRUE);
		offset += 4;
	}

	BYTE_COUNT;

	END_OF_SMB

	/* call AndXCommand (if there are any) */
	dissect_smb_command(tvb, pinfo, tree, andxoffset, smb_tree, cmd);

	return offset;
}

static int
dissect_read_andx_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8	wc, cmd=0xff;
	guint16 andxoffset=0, bc, cnt=0;
	int len;

	WORD_COUNT;

	/* next smb command */
	cmd = tvb_get_guint8(tvb, offset);
	if(cmd!=0xff){
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: %s (0x%02x)", decode_smb_name(cmd), cmd);
	} else {
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: No further commands (0xff)");
	}
	offset += 1;
 
	/* reserved byte */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 1, TRUE);
	offset += 1;

	/* andxoffset */
	andxoffset = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_andxoffset, tvb, offset, 2, andxoffset);
	offset += 2;

	/* remaining */
	proto_tree_add_item(tree, hf_smb_remaining, tvb, offset, 2, TRUE);
	offset += 2;

	/* data compaction mode */
	proto_tree_add_item(tree, hf_smb_dcm, tvb, offset, 2, TRUE);
	offset += 2;

	/* 2 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 2, TRUE);
	offset += 2;

	/* data len */
	cnt = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_data_len, tvb, offset, 2, cnt);
	offset += 2;

	/* data offset */
	proto_tree_add_item(tree, hf_smb_data_offset, tvb, offset, 2, TRUE);
	offset += 2;

	/* 10 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 10, TRUE);
	offset += 10;

	BYTE_COUNT;

	/* file data */
	if(bc>cnt){
		/* We have some initial padding bytes. */
		/* XXX - use the data offset here instead? */
	        proto_tree_add_item(tree, hf_smb_padding, tvb, offset, bc-cnt, TRUE);
		offset += bc-cnt;
		bc = cnt;
	}
	len = tvb_length_remaining(tvb, offset);
	if(bc>len){
		proto_tree_add_bytes_format(tree, hf_smb_file_data, tvb, offset, len, tvb_get_ptr(tvb, offset, len),"File Data: Incomplete. Only %d of %d bytes", len, bc);
		offset += len;
	} else {
		proto_tree_add_item(tree, hf_smb_file_data, tvb, offset, bc, TRUE);
		offset += bc;
	}
	bc = 0;

	END_OF_SMB

	/* call AndXCommand (if there are any) */
	dissect_smb_command(tvb, pinfo, tree, andxoffset, smb_tree, cmd);

	return offset;
}

static const true_false_string tfs_setup_action_guest = {
	"Logged in as GUEST",
	"Not logged in as GUEST"
};
static int
dissect_setup_action(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint16 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohs(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 2,
			"Action: 0x%04x", mask);
		tree = proto_item_add_subtree(item, ett_smb_setup_action);
	}

	proto_tree_add_boolean(tree, hf_smb_setup_action_guest,
		tvb, offset, 2, mask);

	offset += 2;

	return offset;
}
 

static int
dissect_session_setup_andx_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8	wc, cmd=0xff;
	guint16 bc;
	guint16 andxoffset=0;
	int an_len;
	const char *an;
	int dn_len;
	const char *dn;
	guint16 pwlen=0;
	guint16 sbloblen=0;
	guint16 apwlen=0, upwlen=0;

	WORD_COUNT;

	/* next smb command */
	cmd = tvb_get_guint8(tvb, offset);
	if(cmd!=0xff){
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: %s (0x%02x)", decode_smb_name(cmd), cmd);
	} else {
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: No further commands (0xff)");
	}
	offset += 1;

	/* reserved byte */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 1, TRUE);
	offset += 1;

	/* andxoffset */
	andxoffset = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_andxoffset, tvb, offset, 2, andxoffset);
	offset += 2;

	/* Maximum Buffer Size */
	proto_tree_add_item(tree, hf_smb_max_buf_size, tvb, offset, 2, TRUE);
	offset += 2;

	/* Maximum Multiplex Count */
	proto_tree_add_item(tree, hf_smb_max_mpx_count, tvb, offset, 2, TRUE);
	offset += 2;

	/* VC Number */
	proto_tree_add_item(tree, hf_smb_vc_num, tvb, offset, 2, TRUE);
	offset += 2;

	/* session key */
	proto_tree_add_item(tree, hf_smb_session_key, tvb, offset, 4, TRUE);
	offset += 4;

	switch (wc) {
	case 10:
		/* password length, ASCII*/
		pwlen = tvb_get_letohs(tvb, offset);
		proto_tree_add_uint(tree, hf_smb_password_len,
			tvb, offset, 2, pwlen);
		offset += 2;

		/* 4 reserved bytes */
		proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 4, TRUE);
		offset += 4;

		break;

	case 12:
		/* security blob length */
		sbloblen = tvb_get_letohs(tvb, offset);
		proto_tree_add_uint(tree, hf_smb_security_blob_len, tvb, offset, 2, sbloblen);
		offset += 2;

		/* 4 reserved bytes */
		proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 4, TRUE);
		offset += 4;

		/* capabilities */
		dissect_negprot_capabilities(tvb, pinfo, tree, offset);
		offset += 4;

		break;

	case 13:
		/* password length, ANSI*/
		apwlen = tvb_get_letohs(tvb, offset);
		proto_tree_add_uint(tree, hf_smb_ansi_password_len,
			tvb, offset, 2, apwlen);
		offset += 2;

		/* password length, Unicode*/
		upwlen = tvb_get_letohs(tvb, offset);
		proto_tree_add_uint(tree, hf_smb_unicode_password_len,
			tvb, offset, 2, upwlen);
		offset += 2;

		/* 4 reserved bytes */
		proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 4, TRUE);
		offset += 4;

		/* capabilities */
		dissect_negprot_capabilities(tvb, pinfo, tree, offset);
		offset += 4;

		break;
	}

	BYTE_COUNT;

	if (wc==12) {
		/* security blob */
		/* XXX - is this ASN.1-encoded?  Is it a Kerberos
		   data structure, at least in NT 5.0-and-later
		   server replies? */
		if(sbloblen){
			CHECK_BYTE_COUNT(sbloblen);
			proto_tree_add_item(tree, hf_smb_security_blob,
				tvb, offset, sbloblen, TRUE);
			COUNT_BYTES(sbloblen);
		}

		/* OS */
		an = get_unicode_or_ascii_string_tvb(tvb, &offset,
			pinfo, &an_len, FALSE, FALSE, &bc);
		if (an == NULL)
			goto endofcommand;
		proto_tree_add_string(tree, hf_smb_os, tvb,
			offset, an_len, an);
		COUNT_BYTES(an_len);

		/* LANMAN */
		/* XXX - pre-W2K NT systems appear to stick an extra 2 bytes of
		 * padding/null string/whatever in front of this. W2K doesn't
		 * appear to. I suspect that's a bug that got fixed; I also
		 * suspect that, in practice, nobody ever looks at that field
		 * because the bug didn't appear to get fixed until NT 5.0....
		 */
		an = get_unicode_or_ascii_string_tvb(tvb, &offset,
			pinfo, &an_len, FALSE, FALSE, &bc);
		if (an == NULL)
			goto endofcommand;
		proto_tree_add_string(tree, hf_smb_lanman, tvb,
			offset, an_len, an);
		COUNT_BYTES(an_len);

		/* Primary domain */
		/* XXX - pre-W2K NT systems sometimes appear to stick an extra
		 * byte in front of this, at least if all the strings are
		 * ASCII and the account name is empty. Another bug?
		 */
		dn = get_unicode_or_ascii_string_tvb(tvb, &offset,
			pinfo, &dn_len, FALSE, FALSE, &bc);
		if (dn == NULL)
			goto endofcommand;
		proto_tree_add_string(tree, hf_smb_primary_domain, tvb,
			offset, dn_len, dn);
		COUNT_BYTES(dn_len);
	} else {
		switch (wc) {

		case 10:
			if(pwlen){
				/* password, ASCII */
				CHECK_BYTE_COUNT(pwlen);
				proto_tree_add_item(tree, hf_smb_password, 
					tvb, offset, pwlen, TRUE);
				COUNT_BYTES(pwlen);
			}

			break;

		case 13:
			if(apwlen){
				/* password, ANSI */
				CHECK_BYTE_COUNT(apwlen);
				proto_tree_add_item(tree, hf_smb_ansi_password, 
					tvb, offset, apwlen, TRUE);
				COUNT_BYTES(apwlen);
			}

			if(upwlen){
				/* password, Unicode */
				CHECK_BYTE_COUNT(upwlen);
				proto_tree_add_item(tree, hf_smb_unicode_password, 
					tvb, offset, upwlen, TRUE);
				COUNT_BYTES(upwlen);
			}

			break;
		}

		/* Account Name */
		an = get_unicode_or_ascii_string_tvb(tvb, &offset,
			pinfo, &an_len, FALSE, FALSE, &bc);
		if (an == NULL)
			goto endofcommand;
		proto_tree_add_string(tree, hf_smb_account, tvb, offset, an_len,
			an);
		COUNT_BYTES(an_len);

		/* Primary domain */
		/* XXX - pre-W2K NT systems sometimes appear to stick an extra
		 * byte in front of this, at least if all the strings are
		 * ASCII and the account name is empty. Another bug?
		 */
		dn = get_unicode_or_ascii_string_tvb(tvb, &offset,
			pinfo, &dn_len, FALSE, FALSE, &bc);
		if (dn == NULL)
			goto endofcommand;
		proto_tree_add_string(tree, hf_smb_primary_domain, tvb,
			offset, dn_len, dn);
		COUNT_BYTES(dn_len);

		if (check_col(pinfo->fd, COL_INFO)) {
			col_append_fstr(pinfo->fd, COL_INFO, ", User: %s@%s",
			an,dn);
		}

		/* OS */
		an = get_unicode_or_ascii_string_tvb(tvb, &offset,
			pinfo, &an_len, FALSE, FALSE, &bc);
		if (an == NULL)
			goto endofcommand;
		proto_tree_add_string(tree, hf_smb_os, tvb,
			offset, an_len, an);
		COUNT_BYTES(an_len);

		/* LANMAN */
		/* XXX - pre-W2K NT systems appear to stick an extra 2 bytes of
		 * padding/null string/whatever in front of this. W2K doesn't
		 * appear to. I suspect that's a bug that got fixed; I also
		 * suspect that, in practice, nobody ever looks at that field
		 * because the bug didn't appear to get fixed until NT 5.0....
		 */
		an = get_unicode_or_ascii_string_tvb(tvb, &offset,
			pinfo, &an_len, FALSE, FALSE, &bc);
		if (an == NULL)
			goto endofcommand;
		proto_tree_add_string(tree, hf_smb_lanman, tvb,
			offset, an_len, an);
		COUNT_BYTES(an_len);
	}

	END_OF_SMB

	/* call AndXCommand (if there are any) */
	dissect_smb_command(tvb, pinfo, tree, andxoffset, smb_tree, cmd);

	return offset;
}

static int
dissect_session_setup_andx_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8	wc, cmd=0xff;
	guint16 andxoffset=0, bc;
	guint16 sbloblen=0;
	int an_len;
	const char *an;

	WORD_COUNT;

	/* next smb command */
	cmd = tvb_get_guint8(tvb, offset);
	if(cmd!=0xff){
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: %s (0x%02x)", decode_smb_name(cmd), cmd);
	} else {
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: No further commands (0xff)");
	}
	offset += 1;

	/* reserved byte */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 1, TRUE);
	offset += 1;

	/* andxoffset */
	andxoffset = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_andxoffset, tvb, offset, 2, andxoffset);
	offset += 2;

	/* flags */
	offset = dissect_setup_action(tvb, pinfo, tree, offset);

	if(wc==4){
		/* security blob length */
		sbloblen = tvb_get_letohs(tvb, offset);
		proto_tree_add_uint(tree, hf_smb_security_blob_len, tvb, offset, 2, sbloblen);
		offset += 2;
	}

	BYTE_COUNT;

	if(wc==4) {
		/* security blob */
		/* XXX - is this ASN.1-encoded?  Is it a Kerberos
		   data structure, at least in NT 5.0-and-later
		   server replies? */
		if(sbloblen){
			CHECK_BYTE_COUNT(sbloblen);
			proto_tree_add_item(tree, hf_smb_security_blob,
				tvb, offset, sbloblen, TRUE);
			COUNT_BYTES(sbloblen);
		}
	}

	/* OS */
	an = get_unicode_or_ascii_string_tvb(tvb, &offset,
		pinfo, &an_len, FALSE, FALSE, &bc);
	if (an == NULL)
		goto endofcommand;
	proto_tree_add_string(tree, hf_smb_os, tvb,
		offset, an_len, an);
	COUNT_BYTES(an_len);

	/* LANMAN */
	an = get_unicode_or_ascii_string_tvb(tvb, &offset,
		pinfo, &an_len, FALSE, FALSE, &bc);
	if (an == NULL)
		goto endofcommand;
	proto_tree_add_string(tree, hf_smb_lanman, tvb,
		offset, an_len, an);
	COUNT_BYTES(an_len);

	if(wc==3) {
		/* Primary domain */
		an = get_unicode_or_ascii_string_tvb(tvb, &offset,
			pinfo, &an_len, FALSE, FALSE, &bc);
		if (an == NULL)
			goto endofcommand;
		proto_tree_add_string(tree, hf_smb_primary_domain, tvb,
			offset, an_len, an);
		COUNT_BYTES(an_len);
	}

	END_OF_SMB

	/* call AndXCommand (if there are any) */
	dissect_smb_command(tvb, pinfo, tree, andxoffset, smb_tree, cmd);

	return offset;
}

 
static int
dissect_empty_andx(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8	wc, cmd=0xff;
	guint16 andxoffset=0;
	guint16 bc;

	WORD_COUNT;

	/* next smb command */
	cmd = tvb_get_guint8(tvb, offset);
	if(cmd!=0xff){
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: %s (0x%02x)", decode_smb_name(cmd), cmd);
	} else {
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: No further commands (0xff)");
	}
	offset += 1;

	/* reserved byte */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 1, TRUE);
	offset += 1;

	/* andxoffset */
	andxoffset = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_andxoffset, tvb, offset, 2, andxoffset);
	offset += 2;

	BYTE_COUNT;

	END_OF_SMB

	/* call AndXCommand (if there are any) */
	dissect_smb_command(tvb, pinfo, tree, andxoffset, smb_tree, cmd);

	return offset;
}

 
static const true_false_string tfs_connect_support_search = {
	"Exclusive search bits supported",
	"Exclusive search bits not supported"
};
static const true_false_string tfs_connect_support_in_dfs = {
	"Share is in Dfs",
	"Share isn't in Dfs"
};

static int
dissect_connect_support_bits(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint16 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohs(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 2,
			"Optional Support: 0x%04x", mask);
		tree = proto_item_add_subtree(item, ett_smb_connect_support_bits);
	}

	proto_tree_add_boolean(tree, hf_smb_connect_support_search,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_connect_support_in_dfs,
		tvb, offset, 2, mask);

	offset += 2;

	return offset;
}

static const true_false_string tfs_disconnect_tid = {
	"DISCONNECT TID",
	"Do NOT disconnect TID"
};

static int
dissect_connect_flags(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint16 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohs(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 2,
			"Flags: 0x%04x", mask);
		tree = proto_item_add_subtree(item, ett_smb_connect_flags);
	}

	proto_tree_add_boolean(tree, hf_smb_connect_flags_dtid,
		tvb, offset, 2, mask);

	offset += 2;

	return offset;
}

static int
dissect_tree_connect_andx_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8	wc, cmd=0xff;
	guint16 bc;
	guint16 andxoffset=0, pwlen=0;
	int an_len;
	const char *an;

	WORD_COUNT;

	/* next smb command */
	cmd = tvb_get_guint8(tvb, offset);
	if(cmd!=0xff){
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: %s (0x%02x)", decode_smb_name(cmd), cmd);
	} else {
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: No further commands");
	}
	offset += 1;

	/* reserved byte */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 1, TRUE);
	offset += 1;

	/* andxoffset */
	andxoffset = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_andxoffset, tvb, offset, 2, andxoffset);
	offset += 2;

	/* flags */
	offset = dissect_connect_flags(tvb, pinfo, tree, offset);

	/* password length*/
	pwlen = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_password_len, tvb, offset, 2, pwlen);
	offset += 2;

	BYTE_COUNT;

	/* password */
	CHECK_BYTE_COUNT(pwlen);
	proto_tree_add_item(tree, hf_smb_password, 
		tvb, offset, pwlen, TRUE);
	COUNT_BYTES(pwlen);

	/* Path */
	an = get_unicode_or_ascii_string_tvb(tvb, &offset,
		pinfo, &an_len, FALSE, FALSE, &bc);
	if (an == NULL)
		goto endofcommand;
	proto_tree_add_string(tree, hf_smb_path, tvb,
		offset, an_len, an);
	COUNT_BYTES(an_len);

	if (check_col(pinfo->fd, COL_INFO)) {
		col_append_fstr(pinfo->fd, COL_INFO, ", Path: %s", an);
	}

	/*
	 * NOTE: the Service string is always ASCII, even if the
	 * "strings are Unicode" bit is set in the flags2 field
	 * of the SMB.
	 */

	/* Service */
	/* XXX - what if this runs past bc? */
	an_len = tvb_strsize(tvb, offset);
	CHECK_BYTE_COUNT(an_len);
	an = tvb_get_ptr(tvb, offset, an_len);
	proto_tree_add_string(tree, hf_smb_service, tvb,
		offset, an_len, an);
	COUNT_BYTES(an_len);

	END_OF_SMB

	/* call AndXCommand (if there are any) */
	dissect_smb_command(tvb, pinfo, tree, andxoffset, smb_tree, cmd);

	return offset;
}


static int
dissect_tree_connect_andx_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8	wc, wleft, cmd=0xff;
	guint16 andxoffset=0;
	guint16 bc;
	int an_len;
	const char *an;

	WORD_COUNT;

	wleft = wc;	/* this is at least 1 */

	/* next smb command */
	cmd = tvb_get_guint8(tvb, offset);
	if(cmd!=0xff){
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: %s (0x%02x)", decode_smb_name(cmd), cmd);
	} else {
		proto_tree_add_uint_format(tree, hf_smb_cmd, tvb, offset, 1, cmd, "AndXCommand: No further commands");
	}
	offset += 1;

	/* reserved byte */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 1, TRUE);
	offset += 1;

	wleft--;
	if (wleft == 0)
		goto bytecount;

	/* andxoffset */
	andxoffset = tvb_get_letohs(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_andxoffset, tvb, offset, 2, andxoffset);
	offset += 2;
	wleft--;
	if (wleft == 0)
		goto bytecount;

	/* flags */
	offset = dissect_connect_support_bits(tvb, pinfo, tree, offset);
	wleft--;

	/* XXX - I've seen captures where this is 7, but I have no
	   idea how to dissect it.  I'm guessing the third word
	   contains connect support bits, which looks plausible
	   from the values I've seen. */

	while (wleft != 0) {
		proto_tree_add_text(tree, tvb, offset, 2,
		    "Word parameter: 0x%04x", tvb_get_letohs(tvb, offset));
		offset += 2;
		wleft--;
	}

	BYTE_COUNT;

	/*
	 * NOTE: even though the SNIA CIFS spec doesn't say there's
	 * a "Service" string if there's a word count of 2, the
	 * document at
	 *
	 *	ftp://ftp.microsoft.com/developr/drg/CIFS/dosextp.txt
	 *
	 * (it's in an ugly format - text intended to be sent to a
	 * printer, with backspaces and overstrikes used for boldfacing
	 * and underlining; UNIX "col -b" can be used to strip the
	 * overstrikes out) says there's a "Service" string there, and
	 * some network traffic has it.
	 */

	/*
	 * NOTE: the Service string is always ASCII, even if the
	 * "strings are Unicode" bit is set in the flags2 field
	 * of the SMB.
	 */

	/* Service */
	/* XXX - what if this runs past bc? */
	an_len = tvb_strsize(tvb, offset);
	CHECK_BYTE_COUNT(an_len);
	an = tvb_get_ptr(tvb, offset, an_len);
	proto_tree_add_string(tree, hf_smb_service, tvb,
		offset, an_len, an);
	COUNT_BYTES(an_len);

	if(wc==3){
		if (bc != 0) {
			/*
			 * Sometimes this isn't present.
			 */

			/* Native FS */
			an = get_unicode_or_ascii_string_tvb(tvb, &offset,
				pinfo, &an_len, /*TRUE*/FALSE, FALSE, &bc);
			if (an == NULL)
				goto endofcommand;
			proto_tree_add_string(tree, hf_smb_fs, tvb,
				offset, an_len, an);
			COUNT_BYTES(an_len);
		}
	}

	END_OF_SMB

	/* call AndXCommand (if there are any) */
	dissect_smb_command(tvb, pinfo, tree, andxoffset, smb_tree, cmd);

	return offset;
}



/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
   NT Transaction command  begins here
   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
#define NT_TRANS_CREATE		1
#define NT_TRANS_IOCTL		2
#define NT_TRANS_SSD		3
#define NT_TRANS_NOTIFY		4
#define NT_TRANS_RENAME		5
#define NT_TRANS_QSD		6
static const value_string nt_cmd_vals[] = {
	{NT_TRANS_CREATE,	"NT CREATE"},
	{NT_TRANS_IOCTL,	"NT IOCTL"},
	{NT_TRANS_SSD,		"NT SET SECURITY DESC"},
	{NT_TRANS_NOTIFY,	"NT NOTIFY"},
	{NT_TRANS_RENAME,	"NT RENAME"},
	{NT_TRANS_QSD,		"NT QUERY SECURITY DESC"},
	{0, NULL}
};

static const value_string nt_ioctl_isfsctl_vals[] = {
	{0,	"Device IOCTL"},
	{1,	"FS control : FSCTL"},
	{0, NULL}
};

#define NT_IOCTL_FLAGS_ROOT_HANDLE	0x01
static const true_false_string tfs_nt_ioctl_flags_root_handle = {
	"Apply the command to share root handle (MUST BE DFS)",
	"Apply to this share",
};

static const value_string nt_notify_action_vals[] = {
	{1,	"ADDED (object was added"},
	{2,	"REMOVED (object was removed)"},
	{3,	"MODIFIED (object was modified)"},
	{4,	"RENAMED_OLD_NAME (this is the old name of object)"},
	{5,	"RENAMED_NEW_NAME (this is the new name of object)"},
	{6,	"ADDED_STREAM (a stream was added)"},
	{7,	"REMOVED_STREAM (a stream was removed)"},
	{8,	"MODIFIED_STREAM (a stream was modified)"},
	{0, NULL}
};

static const value_string watch_tree_vals[] = {
	{0,	"Current directory only"},
	{1,	"Subdirectories also"},
	{0, NULL}
};

#define NT_NOTIFY_STREAM_WRITE	0x00000800
#define NT_NOTIFY_STREAM_SIZE	0x00000400
#define NT_NOTIFY_STREAM_NAME	0x00000200
#define NT_NOTIFY_SECURITY	0x00000100
#define NT_NOTIFY_EA		0x00000080
#define NT_NOTIFY_CREATION	0x00000040
#define NT_NOTIFY_LAST_ACCESS	0x00000020
#define NT_NOTIFY_LAST_WRITE	0x00000010
#define NT_NOTIFY_SIZE		0x00000008
#define NT_NOTIFY_ATTRIBUTES	0x00000004
#define NT_NOTIFY_DIR_NAME	0x00000002
#define NT_NOTIFY_FILE_NAME	0x00000001
static const true_false_string tfs_nt_notify_stream_write = {
	"Notify on changes to STREAM WRITE",
	"Do NOT notify on changes to stream write",
};
static const true_false_string tfs_nt_notify_stream_size = {
	"Notify on changes to STREAM SIZE",
	"Do NOT notify on changes to stream size",
};
static const true_false_string tfs_nt_notify_stream_name = {
	"Notify on changes to STREAM NAME",
	"Do NOT notify on changes to stream name",
};
static const true_false_string tfs_nt_notify_security = {
	"Notify on changes to SECURITY",
	"Do NOT notify on changes to security",
};
static const true_false_string tfs_nt_notify_ea = {
	"Notify on changes to EA",
	"Do NOT notify on changes to EA",
};
static const true_false_string tfs_nt_notify_creation = {
	"Notify on changes to CREATION TIME",
	"Do NOT notify on changes to creation time",
};
static const true_false_string tfs_nt_notify_last_access = {
	"Notify on changes to LAST ACCESS TIME",
	"Do NOT notify on changes to last access time",
};
static const true_false_string tfs_nt_notify_last_write = {
	"Notify on changes to LAST WRITE TIME",
	"Do NOT notify on changes to last write time",
};
static const true_false_string tfs_nt_notify_size = {
	"Notify on changes to SIZE",
	"Do NOT notify on changes to size",
};
static const true_false_string tfs_nt_notify_attributes = {
	"Notify on changes to ATTRIBUTES",
	"Do NOT notify on changes to attributes",
};
static const true_false_string tfs_nt_notify_dir_name = {
	"Notify on changes to DIR NAME",
	"Do NOT notify on changes to dir name",
};
static const true_false_string tfs_nt_notify_file_name = {
	"Notify on changes to FILE NAME",
	"Do NOT notify on changes to file name",
};

static const value_string create_disposition_vals[] = {
	{0,	"Supersede (supersede existing file (if it exists))"},
	{1,	"Open (if file exists open it, else fail)"},
	{2,	"Create (if file exists fail, else create it)"},
	{3,	"Open If (if file exists open it, else create it)"},
	{4,	"Overwrite (if file exists overwrite, else fail)"},
	{5,	"Overwrite If (if file exists overwrite, else create it)"},
	{0, NULL}
};

static const value_string impersonation_level_vals[] = {
	{0,	"Anonymous"},
	{1,	"Identification"},
	{2,	"Impersonation"},
	{3,	"Delegation"},
	{0, NULL}
};

static const true_false_string tfs_nt_security_flags_context_tracking = {
	"Security tracking mode is DYNAMIC",
	"Security tracking mode is STATIC",
};

static const true_false_string tfs_nt_security_flags_effective_only = {
	"ONLY ENABLED aspects of the clients security context are available",
	"ALL aspects of the clients security context are available",
};

static const true_false_string tfs_nt_create_bits_oplock = {
	"Requesting OPLOCK",
	"Does NOT request oplock"
};

static const true_false_string tfs_nt_create_bits_boplock = {
	"Requesting BATCH OPLOCK",
	"Does NOT request batch oplock"
};

static const true_false_string tfs_nt_create_bits_dir = {
	"Target of open MUST be a DIRECTORY",
	"Target of open can be a file"
};

static const true_false_string tfs_nt_access_mask_generic_read = {
	"GENERIC READ is set",
	"Generic read in NOT set"
};
static const true_false_string tfs_nt_access_mask_generic_write = {
	"GENERIC WRITE is set",
	"Generic write is NOT set"
};
static const true_false_string tfs_nt_access_mask_generic_execute = {
	"GENERIC EXECUTE is set",
	"Generic execute is NOT set"
};
static const true_false_string tfs_nt_access_mask_generic_all = {
	"GENERIC ALL is set",
	"Generic all is NOT set"
};
static const true_false_string tfs_nt_access_mask_maximum_allowed = {
	"MAXIMUM ALLOWED is set",
	"Maximum allowed is NOT set"
};
static const true_false_string tfs_nt_access_mask_system_security = {
	"SYSTEM SECURITY is set",
	"System security is NOT set"
};
static const true_false_string tfs_nt_access_mask_synchronize = {
	"SYNCHRONIZE access",
	"Do NOT synchronize access"
};
static const true_false_string tfs_nt_access_mask_write_owner = {
	"OWNER may WRITE to the file",
	"Owner can NOT write to the file"
};
static const true_false_string tfs_nt_access_mask_write_dac = {
	"OWNER may WRITE the DAC",
	"Owner may NOT write to the DAC"
};
static const true_false_string tfs_nt_access_mask_read_control = {
	"READ ACCESS to owner, group and ACL of the SID",
	"Read access is NOT granted to owner, group and ACL of the SID"
};
static const true_false_string tfs_nt_access_mask_delete = {
	"DELETE access",
	"NO delete access"
};

static const true_false_string tfs_nt_share_access_delete = {
	"Object can be shared for DELETE",
	"Object can NOT be shared for delete"
};
static const true_false_string tfs_nt_share_access_write = {
	"Object can be shared for WRITE",
	"Object can NOT be shared for write"
};
static const true_false_string tfs_nt_share_access_read = {
	"Object can be shared for READ",
	"Object can NOT be shared for delete"
};

static const value_string oplock_level_vals[] = {
	{0,	"No oplock granted"},
	{1,	"Exclusive oplock granted"},
	{2,	"Batch oplock granted"},
	{3,	"Level II oplock granted"},
	{0, NULL}
};

static const value_string device_type_vals[] = {
	{0x00000001,	"Beep"},
	{0x00000002,	"CDROM"},
	{0x00000003,	"CDROM Filesystem"},
	{0x00000004,	"Controller"},
	{0x00000005,	"Datalink"},
	{0x00000006,	"DFS"},
	{0x00000007,	"Disk"},
	{0x00000008,	"Disk Filesystem"},
	{0x00000009,	"Filesystem"},
	{0x0000000a,	"Inport Port"},
	{0x0000000b,	"Keyboard"},
	{0x0000000c,	"Mailslot"},
	{0x0000000d,	"MIDI-In"},
	{0x0000000e,	"MIDI-Out"},
	{0x0000000f,	"Mouse"},
	{0x00000010,	"Multi UNC Provider"},
	{0x00000011,	"Named Pipe"},
	{0x00000012,	"Network"},
	{0x00000013,	"Network Browser"},
	{0x00000014,	"Network Filesystem"},
	{0x00000015,	"NULL"},
	{0x00000016,	"Parallel Port"},
	{0x00000017,	"Physical card"},
	{0x00000018,	"Printer"},
	{0x00000019,	"Scanner"},
	{0x0000001a,	"Serial Mouse port"},
	{0x0000001b,	"Serial port"},
	{0x0000001c,	"Screen"},
	{0x0000001d,	"Sound"},
	{0x0000001e,	"Streams"},
	{0x0000001f,	"Tape"},
	{0x00000020,	"Tape Filesystem"},
	{0x00000021,	"Transport"},
	{0x00000022,	"Unknown"},
	{0x00000023,	"Video"},
	{0x00000024,	"Virtual Disk"},
	{0x00000025,	"WAVE-In"},
	{0x00000026,	"WAVE-Out"},
	{0x00000027,	"8042 Port"},
	{0x00000028,	"Network Redirector"},
	{0x00000029,	"Battery"},
	{0x0000002a,	"Bus Extender"},
	{0x0000002b,	"Modem"},
	{0x0000002c,	"VDM"},
	{0,	NULL}
};

static const value_string is_directory_vals[] = {
	{0,	"This is NOT a directory"},
	{1,	"This is a DIRECTORY"},
	{0, NULL}
};

typedef struct _nt_trans_data {
	guint32 sd_len;
	guint32 ea_len;
} nt_trans_data;



static int
dissect_nt_security_flags(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint8 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_guint8(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 1,
			"Security Flags: 0x%02x", mask);
		tree = proto_item_add_subtree(item, ett_smb_nt_security_flags);
	}

	proto_tree_add_boolean(tree, hf_smb_nt_security_flags_context_tracking,
		tvb, offset, 1, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_security_flags_effective_only,
		tvb, offset, 1, mask);

	offset += 1;

	return offset;
}

static int
dissect_nt_share_access(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint32 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohl(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 4,
			"Share Access: 0x%08x", mask);
		tree = proto_item_add_subtree(item, ett_smb_nt_share_access);
	}

	proto_tree_add_boolean(tree, hf_smb_nt_share_access_delete,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_share_access_write,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_share_access_read,
		tvb, offset, 4, mask);

	offset += 4;

	return offset;
}


static int
dissect_nt_access_mask(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint32 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohl(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 4,
			"Access Mask: 0x%08x", mask);
		tree = proto_item_add_subtree(item, ett_smb_nt_access_mask);
	}

	proto_tree_add_boolean(tree, hf_smb_nt_access_mask_generic_read,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_access_mask_generic_write,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_access_mask_generic_execute,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_access_mask_generic_all,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_access_mask_maximum_allowed,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_access_mask_system_security,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_access_mask_synchronize,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_access_mask_write_owner,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_access_mask_write_dac,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_access_mask_read_control,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_access_mask_delete,
		tvb, offset, 4, mask);

	offset += 4;

	return offset;
}

static int
dissect_nt_create_bits(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint32 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohl(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 4,
			"Create Flags: 0x%04x", mask);
		tree = proto_item_add_subtree(item, ett_smb_nt_create_bits);
	}

	proto_tree_add_boolean(tree, hf_smb_nt_create_bits_dir,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_create_bits_boplock,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_create_bits_oplock,
		tvb, offset, 4, mask);

	offset += 4;

	return offset;
}
 
static int
dissect_nt_notify_completion_filter(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint32 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohl(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 4,
			"Completion Filter: 0x%08x", mask);
		tree = proto_item_add_subtree(item, ett_smb_nt_notify_completion_filter);
	}
 
	proto_tree_add_boolean(tree, hf_smb_nt_notify_stream_write,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_notify_stream_size,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_notify_stream_name,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_notify_security,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_notify_ea,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_notify_creation,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_notify_last_access,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_notify_last_write,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_notify_size,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_notify_attributes,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_notify_dir_name,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_notify_file_name,
		tvb, offset, 4, mask);

	offset += 4;
	return offset;
}
 
static int
dissect_nt_ioctl_flags(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint8 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_guint8(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 1,
			"Completion Filter: 0x%02x", mask);
		tree = proto_item_add_subtree(item, ett_smb_nt_ioctl_flags);
	}

	proto_tree_add_boolean(tree, hf_smb_nt_ioctl_flags_root_handle,
		tvb, offset, 1, mask);

	offset += 1;
	return offset;
}

/*
 * From the section on ZwQuerySecurityObject in "Windows(R) NT(R)/2000
 * Native API Reference".
 */
static const true_false_string tfs_nt_qsd_owner = {
	"Requesting OWNER security information",
	"NOT requesting owner security information",
};

static const true_false_string tfs_nt_qsd_group = {
	"Requesting GROUP security information",
	"NOT requesting group security information",
};

static const true_false_string tfs_nt_qsd_dacl = {
	"Requesting DACL security information",
	"NOT requesting DACL security information",
};

static const true_false_string tfs_nt_qsd_sacl = {
	"Requesting SACL security information",
	"NOT requesting SACL security information",
};

#define NT_QSD_OWNER	0x00000001
#define NT_QSD_GROUP	0x00000002
#define NT_QSD_DACL	0x00000004
#define NT_QSD_SACL	0x00000008

static int
dissect_security_information_mask(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint32 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohl(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 4,
			"Security Information: 0x%08x", mask);
		tree = proto_item_add_subtree(item, ett_smb_security_information_mask);
	}

	proto_tree_add_boolean(tree, hf_smb_nt_qsd_owner,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_qsd_group,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_qsd_dacl,
		tvb, offset, 4, mask);
	proto_tree_add_boolean(tree, hf_smb_nt_qsd_sacl,
		tvb, offset, 4, mask);

	offset += 4;

	return offset;
}


static int
dissect_nt_trans_data_request(tvbuff_t *tvb, packet_info *pinfo, int offset, proto_tree *parent_tree, int len, nt_trans_data *ntd)
{
	proto_item *item = NULL;
	proto_tree *tree = NULL;
	smb_info_t *si;

	si = (smb_info_t *)pinfo->private_data;

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, len,
				"%s Data",
				val_to_str(si->subcmd, nt_cmd_vals, "Unknown NT transaction (%u)"));
		tree = proto_item_add_subtree(item, ett_smb_nt_trans_setup);
	}

	switch(si->subcmd){
	case NT_TRANS_CREATE:
		/* security descriptor */
		if(ntd->sd_len){
			proto_tree_add_item(tree, hf_smb_security_descriptor, tvb, offset, ntd->sd_len, TRUE);
			offset += ntd->sd_len;
		}

		/* extended attributes */
		if(ntd->ea_len){
			proto_tree_add_item(tree, hf_smb_extended_attributes, tvb, offset, ntd->ea_len, TRUE);
			offset += ntd->ea_len;
		}

		break;
	case NT_TRANS_IOCTL:
		/* ioctl data */
		proto_tree_add_item(tree, hf_smb_nt_ioctl_data, tvb, offset, len, TRUE);
		offset += len;

		break;
	case NT_TRANS_SSD:
		proto_tree_add_item(tree, hf_smb_security_descriptor, tvb, offset, len, TRUE);
		offset += len;
		break;
	case NT_TRANS_NOTIFY:
		break;
	case NT_TRANS_RENAME:
		/* XXX not documented */
		break;
	case NT_TRANS_QSD:
		break;
	}

	return offset;
}

static int
dissect_nt_trans_param_request(tvbuff_t *tvb, packet_info *pinfo, int offset, proto_tree *parent_tree, int len, nt_trans_data *ntd, guint16 bc)
{
	proto_item *item = NULL;
	proto_tree *tree = NULL;
	smb_info_t *si;
	guint32 fn_len;
	const char *fn;

	si = (smb_info_t *)pinfo->private_data;

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, len,
				"%s Parameters",
				val_to_str(si->subcmd, nt_cmd_vals, "Unknown NT transaction (%u)"));
		tree = proto_item_add_subtree(item, ett_smb_nt_trans_setup);
	}

	switch(si->subcmd){
	case NT_TRANS_CREATE:
		/* Create flags */
		offset = dissect_nt_create_bits(tvb, pinfo, tree, offset);

		/* root directory fid */
		proto_tree_add_item(tree, hf_smb_root_dir_fid, tvb, offset, 4, TRUE);
		offset += 4;

		/* nt access mask */
		offset = dissect_nt_access_mask(tvb, pinfo, tree, offset);
	
		/* allocation size */
		proto_tree_add_item(tree, hf_smb_alloc_size64, tvb, offset, 8, TRUE);
		offset += 8;
	
		/* Extended File Attributes */
		offset = dissect_file_ext_attr(tvb, pinfo, tree, offset);

		/* share access */
		offset = dissect_nt_share_access(tvb, pinfo, tree, offset);
	
		/* create disposition */
		proto_tree_add_item(tree, hf_smb_nt_create_disposition, tvb, offset, 4, TRUE);
		offset += 4;

		/* create options */
		proto_tree_add_item(tree, hf_smb_nt_create_options, tvb, offset, 4, TRUE);
		offset += 4;

		/* sd length */
		ntd->sd_len = tvb_get_letohl(tvb, offset);
		proto_tree_add_uint(tree, hf_smb_sd_length, tvb, offset, 4, ntd->sd_len);
		offset += 4;

		/* ea length */
		ntd->ea_len = tvb_get_letohl(tvb, offset);
		proto_tree_add_uint(tree, hf_smb_ea_length, tvb, offset, 4, ntd->ea_len);
		offset += 4;

		/* file name len */
		fn_len = (guint32)tvb_get_letohl(tvb, offset);
		proto_tree_add_uint(tree, hf_smb_file_name_len, tvb, offset, 4, fn_len);
		offset += 4;

		/* impersonation level */
		proto_tree_add_item(tree, hf_smb_nt_impersonation_level, tvb, offset, 4, TRUE);
		offset += 4;
	
		/* security flags */
		offset = dissect_nt_security_flags(tvb, pinfo, tree, offset);

		/* file name */
		fn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &fn_len, TRUE, TRUE, &bc);
		proto_tree_add_string(tree, hf_smb_file_name, tvb, offset, fn_len,
			fn);
		offset += fn_len;

		break;
	case NT_TRANS_IOCTL:
		break;
	case NT_TRANS_SSD:
		/* fid */
		proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
		offset += 2;

		/* 2 reserved bytes */
		proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 2, TRUE);
		offset += 2;

		/* security information */
		offset = dissect_security_information_mask(tvb, pinfo, tree, offset);
		break;
	case NT_TRANS_NOTIFY:
		break;
	case NT_TRANS_RENAME:
		/* XXX not documented */
		break;
	case NT_TRANS_QSD:
		/* fid */
		proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
		offset += 2;

		/* 2 reserved bytes */
		proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 2, TRUE);
		offset += 2;

		/* security information */
		offset = dissect_security_information_mask(tvb, pinfo, tree, offset);
		break;
	}

	return offset;
}

static int
dissect_nt_trans_setup_request(tvbuff_t *tvb, packet_info *pinfo, int offset, proto_tree *parent_tree, int len, nt_trans_data *ntd)
{
	proto_item *item = NULL;
	proto_tree *tree = NULL;
	smb_info_t *si;
	int old_offset = offset;

	si = (smb_info_t *)pinfo->private_data;

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, len,
				"%s Setup",
				val_to_str(si->subcmd, nt_cmd_vals, "Unknown NT transaction (%u)"));
		tree = proto_item_add_subtree(item, ett_smb_nt_trans_setup);
	}
 
	switch(si->subcmd){
	case NT_TRANS_CREATE:
		break;
	case NT_TRANS_IOCTL:
		/* function code */
		proto_tree_add_item(tree, hf_smb_nt_ioctl_function_code, tvb, offset, 4, TRUE);
		offset += 4;

		/* fid */
		proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
		offset += 2;

		/* isfsctl */
		proto_tree_add_item(tree, hf_smb_nt_ioctl_isfsctl, tvb, offset, 1, TRUE);
		offset += 1;

		/* isflags */
		offset = dissect_nt_ioctl_flags(tvb, pinfo, tree, offset);

		break;
	case NT_TRANS_SSD:
		break;
	case NT_TRANS_NOTIFY:
		/* completion filter */
		offset = dissect_nt_notify_completion_filter(tvb, pinfo, tree, offset);

		/* fid */
		proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
		offset += 2;

		/* watch tree */
		proto_tree_add_item(tree, hf_smb_nt_notify_watch_tree, tvb, offset, 1, TRUE);
		offset += 1;

		/* reserved byte */
		proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 1, TRUE);
		offset += 1;

		break;
	case NT_TRANS_RENAME:
		/* XXX not documented */
		break;
	case NT_TRANS_QSD:
		break;
	}
 
	return old_offset+len;
}


static int
dissect_nt_transaction_request(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc, sc;
	guint32 pc=0, po=0, pd, dc=0, od=0, dd;
	smb_info_t *si;
	static nt_trans_data ntd;
	guint16 bc;
	int padcnt;

	si = (smb_info_t *)pinfo->private_data;

	WORD_COUNT;

	if(wc>=19){
		/* primary request */
		/* max setup count */
		proto_tree_add_item(tree, hf_smb_max_setup_count, tvb, offset, 1, TRUE);
		offset += 1;

		/* 2 reserved bytes */
		proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 2, TRUE);
		offset += 2;
	} else {
		/* secondary request */
		/* 3 reserved bytes */
		proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 3, TRUE);
		offset += 3;
	}


	/* total param count */
	proto_tree_add_item(tree, hf_smb_total_param_count, tvb, offset, 4, TRUE);
	offset += 4;
	
	/* total data count */
	proto_tree_add_item(tree, hf_smb_total_data_count, tvb, offset, 4, TRUE);
	offset += 4;

	if(wc>=19){
		/* primary request */
		/* max param count */
		proto_tree_add_item(tree, hf_smb_max_param_count, tvb, offset, 4, TRUE);
		offset += 4;

		/* max data count */
		proto_tree_add_item(tree, hf_smb_max_data_count, tvb, offset, 4, TRUE);
		offset += 4;
	}

	/* param count */
	pc = tvb_get_letohl(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_param_count32, tvb, offset, 4, pc);
	offset += 4;
	
	/* param offset */
	po = tvb_get_letohl(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_param_offset32, tvb, offset, 4, po);
	offset += 4;

	/* param displacement */
	if(wc>=19){
		/* primary request*/
		pd = 0;
	} else {
		/* secondary request */
		pd = tvb_get_letohl(tvb, offset);
		proto_tree_add_uint(tree, hf_smb_param_disp32, tvb, offset, 4, pd);
		offset += 4;
	}

	/* data count */
	dc = tvb_get_letohl(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_data_count32, tvb, offset, 4, dc);
	offset += 4;

	/* data offset */
	od = tvb_get_letohl(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_data_offset32, tvb, offset, 4, od);
	offset += 4;

	/* data displacement */
	if(wc>=19){
		/* primary request */
		dd = 0;
	} else {
		/* secondary request */
		dd = tvb_get_letohl(tvb, offset);
		proto_tree_add_uint(tree, hf_smb_data_disp32, tvb, offset, 4, dd);
		offset += 4;
	}

	/* setup count */
	if(wc>=19){
		/* primary request */
		sc = tvb_get_guint8(tvb, offset);
		proto_tree_add_uint(tree, hf_smb_setup_count, tvb, offset, 1, sc);
		offset += 1;
	} else {
		/* secondary request */
		sc = 0;
	}

	/* function */
	if(wc>=19){
		/* primary request */
		si->subcmd = tvb_get_letohs(tvb, offset);
		proto_tree_add_uint(tree, hf_smb_nt_trans_subcmd, tvb, offset, 2, si->subcmd);
		if(check_col(pinfo->fd, COL_INFO)){
			col_append_fstr(pinfo->fd, COL_INFO, ", %s",
				val_to_str(si->subcmd, nt_cmd_vals, "<unknown>"));
		}
	} else {
		/* secondary request */
		if(check_col(pinfo->fd, COL_INFO)){
			col_append_fstr(pinfo->fd, COL_INFO, " (secondary request)");
		}
	}
	offset += 2;

	/* this is a padding byte */
	if(offset%1){
		/* pad byte */
	        proto_tree_add_item(tree, hf_smb_padding, tvb, offset, 1, TRUE);
		offset += 1;
	}

	/* if there were any setup bytes, decode them */
	if(sc){
		dissect_nt_trans_setup_request(tvb, pinfo, offset, tree, sc*2, &ntd);
		offset += sc*2;
	}

	BYTE_COUNT;
	
	/* parameters */
	if(po>(guint32)offset){
		/* We have some initial padding bytes.
		*/
		padcnt = po-offset;
		if (padcnt > bc)
			padcnt = bc;
	        proto_tree_add_item(tree, hf_smb_padding, tvb, offset, padcnt, TRUE);
		COUNT_BYTES(padcnt);
	}
	if(pc){
		CHECK_BYTE_COUNT(pc);
		dissect_nt_trans_param_request(tvb, pinfo, offset, tree, pc, &ntd, bc);
		COUNT_BYTES(pc);
	}

	/* data */
	if(od>(guint32)offset){
		/* We have some initial padding bytes.
		*/
		padcnt = od-offset;
		if (padcnt > bc)
			padcnt = bc;
	        proto_tree_add_item(tree, hf_smb_padding, tvb, offset, padcnt, TRUE);
		COUNT_BYTES(padcnt);
	}
	if(dc){
		CHECK_BYTE_COUNT(dc);
		dissect_nt_trans_data_request(tvb, pinfo, offset, tree, dc, &ntd);
		COUNT_BYTES(dc);
	}

	END_OF_SMB

	return offset;
}



static int
dissect_nt_trans_data_response(tvbuff_t *tvb, packet_info *pinfo, int offset, proto_tree *parent_tree, int len, nt_trans_data *ntd)
{
	proto_item *item = NULL;
	proto_tree *tree = NULL;
	smb_info_t *si;

	si = (smb_info_t *)pinfo->private_data;

	if(parent_tree){
		if(si->frame_req != 0){
			item = proto_tree_add_text(parent_tree, tvb, offset, len,
				"%s Data",
				val_to_str(si->subcmd, nt_cmd_vals, "Unknown NT transaction (%u)"));
		} else {
			/*
			 * We never saw the request to which this is a
			 * response.
			 */
			item = proto_tree_add_text(parent_tree, tvb, offset, len,
				"%s Data",
				val_to_str(si->subcmd, nt_cmd_vals, "Unknown NT transaction (matching request not seen)"));
		}
		tree = proto_item_add_subtree(item, ett_smb_nt_trans_setup);
	}

	switch(si->subcmd){
	case NT_TRANS_CREATE:
		break;
	case NT_TRANS_IOCTL:
		/* ioctl data */
		proto_tree_add_item(tree, hf_smb_nt_ioctl_data, tvb, offset, len, TRUE);
		offset += len;

		break;
	case NT_TRANS_SSD:
		break;
	case NT_TRANS_NOTIFY:
		break;
	case NT_TRANS_RENAME:
		/* XXX not documented */
		break;
	case NT_TRANS_QSD:
		/*
		 * XXX - this is probably a SECURITY_DESCRIPTOR structure,
		 * which may be documented in the Win32 documentation
		 * somewhere.
		 */
		proto_tree_add_item(tree, hf_smb_security_descriptor, tvb, offset, len, TRUE);
		offset += len;
		break;
	}

	return offset;
}
 
static int
dissect_nt_trans_param_response(tvbuff_t *tvb, packet_info *pinfo, int offset, proto_tree *parent_tree, int len, nt_trans_data *ntd, guint16 bc)
{
	proto_item *item = NULL;
	proto_tree *tree = NULL;
	guint32 fn_len;
	const char *fn;
	smb_info_t *si;

	si = (smb_info_t *)pinfo->private_data;

	if(parent_tree){
		if(si->frame_req != 0){
			item = proto_tree_add_text(parent_tree, tvb, offset, len,
				"%s Parameters",
				val_to_str(si->subcmd, nt_cmd_vals, "Unknown NT transaction (%u)"));
		} else {
			/*
			 * We never saw the request to which this is a
			 * response.
			 */
			item = proto_tree_add_text(parent_tree, tvb, offset, len,
				"%s Parameters",
				val_to_str(si->subcmd, nt_cmd_vals, "Unknown NT transaction (matching request not seen)"));
		}
		tree = proto_item_add_subtree(item, ett_smb_nt_trans_setup);
	}

	switch(si->subcmd){
	case NT_TRANS_CREATE:
		/* oplock level */
	        proto_tree_add_item(tree, hf_smb_oplock_level, tvb, offset, 1, TRUE);
		offset += 1;

		/* reserved byte */
	        proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 1, TRUE);
		offset += 1;
		
		/* fid */
		proto_tree_add_item(tree, hf_smb_fid, tvb, offset, 2, TRUE);
		offset += 2;

		/* create action */
		proto_tree_add_item(tree, hf_smb_create_action, tvb, offset, 4, TRUE);
		offset += 4;

		/* ea error offset */
		proto_tree_add_item(tree, hf_smb_ea_error_offset, tvb, offset, 4, TRUE);
		offset += 4;

		/* create time */
		offset = dissect_smb_64bit_time(tvb, pinfo, tree, offset,
			"Create Time", hf_smb_create_time);
	
		/* access time */
		offset = dissect_smb_64bit_time(tvb, pinfo, tree, offset,
			"Access Time", hf_smb_access_time);
	
		/* last write time */
		offset = dissect_smb_64bit_time(tvb, pinfo, tree, offset,
			"Write Time", hf_smb_last_write_time);
	
		/* last change time */
		offset = dissect_smb_64bit_time(tvb, pinfo, tree, offset,
			"Change Time", hf_smb_change_time);
	
		/* Extended File Attributes */
		offset = dissect_file_ext_attr(tvb, pinfo, tree, offset);

		/* allocation size */
		proto_tree_add_item(tree, hf_smb_alloc_size64, tvb, offset, 8, TRUE);
		offset += 8;

		/* end of file */
		proto_tree_add_item(tree, hf_smb_end_of_file, tvb, offset, 8, TRUE);
		offset += 8;

		/* File Type */
		proto_tree_add_item(tree, hf_smb_file_type, tvb, offset, 2, TRUE);
		offset += 2;

		/* device type */
		/* XXX is this a 16 or 32 bit integer? need to check the spec*/
		proto_tree_add_item(tree, hf_smb_device_type, tvb, offset, 4, TRUE);
		offset += 4;

		/* is directory */
		proto_tree_add_item(tree, hf_smb_is_directory, tvb, offset, 1, TRUE);
		offset += 1;
		break;
	case NT_TRANS_IOCTL:
		break;
	case NT_TRANS_SSD:
		break;
	case NT_TRANS_NOTIFY:
		while(len){
			/* next entry offset */
			proto_tree_add_item(tree, hf_smb_next_entry_offset, tvb, offset, 4, TRUE);
			offset += 4;
			len -= 4;
			/* broken implementations */
			if(len<0)break;
	
			/* action */
			proto_tree_add_item(tree, hf_smb_nt_notify_action, tvb, offset, 4, TRUE);
			offset += 4;
			len -= 4;
			/* broken implementations */
			if(len<0)break;

			/* file name len */
			fn_len = (guint32)tvb_get_letohl(tvb, offset);
			proto_tree_add_uint(tree, hf_smb_file_name_len, tvb, offset, 4, fn_len);
			offset += 4;
			len -= 4;
			/* broken implementations */
			if(len<0)break;

			/* file name */
			fn = get_unicode_or_ascii_string_tvb(tvb, &offset, pinfo, &fn_len, TRUE, TRUE, &bc);
			proto_tree_add_string(tree, hf_smb_file_name, tvb, offset, fn_len,
				fn);
			offset += fn_len;
			len -= fn_len;
			/* broken implementations */
			if(len<0)break;

		}
		break;
	case NT_TRANS_RENAME:
		/* XXX not documented */
		break;
	case NT_TRANS_QSD:
		/*
		 * This appears to be the size of the security
		 * descriptor; the calling sequence of
		 * "ZwQuerySecurityObject()" suggests that it would
		 * be.  The actual security descriptor wouldn't
		 * follow if the max data count in the request
		 * was smaller; this lets the client know how
		 * big a buffer it needs to provide.
		 */
		proto_tree_add_item(tree, hf_smb_security_descriptor_len, tvb, offset, 4, TRUE);
		offset += 4;
		break;
	}

	return offset;
}
 
static int
dissect_nt_trans_setup_response(tvbuff_t *tvb, packet_info *pinfo, int offset, proto_tree *parent_tree, int len, nt_trans_data *ntd)
{
	proto_item *item = NULL;
	proto_tree *tree = NULL;
	smb_info_t *si;

	si = (smb_info_t *)pinfo->private_data;

	if(parent_tree){
		if(si->frame_req != 0){
			item = proto_tree_add_text(parent_tree, tvb, offset, len,
				"%s Setup",
				val_to_str(si->subcmd, nt_cmd_vals, "Unknown NT transaction (%u)"));
		} else {
			/*
			 * We never saw the request to which this is a
			 * response.
			 */
			item = proto_tree_add_text(parent_tree, tvb, offset, len,
				"%s Setup",
				val_to_str(si->subcmd, nt_cmd_vals, "Unknown NT transaction (matching request not seen)"));
		}
		tree = proto_item_add_subtree(item, ett_smb_nt_trans_setup);
	}

	switch(si->subcmd){
	case NT_TRANS_CREATE:
		break;
	case NT_TRANS_IOCTL:
		break;
	case NT_TRANS_SSD:
		break;
	case NT_TRANS_NOTIFY:
		break;
	case NT_TRANS_RENAME:
		/* XXX not documented */
		break;
	case NT_TRANS_QSD:
		break;
	}

	return offset;
}

static int
dissect_nt_transaction_response(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree)
{
	guint8 wc, sc;
	guint32 pc=0, po=0, pd, dc=0, od=0, dd;
	smb_info_t *si;
	static nt_trans_data ntd;
	guint16 bc;
	int padcnt;

	si = (smb_info_t *)pinfo->private_data;

	/* primary request */
	if(si->frame_req != 0){
		proto_tree_add_uint(tree, hf_smb_nt_trans_subcmd, tvb, 0, 0, si->subcmd);
		if(check_col(pinfo->fd, COL_INFO)){
			col_append_fstr(pinfo->fd, COL_INFO, ", %s",
				val_to_str(si->subcmd, nt_cmd_vals, "<unknown (%u)>"));
		}
	} else {
		proto_tree_add_text(tree, tvb, offset, 0,
			"Function: <unknown function - could not find matching request>");
		if(check_col(pinfo->fd, COL_INFO)){
			col_append_fstr(pinfo->fd, COL_INFO, ", <unknown>");
		}
	}

	WORD_COUNT;

	/* 3 reserved bytes */
	proto_tree_add_item(tree, hf_smb_reserved, tvb, offset, 3, TRUE);
	offset += 3;

	/* total param count */
	proto_tree_add_item(tree, hf_smb_total_param_count, tvb, offset, 4, TRUE);
	offset += 4;
	
	/* total data count */
	proto_tree_add_item(tree, hf_smb_total_data_count, tvb, offset, 4, TRUE);
	offset += 4;

	/* param count */
	pc = tvb_get_letohl(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_param_count32, tvb, offset, 4, pc);
	offset += 4;
	
	/* param offset */
	po = tvb_get_letohl(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_param_offset32, tvb, offset, 4, po);
	offset += 4;

	/* param displacement */
	pd = tvb_get_letohl(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_param_disp32, tvb, offset, 4, pd);
	offset += 4;

	/* data count */
	dc = tvb_get_letohl(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_data_count32, tvb, offset, 4, dc);
	offset += 4;

	/* data offset */
	od = tvb_get_letohl(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_data_offset32, tvb, offset, 4, od);
	offset += 4;

	/* data displacement */
	dd = tvb_get_letohl(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_data_disp32, tvb, offset, 4, dd);
	offset += 4;

	/* setup count */
	sc = tvb_get_guint8(tvb, offset);
	proto_tree_add_uint(tree, hf_smb_setup_count, tvb, offset, 1, sc);
	offset += 1;

	/* setup data */	
	if(sc){
		dissect_nt_trans_setup_response(tvb, pinfo, offset, tree, sc*2, &ntd);
		offset += sc*2;
	}

	BYTE_COUNT;
	
	/* parameters */
	if(po>(guint32)offset){
		/* We have some initial padding bytes.
		*/
		padcnt = po-offset;
		if (padcnt > bc)
			padcnt = bc;
	        proto_tree_add_item(tree, hf_smb_padding, tvb, offset, padcnt, TRUE);
		COUNT_BYTES(padcnt);
	}
	if(pc){
		CHECK_BYTE_COUNT(pc);
		dissect_nt_trans_param_response(tvb, pinfo, offset, tree, pc, &ntd, bc);
		COUNT_BYTES(pc);
	}

	/* data */
	if(od>(guint32)offset){
		/* We have some initial padding bytes.
		*/
		padcnt = od-offset;
		if (padcnt > bc)
			padcnt = bc;
	        proto_tree_add_item(tree, hf_smb_padding, tvb, offset, padcnt, TRUE);
		COUNT_BYTES(padcnt);
	}
	if(dc){
		CHECK_BYTE_COUNT(dc);
		dissect_nt_trans_data_response(tvb, pinfo, offset, tree, dc, &ntd);
		COUNT_BYTES(dc);
	}

	END_OF_SMB

	return offset;
}

/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
   NT Transaction command  ends here
   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */



typedef struct _smb_function {
       int (*request)(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree);
       int (*response)(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree);
} smb_function;

smb_function smb_dissector[256] = {
  /* 0x00 Create Dir*/  {dissect_old_dir_request, dissect_empty},
  /* 0x01 Delete Dir*/  {dissect_old_dir_request, dissect_empty},
  /* 0x02 Open File*/  {dissect_open_file_request, dissect_open_file_response},
  /* 0x03 Create File*/  {dissect_create_file_request, dissect_fid},
  /* 0x04 Close File*/  {dissect_close_file_request, dissect_empty},
  /* 0x05 Flush File*/  {dissect_fid, dissect_empty},
  /* 0x06 Delete File*/  {dissect_delete_file_request, dissect_empty},
  /* 0x07 Rename File*/  {dissect_rename_file_request, dissect_empty},
  /* 0x08 Query Info*/  {dissect_query_information_request, dissect_query_information_response},
  /* 0x09 Set Info*/  {dissect_set_information_request, dissect_empty},
  /* 0x0a Read File*/  {dissect_read_file_request, dissect_read_file_response},
  /* 0x0b Write File*/  {dissect_write_file_request, dissect_write_file_response},
  /* 0x0c Lock Byte Range*/  {dissect_lock_request, dissect_empty},
  /* 0x0d Unlock Byte Range*/  {dissect_lock_request, dissect_empty},
  /* 0x0e Create Temp*/  {dissect_create_temporary_request, dissect_create_temporary_response},
  /* 0x0f Create New*/  {dissect_create_file_request, dissect_fid},

  /* 0x10 Check Dir*/  {dissect_old_dir_request, dissect_empty},
  /* 0x11 Process Exit*/  {dissect_empty, dissect_empty},
  /* 0x12 Seek File*/  {dissect_seek_file_request, dissect_seek_file_response},
  /* 0x13 Lock And Read*/  {dissect_read_file_request, dissect_lock_and_read_response},
  /* 0x14 Write And Unlock*/  {dissect_write_file_request, dissect_write_file_response},
  /* 0x15 */  {NULL, NULL},
  /* 0x16 */  {NULL, NULL},
  /* 0x17 */  {NULL, NULL},
  /* 0x18 */  {NULL, NULL},
  /* 0x19 */  {NULL, NULL},
  /* 0x1a Read Raw*/  {dissect_read_raw_request, NULL},
  /* 0x1b Read MPX*/  {dissect_read_mpx_request, dissect_read_mpx_response},
  /* 0x1c */  {NULL, NULL},
  /* 0x1d Write Raw*/  {dissect_write_raw_request, dissect_write_raw_response},
  /* 0x1e Write MPX*/  {dissect_write_mpx_request, dissect_write_mpx_response},
  /* 0x1f */  {NULL, NULL},

  /* 0x20 Write Complete*/  {NULL, dissect_write_and_close_response},
  /* 0x21 */  {NULL, NULL},
  /* 0x22 Set Info2*/  {dissect_set_information2_request, dissect_empty},
  /* 0x23 Query Info2*/  {dissect_fid, dissect_query_information2_response},
  /* 0x24 Locking And X*/  {dissect_locking_andx_request, dissect_locking_andx_response},
  /* 0x25 */  {NULL, NULL},
  /* 0x26 */  {NULL, NULL},
  /* 0x27 */  {NULL, NULL},
  /* 0x28 */  {NULL, NULL},
  /* 0x29 */  {NULL, NULL},
  /* 0x2a Move File*/  {dissect_move_request, dissect_move_response},
  /* 0x2b Echo*/  {dissect_echo_request, dissect_echo_response},
  /* 0x2c Write And Close*/  {dissect_write_and_close_request, dissect_write_and_close_response},
  /* 0x2d Open And X*/  {dissect_open_andx_request, dissect_open_andx_response},
  /* 0x2e Read And X*/  {dissect_read_andx_request, dissect_read_andx_response},
  /* 0x2f */  {NULL, NULL},

  /* 0x30 */  {NULL, NULL},
  /* 0x31 */  {NULL, NULL},
  /* 0x32 */  {NULL, NULL},
  /* 0x33 */  {NULL, NULL},
  /* 0x34 Find Close2*/  {dissect_sid, dissect_empty},
  /* 0x35 */  {NULL, NULL},
  /* 0x36 */  {NULL, NULL},
  /* 0x37 */  {NULL, NULL},
  /* 0x38 */  {NULL, NULL},
  /* 0x39 */  {NULL, NULL},
  /* 0x3a */  {NULL, NULL},
  /* 0x3b */  {NULL, NULL},
  /* 0x3c */  {NULL, NULL},
  /* 0x3d */  {NULL, NULL},
  /* 0x3e */  {NULL, NULL},
  /* 0x3f */  {NULL, NULL},

  /* 0x40 */  {NULL, NULL},
  /* 0x41 */  {NULL, NULL},
  /* 0x42 */  {NULL, NULL},
  /* 0x43 */  {NULL, NULL},
  /* 0x44 */  {NULL, NULL},
  /* 0x45 */  {NULL, NULL},
  /* 0x46 */  {NULL, NULL},
  /* 0x47 */  {NULL, NULL},
  /* 0x48 */  {NULL, NULL},
  /* 0x49 */  {NULL, NULL},
  /* 0x4a */  {NULL, NULL},
  /* 0x4b */  {NULL, NULL},
  /* 0x4c */  {NULL, NULL},
  /* 0x4d */  {NULL, NULL},
  /* 0x4e */  {NULL, NULL},
  /* 0x4f */  {NULL, NULL},

  /* 0x50 */  {NULL, NULL},
  /* 0x51 */  {NULL, NULL},
  /* 0x52 */  {NULL, NULL},
  /* 0x53 */  {NULL, NULL},
  /* 0x54 */  {NULL, NULL},
  /* 0x55 */  {NULL, NULL},
  /* 0x56 */  {NULL, NULL},
  /* 0x57 */  {NULL, NULL},
  /* 0x58 */  {NULL, NULL},
  /* 0x59 */  {NULL, NULL},
  /* 0x5a */  {NULL, NULL},
  /* 0x5b */  {NULL, NULL},
  /* 0x5c */  {NULL, NULL},
  /* 0x5d */  {NULL, NULL},
  /* 0x5e */  {NULL, NULL},
  /* 0x5f */  {NULL, NULL},

  /* 0x60 */  {NULL, NULL},
  /* 0x61 */  {NULL, NULL},
  /* 0x62 */  {NULL, NULL},
  /* 0x63 */  {NULL, NULL},
  /* 0x64 */  {NULL, NULL},
  /* 0x65 */  {NULL, NULL},
  /* 0x66 */  {NULL, NULL},
  /* 0x67 */  {NULL, NULL},
  /* 0x68 */  {NULL, NULL},
  /* 0x69 */  {NULL, NULL},
  /* 0x6a */  {NULL, NULL},
  /* 0x6b */  {NULL, NULL},
  /* 0x6c */  {NULL, NULL},
  /* 0x6d */  {NULL, NULL},
  /* 0x6e */  {NULL, NULL},
  /* 0x6f */  {NULL, NULL},

  /* 0x70 Tree Connect*/  {dissect_tree_connect_request, dissect_tree_connect_response},
  /* 0x71 Tree Disconnect*/  {dissect_empty, dissect_empty},
  /* 0x72 Negotiate Protocol*/	{dissect_negprot_request, dissect_negprot_response},
  /* 0x73 Session Setup And X*/  {dissect_session_setup_andx_request, dissect_session_setup_andx_response},
  /* 0x74 Logoff And X*/  {dissect_empty_andx, dissect_empty_andx},
  /* 0x75 Tree Connect And X*/  {dissect_tree_connect_andx_request, dissect_tree_connect_andx_response},
  /* 0x76 */  {NULL, NULL},
  /* 0x77 */  {NULL, NULL},
  /* 0x78 */  {NULL, NULL},
  /* 0x79 */  {NULL, NULL},
  /* 0x7a */  {NULL, NULL},
  /* 0x7b */  {NULL, NULL},
  /* 0x7c */  {NULL, NULL},
  /* 0x7d */  {NULL, NULL},
  /* 0x7e */  {NULL, NULL},
  /* 0x7f */  {NULL, NULL},

  /* 0x80 Query Info Disk*/  {dissect_empty, dissect_query_information_disk_response},
  /* 0x81 Search Dir*/  {dissect_search_dir_request, dissect_search_dir_response},
  /* 0x82 */  {NULL, NULL},
  /* 0x83 */  {NULL, NULL},
  /* 0x84 */  {NULL, NULL},
  /* 0x85 */  {NULL, NULL},
  /* 0x86 */  {NULL, NULL},
  /* 0x87 */  {NULL, NULL},
  /* 0x88 */  {NULL, NULL},
  /* 0x89 */  {NULL, NULL},
  /* 0x8a */  {NULL, NULL},
  /* 0x8b */  {NULL, NULL},
  /* 0x8c */  {NULL, NULL},
  /* 0x8d */  {NULL, NULL},
  /* 0x8e */  {NULL, NULL},
  /* 0x8f */  {NULL, NULL},

  /* 0x90 */  {NULL, NULL},
  /* 0x91 */  {NULL, NULL},
  /* 0x92 */  {NULL, NULL},
  /* 0x93 */  {NULL, NULL},
  /* 0x94 */  {NULL, NULL},
  /* 0x95 */  {NULL, NULL},
  /* 0x96 */  {NULL, NULL},
  /* 0x97 */  {NULL, NULL},
  /* 0x98 */  {NULL, NULL},
  /* 0x99 */  {NULL, NULL},
  /* 0x9a */  {NULL, NULL},
  /* 0x9b */  {NULL, NULL},
  /* 0x9c */  {NULL, NULL},
  /* 0x9d */  {NULL, NULL},
  /* 0x9e */  {NULL, NULL},
  /* 0x9f */  {NULL, NULL},
  /* 0xa0 NT Transaction*/  	{dissect_nt_transaction_request, dissect_nt_transaction_response},
  /* 0xa1 NT Trans secondary*/	{dissect_nt_transaction_request, dissect_nt_transaction_response},
  /* 0xa2 */  {NULL, NULL},
  /* 0xa3 */  {NULL, NULL},
  /* 0xa4 */  {NULL, NULL},
  /* 0xa5 */  {NULL, NULL},
  /* 0xa6 */  {NULL, NULL},
  /* 0xa7 */  {NULL, NULL},
  /* 0xa8 */  {NULL, NULL},
  /* 0xa9 */  {NULL, NULL},
  /* 0xaa */  {NULL, NULL},
  /* 0xab */  {NULL, NULL},
  /* 0xac */  {NULL, NULL},
  /* 0xad */  {NULL, NULL},
  /* 0xae */  {NULL, NULL},
  /* 0xaf */  {NULL, NULL},

  /* 0xb0 */  {NULL, NULL},
  /* 0xb1 */  {NULL, NULL},
  /* 0xb2 */  {NULL, NULL},
  /* 0xb3 */  {NULL, NULL},
  /* 0xb4 */  {NULL, NULL},
  /* 0xb5 */  {NULL, NULL},
  /* 0xb6 */  {NULL, NULL},
  /* 0xb7 */  {NULL, NULL},
  /* 0xb8 */  {NULL, NULL},
  /* 0xb9 */  {NULL, NULL},
  /* 0xba */  {NULL, NULL},
  /* 0xbb */  {NULL, NULL},
  /* 0xbc */  {NULL, NULL},
  /* 0xbd */  {NULL, NULL},
  /* 0xbe */  {NULL, NULL},
  /* 0xbf */  {NULL, NULL},

  /* 0xc0 */  {NULL, NULL},
  /* 0xc1 */  {NULL, NULL},
  /* 0xc2 Close Print File*/  {dissect_fid, dissect_empty},
  /* 0xc3 */  {NULL, NULL},
  /* 0xc4 */  {NULL, NULL},
  /* 0xc5 */  {NULL, NULL},
  /* 0xc6 */  {NULL, NULL},
  /* 0xc7 */  {NULL, NULL},
  /* 0xc8 */  {NULL, NULL},
  /* 0xc9 */  {NULL, NULL},
  /* 0xca */  {NULL, NULL},
  /* 0xcb */  {NULL, NULL},
  /* 0xcc */  {NULL, NULL},
  /* 0xcd */  {NULL, NULL},
  /* 0xce */  {NULL, NULL},
  /* 0xcf */  {NULL, NULL},

  /* 0xd0 */  {NULL, NULL},
  /* 0xd1 */  {NULL, NULL},
  /* 0xd2 */  {NULL, NULL},
  /* 0xd3 */  {NULL, NULL},
  /* 0xd4 */  {NULL, NULL},
  /* 0xd5 */  {NULL, NULL},
  /* 0xd6 */  {NULL, NULL},
  /* 0xd7 */  {NULL, NULL},
  /* 0xd8 */  {NULL, NULL},
  /* 0xd9 */  {NULL, NULL},
  /* 0xda */  {NULL, NULL},
  /* 0xdb */  {NULL, NULL},
  /* 0xdc */  {NULL, NULL},
  /* 0xdd */  {NULL, NULL},
  /* 0xde */  {NULL, NULL},
  /* 0xdf */  {NULL, NULL},

  /* 0xe0 */  {NULL, NULL},
  /* 0xe1 */  {NULL, NULL},
  /* 0xe2 */  {NULL, NULL},
  /* 0xe3 */  {NULL, NULL},
  /* 0xe4 */  {NULL, NULL},
  /* 0xe5 */  {NULL, NULL},
  /* 0xe6 */  {NULL, NULL},
  /* 0xe7 */  {NULL, NULL},
  /* 0xe8 */  {NULL, NULL},
  /* 0xe9 */  {NULL, NULL},
  /* 0xea */  {NULL, NULL},
  /* 0xeb */  {NULL, NULL},
  /* 0xec */  {NULL, NULL},
  /* 0xed */  {NULL, NULL},
  /* 0xee */  {NULL, NULL},
  /* 0xef */  {NULL, NULL},

  /* 0xf0 */  {NULL, NULL},
  /* 0xf1 */  {NULL, NULL},
  /* 0xf2 */  {NULL, NULL},
  /* 0xf3 */  {NULL, NULL},
  /* 0xf4 */  {NULL, NULL},
  /* 0xf5 */  {NULL, NULL},
  /* 0xf6 */  {NULL, NULL},
  /* 0xf7 */  {NULL, NULL},
  /* 0xf8 */  {NULL, NULL},
  /* 0xf9 */  {NULL, NULL},
  /* 0xfa */  {NULL, NULL},
  /* 0xfb */  {NULL, NULL},
  /* 0xfc */  {NULL, NULL},
  /* 0xfd */  {NULL, NULL},
  /* 0xfe */  {NULL, NULL},
  /* 0xff */  {NULL, NULL},
};

static int
dissect_smb_command(tvbuff_t *tvb, packet_info *pinfo, proto_tree *top_tree, int offset, proto_tree *smb_tree, guint8 cmd)
{
	int old_offset = offset;
	smb_info_t *si;
 
	si = pinfo->private_data;
	if(cmd!=0xff){
		proto_item *cmd_item;
		proto_tree *cmd_tree;
		int (*dissector)(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree);

		if (check_col(pinfo->fd, COL_INFO)) {
			col_add_fstr(pinfo->fd, COL_INFO, "%s %s",
				decode_smb_name(cmd),
				(si->request)? "Request" : "Response");
		}

		cmd_item = proto_tree_add_text(smb_tree, tvb, offset,
			0, "%s %s (0x%02x)",
			decode_smb_name(cmd), 
			(si->request)?"Request":"Response",
			cmd);

		cmd_tree = proto_item_add_subtree(cmd_item, ett_smb_command);

		dissector = (si->request)?
			smb_dissector[cmd].request:smb_dissector[cmd].response;

		if(dissector){
			offset = (*dissector)(tvb, pinfo, cmd_tree, offset, smb_tree);
		}
		proto_item_set_len(cmd_item, offset-old_offset);
	}
	return offset;
}


/* NOTE: this value_string array will also be used to access data directly by
 * index instead of val_to_str() since 
 * 1, the array will always span every value from 0x00 to 0xff and
 * 2, smb_cmd_vals[i].strptr  is much cheaper than  val_to_str(i, smb_cmd_vals,)
 * This means that this value_string array MUST always
 * 1, contain all entries 0x00 to 0xff
 * 2, all entries must be in order.
 */
static const value_string smb_cmd_vals[] = {
  { 0x00, "Create Directory" },
  { 0x01, "Delete Directory" },
  { 0x02, "Open" },
  { 0x03, "Create" },
  { 0x04, "Close" },
  { 0x05, "Flush" },
  { 0x06, "Delete" },
  { 0x07, "Rename" },
  { 0x08, "Query Information" },
  { 0x09, "Set Information" },
  { 0x0A, "Read" },
  { 0x0B, "Write" },
  { 0x0C, "Lock Byte Range" },
  { 0x0D, "Unlock Byte Range" },
  { 0x0E, "Create Temp" },
  { 0x0F, "Create New" },
  { 0x10, "Check Directory" },
  { 0x11, "Process Exit" },
  { 0x12, "Seek" },
  { 0x13, "Lock And Read" },
  { 0x14, "Write And Unlock" },
  { 0x15, "unknown-0x15" },
  { 0x16, "unknown-0x16" },
  { 0x17, "unknown-0x17" },
  { 0x18, "unknown-0x18" },
  { 0x19, "unknown-0x19" },
  { 0x1A, "Read Raw" },
  { 0x1B, "Read MPX" },
  { 0x1C, "Read MPX Secondary" },
  { 0x1D, "Write Raw" },
  { 0x1E, "Write MPX" },
  { 0x1F, "SMBwriteBs" },
  { 0x20, "Write Complete" },
  { 0x21, "unknown-0x21" },
  { 0x22, "Set Information2" },
  { 0x23, "Query Information2" },
  { 0x24, "Locking AndX" },
  { 0x25, "Transaction" },
  { 0x26, "Transaction Secondary" },
  { 0x27, "IOCTL" },
  { 0x28, "IOCTL Secondary" },
  { 0x29, "Copy" },
  { 0x2A, "Move" },
  { 0x2B, "Echo" },
  { 0x2C, "Write And Close" },
  { 0x2D, "Open AndX" },
  { 0x2E, "Read AndX" },
  { 0x2F, "Write AndX" },
  { 0x30, "unknown-0x30" },
  { 0x31, "Close And Tree Discover" },
  { 0x32, "Transaction2" },
  { 0x33, "Transaction2 Secondary" },
  { 0x34, "Find Close2" },
  { 0x35, "Find Notify Close" },
  { 0x36, "unknown-0x36" },
  { 0x37, "unknown-0x37" },
  { 0x38, "unknown-0x38" },
  { 0x39, "unknown-0x39" },
  { 0x3A, "unknown-0x3A" },
  { 0x3B, "unknown-0x3B" },
  { 0x3C, "unknown-0x3C" },
  { 0x3D, "unknown-0x3D" },
  { 0x3E, "unknown-0x3E" },
  { 0x3F, "unknown-0x3F" },
  { 0x40, "unknown-0x40" },
  { 0x41, "unknown-0x41" },
  { 0x42, "unknown-0x42" },
  { 0x43, "unknown-0x43" },
  { 0x44, "unknown-0x44" },
  { 0x45, "unknown-0x45" },
  { 0x46, "unknown-0x46" },
  { 0x47, "unknown-0x47" },
  { 0x48, "unknown-0x48" },
  { 0x49, "unknown-0x49" },
  { 0x4A, "unknown-0x4A" },
  { 0x4B, "unknown-0x4B" },
  { 0x4C, "unknown-0x4C" },
  { 0x4D, "unknown-0x4D" },
  { 0x4E, "unknown-0x4E" },
  { 0x4F, "unknown-0x4F" },
  { 0x50, "unknown-0x50" },
  { 0x51, "unknown-0x51" },
  { 0x52, "unknown-0x52" },
  { 0x53, "unknown-0x53" },
  { 0x54, "unknown-0x54" },
  { 0x55, "unknown-0x55" },
  { 0x56, "unknown-0x56" },
  { 0x57, "unknown-0x57" },
  { 0x58, "unknown-0x58" },
  { 0x59, "unknown-0x59" },
  { 0x5A, "unknown-0x5A" },
  { 0x5B, "unknown-0x5B" },
  { 0x5C, "unknown-0x5C" },
  { 0x5D, "unknown-0x5D" },
  { 0x5E, "unknown-0x5E" },
  { 0x5F, "unknown-0x5F" },
  { 0x60, "unknown-0x60" },
  { 0x61, "unknown-0x61" },
  { 0x62, "unknown-0x62" },
  { 0x63, "unknown-0x63" },
  { 0x64, "unknown-0x64" },
  { 0x65, "unknown-0x65" },
  { 0x66, "unknown-0x66" },
  { 0x67, "unknown-0x67" },
  { 0x68, "unknown-0x68" },
  { 0x69, "unknown-0x69" },
  { 0x6A, "unknown-0x6A" },
  { 0x6B, "unknown-0x6B" },
  { 0x6C, "unknown-0x6C" },
  { 0x6D, "unknown-0x6D" },
  { 0x6E, "unknown-0x6E" },
  { 0x6F, "unknown-0x6F" },
  { 0x70, "Tree Connect" },
  { 0x71, "Tree Disconnect" },
  { 0x72, "Negotiate Protocol" },
  { 0x73, "Session Setup AndX" },
  { 0x74, "Logoff AndX" },
  { 0x75, "Tree Connect AndX" },
  { 0x76, "unknown-0x76" },
  { 0x77, "unknown-0x77" },
  { 0x78, "unknown-0x78" },
  { 0x79, "unknown-0x79" },
  { 0x7A, "unknown-0x7A" },
  { 0x7B, "unknown-0x7B" },
  { 0x7C, "unknown-0x7C" },
  { 0x7D, "unknown-0x7D" },
  { 0x7E, "unknown-0x7E" },
  { 0x7F, "unknown-0x7F" },
  { 0x80, "Query Information Disk" },
  { 0x81, "Search" },
  { 0x82, "Find" },
  { 0x83, "Find Unique" },
  { 0x84, "SMBfclose" },
  { 0x85, "unknown-0x85" },
  { 0x86, "unknown-0x86" },
  { 0x87, "unknown-0x87" },
  { 0x88, "unknown-0x88" },
  { 0x89, "unknown-0x89" },
  { 0x8A, "unknown-0x8A" },
  { 0x8B, "unknown-0x8B" },
  { 0x8C, "unknown-0x8C" },
  { 0x8D, "unknown-0x8D" },
  { 0x8E, "unknown-0x8E" },
  { 0x8F, "unknown-0x8F" },
  { 0x90, "unknown-0x90" },
  { 0x91, "unknown-0x91" },
  { 0x92, "unknown-0x92" },
  { 0x93, "unknown-0x93" },
  { 0x94, "unknown-0x94" },
  { 0x95, "unknown-0x95" },
  { 0x96, "unknown-0x96" },
  { 0x97, "unknown-0x97" },
  { 0x98, "unknown-0x98" },
  { 0x99, "unknown-0x99" },
  { 0x9A, "unknown-0x9A" },
  { 0x9B, "unknown-0x9B" },
  { 0x9C, "unknown-0x9C" },
  { 0x9D, "unknown-0x9D" },
  { 0x9E, "unknown-0x9E" },
  { 0x9F, "unknown-0x9F" },
  { 0xA0, "NT Transact" },
  { 0xA1, "NT Transact Secondary" },
  { 0xA2, "NT Create AndX" },
  { 0xA3, "unknown-0xA3" },
  { 0xA4, "NT Cancel" },
  { 0xA5, "unknown-0xA5" },
  { 0xA6, "unknown-0xA6" },
  { 0xA7, "unknown-0xA7" },
  { 0xA8, "unknown-0xA8" },
  { 0xA9, "unknown-0xA9" },
  { 0xAA, "unknown-0xAA" },
  { 0xAB, "unknown-0xAB" },
  { 0xAC, "unknown-0xAC" },
  { 0xAD, "unknown-0xAD" },
  { 0xAE, "unknown-0xAE" },
  { 0xAF, "unknown-0xAF" },
  { 0xB0, "unknown-0xB0" },
  { 0xB1, "unknown-0xB1" },
  { 0xB2, "unknown-0xB2" },
  { 0xB3, "unknown-0xB3" },
  { 0xB4, "unknown-0xB4" },
  { 0xB5, "unknown-0xB5" },
  { 0xB6, "unknown-0xB6" },
  { 0xB7, "unknown-0xB7" },
  { 0xB8, "unknown-0xB8" },
  { 0xB9, "unknown-0xB9" },
  { 0xBA, "unknown-0xBA" },
  { 0xBB, "unknown-0xBB" },
  { 0xBC, "unknown-0xBC" },
  { 0xBD, "unknown-0xBD" },
  { 0xBE, "unknown-0xBE" },
  { 0xBF, "unknown-0xBF" },
  { 0xC0, "Open Print File" },
  { 0xC1, "Write Print File" },
  { 0xC2, "Close Print File" },
  { 0xC3, "Get Print Queue" },
  { 0xC4, "unknown-0xC4" },
  { 0xC5, "unknown-0xC5" },
  { 0xC6, "unknown-0xC6" },
  { 0xC7, "unknown-0xC7" },
  { 0xC8, "unknown-0xC8" },
  { 0xC9, "unknown-0xC9" },
  { 0xCA, "unknown-0xCA" },
  { 0xCB, "unknown-0xCB" },
  { 0xCC, "unknown-0xCC" },
  { 0xCD, "unknown-0xCD" },
  { 0xCE, "unknown-0xCE" },
  { 0xCF, "unknown-0xCF" },
  { 0xD0, "SMBsends" },
  { 0xD1, "SMBsendb" },
  { 0xD2, "SMBfwdname" },
  { 0xD3, "SMBcancelf" },
  { 0xD4, "SMBgetmac" },
  { 0xD5, "SMBsendstrt" },
  { 0xD6, "SMBsendend" },
  { 0xD7, "SMBsendtxt" },
  { 0xD8, "SMBreadbulk" },
  { 0xD9, "SMBwritebulk" },
  { 0xDA, "SMBwritebulkdata" },
  { 0xDB, "unknown-0xDB" },
  { 0xDC, "unknown-0xDC" },
  { 0xDD, "unknown-0xDD" },
  { 0xDE, "unknown-0xDE" },
  { 0xDF, "unknown-0xDF" },
  { 0xE0, "unknown-0xE0" },
  { 0xE1, "unknown-0xE1" },
  { 0xE2, "unknown-0xE2" },
  { 0xE3, "unknown-0xE3" },
  { 0xE4, "unknown-0xE4" },
  { 0xE5, "unknown-0xE5" },
  { 0xE6, "unknown-0xE6" },
  { 0xE7, "unknown-0xE7" },
  { 0xE8, "unknown-0xE8" },
  { 0xE9, "unknown-0xE9" },
  { 0xEA, "unknown-0xEA" },
  { 0xEB, "unknown-0xEB" },
  { 0xEC, "unknown-0xEC" },
  { 0xED, "unknown-0xED" },
  { 0xEE, "unknown-0xEE" },
  { 0xEF, "unknown-0xEF" },
  { 0xF0, "unknown-0xF0" },
  { 0xF1, "unknown-0xF1" },
  { 0xF2, "unknown-0xF2" },
  { 0xF3, "unknown-0xF3" },
  { 0xF4, "unknown-0xF4" },
  { 0xF5, "unknown-0xF5" },
  { 0xF6, "unknown-0xF6" },
  { 0xF7, "unknown-0xF7" },
  { 0xF8, "unknown-0xF8" },
  { 0xF9, "unknown-0xF9" },
  { 0xFA, "unknown-0xFA" },
  { 0xFB, "unknown-0xFB" },
  { 0xFC, "unknown-0xFC" },
  { 0xFD, "unknown-0xFD" },
  { 0xFE, "SMBinvalid" },
  { 0xFF, "unknown-0xFF" },
  { 0x00, NULL },
};

static char *decode_smb_name(unsigned char cmd)
{
  return(smb_cmd_vals[cmd].strptr);
}
 


/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 * Everything TVBUFFIFIED above this line
 * XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */


/*
 * Struct passed to each SMB decode routine of info it may need
 */


int smb_packet_init_count = 200;

/*
 * This is a hash table matching transaction requests and replies.
 *
 * Unfortunately, the MID is not a transaction ID in, say, the ONC RPC
 * sense; instead, it's a "multiplex ID" used when there's more than one
 * request *currently* in flight, to distinguish replies.
 *
 * This means that the MID and PID don't uniquely identify a request in
 * a conversation.
 *
 * Therefore, we have to use some other value to distinguish between
 * requests with the same MID and PID.
 *
 * On the first pass through the capture, when we first see a request,
 * we hash it by conversation, MID, and PID.
 *
 * When we first see a reply to it, we add it to a new hash table,
 * hashing it by conversation, MID, PID, and frame number of the reply.
 *
 * This works as long as
 *
 *	1) a client doesn't screw up and have multiple requests outstanding
 *	   with the same MID and PID
 *
 * and
 *
 *	2) we don't have, within the same frame, replies to multiple
 *	   requests with the same MID and PID.
 *
 * 2) should happen only if the server screws up and puts the wrong MID or
 * PID into a reply (in which case not only can we not handle this, the
 * client can't handle it either) or if the client has screwed up as per
 * 1) and the server's dutifully replied to both of the requests with the
 * same MID and PID (in which case, again, neither we nor the client can
 * handle this).
 *
 * We don't have to correctly dissect screwups; we just have to keep from
 * dumping core on them.
 *
 * XXX - in addition, we need to keep a hash table of replies, so that we
 * can associate continuations with the reply to which they're a continuation.
 */
struct smb_request_key {
  guint32 conversation;
  guint16 mid;
  guint16 pid;
  guint32 frame_num;
};

static GHashTable *smb_request_hash = NULL;
static GMemChunk *smb_request_keys = NULL;
static GMemChunk *smb_request_vals = NULL;

/*
 * This is a hash table matching continued transation replies and their
 * continuations.
 *
 * It works similarly to the request/reply hash table.
 */
static GHashTable *smb_continuation_hash = NULL;

static GMemChunk *smb_continuation_vals = NULL;

/* Hash Functions */
static gint
smb_equal(gconstpointer v, gconstpointer w)
{
  struct smb_request_key *v1 = (struct smb_request_key *)v;
  struct smb_request_key *v2 = (struct smb_request_key *)w;

#if defined(DEBUG_SMB_HASH)
  printf("Comparing %08X:%u:%u:%u\n      and %08X:%u:%u:%u\n",
	 v1 -> conversation, v1 -> mid, v1 -> pid, v1 -> frame_num,
	 v2 -> conversation, v2 -> mid, v2 -> pid, v2 -> frame_num);
#endif

  if (v1 -> conversation == v2 -> conversation &&
      v1 -> mid          == v2 -> mid &&
      v1 -> pid          == v2 -> pid &&
      v1 -> frame_num    == v2 -> frame_num) {

    return 1;

  }

  return 0;
}

static guint
smb_hash (gconstpointer v)
{
  struct smb_request_key *key = (struct smb_request_key *)v;
  guint val;

  val = (key -> conversation) + (key -> mid) + (key -> pid) +
	(key -> frame_num);

#if defined(DEBUG_SMB_HASH)
  printf("SMB Hash calculated as %u\n", val);
#endif

  return val;

}

/*
 * Free up any state information we've saved, and re-initialize the
 * tables of state information.
 */

/*
 * For a hash table entry, free the address data to which the key refers
 * and the fragment data to which the value refers.
 * (The actual key and value structures get freed by "reassemble_init()".)
 */
static gboolean
free_request_val_data(gpointer key, gpointer value, gpointer user_data)
{
  struct smb_request_val *request_val = value;

  if (request_val->last_transact_command != NULL)
    g_free(request_val->last_transact_command);
  if (request_val->last_param_descrip != NULL)
    g_free(request_val->last_param_descrip);
  if (request_val->last_data_descrip != NULL)
    g_free(request_val->last_data_descrip);
  if (request_val->last_aux_data_descrip != NULL)
    g_free(request_val->last_aux_data_descrip);
  return TRUE;
}

static struct smb_request_val *
do_transaction_hashing(conversation_t *conversation, struct smb_info si,
		       frame_data *fd)
{
  struct smb_request_key request_key, *new_request_key;
  struct smb_request_val *request_val = NULL;
  gpointer               new_request_key_ret, request_val_ret;

  if (si.request) {
    /*
     * This is a request.
     *
     * If this is the first time the frame has been seen, check for
     * an entry for the request in the hash table.  If it's not found,
     * insert an entry for it.
     *
     * If it's the first time it's been seen, then we can't have seen
     * the reply yet, so the reply frame number should be 0, for
     * "unknown".
     */
    if (!fd->flags.visited) {
      request_key.conversation = conversation->index;
      request_key.mid          = si.mid;
      request_key.pid          = si.pid;
      request_key.frame_num    = 0;

      request_val = (struct smb_request_val *) g_hash_table_lookup(smb_request_hash, &request_key);

      if (request_val == NULL) {
	/*
	 * Not found.
	 */
	new_request_key = g_mem_chunk_alloc(smb_request_keys);
	new_request_key -> conversation = conversation->index;
	new_request_key -> mid          = si.mid;
	new_request_key -> pid          = si.pid;
	new_request_key -> frame_num    = 0;

	request_val = g_mem_chunk_alloc(smb_request_vals);
	request_val -> frame = fd->num;
	request_val -> last_transact2_command = -1;		/* unknown */
	request_val -> last_transact_command = NULL;
	request_val -> last_param_descrip = NULL;
	request_val -> last_data_descrip = NULL;
	request_val -> last_aux_data_descrip = NULL;

	g_hash_table_insert(smb_request_hash, new_request_key, request_val);
      } else {
	/*
	 * This means that we've seen another request in this conversation
	 * with the same request and reply, and without an intervening
	 * reply to that first request, and thus won't be using this
	 * "request_val" structure for that request (as we'd use it only
	 * for the reply).
	 *
	 * Clean out the structure, and set it to refer to this frame.
	 */
	request_val -> frame = fd->num;
	request_val -> last_transact2_command = -1;		/* unknown */
	if (request_val -> last_transact_command)
	  g_free(request_val -> last_transact_command);
	request_val -> last_transact_command = NULL;
	if (request_val -> last_param_descrip)
	  g_free(request_val -> last_param_descrip);
	request_val -> last_param_descrip = NULL;
	if (request_val -> last_data_descrip)
	  g_free(request_val -> last_data_descrip);
	request_val -> last_data_descrip = NULL;
	if (request_val -> last_aux_data_descrip)
	  g_free(request_val -> last_aux_data_descrip);
	request_val -> last_aux_data_descrip = NULL;
      }
    }
  } else {
    /*
     * This is a reply.
     */
    if (!fd->flags.visited) {
      /*
       * This is the first time the frame has been seen; check for
       * an entry for a matching request, with an unknown reply frame
       * number, in the hash table.
       *
       * If we find it, re-hash it with this frame's number as the
       * reply frame number.
       */
      request_key.conversation = conversation->index;
      request_key.mid          = si.mid;
      request_key.pid          = si.pid;
      request_key.frame_num    = 0;

      /*
       * Look it up - and, if we find it, get pointers to the key and
       * value structures for it.
       */
      if (g_hash_table_lookup_extended(smb_request_hash, &request_key,
				       &new_request_key_ret,
				       &request_val_ret)) {
	new_request_key = new_request_key_ret;
	request_val = request_val_ret;

	/*
	 * We found it.
	 * Remove the old entry.
	 */
	g_hash_table_remove(smb_request_hash, &request_key);

	/*
	 * Now update the key, and put it back into the hash table with
	 * the new key.
	 */
	new_request_key->frame_num = fd->num;
	g_hash_table_insert(smb_request_hash, new_request_key, request_val);
      }
    } else {
      /*
       * This is not the first time the frame has been seen; check for
       * an entry for a matching request, with this frame's frame
       * number as the reply frame number, in the hash table.
       */
      request_key.conversation = conversation->index;
      request_key.mid          = si.mid;
      request_key.pid          = si.pid;
      request_key.frame_num    = fd->num;

      request_val = (struct smb_request_val *) g_hash_table_lookup(smb_request_hash, &request_key);
    }
  }

  return request_val;
}

static struct smb_continuation_val *
do_continuation_hashing(conversation_t *conversation, struct smb_info si,
		       frame_data *fd, guint16 TotalDataCount,
		       guint16 DataCount, const char **TransactName)
{
  struct smb_request_key request_key, *new_request_key;
  struct smb_continuation_val *continuation_val, *new_continuation_val;
  gpointer               new_request_key_ret, continuation_val_ret;

  continuation_val = NULL;
  if (si.ddisp != 0) {
    /*
     * This reply isn't the first in the series; there should be a
     * reply of which it is a continuation.
     */
    if (!fd->flags.visited) {
      /*
       * This is the first time the frame has been seen; check for
       * an entry for a matching continued message, with an unknown
       * continuation frame number, in the hash table.
       *
       * If we find it, re-hash it with this frame's number as the
       * continuation frame number.
       */
      request_key.conversation = conversation->index;
      request_key.mid          = si.mid;
      request_key.pid          = si.pid;
      request_key.frame_num    = 0;

      /*
       * Look it up - and, if we find it, get pointers to the key and
       * value structures for it.
       */
      if (g_hash_table_lookup_extended(smb_continuation_hash, &request_key,
				       &new_request_key_ret,
				       &continuation_val_ret)) {
	new_request_key = new_request_key_ret;
	continuation_val = continuation_val_ret;

	/*
	 * We found it.
	 * Remove the old entry.
	 */
	g_hash_table_remove(smb_continuation_hash, &request_key);

	/*
	 * Now update the key, and put it back into the hash table with
	 * the new key.
	 */
	new_request_key->frame_num = fd->num;
	g_hash_table_insert(smb_continuation_hash, new_request_key,
			    continuation_val);
      }
    } else {
      /*
       * This is not the first time the frame has been seen; check for
       * an entry for a matching request, with this frame's frame
       * number as the continuation frame number, in the hash table.
       */
      request_key.conversation = conversation->index;
      request_key.mid          = si.mid;
      request_key.pid          = si.pid;
      request_key.frame_num    = fd->num;

      continuation_val = (struct smb_continuation_val *)
		g_hash_table_lookup(smb_continuation_hash, &request_key);
    }
  }

  /*
   * If we found the entry for the message of which this is a continuation,
   * and our caller cares, get the transaction name for that message, as
   * it's the transaction name for this message as well.
   */
  if (continuation_val != NULL && TransactName != NULL)
    *TransactName = continuation_val -> transact_name;

  if (TotalDataCount > DataCount + si.ddisp) {
    /*
     * This reply isn't the last in the series; there should be a
     * continuation for it later in the capture.
     *
     * If this is the first time the frame has been seen, check for
     * an entry for the reply in the hash table.  If it's not found,
     * insert an entry for it.
     *
     * If it's the first time it's been seen, then we can't have seen
     * the continuation yet, so the continuation frame number should
     * be 0, for "unknown".
     */
    if (!fd->flags.visited) {
      request_key.conversation = conversation->index;
      request_key.mid          = si.mid;
      request_key.pid          = si.pid;
      request_key.frame_num    = 0;

      new_continuation_val = (struct smb_continuation_val *)
		g_hash_table_lookup(smb_continuation_hash, &request_key);

      if (new_continuation_val == NULL) {
	/*
	 * Not found.
	 */
	new_request_key = g_mem_chunk_alloc(smb_request_keys);
	new_request_key -> conversation = conversation->index;
	new_request_key -> mid          = si.mid;
	new_request_key -> pid          = si.pid;
	new_request_key -> frame_num    = 0;

	new_continuation_val = g_mem_chunk_alloc(smb_continuation_vals);
	new_continuation_val -> frame = fd->num;
	if (TransactName != NULL)
	  new_continuation_val -> transact_name = *TransactName;
	else
	  new_continuation_val -> transact_name = NULL;

	g_hash_table_insert(smb_continuation_hash, new_request_key,
			    new_continuation_val);
      } else {
	/*
	 * This presumably means we never saw the continuation of
	 * the message we found, and this is a reply to a different
	 * request; as we never saw the continuation of that message,
	 * we won't be using this "request_val" structure for that
	 * message (as we'd use it only for the continuation).
	 *
	 * Clean out the structure, and set it to refer to this frame.
	 */
	new_continuation_val -> frame = fd->num;
      }
    }
  }

  return continuation_val;
}

static void
smb_init_protocol(void)
{
#if defined(DEBUG_SMB_HASH)
  printf("Initializing SMB hashtable area\n");
#endif

  if (smb_request_hash) {
    /*
     * Remove all entries from the hash table and free all strings
     * attached to the keys and values.  (The keys and values
     * themselves are freed with "g_mem_chunk_destroy()" calls
     * below.)
     */
    g_hash_table_foreach_remove(smb_request_hash, free_request_val_data, NULL);
    g_hash_table_destroy(smb_request_hash);
  }
  if (smb_continuation_hash)
    g_hash_table_destroy(smb_continuation_hash);
  if (smb_request_keys)
    g_mem_chunk_destroy(smb_request_keys);
  if (smb_request_vals)
    g_mem_chunk_destroy(smb_request_vals);
  if (smb_continuation_vals)
    g_mem_chunk_destroy(smb_continuation_vals);

  smb_request_hash = g_hash_table_new(smb_hash, smb_equal);
  smb_continuation_hash = g_hash_table_new(smb_hash, smb_equal);
  smb_request_keys = g_mem_chunk_new("smb_request_keys",
				     sizeof(struct smb_request_key),
				     smb_packet_init_count * sizeof(struct smb_request_key), G_ALLOC_AND_FREE);
  smb_request_vals = g_mem_chunk_new("smb_request_vals",
				     sizeof(struct smb_request_val),
				     smb_packet_init_count * sizeof(struct smb_request_val), G_ALLOC_AND_FREE);
  smb_continuation_vals = g_mem_chunk_new("smb_continuation_vals",
				     sizeof(struct smb_continuation_val),
				     smb_packet_init_count * sizeof(struct smb_continuation_val), G_ALLOC_AND_FREE);
}

static void (*dissect[256])(const u_char *, int, frame_data *, proto_tree *, proto_tree *, struct smb_info si, int, int);

void 
dissect_unknown_smb(const u_char *pd, int offset, frame_data *fd, proto_tree *parent, proto_tree *tree, struct smb_info si, int max_data, int SMB_offset)
{

  if (tree) {

    proto_tree_add_text(tree, NullTVB, offset, END_OF_FRAME, "Data (%u bytes)", 
			END_OF_FRAME); 

  }

}

/* Max string length for displaying Unicode strings.  */
#define	MAX_UNICODE_STR_LEN	256

/* Turn a little-endian Unicode '\0'-terminated string into a string we
   can display.
   XXX - for now, we just handle the ISO 8859-1 characters. */
static gchar *
unicode_to_str(const guint8 *us, int *us_lenp) {
  static gchar  str[3][MAX_UNICODE_STR_LEN+3+1];
  static gchar *cur;
  gchar        *p;
  int           len;
  int           us_len;
  int           overflow = 0;

  NullTVB; /* remove this function when we are fully tvbuffified */
  if (cur == &str[0][0]) {
    cur = &str[1][0];
  } else if (cur == &str[1][0]) {  
    cur = &str[2][0];
  } else {  
    cur = &str[0][0];
  }
  p = cur;
  len = MAX_UNICODE_STR_LEN;
  us_len = 0;
  while (*us != 0 || *(us + 1) != 0) {
    if (len > 0) {
      *p++ = *us;
      len--;
    } else
      overflow = 1;
    us += 2;
    us_len += 2;
  }
  if (overflow) {
    /* Note that we're not showing the full string.  */
    *p++ = '.';
    *p++ = '.';
    *p++ = '.';
  }
  *p = '\0';
  *us_lenp = us_len;
  return cur;
}
/* Turn a little-endian Unicode '\0'-terminated string into a string we
   can display.
   XXX - for now, we just handle the ISO 8859-1 characters.
   If exactlen==TRUE then us_lenp contains the exact len of the string in
   bytes. It might not be null terminated !
   bc specifies the number of bytes in the byte parameters; Windows 2000,
   at least, appears, in some cases, to put only 1 byte of 0 at the end
   of a Unicode string if the byte count
*/
static gchar *
unicode_to_str_tvb(tvbuff_t *tvb, int offset, int *us_lenp, gboolean exactlen,
		   guint16 bc)
{
  static gchar  str[3][MAX_UNICODE_STR_LEN+3+1];
  static gchar *cur;
  gchar        *p;
  guint16       uchar;
  int           len;
  int           us_len;
  int           overflow = 0;

  if (cur == &str[0][0]) {
    cur = &str[1][0];
  } else if (cur == &str[1][0]) {  
    cur = &str[2][0];
  } else {  
    cur = &str[0][0];
  }
  p = cur;
  len = MAX_UNICODE_STR_LEN;
  us_len = 0;
  for (;;) {
    if (bc == 0)
      break;
    if (bc == 1) {
      /* XXX - explain this */
      if (!exactlen)
        us_len += 1;	/* this is a one-byte null terminator */
      break;
    }
    uchar = tvb_get_letohs(tvb, offset);
    if (uchar == 0) {
      us_len += 2;	/* this is a two-byte null terminator */
      break;
    }
    if (len > 0) {
      if ((uchar & 0xFF00) == 0)
        *p++ = uchar;	/* ISO 8859-1 */
      else
        *p++ = '?';	/* not 8859-1 */
      len--;
    } else
      overflow = 1;
    offset += 2;
    bc -= 2;
    us_len += 2;
    if(exactlen){
      if(us_len>= *us_lenp){
        break;
      }
    }
  }
  if (overflow) {
    /* Note that we're not showing the full string.  */
    *p++ = '.';
    *p++ = '.';
    *p++ = '.';
  }
  *p = '\0';
  *us_lenp = us_len;
  return cur;
}
 

/* Get a null terminated string, which is Unicode if "is_unicode" is true
   and ASCII (OEM character set) otherwise.
   XXX - for now, we just handle the ISO 8859-1 subset of Unicode. */
static const gchar *
get_unicode_or_ascii_string(const u_char *pd, int *offsetp, int SMB_offset,
    gboolean is_unicode, int *len)
{
  int offset = *offsetp;
  const gchar *string;
  int string_len;

  NullTVB;  /* delete this function when we are fully tvbuffified */
  if (is_unicode) {
    if ((offset - SMB_offset) % 2) {
      /*
       * XXX - this should be an offset relative to the beginning of the SMB,
       * not an offset relative to the beginning of the frame; if the stuff
       * before the SMB has an odd number of bytes, an offset relative to
       * the beginning of the frame will give the wrong answer.
       */
      offset++;   /* Looks like a pad byte there sometimes */
      *offsetp = offset;
    }
    string = unicode_to_str(pd + offset, &string_len);
    string_len += 2;
  } else {
    string = pd + offset;
    string_len = strlen(string) + 1;
  }
  *len = string_len;
  return string;
}

/* nopad == TRUE : Do not add any padding before this string
 * exactlen == TRUE : len contains the exact len of the string in bytes.
 * bc: pointer to variable with amount of data left in the byte parameters
 *   region
 */
static const gchar *
get_unicode_or_ascii_string_tvb(tvbuff_t *tvb, int *offsetp,
    packet_info *pinfo, int *len, gboolean nopad, gboolean exactlen,
    guint16 *bc)
{
  int offset = *offsetp;
  const gchar *string;
  int string_len;
  smb_info_t *si;

  if (*bc == 0) {
    /* Not enough data in buffer */
    return NULL;
  }
  si = pinfo->private_data;
  if (si->unicode) {
    if ((!nopad) && (*offsetp % 2)) {
      /*
       * XXX - this should be an offset relative to the beginning of the SMB,
       * not an offset relative to the beginning of the frame; if the stuff
       * before the SMB has an odd number of bytes, an offset relative to
       * the beginning of the frame will give the wrong answer.
       */
      (*offsetp)++;   /* Looks like a pad byte there sometimes */
      (*bc)--;
      if (*bc == 0) {
        /* Not enough data in buffer */
        return NULL;
      }
    }
    if(exactlen){
      string_len = *len;
      string = unicode_to_str_tvb(tvb, *offsetp, &string_len, exactlen, *bc);
    } else {
      string = unicode_to_str_tvb(tvb, *offsetp, &string_len, exactlen, *bc);
    }
  } else {
    if(exactlen){
      string = tvb_get_ptr(tvb, *offsetp, *len);
      string_len = *len;
    } else {
      string_len = tvb_strsize(tvb, *offsetp);
      string = tvb_get_ptr(tvb, *offsetp, string_len);
    }
  }
  *len = string_len;
  return string;
}


/*
 * Each dissect routine is passed an offset to wct and works from there 
 */





void
dissect_open_print_file_smb(const u_char *pd, int offset, frame_data *fd, proto_tree *parent, proto_tree *tree, struct smb_info si, int max_data, int SMB_offset)

{
  static const value_string Mode_0x03[] = {
	{ 0, "Text mode (DOS expands TABs)"},
	{ 1, "Graphics mode"},
	{ 0, NULL}
  };
  proto_tree    *Mode_tree;
  proto_item    *ti;
  guint8        WordCount;
  guint8        BufferFormat;
  guint16       SetupLength;
  guint16       Mode;
  guint16       FID;
  guint16       ByteCount;
  const char    *IdentifierString;
  int           string_len;

  if (si.request) {
    /* Request(s) dissect code */

    /* Build display for: Word Count (WCT) */

    WordCount = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Word Count (WCT): %u", WordCount);

    }

    offset += 1; /* Skip Word Count (WCT) */

    /* Build display for: Setup Length */

    SetupLength = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Setup Length: %u", SetupLength);

    }

    offset += 2; /* Skip Setup Length */

    /* Build display for: Mode */

    Mode = GSHORT(pd, offset);

    if (tree) {

      ti = proto_tree_add_text(tree, NullTVB, offset, 2, "Mode: 0x%02x", Mode);
      Mode_tree = proto_item_add_subtree(ti, ett_smb_mode);
      proto_tree_add_text(Mode_tree, NullTVB, offset, 2, "%s",
                          decode_enumerated_bitfield(Mode, 0x03, 16, Mode_0x03, "%s"));
    
    }

    offset += 2; /* Skip Mode */

    /* Build display for: Byte Count (BCC) */

    ByteCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Byte Count (BCC): %u", ByteCount);

    }

    offset += 2; /* Skip Byte Count (BCC) */

    /* Build display for: Buffer Format */

    BufferFormat = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Buffer Format: %s (%u)",
			  val_to_str(BufferFormat, buffer_format_vals, "Unknown"),
			  BufferFormat);

    }

    offset += 1; /* Skip Buffer Format */

    /* Build display for: Identifier String */

    IdentifierString = get_unicode_or_ascii_string(pd, &offset, SMB_offset, si.unicode, &string_len);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, string_len, "Identifier String: %s", IdentifierString);

    }

    offset += string_len; /* Skip Identifier String */

  } else {
    /* Response(s) dissect code */

    /* Build display for: Word Count (WCT) */

    WordCount = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Word Count (WCT): %u", WordCount);

    }

    offset += 1; /* Skip Word Count (WCT) */

    /* Build display for: FID */

    FID = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "FID: 0x%04x", FID);

    }

    offset += 2; /* Skip FID */

    /* Build display for: Byte Count (BCC) */

    ByteCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Byte Count (BCC): %u", ByteCount);

    }

    offset += 2; /* Skip Byte Count (BCC) */

  }

}


void
dissect_get_print_queue_smb(const u_char *pd, int offset, frame_data *fd, proto_tree *parent, proto_tree *tree, struct smb_info si, int max_data, int SMB_offset)

{
  guint8        WordCount;
  guint8        BufferFormat;
  guint16       StartIndex;
  guint16       RestartIndex;
  guint16       MaxCount;
  guint16       DataLength;
  guint16       Count;
  guint16       ByteCount;

  if (si.request) {
    /* Request(s) dissect code */

    /* Build display for: Word Count */

    WordCount = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Word Count (WCT): %u", WordCount);

    }

    offset += 1; /* Skip Word Count */

    /* Build display for: Max Count */

    MaxCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Max Count: %u", MaxCount);

    }

    offset += 2; /* Skip Max Count */

    /* Build display for: Start Index */

    StartIndex = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Start Index: %u", StartIndex);

    }

    offset += 2; /* Skip Start Index */

    /* Build display for: Byte Count (BCC) */

    ByteCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Byte Count (BCC): %u", ByteCount);

    }

    offset += 2; /* Skip Byte Count (BCC) */

  } else {
    /* Response(s) dissect code */

    /* Build display for: Word Count (WCT) */

    WordCount = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Word Count (WCT): %u", WordCount);

    }

    offset += 1; /* Skip Word Count (WCT) */

    if (WordCount != 0) {

      /* Build display for: Count */

      Count = GSHORT(pd, offset);

      if (tree) {

	proto_tree_add_text(tree, NullTVB, offset, 2, "Count: %u", Count);

      }

      offset += 2; /* Skip Count */

      /* Build display for: Restart Index */

      RestartIndex = GSHORT(pd, offset);

      if (tree) {

	proto_tree_add_text(tree, NullTVB, offset, 2, "Restart Index: %u", RestartIndex);

      }

      offset += 2; /* Skip Restart Index */

      /* Build display for: Byte Count (BCC) */

    }

    ByteCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Byte Count (BCC): %u", ByteCount);

    }

    offset += 2; /* Skip Byte Count (BCC) */

    /* Build display for: Buffer Format */

    BufferFormat = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Buffer Format: %s (%u)",
			  val_to_str(BufferFormat, buffer_format_vals, "Unknown"),
			  BufferFormat);

    }

    offset += 1; /* Skip Buffer Format */

    /* Build display for: Data Length */

    DataLength = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Data Length: %u", DataLength);

    }

    offset += 2; /* Skip Data Length */

  }

}


void
dissect_write_print_file_smb(const u_char *pd, int offset, frame_data *fd, proto_tree *parent, proto_tree *tree, struct smb_info si, int max_data, int SMB_offset)

{
  guint8        WordCount;
  guint8        BufferFormat;
  guint16       FID;
  guint16       DataLength;
  guint16       ByteCount;

  if (si.request) {
    /* Request(s) dissect code */

    /* Build display for: Word Count (WCT) */

    WordCount = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Word Count (WCT): %u", WordCount);

    }

    offset += 1; /* Skip Word Count (WCT) */

    /* Build display for: FID */

    FID = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "FID: 0x%04x", FID);

    }

    offset += 2; /* Skip FID */

    /* Build display for: Byte Count (BCC) */

    ByteCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Byte Count (BCC): %u", ByteCount);

    }

    offset += 2; /* Skip Byte Count (BCC) */

    /* Build display for: Buffer Format */

    BufferFormat = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Buffer Format: %s (%u)",
			  val_to_str(BufferFormat, buffer_format_vals, "Unknown"),
			  BufferFormat);

    }

    offset += 1; /* Skip Buffer Format */

    /* Build display for: Data Length */

    DataLength = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Data Length: %u", DataLength);

    }

    offset += 2; /* Skip Data Length */

  } else {
    /* Response(s) dissect code */

    /* Build display for: Word Count (WCT) */

    WordCount = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Word Count (WCT): %u", WordCount);

    }

    offset += 1; /* Skip Word Count (WCT) */

    /* Build display for: Byte Count (BCC) */

    ByteCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Byte Count (BCC): %u", ByteCount);

    }

    offset += 2; /* Skip Byte Count (BCC) */

  }

}


static const value_string trans2_cmd_vals[] = {
  { 0x00, "TRANS2_OPEN" },
  { 0x01, "TRANS2_FIND_FIRST2" },
  { 0x02, "TRANS2_FIND_NEXT2" },
  { 0x03, "TRANS2_QUERY_FS_INFORMATION" },
  { 0x05, "TRANS2_QUERY_PATH_INFORMATION" },
  { 0x06, "TRANS2_SET_PATH_INFORMATION" },
  { 0x07, "TRANS2_QUERY_FILE_INFORMATION" },
  { 0x08, "TRANS2_SET_FILE_INFORMATION" },
  { 0x09, "TRANS2_FSCTL" },
  { 0x0A, "TRANS2_IOCTL2" },
  { 0x0B, "TRANS2_FIND_NOTIFY_FIRST" },
  { 0x0C, "TRANS2_FIND_NOTIFY_NEXT" },
  { 0x0D, "TRANS2_CREATE_DIRECTORY" },
  { 0x0E, "TRANS2_SESSION_SETUP" },
  { 0x10, "TRANS2_GET_DFS_REFERRAL" },
  { 0x11, "TRANS2_REPORT_DFS_INCONSISTENCY" },
  { 0,    NULL }
};

void
dissect_transact2_smb(const u_char *pd, int offset, frame_data *fd, proto_tree *parent, proto_tree *tree, struct smb_info si, int max_data, int SMB_offset)

{
  proto_tree    *Flags_tree;
  proto_item    *ti;
  guint8        WordCount;
  guint8        SetupCount;
  guint8        Reserved3;
  guint8        Reserved1;
  guint8        MaxSetupCount;
  guint8        Data;
  guint32       Timeout;
  guint16       TotalParameterCount;
  guint16       TotalDataCount;
  guint16       Setup;
  guint16       Reserved2;
  guint16       ParameterOffset;
  guint16       ParameterDisplacement;
  guint16       ParameterCount;
  guint16       MaxParameterCount;
  guint16       MaxDataCount;
  guint16       Flags;
  guint16       DataOffset;
  guint16       DataDisplacement;
  guint16       DataCount;
  guint16       ByteCount;
  conversation_t *conversation;
  struct smb_request_val *request_val;
  struct smb_continuation_val *continuation_val;

  /*
   * Find out what conversation this packet is part of.
   * XXX - this should really be done by the transport-layer protocol,
   * although for connectionless transports, we may not want to do that
   * unless we know some higher-level protocol will want it - or we
   * may want to do it, so you can say e.g. "show only the packets in
   * this UDP 'connection'".
   *
   * Note that we don't have to worry about the direction this packet
   * was going - the conversation code handles that for us, treating
   * packets from A:X to B:Y as being part of the same conversation as
   * packets from B:Y to A:X.
   */
  conversation = find_conversation(&pi.src, &pi.dst, pi.ptype,
				pi.srcport, pi.destport, 0);
  if (conversation == NULL) {
    /* It's not part of any conversation - create a new one. */
    conversation = conversation_new(&pi.src, &pi.dst, pi.ptype,
				pi.srcport, pi.destport, 0);
  }

  si.conversation = conversation;  /* Save this for later */

  request_val = do_transaction_hashing(conversation, si, fd);

  si.request_val = request_val;  /* Save this for later */

  if (si.request) {
    /* Request(s) dissect code */
  
    /* Build display for: Word Count (WCT) */

    WordCount = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Word Count (WCT): %u", WordCount);

    }

    offset += 1; /* Skip Word Count (WCT) */

    /* Build display for: Total Parameter Count */

    TotalParameterCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Total Parameter Count: %u", TotalParameterCount);

    }

    offset += 2; /* Skip Total Parameter Count */

    /* Build display for: Total Data Count */

    TotalDataCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Total Data Count: %u", TotalDataCount);

    }

    offset += 2; /* Skip Total Data Count */

    /* Build display for: Max Parameter Count */

    MaxParameterCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Max Parameter Count: %u", MaxParameterCount);

    }

    offset += 2; /* Skip Max Parameter Count */

    /* Build display for: Max Data Count */

    MaxDataCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Max Data Count: %u", MaxDataCount);

    }

    offset += 2; /* Skip Max Data Count */

    /* Build display for: Max Setup Count */

    MaxSetupCount = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Max Setup Count: %u", MaxSetupCount);

    }

    offset += 1; /* Skip Max Setup Count */

    /* Build display for: Reserved1 */

    Reserved1 = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Reserved1: %u", Reserved1);

    }

    offset += 1; /* Skip Reserved1 */

    /* Build display for: Flags */

    Flags = GSHORT(pd, offset);

    if (tree) {

      ti = proto_tree_add_text(tree, NullTVB, offset, 2, "Flags: 0x%02x", Flags);
      Flags_tree = proto_item_add_subtree(ti, ett_smb_flags);
      proto_tree_add_text(Flags_tree, NullTVB, offset, 2, "%s",
                          decode_boolean_bitfield(Flags, 0x01, 16, "Also disconnect TID", "Dont disconnect TID"));
      proto_tree_add_text(Flags_tree, NullTVB, offset, 2, "%s",
                          decode_boolean_bitfield(Flags, 0x02, 16, "One way transaction", "Two way transaction"));
    
    }

    offset += 2; /* Skip Flags */

    /* Build display for: Timeout */

    Timeout = GWORD(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 4, "Timeout: %u", Timeout);

    }

    offset += 4; /* Skip Timeout */

    /* Build display for: Reserved2 */

    Reserved2 = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Reserved2: %u", Reserved2);

    }

    offset += 2; /* Skip Reserved2 */

    /* Build display for: Parameter Count */

    if (!BYTES_ARE_IN_FRAME(offset, 2))
      return;

    ParameterCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Parameter Count: %u", ParameterCount);

    }

    offset += 2; /* Skip Parameter Count */

    /* Build display for: Parameter Offset */

    if (!BYTES_ARE_IN_FRAME(offset, 2))
      return;

    ParameterOffset = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Parameter Offset: %u", ParameterOffset);

    }

    offset += 2; /* Skip Parameter Offset */

    /* Build display for: Data Count */

    if (!BYTES_ARE_IN_FRAME(offset, 2))
      return;

    DataCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Data Count: %u", DataCount);

    }

    offset += 2; /* Skip Data Count */

    /* Build display for: Data Offset */

    if (!BYTES_ARE_IN_FRAME(offset, 2))
      return;

    DataOffset = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Data Offset: %u", DataOffset);

    }

    offset += 2; /* Skip Data Offset */

    /* Build display for: Setup Count */

    if (!BYTES_ARE_IN_FRAME(offset, 2))
      return;

    SetupCount = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Setup Count: %u", SetupCount);

    }

    offset += 1; /* Skip Setup Count */

    /* Build display for: Reserved3 */

    Reserved3 = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Reserved3: %u", Reserved3);

    }

    offset += 1; /* Skip Reserved3 */

    /* Build display for: Setup */

    if (SetupCount != 0) {

      int i;

      if (!BYTES_ARE_IN_FRAME(offset, 2))
	return;

      /*
       * First Setup word is transaction code.
       */
      Setup = GSHORT(pd, offset);

      if (!fd->flags.visited) {
	/*
	 * This is the first time this frame has been seen; remember
	 * the transaction code.
	 */
	g_assert(request_val -> last_transact2_command == -1);
        request_val -> last_transact2_command = Setup;  /* Save for later */
      }

      if (check_col(fd, COL_INFO)) {

	col_add_fstr(fd, COL_INFO, "%s Request",
		     val_to_str(Setup, trans2_cmd_vals, "Unknown (0x%02X)"));

      }

      if (tree) {

	proto_tree_add_text(tree, NullTVB, offset, 2, "Setup1: %s",
			  val_to_str(Setup, trans2_cmd_vals, "Unknown (0x%02X)"));

      }

      offset += 2; /* Skip Setup word */

      for (i = 2; i <= SetupCount; i++) {

	if (!BYTES_ARE_IN_FRAME(offset, 2))
	  return;

	Setup = GSHORT(pd, offset);

	if (tree) {

	  proto_tree_add_text(tree, NullTVB, offset, 2, "Setup%i: %u", i, Setup);

	}

	offset += 2; /* Skip Setup word */

      }

    }

    /* Build display for: Byte Count (BCC) */

    if (!BYTES_ARE_IN_FRAME(offset, 2))
      return;

    ByteCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Byte Count (BCC): %u", ByteCount);

    }

    offset += 2; /* Skip Byte Count (BCC) */

    if (offset < (SMB_offset + ParameterOffset)) {

      int pad1Count = SMB_offset + ParameterOffset - offset;

      /* Build display for: Pad1 */

      if (tree) {

	proto_tree_add_text(tree, NullTVB, offset, pad1Count, "Pad1: %s",
			    bytes_to_str(pd + offset, pad1Count));
      }

      offset += pad1Count; /* Skip Pad1 */

    }

    if (ParameterCount > 0) {

      /* Build display for: Parameters */

      if (tree) {

	proto_tree_add_text(tree, NullTVB, SMB_offset + ParameterOffset,
			    ParameterCount, "Parameters: %s",
			    bytes_to_str(pd + SMB_offset + ParameterOffset,
					 ParameterCount));

      }

      offset += ParameterCount; /* Skip Parameters */

    }

    if (DataCount > 0 && offset < (SMB_offset + DataOffset)) {

      int pad2Count = SMB_offset + DataOffset - offset;
	
      /* Build display for: Pad2 */

      if (tree) {

	proto_tree_add_text(tree, NullTVB, offset, pad2Count, "Pad2: %s",
			    bytes_to_str(pd + offset, pad2Count));

      }

      offset += pad2Count; /* Skip Pad2 */

    }

    if (DataCount > 0) {

      /* Build display for: Data */

      Data = GBYTE(pd, offset);

      if (tree) {

	proto_tree_add_text(tree, NullTVB, SMB_offset + DataOffset,
			    DataCount, "Data: %s",
			    bytes_to_str(pd + offset, DataCount));

      }

      offset += DataCount; /* Skip Data */

    }
  } else {
    /* Response(s) dissect code */

    /* Pick up the last transact2 command and put it in the right places */

    if (check_col(fd, COL_INFO)) {

      if (request_val == NULL)
	col_set_str(fd, COL_INFO, "Response to unknown SMBtrans2");
      else if (request_val -> last_transact2_command == -1)
	col_set_str(fd, COL_INFO, "Response to SMBtrans2 of unknown type");
      else
	col_add_fstr(fd, COL_INFO, "%s Response",
		     val_to_str(request_val -> last_transact2_command,
				trans2_cmd_vals, "Unknown (0x%02X)"));

    }

    /* Build display for: Word Count (WCT) */

    WordCount = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Word Count (WCT): %u", WordCount);

    }

    offset += 1; /* Skip Word Count (WCT) */

    if (WordCount == 0) {

      /* Interim response. */

      if (check_col(fd, COL_INFO)) {

	if (request_val == NULL)
	  col_set_str(fd, COL_INFO, "Interim response to unknown SMBtrans2");
	else if (request_val -> last_transact2_command == -1)
	  col_set_str(fd, COL_INFO, "Interim response to SMBtrans2 of unknown type");
	else
	  col_add_fstr(fd, COL_INFO, "%s interim response",
		       val_to_str(request_val -> last_transact2_command,
				  trans2_cmd_vals, "Unknown (0x%02X)"));

      }

      /* Build display for: Byte Count (BCC) */

      ByteCount = GSHORT(pd, offset);

      if (tree) {

	proto_tree_add_text(tree, NullTVB, offset, 2, "Byte Count (BCC): %u", ByteCount);

      }

      offset += 2; /* Skip Byte Count (BCC) */

      return;

    }

    /* Build display for: Total Parameter Count */

    TotalParameterCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Total Parameter Count: %u", TotalParameterCount);

    }

    offset += 2; /* Skip Total Parameter Count */

    /* Build display for: Total Data Count */

    TotalDataCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Total Data Count: %u", TotalDataCount);

    }

    offset += 2; /* Skip Total Data Count */

    /* Build display for: Reserved2 */

    Reserved2 = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Reserved2: %u", Reserved2);

    }

    offset += 2; /* Skip Reserved2 */

    /* Build display for: Parameter Count */

    ParameterCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Parameter Count: %u", ParameterCount);

    }

    offset += 2; /* Skip Parameter Count */

    /* Build display for: Parameter Offset */

    ParameterOffset = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Parameter Offset: %u", ParameterOffset);

    }

    offset += 2; /* Skip Parameter Offset */

    /* Build display for: Parameter Displacement */

    ParameterDisplacement = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Parameter Displacement: %u", ParameterDisplacement);

    }

    offset += 2; /* Skip Parameter Displacement */

    /* Build display for: Data Count */

    DataCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Data Count: %u", DataCount);

    }

    offset += 2; /* Skip Data Count */

    /* Build display for: Data Offset */

    DataOffset = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Data Offset: %u", DataOffset);

    }

    offset += 2; /* Skip Data Offset */

    /* Build display for: Data Displacement */

    if (!BYTES_ARE_IN_FRAME(offset, 2))
      return;

    DataDisplacement = GSHORT(pd, offset);
    si.ddisp = DataDisplacement;

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Data Displacement: %u", DataDisplacement);

    }

    offset += 2; /* Skip Data Displacement */

    /* Build display for: Setup Count */

    SetupCount = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Setup Count: %u", SetupCount);

    }

    offset += 1; /* Skip Setup Count */

    /* Build display for: Reserved3 */

    Reserved3 = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Reserved3: %u", Reserved3);

    }

    offset += 1; /* Skip Reserved3 */

    if (SetupCount != 0) {

      int i;

      for (i = 1; i <= SetupCount; i++) {
	
	Setup = GSHORT(pd, offset);

	if (tree) {

	  proto_tree_add_text(tree, NullTVB, offset, 2, "Setup%i: %u", i, Setup);

	}

	offset += 2; /* Skip Setup */

      }
    }

    /* Build display for: Byte Count (BCC) */

    ByteCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Byte Count (BCC): %u", ByteCount);

    }

    offset += 2; /* Skip Byte Count (BCC) */

    if (offset < (SMB_offset + ParameterOffset)) {

      int pad1Count = SMB_offset + ParameterOffset - offset;

      /* Build display for: Pad1 */

      if (tree) {

	proto_tree_add_text(tree, NullTVB, offset, pad1Count, "Pad1: %s",
			    bytes_to_str(pd + offset, pad1Count));
      }

      offset += pad1Count; /* Skip Pad1 */

    }

    /* Build display for: Parameters */

    if (ParameterCount > 0) {

      if (tree) {

	proto_tree_add_text(tree, NullTVB, offset, ParameterCount,
			    "Parameters: %s",
			    bytes_to_str(pd + offset, ParameterCount));

      }

      offset += ParameterCount; /* Skip Parameters */

    }

    if (DataCount > 0 && offset < (SMB_offset + DataOffset)) {

      int pad2Count = SMB_offset + DataOffset - offset;
	
      /* Build display for: Pad2 */

      if (tree) {

	proto_tree_add_text(tree, NullTVB, offset, pad2Count, "Pad2: %s",
			    bytes_to_str(pd + offset, pad2Count));

      }

      offset += pad2Count; /* Skip Pad2 */

    }

    /* Build display for: Data */

    if (DataCount > 0) {

      if (tree) {

	proto_tree_add_text(tree, NullTVB, offset, DataCount,
			    "Data: %s",
			    bytes_to_str(pd + offset, DataCount));

      }

      offset += DataCount; /* Skip Data */

    }

  }

}


static void 
dissect_transact_params(const u_char *pd, int offset, frame_data *fd,
    proto_tree *parent, proto_tree *tree, struct smb_info si, int max_data,
    int SMB_offset, int DataOffset, int DataCount,
    int ParameterOffset, int ParameterCount, int SetupAreaOffset,
    int SetupCount, const char *TransactName)
{
  char             *TransactNameCopy;
  char             *trans_type = NULL, *trans_cmd, *loc_of_slash = NULL;
  int              index;
  const gchar      *Data;
  packet_info      *pinfo;
  tvbuff_t         *next_tvb;
  tvbuff_t         *setup_tvb;

  if (TransactName != NULL) {
    /* Should check for error here ... */

    TransactNameCopy = g_strdup(TransactName);

    if (TransactNameCopy[0] == '\\') {
      trans_type = TransactNameCopy + 1;  /* Skip the slash */
      loc_of_slash = trans_type ? strchr(trans_type, '\\') : NULL;
    }

    if (loc_of_slash) {
      index = loc_of_slash - trans_type;  /* Make it a real index */
      trans_cmd = trans_type + index + 1;
      trans_type[index] = '\0';
    }
    else
      trans_cmd = NULL;
  } else
    trans_cmd = NULL;

  pinfo = &pi;

  /*
   * Number of bytes of parameter.
   */
  si.parameter_count = ParameterCount;

  if (DataOffset < 0) {
    /*
     * This is an interim response, so there're no parameters or data
     * to dissect.
     */
    si.is_interim_response = TRUE;

    /*
     * Create a zero-length tvbuff.
     */
    next_tvb = tvb_create_from_top(pi.captured_len);
  } else {
    /*
     * This isn't an interim response.
     */
    si.is_interim_response = FALSE;

    /*
     * Create a tvbuff for the parameters and data.
     */
    next_tvb = tvb_create_from_top(SMB_offset + ParameterOffset);
  }

  /*
   * Offset of beginning of data from beginning of next_tvb.
   */
  si.data_offset = DataOffset - ParameterOffset;

  /*
   * Number of bytes of data.
   */
  si.data_count = DataCount;

  /*
   * Command.
   */
  si.trans_cmd = trans_cmd;

  /*
   * Pass "si" to the subdissector.
   */
  pinfo->private_data = &si;

  /*
   * Tvbuff for setup area, for mailslot call.
   */
  /*
   * Is there a setup area?
   */
  if (SetupAreaOffset < 0) {
    /*
     * No - create a zero-length tvbuff.
     */
    setup_tvb = tvb_create_from_top(pi.captured_len);
  } else {
    /*
     * Create a tvbuff for the setup area.
     */
    setup_tvb = tvb_create_from_top(SetupAreaOffset);
  }

  if ((trans_cmd == NULL) ||
      (((trans_type == NULL || strcmp(trans_type, "MAILSLOT") != 0) ||
       !dissect_mailslot_smb(setup_tvb, next_tvb, pinfo, parent)) &&
      ((trans_type == NULL || strcmp(trans_type, "PIPE") != 0) ||
       !dissect_pipe_smb(next_tvb, pinfo, parent)))) {

    if (ParameterCount > 0) {

      /* Build display for: Parameters */
      
      if (tree) {

	proto_tree_add_text(tree, NullTVB, SMB_offset + ParameterOffset,
			    ParameterCount, "Parameters: %s",
			    bytes_to_str(pd + SMB_offset + ParameterOffset,
					 ParameterCount));
	  
      }
	
      offset = SMB_offset + ParameterOffset + ParameterCount; /* Skip Parameters */

    }

    if (DataCount > 0 && offset < (SMB_offset + DataOffset)) {

      int pad2Count = SMB_offset + DataOffset - offset;
	
      /* Build display for: Pad2 */

      if (tree) {

	proto_tree_add_text(tree, NullTVB, offset, pad2Count, "Pad2: %s",
			    bytes_to_str(pd + offset, pad2Count));

      }

      offset += pad2Count; /* Skip Pad2 */

    }

    if (DataCount > 0) {

      /* Build display for: Data */

      Data = pd + SMB_offset + DataOffset;

      if (tree) {

	proto_tree_add_text(tree, NullTVB, SMB_offset + DataOffset, DataCount,
			    "Data: %s",
			    bytes_to_str(pd + SMB_offset + DataOffset, DataCount));

      }

      offset += DataCount; /* Skip Data */

    }
  }

}

void
dissect_transact_smb(const u_char *pd, int offset, frame_data *fd,
		     proto_tree *parent, proto_tree *tree,
		     struct smb_info si, int max_data, int SMB_offset)
{
  proto_tree    *Flags_tree;
  proto_item    *ti;
  guint8        WordCount;
  guint8        SetupCount;
  guint8        Reserved3;
  guint8        Reserved1;
  guint8        MaxSetupCount;
  guint32       Timeout;
  guint16       TotalParameterCount;
  guint16       TotalDataCount;
  guint16       Setup = 0;
  guint16       Reserved2;
  guint16       ParameterOffset;
  guint16       ParameterDisplacement;
  guint16       ParameterCount;
  guint16       MaxParameterCount;
  guint16       MaxDataCount;
  guint16       Flags;
  guint16       DataOffset;
  guint16       DataDisplacement;
  guint16       DataCount;
  guint16       ByteCount;
  int           TNlen;
  const char    *TransactName;
  conversation_t *conversation;
  struct smb_request_val *request_val;
  guint16	SetupAreaOffset;

  /*
   * Find out what conversation this packet is part of
   */

  conversation = find_conversation(&pi.src, &pi.dst, pi.ptype,
				   pi.srcport, pi.destport, 0);

  if (conversation == NULL) {  /* Create a new conversation */

    conversation = conversation_new(&pi.src, &pi.dst, pi.ptype,
				    pi.srcport, pi.destport, 0);

  }

  si.conversation = conversation;  /* Save this */

  request_val = do_transaction_hashing(conversation, si, fd);

  si.request_val = request_val;  /* Save this for later */

  if (si.request) {
    /* Request(s) dissect code */

    /* Build display for: Word Count (WCT) */

    WordCount = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Word Count (WCT): %u", WordCount);

    }

    offset += 1; /* Skip Word Count (WCT) */

    /* Build display for: Total Parameter Count */

    TotalParameterCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Total Parameter Count: %u", TotalParameterCount);

    }

    offset += 2; /* Skip Total Parameter Count */

    /* Build display for: Total Data Count */

    if (!BYTES_ARE_IN_FRAME(offset, 2))
      return;

    TotalDataCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Total Data Count: %u", TotalDataCount);

    }

    offset += 2; /* Skip Total Data Count */

    /* Build display for: Max Parameter Count */

    MaxParameterCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Max Parameter Count: %u", MaxParameterCount);

    }

    offset += 2; /* Skip Max Parameter Count */

    /* Build display for: Max Data Count */

    MaxDataCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Max Data Count: %u", MaxDataCount);

    }

    offset += 2; /* Skip Max Data Count */

    /* Build display for: Max Setup Count */

    MaxSetupCount = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Max Setup Count: %u", MaxSetupCount);

    }

    offset += 1; /* Skip Max Setup Count */

    /* Build display for: Reserved1 */

    Reserved1 = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Reserved1: %u", Reserved1);

    }

    offset += 1; /* Skip Reserved1 */

    /* Build display for: Flags */

    Flags = GSHORT(pd, offset);

    if (tree) {

      ti = proto_tree_add_text(tree, NullTVB, offset, 2, "Flags: 0x%02x", Flags);
      Flags_tree = proto_item_add_subtree(ti, ett_smb_flags);
      proto_tree_add_text(Flags_tree, NullTVB, offset, 2, "%s",
                          decode_boolean_bitfield(Flags, 0x01, 16, "Also disconnect TID", "Dont disconnect TID"));
      proto_tree_add_text(Flags_tree, NullTVB, offset, 2, "%s",
                          decode_boolean_bitfield(Flags, 0x02, 16, "One way transaction", "Two way transaction"));
    
    }

    offset += 2; /* Skip Flags */

    /* Build display for: Timeout */

    Timeout = GWORD(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 4, "Timeout: %u", Timeout);

    }

    offset += 4; /* Skip Timeout */

    /* Build display for: Reserved2 */

    Reserved2 = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Reserved2: %u", Reserved2);

    }

    offset += 2; /* Skip Reserved2 */

    /* Build display for: Parameter Count */

    if (!BYTES_ARE_IN_FRAME(offset, 2))
      return;

    ParameterCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Parameter Count: %u", ParameterCount);

    }

    offset += 2; /* Skip Parameter Count */

    /* Build display for: Parameter Offset */

    if (!BYTES_ARE_IN_FRAME(offset, 2))
      return;

    ParameterOffset = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Parameter Offset: %u", ParameterOffset);

    }

    offset += 2; /* Skip Parameter Offset */

    /* Build display for: Data Count */

    if (!BYTES_ARE_IN_FRAME(offset, 2))
      return;

    DataCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Data Count: %u", DataCount);

    }

    offset += 2; /* Skip Data Count */

    /* Build display for: Data Offset */

    if (!BYTES_ARE_IN_FRAME(offset, 2))
      return;

    DataOffset = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Data Offset: %u", DataOffset);

    }

    offset += 2; /* Skip Data Offset */

    /* Build display for: Setup Count */

    if (!BYTES_ARE_IN_FRAME(offset, 2))
      return;

    SetupCount = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Setup Count: %u", SetupCount);

    }

    offset += 1; /* Skip Setup Count */

    /* Build display for: Reserved3 */

    Reserved3 = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Reserved3: %u", Reserved3);
    }

    offset += 1; /* Skip Reserved3 */
 
    SetupAreaOffset = offset;

    /* Build display for: Setup */

    if (SetupCount > 0) {

      int i = SetupCount;

      if (!BYTES_ARE_IN_FRAME(offset, 2))
	return;

      Setup = GSHORT(pd, offset);

      for (i = 1; i <= SetupCount; i++) {
	
	if (!BYTES_ARE_IN_FRAME(offset, 2))
	  return;

	Setup = GSHORT(pd, offset);

	if (tree) {

	  proto_tree_add_text(tree, NullTVB, offset, 2, "Setup%i: %u", i, Setup);

	}

	offset += 2; /* Skip Setup */

      }

    }

    /* Build display for: Byte Count (BCC) */

    if (!BYTES_ARE_IN_FRAME(offset, 2))
      return;

    ByteCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Byte Count (BCC): %u", ByteCount);

    }

    offset += 2; /* Skip Byte Count (BCC) */

    /* Build display for: Transact Name */

    TransactName = get_unicode_or_ascii_string(pd, &offset, SMB_offset, si.unicode, &TNlen);

    if (!fd->flags.visited) {
      /*
       * This is the first time this frame has been seen; remember
       * the transaction name.
       */
      g_assert(request_val -> last_transact_command == NULL);
      request_val -> last_transact_command = g_strdup(TransactName);
    }

    if (check_col(fd, COL_INFO)) {

      col_add_fstr(fd, COL_INFO, "%s Request", TransactName);

    }

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, TNlen, "Transact Name: %s", TransactName);

    }

    offset += TNlen; /* Skip Transact Name */
    if (si.unicode) offset += 2;   /* There are two more extraneous bytes there*/

    if (offset < (SMB_offset + ParameterOffset)) {

      int pad1Count = SMB_offset + ParameterOffset - offset;

      /* Build display for: Pad1 */

      if (tree) {

	proto_tree_add_text(tree, NullTVB, offset, pad1Count, "Pad1: %s",
			    bytes_to_str(pd + offset, pad1Count));
      }

      offset += pad1Count; /* Skip Pad1 */

    }

    /* Let's see if we can decode this */

    dissect_transact_params(pd, offset, fd, parent, tree, si, max_data,
			    SMB_offset, DataOffset, DataCount,
			    ParameterOffset, ParameterCount,
			    SetupAreaOffset, SetupCount, TransactName);

  } else {
    /* Response(s) dissect code */

    if (check_col(fd, COL_INFO)) {
      if ( request_val == NULL )
	col_set_str(fd, COL_INFO, "Response to unknown SMBtrans");
      else if (request_val -> last_transact_command == NULL)
	col_set_str(fd, COL_INFO, "Response to SMBtrans of unknown type");
      else
	col_add_fstr(fd, COL_INFO, "%s Response",
		     request_val -> last_transact_command);

    }

    /* Build display for: Word Count (WCT) */

    WordCount = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Word Count (WCT): %u", WordCount);

    }

    offset += 1; /* Skip Word Count (WCT) */

    if (WordCount == 0) {

      /* Interim response. */

      if (check_col(fd, COL_INFO)) {
	if ( request_val == NULL )
	  col_set_str(fd, COL_INFO, "Interim response to unknown SMBtrans");
	else if (request_val -> last_transact_command == NULL)
	  col_set_str(fd, COL_INFO, "Interim response to SMBtrans of unknown type");
	else
	  col_add_fstr(fd, COL_INFO, "%s interim response",
		       request_val -> last_transact_command);

      }

      /* Build display for: Byte Count (BCC) */

      ByteCount = GSHORT(pd, offset);

      if (tree) {

	proto_tree_add_text(tree, NullTVB, offset, 2, "Byte Count (BCC): %u", ByteCount);

      }

      offset += 2; /* Skip Byte Count (BCC) */

      /* Dissect the interim response by showing the type of request to
         which it's a reply, if we have that information. */
      if (request_val != NULL) {
	dissect_transact_params(pd, offset, fd, parent, tree, si, max_data,
				SMB_offset, -1, -1, -1, -1, -1, -1,
	  			request_val -> last_transact_command);
      }

      return;

    }

    /* Build display for: Total Parameter Count */

    TotalParameterCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Total Parameter Count: %u", TotalParameterCount);

    }

    offset += 2; /* Skip Total Parameter Count */

    /* Build display for: Total Data Count */

    TotalDataCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Total Data Count: %u", TotalDataCount);

    }

    offset += 2; /* Skip Total Data Count */

    /* Build display for: Reserved2 */

    Reserved2 = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Reserved2: %u", Reserved2);

    }

    offset += 2; /* Skip Reserved2 */

    /* Build display for: Parameter Count */

    ParameterCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Parameter Count: %u", ParameterCount);

    }

    offset += 2; /* Skip Parameter Count */

    /* Build display for: Parameter Offset */

    ParameterOffset = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Parameter Offset: %u", ParameterOffset);

    }

    offset += 2; /* Skip Parameter Offset */

    /* Build display for: Parameter Displacement */

    ParameterDisplacement = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Parameter Displacement: %u", ParameterDisplacement);

    }

    offset += 2; /* Skip Parameter Displacement */

    /* Build display for: Data Count */

    DataCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Data Count: %u", DataCount);

    }

    offset += 2; /* Skip Data Count */

    /* Build display for: Data Offset */

    DataOffset = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Data Offset: %u", DataOffset);

    }

    offset += 2; /* Skip Data Offset */

    /* Build display for: Data Displacement */

    if (!BYTES_ARE_IN_FRAME(offset, 2))
      return;

    DataDisplacement = GSHORT(pd, offset);
    si.ddisp = DataDisplacement;

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Data Displacement: %u", DataDisplacement);

    }

    offset += 2; /* Skip Data Displacement */

    /* Build display for: Setup Count */

    SetupCount = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Setup Count: %u", SetupCount);

    }

    offset += 1; /* Skip Setup Count */

 
    /* Build display for: Reserved3 */

    Reserved3 = GBYTE(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 1, "Reserved3: %u", Reserved3);

    }


    offset += 1; /* Skip Reserved3 */
 
    SetupAreaOffset = offset;	

    /* Build display for: Setup */

    if (SetupCount > 0) {

      int i = SetupCount;

      Setup = GSHORT(pd, offset);

      for (i = 1; i <= SetupCount; i++) {
	
	Setup = GSHORT(pd, offset);

	if (tree) {

	  proto_tree_add_text(tree, NullTVB, offset, 2, "Setup%i: %u", i, Setup);

	}

	offset += 2; /* Skip Setup */

      }

    }

    /* Build display for: Byte Count (BCC) */

    ByteCount = GSHORT(pd, offset);

    if (tree) {

      proto_tree_add_text(tree, NullTVB, offset, 2, "Byte Count (BCC): %u", ByteCount);

    }

    offset += 2; /* Skip Byte Count (BCC) */

    /* Build display for: Pad1 */

    if (offset < (SMB_offset + ParameterOffset)) {

      int pad1Count = SMB_offset + ParameterOffset - offset;

      /* Build display for: Pad1 */

      if (tree) {

	proto_tree_add_text(tree, NullTVB, offset, pad1Count, "Pad1: %s",
			    bytes_to_str(pd + offset, pad1Count));
      }

      offset += pad1Count; /* Skip Pad1 */

    }

    if (request_val != NULL)
      TransactName = request_val -> last_transact_command;
    else
      TransactName = NULL;

    /*
     * Make an entry for this, if it's continued; get the entry for
     * the message of which it's a continuation, and get the transaction
     * name for that message, if it's a continuation.
     *
     * XXX - eventually, do reassembly of all the continuations, so
     * we can dissect the entire reply.
     */
    si.continuation_val = do_continuation_hashing(conversation, si, fd,
					          TotalDataCount, DataCount,
					          &TransactName);
    dissect_transact_params(pd, offset, fd, parent, tree, si, max_data,
			    SMB_offset, DataOffset, DataCount,
			    ParameterOffset, ParameterCount,
			    SetupAreaOffset, SetupCount, TransactName);

  }

}





static void (*dissect[256])(const u_char *, int, frame_data *, proto_tree *, proto_tree *, struct smb_info, int, int) = {

  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,

  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,      /* unknown SMB 0x19 */
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,      /* SMBreadBs read block (secondary response) */
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,      /* SMBwriteBs write block (secondary request) */

  dissect_unknown_smb,
  dissect_unknown_smb,      /* unknown SMB 0x21 */
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_transact_smb,      /* SMBtrans transaction - name, bytes in/out */
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,      /* SMBwriteX write and X */

  dissect_unknown_smb,      /* unknown SMB 0x30 */
  dissect_unknown_smb,      /* unknown SMB 0x31 */
  dissect_transact2_smb,    /* SMBtrans2 transaction */
  dissect_unknown_smb,      /* unknown SMB 0x33 */
  dissect_unknown_smb,
  dissect_unknown_smb,      /* unknown SMB 0x35 */
  dissect_unknown_smb,      /* unknown SMB 0x36 */
  dissect_unknown_smb,      /* unknown SMB 0x37 */
  dissect_unknown_smb,      /* unknown SMB 0x38 */
  dissect_unknown_smb,      /* unknown SMB 0x39 */
  dissect_unknown_smb,      /* unknown SMB 0x3a */
  dissect_unknown_smb,      /* unknown SMB 0x3b */
  dissect_unknown_smb,      /* unknown SMB 0x3c */
  dissect_unknown_smb,      /* unknown SMB 0x3d */
  dissect_unknown_smb,      /* unknown SMB 0x3e */
  dissect_unknown_smb,      /* unknown SMB 0x3f */

  dissect_unknown_smb,      /* unknown SMB 0x40 */
  dissect_unknown_smb,      /* unknown SMB 0x41 */
  dissect_unknown_smb,      /* unknown SMB 0x42 */
  dissect_unknown_smb,      /* unknown SMB 0x43 */
  dissect_unknown_smb,      /* unknown SMB 0x44 */
  dissect_unknown_smb,      /* unknown SMB 0x45 */
  dissect_unknown_smb,      /* unknown SMB 0x46 */
  dissect_unknown_smb,      /* unknown SMB 0x47 */
  dissect_unknown_smb,      /* unknown SMB 0x48 */
  dissect_unknown_smb,      /* unknown SMB 0x49 */
  dissect_unknown_smb,      /* unknown SMB 0x4a */
  dissect_unknown_smb,      /* unknown SMB 0x4b */
  dissect_unknown_smb,      /* unknown SMB 0x4c */
  dissect_unknown_smb,      /* unknown SMB 0x4d */
  dissect_unknown_smb,      /* unknown SMB 0x4e */
  dissect_unknown_smb,      /* unknown SMB 0x4f */

  dissect_unknown_smb,      /* unknown SMB 0x50 */
  dissect_unknown_smb,      /* unknown SMB 0x51 */
  dissect_unknown_smb,      /* unknown SMB 0x52 */
  dissect_unknown_smb,      /* unknown SMB 0x53 */
  dissect_unknown_smb,      /* unknown SMB 0x54 */
  dissect_unknown_smb,      /* unknown SMB 0x55 */
  dissect_unknown_smb,      /* unknown SMB 0x56 */
  dissect_unknown_smb,      /* unknown SMB 0x57 */
  dissect_unknown_smb,      /* unknown SMB 0x58 */
  dissect_unknown_smb,      /* unknown SMB 0x59 */
  dissect_unknown_smb,      /* unknown SMB 0x5a */
  dissect_unknown_smb,      /* unknown SMB 0x5b */
  dissect_unknown_smb,      /* unknown SMB 0x5c */
  dissect_unknown_smb,      /* unknown SMB 0x5d */
  dissect_unknown_smb,      /* unknown SMB 0x5e */
  dissect_unknown_smb,      /* unknown SMB 0x5f */

  dissect_unknown_smb,      /* unknown SMB 0x60 */
  dissect_unknown_smb,      /* unknown SMB 0x61 */
  dissect_unknown_smb,      /* unknown SMB 0x62 */
  dissect_unknown_smb,      /* unknown SMB 0x63 */
  dissect_unknown_smb,      /* unknown SMB 0x64 */
  dissect_unknown_smb,      /* unknown SMB 0x65 */
  dissect_unknown_smb,      /* unknown SMB 0x66 */
  dissect_unknown_smb,      /* unknown SMB 0x67 */
  dissect_unknown_smb,      /* unknown SMB 0x68 */
  dissect_unknown_smb,      /* unknown SMB 0x69 */
  dissect_unknown_smb,      /* unknown SMB 0x6a */
  dissect_unknown_smb,      /* unknown SMB 0x6b */
  dissect_unknown_smb,      /* unknown SMB 0x6c */
  dissect_unknown_smb,      /* unknown SMB 0x6d */
  dissect_unknown_smb,      /* unknown SMB 0x6e */
  dissect_unknown_smb,      /* unknown SMB 0x6f */

  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,      /* unknown SMB 0x76 */
  dissect_unknown_smb,      /* unknown SMB 0x77 */
  dissect_unknown_smb,      /* unknown SMB 0x78 */
  dissect_unknown_smb,      /* unknown SMB 0x79 */
  dissect_unknown_smb,      /* unknown SMB 0x7a */
  dissect_unknown_smb,      /* unknown SMB 0x7b */
  dissect_unknown_smb,      /* unknown SMB 0x7c */
  dissect_unknown_smb,      /* unknown SMB 0x7d */
  dissect_unknown_smb,      /* unknown SMB 0x7e */
  dissect_unknown_smb,      /* unknown SMB 0x7f */

  dissect_unknown_smb,
  dissect_unknown_smb,
  dissect_unknown_smb,      /* SMBffirst find first */
  dissect_unknown_smb,      /* SMBfunique find unique */
  dissect_unknown_smb,      /* SMBfclose find close */
  dissect_unknown_smb,      /* unknown SMB 0x85 */
  dissect_unknown_smb,      /* unknown SMB 0x86 */
  dissect_unknown_smb,      /* unknown SMB 0x87 */
  dissect_unknown_smb,      /* unknown SMB 0x88 */
  dissect_unknown_smb,      /* unknown SMB 0x89 */
  dissect_unknown_smb,      /* unknown SMB 0x8a */
  dissect_unknown_smb,      /* unknown SMB 0x8b */
  dissect_unknown_smb,      /* unknown SMB 0x8c */
  dissect_unknown_smb,      /* unknown SMB 0x8d */
  dissect_unknown_smb,      /* unknown SMB 0x8e */
  dissect_unknown_smb,      /* unknown SMB 0x8f */

  dissect_unknown_smb,      /* unknown SMB 0x90 */
  dissect_unknown_smb,      /* unknown SMB 0x91 */
  dissect_unknown_smb,      /* unknown SMB 0x92 */
  dissect_unknown_smb,      /* unknown SMB 0x93 */
  dissect_unknown_smb,      /* unknown SMB 0x94 */
  dissect_unknown_smb,      /* unknown SMB 0x95 */
  dissect_unknown_smb,      /* unknown SMB 0x96 */
  dissect_unknown_smb,      /* unknown SMB 0x97 */
  dissect_unknown_smb,      /* unknown SMB 0x98 */
  dissect_unknown_smb,      /* unknown SMB 0x99 */
  dissect_unknown_smb,      /* unknown SMB 0x9a */
  dissect_unknown_smb,      /* unknown SMB 0x9b */
  dissect_unknown_smb,      /* unknown SMB 0x9c */
  dissect_unknown_smb,      /* unknown SMB 0x9d */
  dissect_unknown_smb,      /* unknown SMB 0x9e */
  dissect_unknown_smb,      /* unknown SMB 0x9f */

  dissect_unknown_smb,      /* unknown SMB 0xa0 */
  dissect_unknown_smb,      /* unknown SMB 0xa1 */
  dissect_unknown_smb,      /* unknown SMB 0xa2 */
  dissect_unknown_smb,      /* unknown SMB 0xa3 */
  dissect_unknown_smb,      /* unknown SMB 0xa4 */
  dissect_unknown_smb,      /* unknown SMB 0xa5 */
  dissect_unknown_smb,      /* unknown SMB 0xa6 */
  dissect_unknown_smb,      /* unknown SMB 0xa7 */
  dissect_unknown_smb,      /* unknown SMB 0xa8 */
  dissect_unknown_smb,      /* unknown SMB 0xa9 */
  dissect_unknown_smb,      /* unknown SMB 0xaa */
  dissect_unknown_smb,      /* unknown SMB 0xab */
  dissect_unknown_smb,      /* unknown SMB 0xac */
  dissect_unknown_smb,      /* unknown SMB 0xad */
  dissect_unknown_smb,      /* unknown SMB 0xae */
  dissect_unknown_smb,      /* unknown SMB 0xaf */

  dissect_unknown_smb,      /* unknown SMB 0xb0 */
  dissect_unknown_smb,      /* unknown SMB 0xb1 */
  dissect_unknown_smb,      /* unknown SMB 0xb2 */
  dissect_unknown_smb,      /* unknown SMB 0xb3 */
  dissect_unknown_smb,      /* unknown SMB 0xb4 */
  dissect_unknown_smb,      /* unknown SMB 0xb5 */
  dissect_unknown_smb,      /* unknown SMB 0xb6 */
  dissect_unknown_smb,      /* unknown SMB 0xb7 */
  dissect_unknown_smb,      /* unknown SMB 0xb8 */
  dissect_unknown_smb,      /* unknown SMB 0xb9 */
  dissect_unknown_smb,      /* unknown SMB 0xba */
  dissect_unknown_smb,      /* unknown SMB 0xbb */
  dissect_unknown_smb,      /* unknown SMB 0xbc */
  dissect_unknown_smb,      /* unknown SMB 0xbd */
  dissect_unknown_smb,      /* unknown SMB 0xbe */
  dissect_unknown_smb,      /* unknown SMB 0xbf */

  dissect_open_print_file_smb,/* SMBsplopen open a print spool file */
  dissect_write_print_file_smb,/* SMBsplwr write to a print spool file */
  dissect_unknown_smb,
  dissect_get_print_queue_smb, /* SMBsplretq return print queue */
  dissect_unknown_smb,      /* unknown SMB 0xc4 */
  dissect_unknown_smb,      /* unknown SMB 0xc5 */
  dissect_unknown_smb,      /* unknown SMB 0xc6 */
  dissect_unknown_smb,      /* unknown SMB 0xc7 */
  dissect_unknown_smb,      /* unknown SMB 0xc8 */
  dissect_unknown_smb,      /* unknown SMB 0xc9 */
  dissect_unknown_smb,      /* unknown SMB 0xca */
  dissect_unknown_smb,      /* unknown SMB 0xcb */
  dissect_unknown_smb,      /* unknown SMB 0xcc */
  dissect_unknown_smb,      /* unknown SMB 0xcd */
  dissect_unknown_smb,      /* unknown SMB 0xce */
  dissect_unknown_smb,      /* unknown SMB 0xcf */

  dissect_unknown_smb,      /* SMBsends send a single block message */
  dissect_unknown_smb,      /* SMBsendb send a broadcast message */
  dissect_unknown_smb,      /* SMBfwdname forward user name */
  dissect_unknown_smb,      /* SMBcancelf cancel forward */
  dissect_unknown_smb,      /* SMBgetmac get a machine name */
  dissect_unknown_smb,      /* SMBsendstrt send start of multi-block message */
  dissect_unknown_smb,      /* SMBsendend send end of multi-block message */
  dissect_unknown_smb,      /* SMBsendtxt send text of multi-block message */
  dissect_unknown_smb,      /* unknown SMB 0xd8 */
  dissect_unknown_smb,      /* unknown SMB 0xd9 */
  dissect_unknown_smb,      /* unknown SMB 0xda */
  dissect_unknown_smb,      /* unknown SMB 0xdb */
  dissect_unknown_smb,      /* unknown SMB 0xdc */
  dissect_unknown_smb,      /* unknown SMB 0xdd */
  dissect_unknown_smb,      /* unknown SMB 0xde */
  dissect_unknown_smb,      /* unknown SMB 0xdf */

  dissect_unknown_smb,      /* unknown SMB 0xe0 */
  dissect_unknown_smb,      /* unknown SMB 0xe1 */
  dissect_unknown_smb,      /* unknown SMB 0xe2 */
  dissect_unknown_smb,      /* unknown SMB 0xe3 */
  dissect_unknown_smb,      /* unknown SMB 0xe4 */
  dissect_unknown_smb,      /* unknown SMB 0xe5 */
  dissect_unknown_smb,      /* unknown SMB 0xe6 */
  dissect_unknown_smb,      /* unknown SMB 0xe7 */
  dissect_unknown_smb,      /* unknown SMB 0xe8 */
  dissect_unknown_smb,      /* unknown SMB 0xe9 */
  dissect_unknown_smb,      /* unknown SMB 0xea */
  dissect_unknown_smb,      /* unknown SMB 0xeb */
  dissect_unknown_smb,      /* unknown SMB 0xec */
  dissect_unknown_smb,      /* unknown SMB 0xed */
  dissect_unknown_smb,      /* unknown SMB 0xee */
  dissect_unknown_smb,      /* unknown SMB 0xef */

  dissect_unknown_smb,      /* unknown SMB 0xf0 */
  dissect_unknown_smb,      /* unknown SMB 0xf1 */
  dissect_unknown_smb,      /* unknown SMB 0xf2 */
  dissect_unknown_smb,      /* unknown SMB 0xf3 */
  dissect_unknown_smb,      /* unknown SMB 0xf4 */
  dissect_unknown_smb,      /* unknown SMB 0xf5 */
  dissect_unknown_smb,      /* unknown SMB 0xf6 */
  dissect_unknown_smb,      /* unknown SMB 0xf7 */
  dissect_unknown_smb,      /* unknown SMB 0xf8 */
  dissect_unknown_smb,      /* unknown SMB 0xf9 */
  dissect_unknown_smb,      /* unknown SMB 0xfa */
  dissect_unknown_smb,      /* unknown SMB 0xfb */
  dissect_unknown_smb,      /* unknown SMB 0xfc */
  dissect_unknown_smb,      /* unknown SMB 0xfd */
  dissect_unknown_smb,      /* SMBinvalid invalid command */
  dissect_unknown_smb       /* unknown SMB 0xff */

};

static const value_string errcls_types[] = {
  { SMB_SUCCESS, "Success"},
  { SMB_ERRDOS, "DOS Error"},
  { SMB_ERRSRV, "Server Error"},
  { SMB_ERRHRD, "Hardware Error"},
  { SMB_ERRCMD, "Command Error - Not an SMB format command"},
  { 0, NULL }
};

static const value_string DOS_errors[] = {
  {SMBE_badfunc, "Invalid function (or system call)"},
  {SMBE_badfile, "File not found (pathname error)"},
  {SMBE_badpath, "Directory not found"},
  {SMBE_nofids, "Too many open files"},
  {SMBE_noaccess, "Access denied"},
  {SMBE_badfid, "Invalid fid"},
  {SMBE_nomem,  "Out of memory"},
  {SMBE_badmem, "Invalid memory block address"},
  {SMBE_badenv, "Invalid environment"},
  {SMBE_badaccess, "Invalid open mode"},
  {SMBE_baddata, "Invalid data (only from ioctl call)"},
  {SMBE_res, "Reserved error code?"}, 
  {SMBE_baddrive, "Invalid drive"},
  {SMBE_remcd, "Attempt to delete current directory"},
  {SMBE_diffdevice, "Rename/move across different filesystems"},
  {SMBE_nofiles, "No more files found in file search"},
  {SMBE_badshare, "Share mode on file conflict with open mode"},
  {SMBE_lock, "Lock request conflicts with existing lock"},
  {SMBE_unsup, "Request unsupported, returned by Win 95"},
  {SMBE_nosuchshare, "Requested share does not exist"},
  {SMBE_filexists, "File in operation already exists"},
  {SMBE_cannotopen, "Cannot open the file specified"},
  {SMBE_unknownlevel, "Unknown level??"},
  {SMBE_badpipe, "Named pipe invalid"},
  {SMBE_pipebusy, "All instances of pipe are busy"},
  {SMBE_pipeclosing, "Named pipe close in progress"},
  {SMBE_notconnected, "No process on other end of named pipe"},
  {SMBE_moredata, "More data to be returned"},
  {SMBE_baddirectory,  "Invalid directory name in a path."},
  {SMBE_eas_didnt_fit, "Extended attributes didn't fit"},
  {SMBE_eas_nsup, "Extended attributes not supported"},
  {SMBE_notify_buf_small, "Buffer too small to return change notify."},
  {SMBE_unknownipc, "Unknown IPC Operation"},
  {SMBE_noipc, "Don't support ipc"},
  {0, NULL}
  };

/* Error codes for the ERRSRV class */

static const value_string SRV_errors[] = {
  {SMBE_error, "Non specific error code"},
  {SMBE_badpw, "Bad password"},
  {SMBE_badtype, "Reserved"},
  {SMBE_access, "No permissions to perform the requested operation"},
  {SMBE_invnid, "TID invalid"},
  {SMBE_invnetname, "Invalid network name. Service not found"},
  {SMBE_invdevice, "Invalid device"},
  {SMBE_unknownsmb, "Unknown SMB, from NT 3.5 response"},
  {SMBE_qfull, "Print queue full"},
  {SMBE_qtoobig, "Queued item too big"},
  {SMBE_qeof, "EOF on print queue dump"},
  {SMBE_invpfid, "Invalid print file in smb_fid"},
  {SMBE_smbcmd, "Unrecognised command"},
  {SMBE_srverror, "SMB server internal error"},
  {SMBE_filespecs, "Fid and pathname invalid combination"},
  {SMBE_badlink, "Bad link in request ???"},
  {SMBE_badpermits, "Access specified for a file is not valid"},
  {SMBE_badpid, "Bad process id in request"},
  {SMBE_setattrmode, "Attribute mode invalid"},
  {SMBE_paused, "Message server paused"},
  {SMBE_msgoff, "Not receiving messages"},
  {SMBE_noroom, "No room for message"},
  {SMBE_rmuns, "Too many remote usernames"},
  {SMBE_timeout, "Operation timed out"},
  {SMBE_noresource, "No resources currently available for request."},
  {SMBE_toomanyuids, "Too many userids"},
  {SMBE_baduid, "Bad userid"},
  {SMBE_useMPX, "Temporarily unable to use raw mode, use MPX mode"},
  {SMBE_useSTD, "Temporarily unable to use raw mode, use standard mode"},
  {SMBE_contMPX, "Resume MPX mode"},
  {SMBE_badPW, "Bad Password???"},
  {SMBE_nosupport, "Operation not supported"},
  { 0, NULL}
};

/* Error codes for the ERRHRD class */

static const value_string HRD_errors[] = {
  {SMBE_nowrite, "Read only media"},
  {SMBE_badunit, "Unknown device"},
  {SMBE_notready, "Drive not ready"},
  {SMBE_badcmd, "Unknown command"},
  {SMBE_data, "Data (CRC) error"},
  {SMBE_badreq, "Bad request structure length"},
  {SMBE_seek, "Seek error???"},
  {SMBE_badmedia, "Bad media???"},
  {SMBE_badsector, "Bad sector???"},
  {SMBE_nopaper, "No paper in printer???"},
  {SMBE_write, "Write error???"},
  {SMBE_read, "Read error???"},
  {SMBE_general, "General error???"},
  {SMBE_badshare, "A open conflicts with an existing open"},
  {SMBE_lock, "Lock/unlock error"},
  {SMBE_wrongdisk,  "Wrong disk???"},
  {SMBE_FCBunavail, "FCB unavailable???"},
  {SMBE_sharebufexc, "Share buffer excluded???"},
  {SMBE_diskfull, "Disk full???"},
  {0, NULL}
};

char *decode_smb_error(guint8 errcls, guint16 errcode)
{

  switch (errcls) {

  case SMB_SUCCESS:

    return("No Error");   /* No error ??? */
    break;

  case SMB_ERRDOS:

    return(val_to_str(errcode, DOS_errors, "Unknown DOS error (%x)"));
    break;

  case SMB_ERRSRV:

    return(val_to_str(errcode, SRV_errors, "Unknown SRV error (%x)"));
    break;

  case SMB_ERRHRD:

    return(val_to_str(errcode, HRD_errors, "Unknown HRD error (%x)"));
    break;

  default:

    return("Unknown error class!");

  }

}

/*
 * NT error codes.
 *
 * From
 *
 *	http://www.wildpackets.com/elements/SMB_NT_Status_Codes.txt
 */
static const value_string NT_errors[] = {
  { 0x00000000, "STATUS_SUCCESS" },
  { 0x00000000, "STATUS_WAIT_0" },
  { 0x00000001, "STATUS_WAIT_1" },
  { 0x00000002, "STATUS_WAIT_2" },
  { 0x00000003, "STATUS_WAIT_3" },
  { 0x0000003F, "STATUS_WAIT_63" },
  { 0x00000080, "STATUS_ABANDONED" },
  { 0x00000080, "STATUS_ABANDONED_WAIT_0" },
  { 0x000000BF, "STATUS_ABANDONED_WAIT_63" },
  { 0x000000C0, "STATUS_USER_APC" },
  { 0x00000100, "STATUS_KERNEL_APC" },
  { 0x00000101, "STATUS_ALERTED" },
  { 0x00000102, "STATUS_TIMEOUT" },
  { 0x00000103, "STATUS_PENDING" },
  { 0x00000104, "STATUS_REPARSE" },
  { 0x00000105, "STATUS_MORE_ENTRIES" },
  { 0x00000106, "STATUS_NOT_ALL_ASSIGNED" },
  { 0x00000107, "STATUS_SOME_NOT_MAPPED" },
  { 0x00000108, "STATUS_OPLOCK_BREAK_IN_PROGRESS" },
  { 0x00000109, "STATUS_VOLUME_MOUNTED" },
  { 0x0000010A, "STATUS_RXACT_COMMITTED" },
  { 0x0000010B, "STATUS_NOTIFY_CLEANUP" },
  { 0x0000010C, "STATUS_NOTIFY_ENUM_DIR" },
  { 0x0000010D, "STATUS_NO_QUOTAS_FOR_ACCOUNT" },
  { 0x0000010E, "STATUS_PRIMARY_TRANSPORT_CONNECT_FAILED" },
  { 0x00000110, "STATUS_PAGE_FAULT_TRANSITION" },
  { 0x00000111, "STATUS_PAGE_FAULT_DEMAND_ZERO" },
  { 0x00000112, "STATUS_PAGE_FAULT_COPY_ON_WRITE" },
  { 0x00000113, "STATUS_PAGE_FAULT_GUARD_PAGE" },
  { 0x00000114, "STATUS_PAGE_FAULT_PAGING_FILE" },
  { 0x00000115, "STATUS_CACHE_PAGE_LOCKED" },
  { 0x00000116, "STATUS_CRASH_DUMP" },
  { 0x00000117, "STATUS_BUFFER_ALL_ZEROS" },
  { 0x00000118, "STATUS_REPARSE_OBJECT" },
  { 0x40000000, "STATUS_OBJECT_NAME_EXISTS" },
  { 0x40000001, "STATUS_THREAD_WAS_SUSPENDED" },
  { 0x40000002, "STATUS_WORKING_SET_LIMIT_RANGE" },
  { 0x40000003, "STATUS_IMAGE_NOT_AT_BASE" },
  { 0x40000004, "STATUS_RXACT_STATE_CREATED" },
  { 0x40000005, "STATUS_SEGMENT_NOTIFICATION" },
  { 0x40000006, "STATUS_LOCAL_USER_SESSION_KEY" },
  { 0x40000007, "STATUS_BAD_CURRENT_DIRECTORY" },
  { 0x40000008, "STATUS_SERIAL_MORE_WRITES" },
  { 0x40000009, "STATUS_REGISTRY_RECOVERED" },
  { 0x4000000A, "STATUS_FT_READ_RECOVERY_FROM_BACKUP" },
  { 0x4000000B, "STATUS_FT_WRITE_RECOVERY" },
  { 0x4000000C, "STATUS_SERIAL_COUNTER_TIMEOUT" },
  { 0x4000000D, "STATUS_NULL_LM_PASSWORD" },
  { 0x4000000E, "STATUS_IMAGE_MACHINE_TYPE_MISMATCH" },
  { 0x4000000F, "STATUS_RECEIVE_PARTIAL" },
  { 0x40000010, "STATUS_RECEIVE_EXPEDITED" },
  { 0x40000011, "STATUS_RECEIVE_PARTIAL_EXPEDITED" },
  { 0x40000012, "STATUS_EVENT_DONE" },
  { 0x40000013, "STATUS_EVENT_PENDING" },
  { 0x40000014, "STATUS_CHECKING_FILE_SYSTEM" },
  { 0x40000015, "STATUS_FATAL_APP_EXIT" },
  { 0x40000016, "STATUS_PREDEFINED_HANDLE" },
  { 0x40000017, "STATUS_WAS_UNLOCKED" },
  { 0x40000018, "STATUS_SERVICE_NOTIFICATION" },
  { 0x40000019, "STATUS_WAS_LOCKED" },
  { 0x4000001A, "STATUS_LOG_HARD_ERROR" },
  { 0x4000001B, "STATUS_ALREADY_WIN32" },
  { 0x4000001C, "STATUS_WX86_UNSIMULATE" },
  { 0x4000001D, "STATUS_WX86_CONTINUE" },
  { 0x4000001E, "STATUS_WX86_SINGLE_STEP" },
  { 0x4000001F, "STATUS_WX86_BREAKPOINT" },
  { 0x40000020, "STATUS_WX86_EXCEPTION_CONTINUE" },
  { 0x40000021, "STATUS_WX86_EXCEPTION_LASTCHANCE" },
  { 0x40000022, "STATUS_WX86_EXCEPTION_CHAIN" },
  { 0x40000023, "STATUS_IMAGE_MACHINE_TYPE_MISMATCH_EXE" },
  { 0x40000024, "STATUS_NO_YIELD_PERFORMED" },
  { 0x40000025, "STATUS_TIMER_RESUME_IGNORED" },
  { 0x80000001, "STATUS_GUARD_PAGE_VIOLATION" },
  { 0x80000002, "STATUS_DATATYPE_MISALIGNMENT" },
  { 0x80000003, "STATUS_BREAKPOINT" },
  { 0x80000004, "STATUS_SINGLE_STEP" },
  { 0x80000005, "STATUS_BUFFER_OVERFLOW" },
  { 0x80000006, "STATUS_NO_MORE_FILES" },
  { 0x80000007, "STATUS_WAKE_SYSTEM_DEBUGGER" },
  { 0x8000000A, "STATUS_HANDLES_CLOSED" },
  { 0x8000000B, "STATUS_NO_INHERITANCE" },
  { 0x8000000C, "STATUS_GUID_SUBSTITUTION_MADE" },
  { 0x8000000D, "STATUS_PARTIAL_COPY" },
  { 0x8000000E, "STATUS_DEVICE_PAPER_EMPTY" },
  { 0x8000000F, "STATUS_DEVICE_POWERED_OFF" },
  { 0x80000010, "STATUS_DEVICE_OFF_LINE" },
  { 0x80000011, "STATUS_DEVICE_BUSY" },
  { 0x80000012, "STATUS_NO_MORE_EAS" },
  { 0x80000013, "STATUS_INVALID_EA_NAME" },
  { 0x80000014, "STATUS_EA_LIST_INCONSISTENT" },
  { 0x80000015, "STATUS_INVALID_EA_FLAG" },
  { 0x80000016, "STATUS_VERIFY_REQUIRED" },
  { 0x80000017, "STATUS_EXTRANEOUS_INFORMATION" },
  { 0x80000018, "STATUS_RXACT_COMMIT_NECESSARY" },
  { 0x8000001A, "STATUS_NO_MORE_ENTRIES" },
  { 0x8000001B, "STATUS_FILEMARK_DETECTED" },
  { 0x8000001C, "STATUS_MEDIA_CHANGED" },
  { 0x8000001D, "STATUS_BUS_RESET" },
  { 0x8000001E, "STATUS_END_OF_MEDIA" },
  { 0x8000001F, "STATUS_BEGINNING_OF_MEDIA" },
  { 0x80000020, "STATUS_MEDIA_CHECK" },
  { 0x80000021, "STATUS_SETMARK_DETECTED" },
  { 0x80000022, "STATUS_NO_DATA_DETECTED" },
  { 0x80000023, "STATUS_REDIRECTOR_HAS_OPEN_HANDLES" },
  { 0x80000024, "STATUS_SERVER_HAS_OPEN_HANDLES" },
  { 0x80000025, "STATUS_ALREADY_DISCONNECTED" },
  { 0x80000026, "STATUS_LONGJUMP" },
  { 0xC0000001, "STATUS_UNSUCCESSFUL" },
  { 0xC0000002, "STATUS_NOT_IMPLEMENTED" },
  { 0xC0000003, "STATUS_INVALID_INFO_CLASS" },
  { 0xC0000004, "STATUS_INFO_LENGTH_MISMATCH" },
  { 0xC0000005, "STATUS_ACCESS_VIOLATION" },
  { 0xC0000006, "STATUS_IN_PAGE_ERROR" },
  { 0xC0000007, "STATUS_PAGEFILE_QUOTA" },
  { 0xC0000008, "STATUS_INVALID_HANDLE" },
  { 0xC0000009, "STATUS_BAD_INITIAL_STACK" },
  { 0xC000000A, "STATUS_BAD_INITIAL_PC" },
  { 0xC000000B, "STATUS_INVALID_CID" },
  { 0xC000000C, "STATUS_TIMER_NOT_CANCELED" },
  { 0xC000000D, "STATUS_INVALID_PARAMETER" },
  { 0xC000000E, "STATUS_NO_SUCH_DEVICE" },
  { 0xC000000F, "STATUS_NO_SUCH_FILE" },
  { 0xC0000010, "STATUS_INVALID_DEVICE_REQUEST" },
  { 0xC0000011, "STATUS_END_OF_FILE" },
  { 0xC0000012, "STATUS_WRONG_VOLUME" },
  { 0xC0000013, "STATUS_NO_MEDIA_IN_DEVICE" },
  { 0xC0000014, "STATUS_UNRECOGNIZED_MEDIA" },
  { 0xC0000015, "STATUS_NONEXISTENT_SECTOR" },
  { 0xC0000016, "STATUS_MORE_PROCESSING_REQUIRED" },
  { 0xC0000017, "STATUS_NO_MEMORY" },
  { 0xC0000018, "STATUS_CONFLICTING_ADDRESSES" },
  { 0xC0000019, "STATUS_NOT_MAPPED_VIEW" },
  { 0xC000001A, "STATUS_UNABLE_TO_FREE_VM" },
  { 0xC000001B, "STATUS_UNABLE_TO_DELETE_SECTION" },
  { 0xC000001C, "STATUS_INVALID_SYSTEM_SERVICE" },
  { 0xC000001D, "STATUS_ILLEGAL_INSTRUCTION" },
  { 0xC000001E, "STATUS_INVALID_LOCK_SEQUENCE" },
  { 0xC000001F, "STATUS_INVALID_VIEW_SIZE" },
  { 0xC0000020, "STATUS_INVALID_FILE_FOR_SECTION" },
  { 0xC0000021, "STATUS_ALREADY_COMMITTED" },
  { 0xC0000022, "STATUS_ACCESS_DENIED" },
  { 0xC0000023, "STATUS_BUFFER_TOO_SMALL" },
  { 0xC0000024, "STATUS_OBJECT_TYPE_MISMATCH" },
  { 0xC0000025, "STATUS_NONCONTINUABLE_EXCEPTION" },
  { 0xC0000026, "STATUS_INVALID_DISPOSITION" },
  { 0xC0000027, "STATUS_UNWIND" },
  { 0xC0000028, "STATUS_BAD_STACK" },
  { 0xC0000029, "STATUS_INVALID_UNWIND_TARGET" },
  { 0xC000002A, "STATUS_NOT_LOCKED" },
  { 0xC000002B, "STATUS_PARITY_ERROR" },
  { 0xC000002C, "STATUS_UNABLE_TO_DECOMMIT_VM" },
  { 0xC000002D, "STATUS_NOT_COMMITTED" },
  { 0xC000002E, "STATUS_INVALID_PORT_ATTRIBUTES" },
  { 0xC000002F, "STATUS_PORT_MESSAGE_TOO_LONG" },
  { 0xC0000030, "STATUS_INVALID_PARAMETER_MIX" },
  { 0xC0000031, "STATUS_INVALID_QUOTA_LOWER" },
  { 0xC0000032, "STATUS_DISK_CORRUPT_ERROR" },
  { 0xC0000033, "STATUS_OBJECT_NAME_INVALID" },
  { 0xC0000034, "STATUS_OBJECT_NAME_NOT_FOUND" },
  { 0xC0000035, "STATUS_OBJECT_NAME_COLLISION" },
  { 0xC0000037, "STATUS_PORT_DISCONNECTED" },
  { 0xC0000038, "STATUS_DEVICE_ALREADY_ATTACHED" },
  { 0xC0000039, "STATUS_OBJECT_PATH_INVALID" },
  { 0xC000003A, "STATUS_OBJECT_PATH_NOT_FOUND" },
  { 0xC000003B, "STATUS_OBJECT_PATH_SYNTAX_BAD" },
  { 0xC000003C, "STATUS_DATA_OVERRUN" },
  { 0xC000003D, "STATUS_DATA_LATE_ERROR" },
  { 0xC000003E, "STATUS_DATA_ERROR" },
  { 0xC000003F, "STATUS_CRC_ERROR" },
  { 0xC0000040, "STATUS_SECTION_TOO_BIG" },
  { 0xC0000041, "STATUS_PORT_CONNECTION_REFUSED" },
  { 0xC0000042, "STATUS_INVALID_PORT_HANDLE" },
  { 0xC0000043, "STATUS_SHARING_VIOLATION" },
  { 0xC0000044, "STATUS_QUOTA_EXCEEDED" },
  { 0xC0000045, "STATUS_INVALID_PAGE_PROTECTION" },
  { 0xC0000046, "STATUS_MUTANT_NOT_OWNED" },
  { 0xC0000047, "STATUS_SEMAPHORE_LIMIT_EXCEEDED" },
  { 0xC0000048, "STATUS_PORT_ALREADY_SET" },
  { 0xC0000049, "STATUS_SECTION_NOT_IMAGE" },
  { 0xC000004A, "STATUS_SUSPEND_COUNT_EXCEEDED" },
  { 0xC000004B, "STATUS_THREAD_IS_TERMINATING" },
  { 0xC000004C, "STATUS_BAD_WORKING_SET_LIMIT" },
  { 0xC000004D, "STATUS_INCOMPATIBLE_FILE_MAP" },
  { 0xC000004E, "STATUS_SECTION_PROTECTION" },
  { 0xC000004F, "STATUS_EAS_NOT_SUPPORTED" },
  { 0xC0000050, "STATUS_EA_TOO_LARGE" },
  { 0xC0000051, "STATUS_NONEXISTENT_EA_ENTRY" },
  { 0xC0000052, "STATUS_NO_EAS_ON_FILE" },
  { 0xC0000053, "STATUS_EA_CORRUPT_ERROR" },
  { 0xC0000054, "STATUS_FILE_LOCK_CONFLICT" },
  { 0xC0000055, "STATUS_LOCK_NOT_GRANTED" },
  { 0xC0000056, "STATUS_DELETE_PENDING" },
  { 0xC0000057, "STATUS_CTL_FILE_NOT_SUPPORTED" },
  { 0xC0000058, "STATUS_UNKNOWN_REVISION" },
  { 0xC0000059, "STATUS_REVISION_MISMATCH" },
  { 0xC000005A, "STATUS_INVALID_OWNER" },
  { 0xC000005B, "STATUS_INVALID_PRIMARY_GROUP" },
  { 0xC000005C, "STATUS_NO_IMPERSONATION_TOKEN" },
  { 0xC000005D, "STATUS_CANT_DISABLE_MANDATORY" },
  { 0xC000005E, "STATUS_NO_LOGON_SERVERS" },
  { 0xC000005F, "STATUS_NO_SUCH_LOGON_SESSION" },
  { 0xC0000060, "STATUS_NO_SUCH_PRIVILEGE" },
  { 0xC0000061, "STATUS_PRIVILEGE_NOT_HELD" },
  { 0xC0000062, "STATUS_INVALID_ACCOUNT_NAME" },
  { 0xC0000063, "STATUS_USER_EXISTS" },
  { 0xC0000064, "STATUS_NO_SUCH_USER" },
  { 0xC0000065, "STATUS_GROUP_EXISTS" },
  { 0xC0000066, "STATUS_NO_SUCH_GROUP" },
  { 0xC0000067, "STATUS_MEMBER_IN_GROUP" },
  { 0xC0000068, "STATUS_MEMBER_NOT_IN_GROUP" },
  { 0xC0000069, "STATUS_LAST_ADMIN" },
  { 0xC000006A, "STATUS_WRONG_PASSWORD" },
  { 0xC000006B, "STATUS_ILL_FORMED_PASSWORD" },
  { 0xC000006C, "STATUS_PASSWORD_RESTRICTION" },
  { 0xC000006D, "STATUS_LOGON_FAILURE" },
  { 0xC000006E, "STATUS_ACCOUNT_RESTRICTION" },
  { 0xC000006F, "STATUS_INVALID_LOGON_HOURS" },
  { 0xC0000070, "STATUS_INVALID_WORKSTATION" },
  { 0xC0000071, "STATUS_PASSWORD_EXPIRED" },
  { 0xC0000072, "STATUS_ACCOUNT_DISABLED" },
  { 0xC0000073, "STATUS_NONE_MAPPED" },
  { 0xC0000074, "STATUS_TOO_MANY_LUIDS_REQUESTED" },
  { 0xC0000075, "STATUS_LUIDS_EXHAUSTED" },
  { 0xC0000076, "STATUS_INVALID_SUB_AUTHORITY" },
  { 0xC0000077, "STATUS_INVALID_ACL" },
  { 0xC0000078, "STATUS_INVALID_SID" },
  { 0xC0000079, "STATUS_INVALID_SECURITY_DESCR" },
  { 0xC000007A, "STATUS_PROCEDURE_NOT_FOUND" },
  { 0xC000007B, "STATUS_INVALID_IMAGE_FORMAT" },
  { 0xC000007C, "STATUS_NO_TOKEN" },
  { 0xC000007D, "STATUS_BAD_INHERITANCE_ACL" },
  { 0xC000007E, "STATUS_RANGE_NOT_LOCKED" },
  { 0xC000007F, "STATUS_DISK_FULL" },
  { 0xC0000080, "STATUS_SERVER_DISABLED" },
  { 0xC0000081, "STATUS_SERVER_NOT_DISABLED" },
  { 0xC0000082, "STATUS_TOO_MANY_GUIDS_REQUESTED" },
  { 0xC0000083, "STATUS_GUIDS_EXHAUSTED" },
  { 0xC0000084, "STATUS_INVALID_ID_AUTHORITY" },
  { 0xC0000085, "STATUS_AGENTS_EXHAUSTED" },
  { 0xC0000086, "STATUS_INVALID_VOLUME_LABEL" },
  { 0xC0000087, "STATUS_SECTION_NOT_EXTENDED" },
  { 0xC0000088, "STATUS_NOT_MAPPED_DATA" },
  { 0xC0000089, "STATUS_RESOURCE_DATA_NOT_FOUND" },
  { 0xC000008A, "STATUS_RESOURCE_TYPE_NOT_FOUND" },
  { 0xC000008B, "STATUS_RESOURCE_NAME_NOT_FOUND" },
  { 0xC000008C, "STATUS_ARRAY_BOUNDS_EXCEEDED" },
  { 0xC000008D, "STATUS_FLOAT_DENORMAL_OPERAND" },
  { 0xC000008E, "STATUS_FLOAT_DIVIDE_BY_ZERO" },
  { 0xC000008F, "STATUS_FLOAT_INEXACT_RESULT" },
  { 0xC0000090, "STATUS_FLOAT_INVALID_OPERATION" },
  { 0xC0000091, "STATUS_FLOAT_OVERFLOW" },
  { 0xC0000092, "STATUS_FLOAT_STACK_CHECK" },
  { 0xC0000093, "STATUS_FLOAT_UNDERFLOW" },
  { 0xC0000094, "STATUS_INTEGER_DIVIDE_BY_ZERO" },
  { 0xC0000095, "STATUS_INTEGER_OVERFLOW" },
  { 0xC0000096, "STATUS_PRIVILEGED_INSTRUCTION" },
  { 0xC0000097, "STATUS_TOO_MANY_PAGING_FILES" },
  { 0xC0000098, "STATUS_FILE_INVALID" },
  { 0xC0000099, "STATUS_ALLOTTED_SPACE_EXCEEDED" },
  { 0xC000009A, "STATUS_INSUFFICIENT_RESOURCES" },
  { 0xC000009B, "STATUS_DFS_EXIT_PATH_FOUND" },
  { 0xC000009C, "STATUS_DEVICE_DATA_ERROR" },
  { 0xC000009D, "STATUS_DEVICE_NOT_CONNECTED" },
  { 0xC000009E, "STATUS_DEVICE_POWER_FAILURE" },
  { 0xC000009F, "STATUS_FREE_VM_NOT_AT_BASE" },
  { 0xC00000A0, "STATUS_MEMORY_NOT_ALLOCATED" },
  { 0xC00000A1, "STATUS_WORKING_SET_QUOTA" },
  { 0xC00000A2, "STATUS_MEDIA_WRITE_PROTECTED" },
  { 0xC00000A3, "STATUS_DEVICE_NOT_READY" },
  { 0xC00000A4, "STATUS_INVALID_GROUP_ATTRIBUTES" },
  { 0xC00000A5, "STATUS_BAD_IMPERSONATION_LEVEL" },
  { 0xC00000A6, "STATUS_CANT_OPEN_ANONYMOUS" },
  { 0xC00000A7, "STATUS_BAD_VALIDATION_CLASS" },
  { 0xC00000A8, "STATUS_BAD_TOKEN_TYPE" },
  { 0xC00000A9, "STATUS_BAD_MASTER_BOOT_RECORD" },
  { 0xC00000AA, "STATUS_INSTRUCTION_MISALIGNMENT" },
  { 0xC00000AB, "STATUS_INSTANCE_NOT_AVAILABLE" },
  { 0xC00000AC, "STATUS_PIPE_NOT_AVAILABLE" },
  { 0xC00000AD, "STATUS_INVALID_PIPE_STATE" },
  { 0xC00000AE, "STATUS_PIPE_BUSY" },
  { 0xC00000AF, "STATUS_ILLEGAL_FUNCTION" },
  { 0xC00000B0, "STATUS_PIPE_DISCONNECTED" },
  { 0xC00000B1, "STATUS_PIPE_CLOSING" },
  { 0xC00000B2, "STATUS_PIPE_CONNECTED" },
  { 0xC00000B3, "STATUS_PIPE_LISTENING" },
  { 0xC00000B4, "STATUS_INVALID_READ_MODE" },
  { 0xC00000B5, "STATUS_IO_TIMEOUT" },
  { 0xC00000B6, "STATUS_FILE_FORCED_CLOSED" },
  { 0xC00000B7, "STATUS_PROFILING_NOT_STARTED" },
  { 0xC00000B8, "STATUS_PROFILING_NOT_STOPPED" },
  { 0xC00000B9, "STATUS_COULD_NOT_INTERPRET" },
  { 0xC00000BA, "STATUS_FILE_IS_A_DIRECTORY" },
  { 0xC00000BB, "STATUS_NOT_SUPPORTED" },
  { 0xC00000BC, "STATUS_REMOTE_NOT_LISTENING" },
  { 0xC00000BD, "STATUS_DUPLICATE_NAME" },
  { 0xC00000BE, "STATUS_BAD_NETWORK_PATH" },
  { 0xC00000BF, "STATUS_NETWORK_BUSY" },
  { 0xC00000C0, "STATUS_DEVICE_DOES_NOT_EXIST" },
  { 0xC00000C1, "STATUS_TOO_MANY_COMMANDS" },
  { 0xC00000C2, "STATUS_ADAPTER_HARDWARE_ERROR" },
  { 0xC00000C3, "STATUS_INVALID_NETWORK_RESPONSE" },
  { 0xC00000C4, "STATUS_UNEXPECTED_NETWORK_ERROR" },
  { 0xC00000C5, "STATUS_BAD_REMOTE_ADAPTER" },
  { 0xC00000C6, "STATUS_PRINT_QUEUE_FULL" },
  { 0xC00000C7, "STATUS_NO_SPOOL_SPACE" },
  { 0xC00000C8, "STATUS_PRINT_CANCELLED" },
  { 0xC00000C9, "STATUS_NETWORK_NAME_DELETED" },
  { 0xC00000CA, "STATUS_NETWORK_ACCESS_DENIED" },
  { 0xC00000CB, "STATUS_BAD_DEVICE_TYPE" },
  { 0xC00000CC, "STATUS_BAD_NETWORK_NAME" },
  { 0xC00000CD, "STATUS_TOO_MANY_NAMES" },
  { 0xC00000CE, "STATUS_TOO_MANY_SESSIONS" },
  { 0xC00000CF, "STATUS_SHARING_PAUSED" },
  { 0xC00000D0, "STATUS_REQUEST_NOT_ACCEPTED" },
  { 0xC00000D1, "STATUS_REDIRECTOR_PAUSED" },
  { 0xC00000D2, "STATUS_NET_WRITE_FAULT" },
  { 0xC00000D3, "STATUS_PROFILING_AT_LIMIT" },
  { 0xC00000D4, "STATUS_NOT_SAME_DEVICE" },
  { 0xC00000D5, "STATUS_FILE_RENAMED" },
  { 0xC00000D6, "STATUS_VIRTUAL_CIRCUIT_CLOSED" },
  { 0xC00000D7, "STATUS_NO_SECURITY_ON_OBJECT" },
  { 0xC00000D8, "STATUS_CANT_WAIT" },
  { 0xC00000D9, "STATUS_PIPE_EMPTY" },
  { 0xC00000DA, "STATUS_CANT_ACCESS_DOMAIN_INFO" },
  { 0xC00000DB, "STATUS_CANT_TERMINATE_SELF" },
  { 0xC00000DC, "STATUS_INVALID_SERVER_STATE" },
  { 0xC00000DD, "STATUS_INVALID_DOMAIN_STATE" },
  { 0xC00000DE, "STATUS_INVALID_DOMAIN_ROLE" },
  { 0xC00000DF, "STATUS_NO_SUCH_DOMAIN" },
  { 0xC00000E0, "STATUS_DOMAIN_EXISTS" },
  { 0xC00000E1, "STATUS_DOMAIN_LIMIT_EXCEEDED" },
  { 0xC00000E2, "STATUS_OPLOCK_NOT_GRANTED" },
  { 0xC00000E3, "STATUS_INVALID_OPLOCK_PROTOCOL" },
  { 0xC00000E4, "STATUS_INTERNAL_DB_CORRUPTION" },
  { 0xC00000E5, "STATUS_INTERNAL_ERROR" },
  { 0xC00000E6, "STATUS_GENERIC_NOT_MAPPED" },
  { 0xC00000E7, "STATUS_BAD_DESCRIPTOR_FORMAT" },
  { 0xC00000E8, "STATUS_INVALID_USER_BUFFER" },
  { 0xC00000E9, "STATUS_UNEXPECTED_IO_ERROR" },
  { 0xC00000EA, "STATUS_UNEXPECTED_MM_CREATE_ERR" },
  { 0xC00000EB, "STATUS_UNEXPECTED_MM_MAP_ERROR" },
  { 0xC00000EC, "STATUS_UNEXPECTED_MM_EXTEND_ERR" },
  { 0xC00000ED, "STATUS_NOT_LOGON_PROCESS" },
  { 0xC00000EE, "STATUS_LOGON_SESSION_EXISTS" },
  { 0xC00000EF, "STATUS_INVALID_PARAMETER_1" },
  { 0xC00000F0, "STATUS_INVALID_PARAMETER_2" },
  { 0xC00000F1, "STATUS_INVALID_PARAMETER_3" },
  { 0xC00000F2, "STATUS_INVALID_PARAMETER_4" },
  { 0xC00000F3, "STATUS_INVALID_PARAMETER_5" },
  { 0xC00000F4, "STATUS_INVALID_PARAMETER_6" },
  { 0xC00000F5, "STATUS_INVALID_PARAMETER_7" },
  { 0xC00000F6, "STATUS_INVALID_PARAMETER_8" },
  { 0xC00000F7, "STATUS_INVALID_PARAMETER_9" },
  { 0xC00000F8, "STATUS_INVALID_PARAMETER_10" },
  { 0xC00000F9, "STATUS_INVALID_PARAMETER_11" },
  { 0xC00000FA, "STATUS_INVALID_PARAMETER_12" },
  { 0xC00000FB, "STATUS_REDIRECTOR_NOT_STARTED" },
  { 0xC00000FC, "STATUS_REDIRECTOR_STARTED" },
  { 0xC00000FD, "STATUS_STACK_OVERFLOW" },
  { 0xC00000FE, "STATUS_NO_SUCH_PACKAGE" },
  { 0xC00000FF, "STATUS_BAD_FUNCTION_TABLE" },
  { 0xC0000100, "STATUS_VARIABLE_NOT_FOUND" },
  { 0xC0000101, "STATUS_DIRECTORY_NOT_EMPTY" },
  { 0xC0000102, "STATUS_FILE_CORRUPT_ERROR" },
  { 0xC0000103, "STATUS_NOT_A_DIRECTORY" },
  { 0xC0000104, "STATUS_BAD_LOGON_SESSION_STATE" },
  { 0xC0000105, "STATUS_LOGON_SESSION_COLLISION" },
  { 0xC0000106, "STATUS_NAME_TOO_LONG" },
  { 0xC0000107, "STATUS_FILES_OPEN" },
  { 0xC0000108, "STATUS_CONNECTION_IN_USE" },
  { 0xC0000109, "STATUS_MESSAGE_NOT_FOUND" },
  { 0xC000010A, "STATUS_PROCESS_IS_TERMINATING" },
  { 0xC000010B, "STATUS_INVALID_LOGON_TYPE" },
  { 0xC000010C, "STATUS_NO_GUID_TRANSLATION" },
  { 0xC000010D, "STATUS_CANNOT_IMPERSONATE" },
  { 0xC000010E, "STATUS_IMAGE_ALREADY_LOADED" },
  { 0xC000010F, "STATUS_ABIOS_NOT_PRESENT" },
  { 0xC0000110, "STATUS_ABIOS_LID_NOT_EXIST" },
  { 0xC0000111, "STATUS_ABIOS_LID_ALREADY_OWNED" },
  { 0xC0000112, "STATUS_ABIOS_NOT_LID_OWNER" },
  { 0xC0000113, "STATUS_ABIOS_INVALID_COMMAND" },
  { 0xC0000114, "STATUS_ABIOS_INVALID_LID" },
  { 0xC0000115, "STATUS_ABIOS_SELECTOR_NOT_AVAILABLE" },
  { 0xC0000116, "STATUS_ABIOS_INVALID_SELECTOR" },
  { 0xC0000117, "STATUS_NO_LDT" },
  { 0xC0000118, "STATUS_INVALID_LDT_SIZE" },
  { 0xC0000119, "STATUS_INVALID_LDT_OFFSET" },
  { 0xC000011A, "STATUS_INVALID_LDT_DESCRIPTOR" },
  { 0xC000011B, "STATUS_INVALID_IMAGE_NE_FORMAT" },
  { 0xC000011C, "STATUS_RXACT_INVALID_STATE" },
  { 0xC000011D, "STATUS_RXACT_COMMIT_FAILURE" },
  { 0xC000011E, "STATUS_MAPPED_FILE_SIZE_ZERO" },
  { 0xC000011F, "STATUS_TOO_MANY_OPENED_FILES" },
  { 0xC0000120, "STATUS_CANCELLED" },
  { 0xC0000121, "STATUS_CANNOT_DELETE" },
  { 0xC0000122, "STATUS_INVALID_COMPUTER_NAME" },
  { 0xC0000123, "STATUS_FILE_DELETED" },
  { 0xC0000124, "STATUS_SPECIAL_ACCOUNT" },
  { 0xC0000125, "STATUS_SPECIAL_GROUP" },
  { 0xC0000126, "STATUS_SPECIAL_USER" },
  { 0xC0000127, "STATUS_MEMBERS_PRIMARY_GROUP" },
  { 0xC0000128, "STATUS_FILE_CLOSED" },
  { 0xC0000129, "STATUS_TOO_MANY_THREADS" },
  { 0xC000012A, "STATUS_THREAD_NOT_IN_PROCESS" },
  { 0xC000012B, "STATUS_TOKEN_ALREADY_IN_USE" },
  { 0xC000012C, "STATUS_PAGEFILE_QUOTA_EXCEEDED" },
  { 0xC000012D, "STATUS_COMMITMENT_LIMIT" },
  { 0xC000012E, "STATUS_INVALID_IMAGE_LE_FORMAT" },
  { 0xC000012F, "STATUS_INVALID_IMAGE_NOT_MZ" },
  { 0xC0000130, "STATUS_INVALID_IMAGE_PROTECT" },
  { 0xC0000131, "STATUS_INVALID_IMAGE_WIN_16" },
  { 0xC0000132, "STATUS_LOGON_SERVER_CONFLICT" },
  { 0xC0000133, "STATUS_TIME_DIFFERENCE_AT_DC" },
  { 0xC0000134, "STATUS_SYNCHRONIZATION_REQUIRED" },
  { 0xC0000135, "STATUS_DLL_NOT_FOUND" },
  { 0xC0000136, "STATUS_OPEN_FAILED" },
  { 0xC0000137, "STATUS_IO_PRIVILEGE_FAILED" },
  { 0xC0000138, "STATUS_ORDINAL_NOT_FOUND" },
  { 0xC0000139, "STATUS_ENTRYPOINT_NOT_FOUND" },
  { 0xC000013A, "STATUS_CONTROL_C_EXIT" },
  { 0xC000013B, "STATUS_LOCAL_DISCONNECT" },
  { 0xC000013C, "STATUS_REMOTE_DISCONNECT" },
  { 0xC000013D, "STATUS_REMOTE_RESOURCES" },
  { 0xC000013E, "STATUS_LINK_FAILED" },
  { 0xC000013F, "STATUS_LINK_TIMEOUT" },
  { 0xC0000140, "STATUS_INVALID_CONNECTION" },
  { 0xC0000141, "STATUS_INVALID_ADDRESS" },
  { 0xC0000142, "STATUS_DLL_INIT_FAILED" },
  { 0xC0000143, "STATUS_MISSING_SYSTEMFILE" },
  { 0xC0000144, "STATUS_UNHANDLED_EXCEPTION" },
  { 0xC0000145, "STATUS_APP_INIT_FAILURE" },
  { 0xC0000146, "STATUS_PAGEFILE_CREATE_FAILED" },
  { 0xC0000147, "STATUS_NO_PAGEFILE" },
  { 0xC0000148, "STATUS_INVALID_LEVEL" },
  { 0xC0000149, "STATUS_WRONG_PASSWORD_CORE" },
  { 0xC000014A, "STATUS_ILLEGAL_FLOAT_CONTEXT" },
  { 0xC000014B, "STATUS_PIPE_BROKEN" },
  { 0xC000014C, "STATUS_REGISTRY_CORRUPT" },
  { 0xC000014D, "STATUS_REGISTRY_IO_FAILED" },
  { 0xC000014E, "STATUS_NO_EVENT_PAIR" },
  { 0xC000014F, "STATUS_UNRECOGNIZED_VOLUME" },
  { 0xC0000150, "STATUS_SERIAL_NO_DEVICE_INITED" },
  { 0xC0000151, "STATUS_NO_SUCH_ALIAS" },
  { 0xC0000152, "STATUS_MEMBER_NOT_IN_ALIAS" },
  { 0xC0000153, "STATUS_MEMBER_IN_ALIAS" },
  { 0xC0000154, "STATUS_ALIAS_EXISTS" },
  { 0xC0000155, "STATUS_LOGON_NOT_GRANTED" },
  { 0xC0000156, "STATUS_TOO_MANY_SECRETS" },
  { 0xC0000157, "STATUS_SECRET_TOO_LONG" },
  { 0xC0000158, "STATUS_INTERNAL_DB_ERROR" },
  { 0xC0000159, "STATUS_FULLSCREEN_MODE" },
  { 0xC000015A, "STATUS_TOO_MANY_CONTEXT_IDS" },
  { 0xC000015B, "STATUS_LOGON_TYPE_NOT_GRANTED" },
  { 0xC000015C, "STATUS_NOT_REGISTRY_FILE" },
  { 0xC000015D, "STATUS_NT_CROSS_ENCRYPTION_REQUIRED" },
  { 0xC000015E, "STATUS_DOMAIN_CTRLR_CONFIG_ERROR" },
  { 0xC000015F, "STATUS_FT_MISSING_MEMBER" },
  { 0xC0000160, "STATUS_ILL_FORMED_SERVICE_ENTRY" },
  { 0xC0000161, "STATUS_ILLEGAL_CHARACTER" },
  { 0xC0000162, "STATUS_UNMAPPABLE_CHARACTER" },
  { 0xC0000163, "STATUS_UNDEFINED_CHARACTER" },
  { 0xC0000164, "STATUS_FLOPPY_VOLUME" },
  { 0xC0000165, "STATUS_FLOPPY_ID_MARK_NOT_FOUND" },
  { 0xC0000166, "STATUS_FLOPPY_WRONG_CYLINDER" },
  { 0xC0000167, "STATUS_FLOPPY_UNKNOWN_ERROR" },
  { 0xC0000168, "STATUS_FLOPPY_BAD_REGISTERS" },
  { 0xC0000169, "STATUS_DISK_RECALIBRATE_FAILED" },
  { 0xC000016A, "STATUS_DISK_OPERATION_FAILED" },
  { 0xC000016B, "STATUS_DISK_RESET_FAILED" },
  { 0xC000016C, "STATUS_SHARED_IRQ_BUSY" },
  { 0xC000016D, "STATUS_FT_ORPHANING" },
  { 0xC000016E, "STATUS_BIOS_FAILED_TO_CONNECT_INTERRUPT" },
  { 0xC0000172, "STATUS_PARTITION_FAILURE" },
  { 0xC0000173, "STATUS_INVALID_BLOCK_LENGTH" },
  { 0xC0000174, "STATUS_DEVICE_NOT_PARTITIONED" },
  { 0xC0000175, "STATUS_UNABLE_TO_LOCK_MEDIA" },
  { 0xC0000176, "STATUS_UNABLE_TO_UNLOAD_MEDIA" },
  { 0xC0000177, "STATUS_EOM_OVERFLOW" },
  { 0xC0000178, "STATUS_NO_MEDIA" },
  { 0xC000017A, "STATUS_NO_SUCH_MEMBER" },
  { 0xC000017B, "STATUS_INVALID_MEMBER" },
  { 0xC000017C, "STATUS_KEY_DELETED" },
  { 0xC000017D, "STATUS_NO_LOG_SPACE" },
  { 0xC000017E, "STATUS_TOO_MANY_SIDS" },
  { 0xC000017F, "STATUS_LM_CROSS_ENCRYPTION_REQUIRED" },
  { 0xC0000180, "STATUS_KEY_HAS_CHILDREN" },
  { 0xC0000181, "STATUS_CHILD_MUST_BE_VOLATILE" },
  { 0xC0000182, "STATUS_DEVICE_CONFIGURATION_ERROR" },
  { 0xC0000183, "STATUS_DRIVER_INTERNAL_ERROR" },
  { 0xC0000184, "STATUS_INVALID_DEVICE_STATE" },
  { 0xC0000185, "STATUS_IO_DEVICE_ERROR" },
  { 0xC0000186, "STATUS_DEVICE_PROTOCOL_ERROR" },
  { 0xC0000187, "STATUS_BACKUP_CONTROLLER" },
  { 0xC0000188, "STATUS_LOG_FILE_FULL" },
  { 0xC0000189, "STATUS_TOO_LATE" },
  { 0xC000018A, "STATUS_NO_TRUST_LSA_SECRET" },
  { 0xC000018B, "STATUS_NO_TRUST_SAM_ACCOUNT" },
  { 0xC000018C, "STATUS_TRUSTED_DOMAIN_FAILURE" },
  { 0xC000018D, "STATUS_TRUSTED_RELATIONSHIP_FAILURE" },
  { 0xC000018E, "STATUS_EVENTLOG_FILE_CORRUPT" },
  { 0xC000018F, "STATUS_EVENTLOG_CANT_START" },
  { 0xC0000190, "STATUS_TRUST_FAILURE" },
  { 0xC0000191, "STATUS_MUTANT_LIMIT_EXCEEDED" },
  { 0xC0000192, "STATUS_NETLOGON_NOT_STARTED" },
  { 0xC0000193, "STATUS_ACCOUNT_EXPIRED" },
  { 0xC0000194, "STATUS_POSSIBLE_DEADLOCK" },
  { 0xC0000195, "STATUS_NETWORK_CREDENTIAL_CONFLICT" },
  { 0xC0000196, "STATUS_REMOTE_SESSION_LIMIT" },
  { 0xC0000197, "STATUS_EVENTLOG_FILE_CHANGED" },
  { 0xC0000198, "STATUS_NOLOGON_INTERDOMAIN_TRUST_ACCOUNT" },
  { 0xC0000199, "STATUS_NOLOGON_WORKSTATION_TRUST_ACCOUNT" },
  { 0xC000019A, "STATUS_NOLOGON_SERVER_TRUST_ACCOUNT" },
  { 0xC000019B, "STATUS_DOMAIN_TRUST_INCONSISTENT" },
  { 0xC000019C, "STATUS_FS_DRIVER_REQUIRED" },
  { 0xC0000202, "STATUS_NO_USER_SESSION_KEY" },
  { 0xC0000203, "STATUS_USER_SESSION_DELETED" },
  { 0xC0000204, "STATUS_RESOURCE_LANG_NOT_FOUND" },
  { 0xC0000205, "STATUS_INSUFF_SERVER_RESOURCES" },
  { 0xC0000206, "STATUS_INVALID_BUFFER_SIZE" },
  { 0xC0000207, "STATUS_INVALID_ADDRESS_COMPONENT" },
  { 0xC0000208, "STATUS_INVALID_ADDRESS_WILDCARD" },
  { 0xC0000209, "STATUS_TOO_MANY_ADDRESSES" },
  { 0xC000020A, "STATUS_ADDRESS_ALREADY_EXISTS" },
  { 0xC000020B, "STATUS_ADDRESS_CLOSED" },
  { 0xC000020C, "STATUS_CONNECTION_DISCONNECTED" },
  { 0xC000020D, "STATUS_CONNECTION_RESET" },
  { 0xC000020E, "STATUS_TOO_MANY_NODES" },
  { 0xC000020F, "STATUS_TRANSACTION_ABORTED" },
  { 0xC0000210, "STATUS_TRANSACTION_TIMED_OUT" },
  { 0xC0000211, "STATUS_TRANSACTION_NO_RELEASE" },
  { 0xC0000212, "STATUS_TRANSACTION_NO_MATCH" },
  { 0xC0000213, "STATUS_TRANSACTION_RESPONDED" },
  { 0xC0000214, "STATUS_TRANSACTION_INVALID_ID" },
  { 0xC0000215, "STATUS_TRANSACTION_INVALID_TYPE" },
  { 0xC0000216, "STATUS_NOT_SERVER_SESSION" },
  { 0xC0000217, "STATUS_NOT_CLIENT_SESSION" },
  { 0xC0000218, "STATUS_CANNOT_LOAD_REGISTRY_FILE" },
  { 0xC0000219, "STATUS_DEBUG_ATTACH_FAILED" },
  { 0xC000021A, "STATUS_SYSTEM_PROCESS_TERMINATED" },
  { 0xC000021B, "STATUS_DATA_NOT_ACCEPTED" },
  { 0xC000021C, "STATUS_NO_BROWSER_SERVERS_FOUND" },
  { 0xC000021D, "STATUS_VDM_HARD_ERROR" },
  { 0xC000021E, "STATUS_DRIVER_CANCEL_TIMEOUT" },
  { 0xC000021F, "STATUS_REPLY_MESSAGE_MISMATCH" },
  { 0xC0000220, "STATUS_MAPPED_ALIGNMENT" },
  { 0xC0000221, "STATUS_IMAGE_CHECKSUM_MISMATCH" },
  { 0xC0000222, "STATUS_LOST_WRITEBEHIND_DATA" },
  { 0xC0000223, "STATUS_CLIENT_SERVER_PARAMETERS_INVALID" },
  { 0xC0000224, "STATUS_PASSWORD_MUST_CHANGE" },
  { 0xC0000225, "STATUS_NOT_FOUND" },
  { 0xC0000226, "STATUS_NOT_TINY_STREAM" },
  { 0xC0000227, "STATUS_RECOVERY_FAILURE" },
  { 0xC0000228, "STATUS_STACK_OVERFLOW_READ" },
  { 0xC0000229, "STATUS_FAIL_CHECK" },
  { 0xC000022A, "STATUS_DUPLICATE_OBJECTID" },
  { 0xC000022B, "STATUS_OBJECTID_EXISTS" },
  { 0xC000022C, "STATUS_CONVERT_TO_LARGE" },
  { 0xC000022D, "STATUS_RETRY" },
  { 0xC000022E, "STATUS_FOUND_OUT_OF_SCOPE" },
  { 0xC000022F, "STATUS_ALLOCATE_BUCKET" },
  { 0xC0000230, "STATUS_PROPSET_NOT_FOUND" },
  { 0xC0000231, "STATUS_MARSHALL_OVERFLOW" },
  { 0xC0000232, "STATUS_INVALID_VARIANT" },
  { 0xC0000233, "STATUS_DOMAIN_CONTROLLER_NOT_FOUND" },
  { 0xC0000234, "STATUS_ACCOUNT_LOCKED_OUT" },
  { 0xC0000235, "STATUS_HANDLE_NOT_CLOSABLE" },
  { 0xC0000236, "STATUS_CONNECTION_REFUSED" },
  { 0xC0000237, "STATUS_GRACEFUL_DISCONNECT" },
  { 0xC0000238, "STATUS_ADDRESS_ALREADY_ASSOCIATED" },
  { 0xC0000239, "STATUS_ADDRESS_NOT_ASSOCIATED" },
  { 0xC000023A, "STATUS_CONNECTION_INVALID" },
  { 0xC000023B, "STATUS_CONNECTION_ACTIVE" },
  { 0xC000023C, "STATUS_NETWORK_UNREACHABLE" },
  { 0xC000023D, "STATUS_HOST_UNREACHABLE" },
  { 0xC000023E, "STATUS_PROTOCOL_UNREACHABLE" },
  { 0xC000023F, "STATUS_PORT_UNREACHABLE" },
  { 0xC0000240, "STATUS_REQUEST_ABORTED" },
  { 0xC0000241, "STATUS_CONNECTION_ABORTED" },
  { 0xC0000242, "STATUS_BAD_COMPRESSION_BUFFER" },
  { 0xC0000243, "STATUS_USER_MAPPED_FILE" },
  { 0xC0000244, "STATUS_AUDIT_FAILED" },
  { 0xC0000245, "STATUS_TIMER_RESOLUTION_NOT_SET" },
  { 0xC0000246, "STATUS_CONNECTION_COUNT_LIMIT" },
  { 0xC0000247, "STATUS_LOGIN_TIME_RESTRICTION" },
  { 0xC0000248, "STATUS_LOGIN_WKSTA_RESTRICTION" },
  { 0xC0000249, "STATUS_IMAGE_MP_UP_MISMATCH" },
  { 0xC0000250, "STATUS_INSUFFICIENT_LOGON_INFO" },
  { 0xC0000251, "STATUS_BAD_DLL_ENTRYPOINT" },
  { 0xC0000252, "STATUS_BAD_SERVICE_ENTRYPOINT" },
  { 0xC0000253, "STATUS_LPC_REPLY_LOST" },
  { 0xC0000254, "STATUS_IP_ADDRESS_CONFLICT1" },
  { 0xC0000255, "STATUS_IP_ADDRESS_CONFLICT2" },
  { 0xC0000256, "STATUS_REGISTRY_QUOTA_LIMIT" },
  { 0xC0000257, "STATUS_PATH_NOT_COVERED" },
  { 0xC0000258, "STATUS_NO_CALLBACK_ACTIVE" },
  { 0xC0000259, "STATUS_LICENSE_QUOTA_EXCEEDED" },
  { 0xC000025A, "STATUS_PWD_TOO_SHORT" },
  { 0xC000025B, "STATUS_PWD_TOO_RECENT" },
  { 0xC000025C, "STATUS_PWD_HISTORY_CONFLICT" },
  { 0xC000025E, "STATUS_PLUGPLAY_NO_DEVICE" },
  { 0xC000025F, "STATUS_UNSUPPORTED_COMPRESSION" },
  { 0xC0000260, "STATUS_INVALID_HW_PROFILE" },
  { 0xC0000261, "STATUS_INVALID_PLUGPLAY_DEVICE_PATH" },
  { 0xC0000262, "STATUS_DRIVER_ORDINAL_NOT_FOUND" },
  { 0xC0000263, "STATUS_DRIVER_ENTRYPOINT_NOT_FOUND" },
  { 0xC0000264, "STATUS_RESOURCE_NOT_OWNED" },
  { 0xC0000265, "STATUS_TOO_MANY_LINKS" },
  { 0xC0000266, "STATUS_QUOTA_LIST_INCONSISTENT" },
  { 0xC0000267, "STATUS_FILE_IS_OFFLINE" },
  { 0xC0000268, "STATUS_EVALUATION_EXPIRATION" },
  { 0xC0000269, "STATUS_ILLEGAL_DLL_RELOCATION" },
  { 0xC000026A, "STATUS_LICENSE_VIOLATION" },
  { 0xC000026B, "STATUS_DLL_INIT_FAILED_LOGOFF" },
  { 0xC000026C, "STATUS_DRIVER_UNABLE_TO_LOAD" },
  { 0xC000026D, "STATUS_DFS_UNAVAILABLE" },
  { 0xC000026E, "STATUS_VOLUME_DISMOUNTED" },
  { 0xC000026F, "STATUS_WX86_INTERNAL_ERROR" },
  { 0xC0000270, "STATUS_WX86_FLOAT_STACK_CHECK" },
  { 0xC0009898, "STATUS_WOW_ASSERTION" },
  { 0xC0020001, "RPC_NT_INVALID_STRING_BINDING" },
  { 0xC0020002, "RPC_NT_WRONG_KIND_OF_BINDING" },
  { 0xC0020003, "RPC_NT_INVALID_BINDING" },
  { 0xC0020004, "RPC_NT_PROTSEQ_NOT_SUPPORTED" },
  { 0xC0020005, "RPC_NT_INVALID_RPC_PROTSEQ" },
  { 0xC0020006, "RPC_NT_INVALID_STRING_UUID" },
  { 0xC0020007, "RPC_NT_INVALID_ENDPOINT_FORMAT" },
  { 0xC0020008, "RPC_NT_INVALID_NET_ADDR" },
  { 0xC0020009, "RPC_NT_NO_ENDPOINT_FOUND" },
  { 0xC002000A, "RPC_NT_INVALID_TIMEOUT" },
  { 0xC002000B, "RPC_NT_OBJECT_NOT_FOUND" },
  { 0xC002000C, "RPC_NT_ALREADY_REGISTERED" },
  { 0xC002000D, "RPC_NT_TYPE_ALREADY_REGISTERED" },
  { 0xC002000E, "RPC_NT_ALREADY_LISTENING" },
  { 0xC002000F, "RPC_NT_NO_PROTSEQS_REGISTERED" },
  { 0xC0020010, "RPC_NT_NOT_LISTENING" },
  { 0xC0020011, "RPC_NT_UNKNOWN_MGR_TYPE" },
  { 0xC0020012, "RPC_NT_UNKNOWN_IF" },
  { 0xC0020013, "RPC_NT_NO_BINDINGS" },
  { 0xC0020014, "RPC_NT_NO_PROTSEQS" },
  { 0xC0020015, "RPC_NT_CANT_CREATE_ENDPOINT" },
  { 0xC0020016, "RPC_NT_OUT_OF_RESOURCES" },
  { 0xC0020017, "RPC_NT_SERVER_UNAVAILABLE" },
  { 0xC0020018, "RPC_NT_SERVER_TOO_BUSY" },
  { 0xC0020019, "RPC_NT_INVALID_NETWORK_OPTIONS" },
  { 0xC002001A, "RPC_NT_NO_CALL_ACTIVE" },
  { 0xC002001B, "RPC_NT_CALL_FAILED" },
  { 0xC002001C, "RPC_NT_CALL_FAILED_DNE" },
  { 0xC002001D, "RPC_NT_PROTOCOL_ERROR" },
  { 0xC002001F, "RPC_NT_UNSUPPORTED_TRANS_SYN" },
  { 0xC0020021, "RPC_NT_UNSUPPORTED_TYPE" },
  { 0xC0020022, "RPC_NT_INVALID_TAG" },
  { 0xC0020023, "RPC_NT_INVALID_BOUND" },
  { 0xC0020024, "RPC_NT_NO_ENTRY_NAME" },
  { 0xC0020025, "RPC_NT_INVALID_NAME_SYNTAX" },
  { 0xC0020026, "RPC_NT_UNSUPPORTED_NAME_SYNTAX" },
  { 0xC0020028, "RPC_NT_UUID_NO_ADDRESS" },
  { 0xC0020029, "RPC_NT_DUPLICATE_ENDPOINT" },
  { 0xC002002A, "RPC_NT_UNKNOWN_AUTHN_TYPE" },
  { 0xC002002B, "RPC_NT_MAX_CALLS_TOO_SMALL" },
  { 0xC002002C, "RPC_NT_STRING_TOO_LONG" },
  { 0xC002002D, "RPC_NT_PROTSEQ_NOT_FOUND" },
  { 0xC002002E, "RPC_NT_PROCNUM_OUT_OF_RANGE" },
  { 0xC002002F, "RPC_NT_BINDING_HAS_NO_AUTH" },
  { 0xC0020030, "RPC_NT_UNKNOWN_AUTHN_SERVICE" },
  { 0xC0020031, "RPC_NT_UNKNOWN_AUTHN_LEVEL" },
  { 0xC0020032, "RPC_NT_INVALID_AUTH_IDENTITY" },
  { 0xC0020033, "RPC_NT_UNKNOWN_AUTHZ_SERVICE" },
  { 0xC0020034, "EPT_NT_INVALID_ENTRY" },
  { 0xC0020035, "EPT_NT_CANT_PERFORM_OP" },
  { 0xC0020036, "EPT_NT_NOT_REGISTERED" },
  { 0xC0020037, "RPC_NT_NOTHING_TO_EXPORT" },
  { 0xC0020038, "RPC_NT_INCOMPLETE_NAME" },
  { 0xC0020039, "RPC_NT_INVALID_VERS_OPTION" },
  { 0xC002003A, "RPC_NT_NO_MORE_MEMBERS" },
  { 0xC002003B, "RPC_NT_NOT_ALL_OBJS_UNEXPORTED" },
  { 0xC002003C, "RPC_NT_INTERFACE_NOT_FOUND" },
  { 0xC002003D, "RPC_NT_ENTRY_ALREADY_EXISTS" },
  { 0xC002003E, "RPC_NT_ENTRY_NOT_FOUND" },
  { 0xC002003F, "RPC_NT_NAME_SERVICE_UNAVAILABLE" },
  { 0xC0020040, "RPC_NT_INVALID_NAF_ID" },
  { 0xC0020041, "RPC_NT_CANNOT_SUPPORT" },
  { 0xC0020042, "RPC_NT_NO_CONTEXT_AVAILABLE" },
  { 0xC0020043, "RPC_NT_INTERNAL_ERROR" },
  { 0xC0020044, "RPC_NT_ZERO_DIVIDE" },
  { 0xC0020045, "RPC_NT_ADDRESS_ERROR" },
  { 0xC0020046, "RPC_NT_FP_DIV_ZERO" },
  { 0xC0020047, "RPC_NT_FP_UNDERFLOW" },
  { 0xC0020048, "RPC_NT_FP_OVERFLOW" },
  { 0xC0030001, "RPC_NT_NO_MORE_ENTRIES" },
  { 0xC0030002, "RPC_NT_SS_CHAR_TRANS_OPEN_FAIL" },
  { 0xC0030003, "RPC_NT_SS_CHAR_TRANS_SHORT_FILE" },
  { 0xC0030004, "RPC_NT_SS_IN_NULL_CONTEXT" },
  { 0xC0030005, "RPC_NT_SS_CONTEXT_MISMATCH" },
  { 0xC0030006, "RPC_NT_SS_CONTEXT_DAMAGED" },
  { 0xC0030007, "RPC_NT_SS_HANDLES_MISMATCH" },
  { 0xC0030008, "RPC_NT_SS_CANNOT_GET_CALL_HANDLE" },
  { 0xC0030009, "RPC_NT_NULL_REF_POINTER" },
  { 0xC003000A, "RPC_NT_ENUM_VALUE_OUT_OF_RANGE" },
  { 0xC003000B, "RPC_NT_BYTE_COUNT_TOO_SMALL" },
  { 0xC003000C, "RPC_NT_BAD_STUB_DATA" },
  { 0xC0020049, "RPC_NT_CALL_IN_PROGRESS" },
  { 0xC002004A, "RPC_NT_NO_MORE_BINDINGS" },
  { 0xC002004B, "RPC_NT_GROUP_MEMBER_NOT_FOUND" },
  { 0xC002004C, "EPT_NT_CANT_CREATE" },
  { 0xC002004D, "RPC_NT_INVALID_OBJECT" },
  { 0xC002004F, "RPC_NT_NO_INTERFACES" },
  { 0xC0020050, "RPC_NT_CALL_CANCELLED" },
  { 0xC0020051, "RPC_NT_BINDING_INCOMPLETE" },
  { 0xC0020052, "RPC_NT_COMM_FAILURE" },
  { 0xC0020053, "RPC_NT_UNSUPPORTED_AUTHN_LEVEL" },
  { 0xC0020054, "RPC_NT_NO_PRINC_NAME" },
  { 0xC0020055, "RPC_NT_NOT_RPC_ERROR" },
  { 0x40020056, "RPC_NT_UUID_LOCAL_ONLY" },
  { 0xC0020057, "RPC_NT_SEC_PKG_ERROR" },
  { 0xC0020058, "RPC_NT_NOT_CANCELLED" },
  { 0xC0030059, "RPC_NT_INVALID_ES_ACTION" },
  { 0xC003005A, "RPC_NT_WRONG_ES_VERSION" },
  { 0xC003005B, "RPC_NT_WRONG_STUB_VERSION" },
  { 0xC003005C, "RPC_NT_INVALID_PIPE_OBJECT" },
  { 0xC003005D, "RPC_NT_INVALID_PIPE_OPERATION" },
  { 0xC003005E, "RPC_NT_WRONG_PIPE_VERSION" },
  { 0x400200AF, "RPC_NT_SEND_INCOMPLETE" },
  { 0,          NULL }
};



static const true_false_string tfs_smb_flags_lock = {
	"Lock&Read, Write&Unlock are supported",
	"Lock&Read, Write&Unlock are not supported"
};
static const true_false_string tfs_smb_flags_receive_buffer = {
	"Receive buffer has been posted",
	"Receive buffer has not been posted"
};
static const true_false_string tfs_smb_flags_caseless = {
	"Path names are caseless",
	"Path names are case sensitive"
};
static const true_false_string tfs_smb_flags_canon = {
	"Pathnames are canonicalized",
	"Pathnames are not canonicalized"
};
static const true_false_string tfs_smb_flags_oplock = {
	"OpLock requested/granted",
	"OpLock not requested/granted"
};
static const true_false_string tfs_smb_flags_notify = {
	"Notify client on all modifications",
	"Notify client only on open"
};
static const true_false_string tfs_smb_flags_response = {
	"Message is a response to the client/redirector",
	"Message is a request to the server"
};

static int
dissect_smb_flags(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint8 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_guint8(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 1,
			"Flags: 0x%02x", mask);
		tree = proto_item_add_subtree(item, ett_smb_flags);
 	}
	proto_tree_add_boolean(tree, hf_smb_flags_response,
		tvb, offset, 1, mask);
	proto_tree_add_boolean(tree, hf_smb_flags_notify,
		tvb, offset, 1, mask);
	proto_tree_add_boolean(tree, hf_smb_flags_oplock,
		tvb, offset, 1, mask);
	proto_tree_add_boolean(tree, hf_smb_flags_canon,
		tvb, offset, 1, mask);
	proto_tree_add_boolean(tree, hf_smb_flags_caseless,
		tvb, offset, 1, mask);
	proto_tree_add_boolean(tree, hf_smb_flags_receive_buffer,
		tvb, offset, 1, mask);
	proto_tree_add_boolean(tree, hf_smb_flags_lock,
		tvb, offset, 1, mask);
	offset += 1;
	return offset;
}


 
static const true_false_string tfs_smb_flags2_long_names_allowed = {
	"Long file names are allowed in the response",
	"Long file names are not allowed in the response"
};
static const true_false_string tfs_smb_flags2_ea = {
	"Extended attributes are supported",
	"Extended attributes are not supported"
};
static const true_false_string tfs_smb_flags2_sec_sig = {
	"Security signatures are supported",
	"Security signatures are not supported"
};
static const true_false_string tfs_smb_flags2_long_names_used = {
	"Path names in request are long file names",
	"Path names in request are not long file names"
};
static const true_false_string tfs_smb_flags2_esn = {
	"Extended security negotiation is supported",
	"Extended security negotiation is not supported"
};
static const true_false_string tfs_smb_flags2_dfs = {
	"Resolve pathnames with DFS",
	"Don't resolve pathnames with DFS"
};
static const true_false_string tfs_smb_flags2_roe = {
	"Permit reads if execute-only",
	"Don't permit reads if execute-only"
};
static const true_false_string tfs_smb_flags2_nt_error = {
	"Error codes are NT error codes",
	"Error codes are DOS error codes"
};
static const true_false_string tfs_smb_flags2_string = {
	"Strings are Unicode",
	"Strings are ASCII"
};
static int
dissect_smb_flags2(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree, int offset)
{
	guint16 mask;
	proto_item *item = NULL;
	proto_tree *tree = NULL;

	mask = tvb_get_letohs(tvb, offset);

	if(parent_tree){
		item = proto_tree_add_text(parent_tree, tvb, offset, 2,
			"Flags2: 0x%04x", mask);
		tree = proto_item_add_subtree(item, ett_smb_flags2);
	}

	proto_tree_add_boolean(tree, hf_smb_flags2_string,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_flags2_nt_error,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_flags2_roe,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_flags2_dfs,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_flags2_esn,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_flags2_long_names_used,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_flags2_sec_sig,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_flags2_ea,
		tvb, offset, 2, mask);
	proto_tree_add_boolean(tree, hf_smb_flags2_long_names_allowed,
		tvb, offset, 2, mask);

	offset += 2;
	return offset;
}



#define SMB_FLAGS_DIRN 0x80


static gboolean
dissect_smb(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree)
{
	int offset = 0;
	proto_item *item = NULL, *hitem = NULL;
	proto_tree *tree = NULL, *htree = NULL;
	guint8          flags;
	guint16         flags2;
	smb_info_t 	si;
	smb_info_t	*sip;
	proto_item *cmd_item = NULL;
	proto_tree *cmd_tree = NULL;
	int (*dissector)(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree, int offset, proto_tree *smb_tree);


	/* must check that this really is a smb packet */
	if (!tvb_bytes_exist(tvb, 0, 4))
	  return FALSE;
	if( (tvb_get_guint8(tvb, 0) != 0xff)
	 || (tvb_get_guint8(tvb, 1) != 'S')
	 || (tvb_get_guint8(tvb, 2) != 'M')
	 || (tvb_get_guint8(tvb, 3) != 'B') ){
		return FALSE;
	}
	 
	if (check_col(pinfo->fd, COL_PROTOCOL)){
		col_set_str(pinfo->fd, COL_PROTOCOL, "SMB");
	}
	if (check_col(pinfo->fd, COL_INFO)){
		col_clear(pinfo->fd, COL_INFO);
	}

	/* start off using the local variable, we will allocate a new one if we
	   need to*/
	sip = &si;
	sip->frame_req = 0;
	sip->frame_res = 0;
	sip->mid = tvb_get_letohs(tvb, offset+30);
	sip->uid = tvb_get_letohs(tvb, offset+28);
	sip->pid = tvb_get_letohs(tvb, offset+26);
	sip->tid = tvb_get_letohs(tvb, offset+24);
	flags2 = tvb_get_letohs(tvb, offset+10);
	if(flags2 & 0x8000){
		sip->unicode = TRUE; /* Mark them as Unicode */
	} else {
		sip->unicode = FALSE;
	}
	flags = tvb_get_guint8(tvb, offset+9);
	sip->request = !(flags&SMB_FLAGS_DIRN);
	sip->cmd = tvb_get_guint8(tvb, offset+4);
	sip->ddisp = 0;

	if (parent_tree) {
		item = proto_tree_add_item(parent_tree, proto_smb, tvb, offset, 
			tvb_length_remaining(tvb, 0), FALSE);
		tree = proto_item_add_subtree(item, ett_smb);

		hitem = proto_tree_add_text(tree, tvb, offset, 32, 
			"SMB Header");

		htree = proto_item_add_subtree(hitem, ett_smb_hdr);
	}

	proto_tree_add_text(htree, tvb, offset, 4, "Server Component: SMB");
	offset += 4;  /* Skip the marker */


	/* store smb_info structure so we can retreive it from the reply */
	if(sip->request){
		sip->src = &pinfo->src;
		sip->dst = &pinfo->dst;
		if(!pinfo->fd->flags.visited){
			if( (sip->mid==0)
			&&  (sip->uid==0)
			&&  (sip->pid==0)
			&&  (sip->tid==0) ){
				/* this is a broadcast SMB packet,
				   there will not be a reply.
				   We dont need to do anything */
				sip->unidir=FALSE;
			} else {
				sip->unidir=TRUE;
				sip = g_mem_chunk_alloc(smb_info_chunk);
				memcpy(sip, &si, sizeof(smb_info_t));
				sip->frame_req = pinfo->fd->num;
				sip->frame_res = 0;
				sip->src=g_malloc(sizeof(address));
				COPY_ADDRESS(sip->src, &pinfo->src);
				sip->dst=g_malloc(sizeof(address));
				COPY_ADDRESS(sip->dst, &pinfo->dst);
				g_hash_table_insert(smb_info_table, sip, sip);
			}
		}
	} else {
		sip->src = &pinfo->dst;
		sip->dst = &pinfo->src;
	}
	sip = g_hash_table_lookup(smb_info_table, sip);
	if(!sip){
		sip = &si;
	}
	/* need to redo these ones, might have changed if we got a new sip */
	sip->request = !(flags&SMB_FLAGS_DIRN);
	if(flags2 & 0x8000){
		sip->unicode = TRUE; /* Mark them as Unicode */
	} else {
		sip->unicode = FALSE;
	}

	if(sip->request){
		if(sip->frame_res){
			proto_tree_add_uint(htree, hf_smb_response_in, tvb, 0, 0, sip->frame_res);
		}
	} else {
		if(!pinfo->fd->flags.visited){
			sip->frame_res=pinfo->fd->num;
		}
		if(sip->frame_req){
			proto_tree_add_uint(htree, hf_smb_response_to, tvb, 0, 0, sip->frame_req);
		}
	}

	/* smb command */
	proto_tree_add_uint_format(htree, hf_smb_cmd, tvb, offset, 1, sip->cmd, "SMB Command: %s (0x%02x)", decode_smb_name(sip->cmd), sip->cmd);
	offset += 1;

	if(flags2 & 0x4000){
		/* handle NT 32 bit error code */
		proto_tree_add_item(htree, hf_smb_nt_status, tvb, offset, 4,
			TRUE);
		offset += 4;
	} else {
		guint8 errclass;
		guint16 errcode;

		/* handle DOS error code & class */
		errclass = tvb_get_guint8(tvb, offset);
		proto_tree_add_uint(htree, hf_smb_error_class, tvb, offset, 1,
			errclass);
		offset += 1;

		/* reserved byte */
		proto_tree_add_item(htree, hf_smb_reserved, tvb, offset, 1, TRUE);
		offset += 1;

		/* error code */
		/* XXX - the type of this field depends on the value of
		 * "errcls", so there is isn't a single value_string array
		 * fo it, so there can't be a single field for it.
		 */
		errcode = tvb_get_letohs(tvb, offset);
		proto_tree_add_uint_format(htree, hf_smb_error_code, tvb,
			offset, 2, errcode, "Error Code: %s",
			decode_smb_error(errclass, errcode));
		offset += 2;
	}

	/* flags */
	offset = dissect_smb_flags(tvb, pinfo, htree, offset);

	/* flags2 */
	offset = dissect_smb_flags2(tvb, pinfo, htree, offset);

	/*
	 * The document at
	 *
	 *	http://www.samba.org/samba/ftp/specs/smbpub.txt
	 *
	 * (a text version of "Microsoft Networks SMB FILE SHARING
	 * PROTOCOL, Document Version 6.0p") says that:
	 *
	 *	the first 2 bytes of these 12 bytes are, for NT Create and X,
	 *	the "High Part of PID";
	 *
	 *	the next four bytes are reserved;
	 *
	 *	the next four bytes are, for SMB-over-IPX (with no
	 *	NetBIOS involved) two bytes of Session ID and two bytes
	 *	of SequenceNumber.
	 *
	 * If we ever implement SMB-over-IPX (which I suspect goes over
	 * IPX sockets 0x0550, 0x0552, and maybe 0x0554, as per the
	 * document in question), we'd probably want to have some way
	 * to determine whether this is SMB-over-IPX or not (which could
	 * be done by adding a PT_IPXSOCKET port type, having the
	 * IPX dissector set "pinfo->srcport" and "pinfo->destport",
	 * and having the SMB dissector check for a port type of
	 * PT_IPXSOCKET and for "pinfo->match_port" being either
	 * IPX_SOCKET_NWLINK_SMB_SERVER or IPX_SOCKET_NWLINK_SMB_REDIR
	 * or, if it also uses 0x0554, IPX_SOCKET_NWLINK_SMB_MESSENGER).
	 */

	/* 12 reserved bytes */
	proto_tree_add_item(htree, hf_smb_reserved, tvb, offset, 12, TRUE);
	offset += 12;

	/* TID */
	proto_tree_add_uint(htree, hf_smb_tid, tvb, offset, 2,
		sip->tid);
	offset += 2;

	/* PID */
	proto_tree_add_uint(htree, hf_smb_pid, tvb, offset, 2,
		sip->pid);
	offset += 2;

	/* UID */
	proto_tree_add_uint(htree, hf_smb_uid, tvb, offset, 2,
		sip->uid);
	offset += 2;

	/* MID */
	proto_tree_add_uint(htree, hf_smb_mid, tvb, offset, 2,
		sip->mid);
	offset += 2;

	if((sip->request)? smb_dissector[sip->cmd].request :
			   smb_dissector[sip->cmd].response){ 
	  /* call smb command dissector */
	  pinfo->private_data = sip;
          dissect_smb_command(tvb, pinfo, parent_tree, offset, tree, sip->cmd);
	} else {
	  const u_char *pd;
	  int SMB_offset;
	  proto_item *cmd_item;
	  proto_tree *cmd_tree;

	  tvb_compat(tvb, &pd, &SMB_offset);
	  offset += SMB_offset;
	  if (check_col(pinfo->fd, COL_INFO)) {
	    col_add_fstr(pinfo->fd, COL_INFO, "%s %s",
			 decode_smb_name(sip->cmd),
			 (sip->request)? "Request" : "Response");
	  }

	  cmd_item = proto_tree_add_text(tree, NullTVB, offset,
			0, "%s %s (0x%02x)",
			decode_smb_name(sip->cmd), 
			(sip->request)?"Request":"Response",
			sip->cmd);
	  tree = proto_item_add_subtree(cmd_item, ett_smb_command);

	  (dissect[sip->cmd])(pd, offset, pinfo->fd, parent_tree, tree, si,
			      tvb_length(tvb), SMB_offset);
	}

	return TRUE;
}






	/* External routines called during the registration process */

extern void register_proto_smb_browse( void);
extern void register_proto_smb_logon( void);
extern void register_proto_smb_mailslot( void);
extern void register_proto_smb_pipe( void);
extern void register_proto_smb_mailslot( void);


void
proto_register_smb(void)
{
	static hf_register_info hf[] = {
	{ &hf_smb_cmd,
		{ "SMB Command", "smb.cmd", FT_UINT8, BASE_HEX,
		VALS(smb_cmd_vals), 0x0, "SMB Command", HFILL }},

	{ &hf_smb_word_count,
		{ "Word Count (WCT)", "smb.wct", FT_UINT8, BASE_DEC,
		NULL, 0x0, "Word Count, count of parameter words", HFILL }},

	{ &hf_smb_byte_count,
		{ "Byte Count (BCC)", "smb.bcc", FT_UINT16, BASE_DEC,
		NULL, 0x0, "Byte Count, count of data bytes", HFILL }},

	{ &hf_smb_response_to,
		{ "Response to", "smb.response_to", FT_UINT32, BASE_DEC,
		NULL, 0, "This packet is a response to the packet in this frame", HFILL }},

	{ &hf_smb_response_in,
		{ "Response in", "smb.response_in", FT_UINT32, BASE_DEC,
		NULL, 0, "The response to this packet is in this packet", HFILL }},

	{ &hf_smb_nt_status,
		{ "NT Status", "smb.nt_status", FT_UINT32, BASE_HEX,
		VALS(NT_errors), 0, "NT Status code", HFILL }},

	{ &hf_smb_error_class,
		{ "Error Class", "smb.error_class", FT_UINT8, BASE_HEX,
		VALS(errcls_types), 0, "DOS Error Class", HFILL }},

	{ &hf_smb_error_code,
		{ "Error Code", "smb.error_code", FT_UINT16, BASE_HEX,
		NULL, 0, "DOS Error Code", HFILL }},

	{ &hf_smb_reserved,
		{ "Reserved", "smb.reserved", FT_BYTES, BASE_HEX,
		NULL, 0, "Reserved bytes, must be zero", HFILL }},

	{ &hf_smb_pid,
		{ "Process ID", "smb.pid", FT_UINT16, BASE_DEC,
		NULL, 0, "Process ID", HFILL }},

	{ &hf_smb_tid,
		{ "Tree ID", "smb.tid", FT_UINT16, BASE_DEC,
		NULL, 0, "Tree ID", HFILL }},

	{ &hf_smb_uid,
		{ "User ID", "smb.uid", FT_UINT16, BASE_DEC,
		NULL, 0, "User ID", HFILL }},

	{ &hf_smb_mid,
		{ "Multiplex ID", "smb.mid", FT_UINT16, BASE_DEC,
		NULL, 0, "Multiplex ID", HFILL }},

	{ &hf_smb_flags_lock,
		{ "Lock and Read", "smb.flags.lock", FT_BOOLEAN, 8,
		TFS(&tfs_smb_flags_lock), 0x01, "Are Lock&Read and Write&Unlock operations supported?", HFILL }},

	{ &hf_smb_flags_receive_buffer,
		{ "Receive Buffer Posted", "smb.flags.receive_buffer", FT_BOOLEAN, 8,
		TFS(&tfs_smb_flags_receive_buffer), 0x02, "Have receive buffers been reported?", HFILL }},

	{ &hf_smb_flags_caseless,
		{ "Case Sensitivity", "smb.flags.caseless", FT_BOOLEAN, 8,
		TFS(&tfs_smb_flags_caseless), 0x08, "Are pathnames caseless or casesensitive?", HFILL }},

	{ &hf_smb_flags_canon,
		{ "Canonicalized Pathnames", "smb.flags.canon", FT_BOOLEAN, 8,
		TFS(&tfs_smb_flags_canon), 0x10, "Are pathnames canonicalized?", HFILL }},

	{ &hf_smb_flags_oplock,
		{ "Oplocks", "smb.flags.oplock", FT_BOOLEAN, 8,
		TFS(&tfs_smb_flags_oplock), 0x20, "Is an oplock requested/granted?", HFILL }},

	{ &hf_smb_flags_notify,
		{ "Notify", "smb.flags.notify", FT_BOOLEAN, 8,
		TFS(&tfs_smb_flags_notify), 0x40, "Notify on open or all?", HFILL }},

	{ &hf_smb_flags_response,
		{ "Request/Response", "smb.flags.response", FT_BOOLEAN, 8,
		TFS(&tfs_smb_flags_response), 0x80, "Is this a request or a response?", HFILL }},

	{ &hf_smb_flags2_long_names_allowed,
		{ "Long Names Allowed", "smb.flags2.long_names_allowed", FT_BOOLEAN, 16,
		TFS(&tfs_smb_flags2_long_names_allowed), 0x0001, "Are long file names allowed in the response?", HFILL }},

	{ &hf_smb_flags2_ea,
		{ "Extended Attributes", "smb.flags2.ea", FT_BOOLEAN, 16,
		TFS(&tfs_smb_flags2_ea), 0x0002, "Are extended attributes supported?", HFILL }},

	{ &hf_smb_flags2_sec_sig,
		{ "Security Signatures", "smb.flags2.sec_sig", FT_BOOLEAN, 16,
		TFS(&tfs_smb_flags2_sec_sig), 0x0004, "Are security signatures supported?", HFILL }},

	{ &hf_smb_flags2_long_names_used,
		{ "Long Names Used", "smb.flags2.long_names_used", FT_BOOLEAN, 16,
		TFS(&tfs_smb_flags2_long_names_used), 0x0040, "Are pathnames in this request long file names?", HFILL }},

	{ &hf_smb_flags2_esn,
		{ "Extended Security Negotiation", "smb.flags2.esn", FT_BOOLEAN, 16,
		TFS(&tfs_smb_flags2_esn), 0x0800, "Is extended security negotiation supported?", HFILL }},

	{ &hf_smb_flags2_dfs,
		{ "Dfs", "smb.flags2.dfs", FT_BOOLEAN, 16,
		TFS(&tfs_smb_flags2_dfs), 0x1000, "Can pathnames be resolved using Dfs?", HFILL }},

	{ &hf_smb_flags2_roe,
		{ "Execute-only Reads", "smb.flags2.roe", FT_BOOLEAN, 16,
		TFS(&tfs_smb_flags2_roe), 0x2000, "Will reads be allowed for execute-only files?", HFILL }},

	{ &hf_smb_flags2_nt_error,
		{ "Error Code Type", "smb.flags2.nt_error", FT_BOOLEAN, 16,
		TFS(&tfs_smb_flags2_nt_error), 0x4000, "Are error codes NT or DOS format?", HFILL }},

	{ &hf_smb_flags2_string,
		{ "Unicode Strings", "smb.flags2.string", FT_BOOLEAN, 16,
		TFS(&tfs_smb_flags2_string), 0x8000, "Are strings ASCII or Unicode?", HFILL }},

	{ &hf_smb_buffer_format,
		{ "Buffer Format", "smb.buffer_format", FT_UINT8, BASE_DEC,
		VALS(buffer_format_vals), 0x0, "Buffer Format, type of buffer", HFILL }},

	{ &hf_smb_dialect_name,
		{ "Name", "smb.dialect.name", FT_STRING, BASE_NONE,
		NULL, 0, "Name of dialect", HFILL }},

	{ &hf_smb_dialect_index,
		{ "Selected Index", "smb.dialect.index", FT_UINT16, BASE_DEC,
		NULL, 0, "Index of selected dialect", HFILL }},

	{ &hf_smb_max_trans_buf_size,
		{ "Max Buffer Size", "smb.max_bufsize", FT_UINT32, BASE_DEC,
		NULL, 0, "Maximum transmit buffer size", HFILL }},

	{ &hf_smb_max_mpx_count,
		{ "Max Mpx Count", "smb.max_mpx_count", FT_UINT16, BASE_DEC,
		NULL, 0, "Maximum pending multiplexed requests", HFILL }},

	{ &hf_smb_max_vcs_num,
		{ "Max VCs", "smb.max_vcs", FT_UINT16, BASE_DEC,
		NULL, 0, "Maximum VCs between client and server", HFILL }},

	{ &hf_smb_session_key,
		{ "Session Key", "smb.session_key", FT_UINT32, BASE_HEX,
		NULL, 0, "Unique token identifying this session", HFILL }},

	{ &hf_smb_server_timezone,
		{ "Time Zone", "smb.server.timezone", FT_INT16, BASE_DEC,
		NULL, 0, "Current timezone at server.", HFILL }},

	{ &hf_smb_encryption_key_length,
		{ "Key Length", "smb.encryption_key_length", FT_UINT16, BASE_DEC,
		NULL, 0, "Encryption key length (must be 0 if not LM2.1 dialect)", HFILL }},

	{ &hf_smb_encryption_key,
		{ "Encryption Key", "smb.encryption_key", FT_BYTES, BASE_HEX,
		NULL, 0, "Challenge/Response Encryption Key (for LM2.1 dialect)", HFILL }},

	{ &hf_smb_primary_domain,
		{ "Primary Domain", "smb.primary_domain", FT_STRING, BASE_NONE,
		NULL, 0, "The server's primary domain", HFILL }},

	{ &hf_smb_max_raw_buf_size,
		{ "Max Raw Buffer", "smb.max_raw", FT_UINT32, BASE_DEC,
		NULL, 0, "Maximum raw buffer size", HFILL }},

	{ &hf_smb_server_guid,
		{ "Server GUID", "smb.server.guid", FT_BYTES, BASE_HEX,
		NULL, 0, "Globally unique identifier for this server", HFILL }},

	{ &hf_smb_security_blob_len,
		{ "Security Blob Length", "smb.security_blob_len", FT_UINT16, BASE_DEC,
		NULL, 0, "Security blob length", HFILL }},

	{ &hf_smb_security_blob,
		{ "Security Blob", "smb.security_blob", FT_BYTES, BASE_HEX,
		NULL, 0, "Security blob", HFILL }},

	{ &hf_smb_sm_mode16,
		{ "Mode", "smb.sm.mode", FT_BOOLEAN, 16,
		TFS(&tfs_sm_mode), SECURITY_MODE_MODE, "User or Share security mode?", HFILL }},

	{ &hf_smb_sm_password16,
		{ "Password", "smb.sm.password", FT_BOOLEAN, 16,
		TFS(&tfs_sm_password), SECURITY_MODE_PASSWORD, "Encrypted or plaintext passwords?", HFILL }},

	{ &hf_smb_sm_mode,
		{ "Mode", "smb.sm.mode", FT_BOOLEAN, 8,
		TFS(&tfs_sm_mode), SECURITY_MODE_MODE, "User or Share security mode?", HFILL }},

	{ &hf_smb_sm_password,
		{ "Password", "smb.sm.password", FT_BOOLEAN, 8,
		TFS(&tfs_sm_password), SECURITY_MODE_PASSWORD, "Encrypted or plaintext passwords?", HFILL }},

	{ &hf_smb_sm_signatures,
		{ "Signatures", "smb.sm.signatures", FT_BOOLEAN, 8,
		TFS(&tfs_sm_signatures), SECURITY_MODE_SIGNATURES, "Are security signatures enabled?", HFILL }},

	{ &hf_smb_sm_sig_required,
		{ "Sig Req", "smb.sm.sig_required", FT_BOOLEAN, 8,
		TFS(&tfs_sm_sig_required), SECURITY_MODE_SIG_REQUIRED, "Are security signatures required?", HFILL }},

	{ &hf_smb_rm_read,
		{ "Read Raw", "smb.rm.read", FT_BOOLEAN, 16,
		TFS(&tfs_rm_read), RAWMODE_READ, "Is Read Raw supported?", HFILL }},

	{ &hf_smb_rm_write,
		{ "Write Raw", "smb.rm.write", FT_BOOLEAN, 16,
		TFS(&tfs_rm_write), RAWMODE_WRITE, "Is Write Raw supported?", HFILL }},

	{ &hf_smb_server_date_time,
		{ "Server Date and Time", "smb.server_date_time", FT_ABSOLUTE_TIME, BASE_NONE,
		NULL, 0, "Current date and time at server", HFILL }},

	{ &hf_smb_server_smb_date,
		{ "Server Date", "smb.server_date_time.smb_date", FT_UINT16, BASE_HEX,
		NULL, 0, "Current date at server, SMB_DATE format", HFILL }},

	{ &hf_smb_server_smb_time,
		{ "Server Time", "smb.server_date_time.smb_time", FT_UINT16, BASE_HEX,
		NULL, 0, "Current time at server, SMB_TIME format", HFILL }},

	{ &hf_smb_server_cap_raw_mode,
		{ "Raw Mode", "smb.server.cap.raw_mode", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_raw_mode), SERVER_CAP_RAW_MODE, "Are Raw Read and Raw Write supported?", HFILL }},

	{ &hf_smb_server_cap_mpx_mode,
		{ "MPX Mode", "smb.server.cap.mpx_mode", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_mpx_mode), SERVER_CAP_MPX_MODE, "Are Read Mpx and Write Mpx supported?", HFILL }},

	{ &hf_smb_server_cap_unicode,
		{ "Unicode", "smb.server.cap.unicode", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_unicode), SERVER_CAP_UNICODE, "Are Unicode strings supported?", HFILL }},

	{ &hf_smb_server_cap_large_files,
		{ "Large Files", "smb.server.cap.large_files", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_large_files), SERVER_CAP_LARGE_FILES, "Are large files (>4GB) supported?", HFILL }},

	{ &hf_smb_server_cap_nt_smbs,
		{ "NT SMBs", "smb.server.cap.nt_smbs", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_nt_smbs), SERVER_CAP_NT_SMBS, "Are NT SMBs supported?", HFILL }},

	{ &hf_smb_server_cap_rpc_remote_apis,
		{ "RPC Remote APIs", "smb.server.cap.rpc_remote_apis", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_rpc_remote_apis), SERVER_CAP_RPC_REMOTE_APIS, "Are RPC Remote APIs supported?", HFILL }},

	{ &hf_smb_server_cap_nt_status,
		{ "NT Status Codes", "smb.server.cap.nt_status", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_nt_status), SERVER_CAP_STATUS32, "Are NT Status Codes supported?", HFILL }},

	{ &hf_smb_server_cap_level_ii_oplocks,
		{ "Level 2 Oplocks", "smb.server.cap.level_2_oplocks", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_level_ii_oplocks), SERVER_CAP_LEVEL_II_OPLOCKS, "Are Level 2 oplocks supported?", HFILL }},

	{ &hf_smb_server_cap_lock_and_read,
		{ "Lock and Read", "smb.server.cap.lock_and_read", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_lock_and_read), SERVER_CAP_LOCK_AND_READ, "Is Lock and Read supported?", HFILL }},

	{ &hf_smb_server_cap_nt_find,
		{ "NT Find", "smb.server.cap.nt_find", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_nt_find), SERVER_CAP_NT_FIND, "Is NT Find supported?", HFILL }},

	{ &hf_smb_server_cap_dfs,
		{ "Dfs", "smb.server.cap.dfs", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_dfs), SERVER_CAP_DFS, "Is Dfs supported?", HFILL }},

	{ &hf_smb_server_cap_infolevel_passthru,
		{ "Infolevel Passthru", "smb.server.cap.infolevel_passthru", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_infolevel_passthru), SERVER_CAP_INFOLEVEL_PASSTHRU, "Is NT information level request passthrough supported?", HFILL }},

	{ &hf_smb_server_cap_large_readx,
		{ "Large ReadX", "smb.server.cap.large_readx", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_large_readx), SERVER_CAP_LARGE_READX, "Is Large Read andX supported?", HFILL }},

	{ &hf_smb_server_cap_large_writex,
		{ "Large WriteX", "smb.server.cap.large_writex", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_large_writex), SERVER_CAP_LARGE_WRITEX, "Is Large Write andX supported?", HFILL }},

	{ &hf_smb_server_cap_unix,
		{ "UNIX", "smb.server.cap.unix", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_unix), SERVER_CAP_UNIX , "Are UNIX extensions supported?", HFILL }},

	{ &hf_smb_server_cap_reserved,
		{ "Reserved", "smb.server.cap.reserved", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_reserved), SERVER_CAP_RESERVED, "RESERVED", HFILL }},

	{ &hf_smb_server_cap_bulk_transfer,
		{ "Bulk Transfer", "smb.server.cap.bulk_transfer", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_bulk_transfer), SERVER_CAP_BULK_TRANSFER, "Are Bulk Read and Bulk Write supported?", HFILL }},

	{ &hf_smb_server_cap_compressed_data,
		{ "Compressed Data", "smb.server.cap.compressed_data", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_compressed_data), SERVER_CAP_COMPRESSED_DATA, "Is compressed data transfer supported?", HFILL }},

	{ &hf_smb_server_cap_extended_security,
		{ "Extended Security", "smb.server.cap.extended_security", FT_BOOLEAN, 32,
		TFS(&tfs_server_cap_extended_security), SERVER_CAP_EXTENDED_SECURITY, "Are Extended security exchanges supported?", HFILL }},

	{ &hf_smb_system_time,
		{ "System Time", "smb.system.time", FT_ABSOLUTE_TIME, BASE_NONE,
		NULL, 0, "System Time", HFILL }},

	{ &hf_smb_unknown,
		{ "Unknown Data", "smb.unknown", FT_BYTES, BASE_HEX,
		NULL, 0, "Unknown Data. Should be implemented by someone", HFILL }},

	{ &hf_smb_dir_name,
		{ "Directory", "smb.dir_name", FT_STRING, BASE_NONE,
		NULL, 0, "SMB Directory Name", HFILL }},

	{ &hf_smb_echo_count,
		{ "Echo Count", "smb.echo.count", FT_UINT16, BASE_DEC,
		NULL, 0, "Number of times to echo data back", HFILL }},

	{ &hf_smb_echo_data,
		{ "Echo Data", "smb.echo.data", FT_BYTES, BASE_HEX,
		NULL, 0, "Data for SMB Echo Request/Response", HFILL }},

	{ &hf_smb_echo_seq_num,
		{ "Echo Seq Num", "smb.echo.seq_num", FT_UINT16, BASE_DEC,
		NULL, 0, "Sequence number for this echo response", HFILL }},

	{ &hf_smb_max_buf_size,
		{ "Max Buffer", "smb.max_buf", FT_UINT16, BASE_DEC,
		NULL, 0, "Max client buffer size", HFILL }},

	{ &hf_smb_path,
		{ "Path", "smb.path", FT_STRING, BASE_NONE,
		NULL, 0, "Path. Server name and share name", HFILL }},

	{ &hf_smb_service,
		{ "Service", "smb.service", FT_STRING, BASE_NONE,
		NULL, 0, "Service name", HFILL }},

	{ &hf_smb_password,
		{ "Password", "smb.password", FT_BYTES, BASE_NONE,
		NULL, 0, "Password", HFILL }},

	{ &hf_smb_ansi_password,
		{ "ANSI Password", "smb.ansi_password", FT_BYTES, BASE_NONE,
		NULL, 0, "ANSI Password", HFILL }},

	{ &hf_smb_unicode_password,
		{ "Unicode Password", "smb.unicode_password", FT_BYTES, BASE_NONE,
		NULL, 0, "Unicode Password", HFILL }},

	{ &hf_smb_move_flags_file,
		{ "Must be file", "smb.move.flags.file", FT_BOOLEAN, 16,
		TFS(&tfs_mf_file), 0x0001, "Must target be a file?", HFILL }},

	{ &hf_smb_move_flags_dir,
		{ "Must be directory", "smb.move.flags.dir", FT_BOOLEAN, 16,
		TFS(&tfs_mf_dir), 0x0002, "Must target be a directory?", HFILL }},

	{ &hf_smb_move_flags_verify,
		{ "Verify writes", "smb.move.flags.verify", FT_BOOLEAN, 16,
		TFS(&tfs_mf_verify), 0x0010, "Verify all writes?", HFILL }},

	{ &hf_smb_count,
		{ "Count", "smb.count", FT_UINT32, BASE_DEC,
		NULL, 0, "Count number of items/bytes", HFILL }},

	{ &hf_smb_file_name,
		{ "File Name", "smb.file", FT_STRING, BASE_NONE,
		NULL, 0, "File Name", HFILL }},

	{ &hf_smb_open_function_create,
		{ "Create", "smb.open.function.create", FT_BOOLEAN, 16,
		TFS(&tfs_of_create), 0x0010, "Create file if it doesn't exist?", HFILL }},

	{ &hf_smb_open_function_open,
		{ "Open", "smb.open.function.open", FT_UINT16, BASE_DEC,
		VALS(of_open), 0x0003, "Action to be taken on open if file exists", HFILL }},

	{ &hf_smb_fid,
		{ "FID", "smb.fid", FT_UINT16, BASE_HEX,
		NULL, 0, "FID: File ID", HFILL }},

	{ &hf_smb_file_attr_read_only_16bit,
		{ "Read Only", "smb.file.attribute.read_only", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_read_only), FILE_ATTRIBUTE_READ_ONLY, "READ ONLY file attribute", HFILL }},

	{ &hf_smb_file_attr_read_only_8bit,
		{ "Read Only", "smb.file.attribute.read_only", FT_BOOLEAN, 8,
		TFS(&tfs_file_attribute_read_only), FILE_ATTRIBUTE_READ_ONLY, "READ ONLY file attribute", HFILL }},

	{ &hf_smb_file_attr_hidden_16bit,
		{ "Hidden", "smb.file.attribute.hidden", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_hidden), FILE_ATTRIBUTE_HIDDEN, "HIDDEN file attribute", HFILL }},

	{ &hf_smb_file_attr_hidden_8bit,
		{ "Hidden", "smb.file.attribute.hidden", FT_BOOLEAN, 8,
		TFS(&tfs_file_attribute_hidden), FILE_ATTRIBUTE_HIDDEN, "HIDDEN file attribute", HFILL }},

	{ &hf_smb_file_attr_system_16bit,
		{ "System", "smb.file.attribute.system", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_system), FILE_ATTRIBUTE_SYSTEM, "SYSTEM file attribute", HFILL }},

	{ &hf_smb_file_attr_system_8bit,
		{ "System", "smb.file.attribute.system", FT_BOOLEAN, 8,
		TFS(&tfs_file_attribute_system), FILE_ATTRIBUTE_SYSTEM, "SYSTEM file attribute", HFILL }},

	{ &hf_smb_file_attr_volume_16bit,
		{ "Volume ID", "smb.file.attribute.volume", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_volume), FILE_ATTRIBUTE_VOLUME, "VOLUME file attribute", HFILL }},

	{ &hf_smb_file_attr_volume_8bit,
		{ "Volume ID", "smb.file.attribute.volume", FT_BOOLEAN, 8,
		TFS(&tfs_file_attribute_volume), FILE_ATTRIBUTE_VOLUME, "VOLUME ID file attribute", HFILL }},

	{ &hf_smb_file_attr_directory_16bit,
		{ "Directory", "smb.file.attribute.directory", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_directory), FILE_ATTRIBUTE_DIRECTORY, "DIRECTORY file attribute", HFILL }},

	{ &hf_smb_file_attr_directory_8bit,
		{ "Directory", "smb.file.attribute.directory", FT_BOOLEAN, 8,
		TFS(&tfs_file_attribute_directory), FILE_ATTRIBUTE_DIRECTORY, "DIRECTORY file attribute", HFILL }},

	{ &hf_smb_file_attr_archive_16bit,
		{ "Archive", "smb.file.attribute.archive", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_archive), FILE_ATTRIBUTE_ARCHIVE, "ARCHIVE file attribute", HFILL }},

	{ &hf_smb_file_attr_archive_8bit,
		{ "Archive", "smb.file.attribute.archive", FT_BOOLEAN, 8,
		TFS(&tfs_file_attribute_archive), FILE_ATTRIBUTE_ARCHIVE, "ARCHIVE file attribute", HFILL }},

	{ &hf_smb_file_attr_device,
		{ "Device", "smb.file.attribute.device", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_device), FILE_ATTRIBUTE_DEVICE, "Is this file a device?", HFILL }},

	{ &hf_smb_file_attr_normal,
		{ "Normal", "smb.file.attribute.normal", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_normal), FILE_ATTRIBUTE_NORMAL, "Is this a normal file?", HFILL }},

	{ &hf_smb_file_attr_temporary,
		{ "Temporary", "smb.file.attribute.temporary", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_temporary), FILE_ATTRIBUTE_TEMPORARY, "Is this a temporary file?", HFILL }},

	{ &hf_smb_file_attr_sparse,
		{ "Sparse", "smb.file.attribute.sparse", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_sparse), FILE_ATTRIBUTE_SPARSE, "Is this a sparse file?", HFILL }},

	{ &hf_smb_file_attr_reparse,
		{ "Reparse Point", "smb.file.attribute.reparse", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_reparse), FILE_ATTRIBUTE_REPARSE, "Does this file have an associated reparse point?", HFILL }},

	{ &hf_smb_file_attr_compressed,
		{ "Compressed", "smb.file.attribute.compressed", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_compressed), FILE_ATTRIBUTE_COMPRESSED, "Is this file compressed?", HFILL }},

	{ &hf_smb_file_attr_offline,
		{ "Offline", "smb.file.attribute.offline", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_offline), FILE_ATTRIBUTE_OFFLINE, "Is this file offline?", HFILL }},

	{ &hf_smb_file_attr_not_content_indexed,
		{ "Content Indexed", "smb.file.attribute.not_content_indexed", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_not_content_indexed), FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, "May this file be indexed by the content indexing service", HFILL }},

	{ &hf_smb_file_attr_encrypted,
		{ "Encrypted", "smb.file.attribute.encrypted", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_encrypted), FILE_ATTRIBUTE_ENCRYPTED, "Is this file encrypted?", HFILL }},

	{ &hf_smb_file_size,
		{ "File Size", "smb.file.size", FT_UINT32, BASE_DEC,
		NULL, 0, "File Size", HFILL }},

	{ &hf_smb_search_attribute_read_only,
		{ "Read Only", "smb.search.attribute.read_only", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_read_only), FILE_ATTRIBUTE_READ_ONLY, "READ ONLY search attribute", HFILL }},

	{ &hf_smb_search_attribute_hidden,
		{ "Hidden", "smb.search.attribute.hidden", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_hidden), FILE_ATTRIBUTE_HIDDEN, "HIDDEN search attribute", HFILL }},

	{ &hf_smb_search_attribute_system,
		{ "System", "smb.search.attribute.system", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_system), FILE_ATTRIBUTE_SYSTEM, "SYSTEM search attribute", HFILL }},

	{ &hf_smb_search_attribute_volume,
		{ "Volume ID", "smb.search.attribute.volume", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_volume), FILE_ATTRIBUTE_VOLUME, "VOLUME ID search attribute", HFILL }},

	{ &hf_smb_search_attribute_directory,
		{ "Directory", "smb.search.attribute.directory", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_directory), FILE_ATTRIBUTE_DIRECTORY, "DIRECTORY search attribute", HFILL }},

	{ &hf_smb_search_attribute_archive,
		{ "Archive", "smb.search.attribute.archive", FT_BOOLEAN, 16,
		TFS(&tfs_file_attribute_archive), FILE_ATTRIBUTE_ARCHIVE, "ARCHIVE search attribute", HFILL }},

	{ &hf_smb_access_mode,
		{ "Access Mode", "smb.access.mode", FT_UINT16, BASE_DEC,
		VALS(da_access_vals), 0x0007, "Access Mode", HFILL }},

	{ &hf_smb_access_sharing,
		{ "Sharing Mode", "smb.access.sharing", FT_UINT16, BASE_DEC,
		VALS(da_sharing_vals), 0x0070, "Sharing Mode", HFILL }},

	{ &hf_smb_access_locality,
		{ "Locality", "smb.access.locality", FT_UINT16, BASE_DEC,
		VALS(da_locality_vals), 0x0700, "Locality of reference", HFILL }},

	{ &hf_smb_access_caching,
		{ "Caching", "smb.access.caching", FT_BOOLEAN, 16,
		TFS(&tfs_da_caching), 0x1000, "Caching mode?", HFILL }},

	{ &hf_smb_access_writetru,
		{ "Writethrough", "smb.access.writethrough", FT_BOOLEAN, 16,
		TFS(&tfs_da_writetru), 0x4000, "Writethrough mode?", HFILL }},

	{ &hf_smb_create_time,
		{ "Created", "smb.create.time", FT_ABSOLUTE_TIME, BASE_NONE,
		NULL, 0, "Creation Time", HFILL }},

	{ &hf_smb_create_dos_date,
		{ "Create Date", "smb.create.smb.date", FT_UINT16, BASE_HEX,
		NULL, 0, "Create Date, SMB_DATE format", HFILL }},

	{ &hf_smb_create_dos_time,
		{ "Create Time", "smb.create.smb.time", FT_UINT16, BASE_HEX,
		NULL, 0, "Create Time, SMB_TIME format", HFILL }},

	{ &hf_smb_last_write_time,
		{ "Last Write", "smb.last_write.time", FT_ABSOLUTE_TIME, BASE_NONE,
		NULL, 0, "Time this file was last written to", HFILL }},

	{ &hf_smb_last_write_dos_date,
		{ "Last Write Date", "smb.last_write.smb.date", FT_UINT16, BASE_HEX,
		NULL, 0, "Last Write Date, SMB_DATE format", HFILL }},

	{ &hf_smb_last_write_dos_time,
		{ "Last Write Time", "smb.last_write.smb.time", FT_UINT16, BASE_HEX,
		NULL, 0, "Last Write Time, SMB_TIME format", HFILL }},

	{ &hf_smb_old_file_name,
		{ "Old File Name", "smb.file", FT_STRING, BASE_NONE,
		NULL, 0, "Old File Name (When renaming a file)", HFILL }},

	{ &hf_smb_offset,
		{ "Offset", "smb.offset", FT_UINT32, BASE_DEC,
		NULL, 0, "Offset in file", HFILL }},

	{ &hf_smb_remaining,
		{ "Remaining", "smb.remaining", FT_UINT32, BASE_DEC,
		NULL, 0, "Remaining number of bytes", HFILL }},

	{ &hf_smb_padding,
		{ "Padding", "smb.padding", FT_BYTES, BASE_HEX,
		NULL, 0, "Padding or unknown data", HFILL }},

	{ &hf_smb_file_data,
		{ "File Data", "smb.file.data", FT_BYTES, BASE_HEX,
		NULL, 0, "Data read/written to the file", HFILL }},

	{ &hf_smb_data_len,
		{ "Data Length", "smb.data_len", FT_UINT16, BASE_DEC,
		NULL, 0, "Length of data", HFILL }},

	{ &hf_smb_seek_mode,
		{ "Seek Mode", "smb.seek_mode", FT_UINT16, BASE_DEC,
		VALS(seek_mode_vals), 0, "Seek Mode, what type of seek", HFILL }},

	{ &hf_smb_access_time,
		{ "Last Access", "smb.access.time", FT_ABSOLUTE_TIME, BASE_NONE,
		NULL, 0, "Last Access Time", HFILL }},

	{ &hf_smb_access_dos_date,
		{ "Last Access Date", "smb.access.smb.date", FT_UINT16, BASE_HEX,
		NULL, 0, "Last Access Date, SMB_DATE format", HFILL }},

	{ &hf_smb_access_dos_time,
		{ "Last Access Time", "smb.access.smb.time", FT_UINT16, BASE_HEX,
		NULL, 0, "Last Access Time, SMB_TIME format", HFILL }},

	{ &hf_smb_data_size,
		{ "Data Size", "smb.data_size", FT_UINT32, BASE_DEC,
		NULL, 0, "Data Size", HFILL }},

	{ &hf_smb_alloc_size,
		{ "Allocation Size", "smb.alloc_size", FT_UINT32, BASE_DEC,
		NULL, 0, "Number of bytes to reserve on create or truncate", HFILL }},

	{ &hf_smb_max_count,
		{ "Max Count", "smb.maxcount", FT_UINT16, BASE_DEC,
		NULL, 0, "Maximum Count", HFILL }},

	{ &hf_smb_min_count,
		{ "Min Count", "smb.mincount", FT_UINT16, BASE_DEC,
		NULL, 0, "Minimum Count", HFILL }},

	{ &hf_smb_timeout,
		{ "Timeout", "smb.timeout", FT_UINT32, BASE_DEC,
		NULL, 0, "Timeout in miliseconds", HFILL }},

	{ &hf_smb_high_offset,
		{ "High Offset", "smb.offset_high", FT_UINT32, BASE_DEC,
		NULL, 0, "High 32 Bits Of File Offset", HFILL }},

	{ &hf_smb_units,
		{ "Total Units", "smb.units", FT_UINT16, BASE_DEC,
		NULL, 0, "Total number of units at server", HFILL }},

	{ &hf_smb_bpu,
		{ "Blocks Per Unit", "smb.bpu", FT_UINT16, BASE_DEC,
		NULL, 0, "Blocks per unit at server", HFILL }},

	{ &hf_smb_blocksize,
                { "Block Size", "smb.blocksize", FT_UINT16, BASE_DEC,
		NULL, 0, "Block size (in bytes) at server", HFILL }},

	{ &hf_smb_freeunits,
		{ "Free Units", "smb.free_units", FT_UINT16, BASE_DEC,
		NULL, 0, "Number of free units at server", HFILL }},

	{ &hf_smb_data_offset,
		{ "Data Offset", "smb.data_offset", FT_UINT16, BASE_DEC,
		NULL, 0, "Data Offset", HFILL }},

	{ &hf_smb_dcm,
		{ "Data Compaction Mode", "smb.dcm", FT_UINT16, BASE_DEC,
		NULL, 0, "Data Compaction Mode", HFILL }},

	{ &hf_smb_request_mask,
		{ "Request Mask", "smb.request.mask", FT_UINT32, BASE_HEX,
		NULL, 0, "Connectionless mode mask", HFILL }},

	{ &hf_smb_response_mask,
		{ "Response Mask", "smb.response.mask", FT_UINT32, BASE_HEX,
		NULL, 0, "Connectionless mode mask", HFILL }},

	{ &hf_smb_sid,
		{ "SID", "smb.sid", FT_UINT16, BASE_HEX,
		NULL, 0, "SID: Search ID, handle for find operations", HFILL }},

	{ &hf_smb_write_raw_mode_write_through,
		{ "Write Through", "smb.write.mode.write_through", FT_BOOLEAN, 16,
		TFS(&tfs_write_raw_mode_write_through), 0x0001, "Write through mode requested?", HFILL }},

	{ &hf_smb_write_raw_mode_return_remaining,
		{ "Return Remaining", "smb.write.mode.return_remaining", FT_BOOLEAN, 16,
		TFS(&tfs_write_raw_mode_return_remaining), 0x0002, "Return remaining data responses?", HFILL }},

	{ &hf_smb_write_raw_mode_connectionless,
		{ "Connectionless", "smb.write.mode.connectionless", FT_BOOLEAN, 16,
		TFS(&tfs_write_raw_mode_connectionless), 0x0080, "Connectionless mode requested?", HFILL }},

	{ &hf_smb_resume_key_len,
		{ "Resume Key Length", "smb.resume.key_len", FT_UINT16, BASE_DEC,
		NULL, 0, "Resume Key length", HFILL }},

	{ &hf_smb_resume_server_cookie,
		{ "Server Cookie", "smb.resume.server.cookie", FT_BYTES, BASE_HEX,
		NULL, 0, "Cookie, must not be modified by the client", HFILL }},

	{ &hf_smb_resume_client_cookie,
		{ "Client Cookie", "smb.resume.client.cookie", FT_BYTES, BASE_HEX,
		NULL, 0, "Cookie, must not be modified by the server", HFILL }},

	{ &hf_smb_andxoffset,
		{ "AndXOffset", "smb.andxoffset", FT_UINT16, BASE_DEC,
		NULL, 0, "Offset to next command in this SMB packet", HFILL }},

	{ &hf_smb_lock_type_large,
		{ "Large Files", "smb.lock.type.large", FT_BOOLEAN, 8,
		TFS(&tfs_lock_type_large), 0x10, "Large file locking requested?", HFILL }},

	{ &hf_smb_lock_type_cancel,
		{ "Cancel", "smb.lock.type.cancel", FT_BOOLEAN, 8,
		TFS(&tfs_lock_type_cancel), 0x08, "Cancel outstanding lock requests?", HFILL }},

	{ &hf_smb_lock_type_change,
		{ "Change", "smb.lock.type.change", FT_BOOLEAN, 8,
		TFS(&tfs_lock_type_change), 0x04, "Change type of lock?", HFILL }},

	{ &hf_smb_lock_type_oplock,
		{ "Oplock Break", "smb.lock.type.oplock_release", FT_BOOLEAN, 8,
		TFS(&tfs_lock_type_oplock), 0x02, "Is this a notification of, or a response to, an oplock break?", HFILL }},

	{ &hf_smb_lock_type_shared,
		{ "Shared", "smb.lock.type.shared", FT_BOOLEAN, 8,
		TFS(&tfs_lock_type_shared), 0x01, "Shared or exclusive lock requested?", HFILL }},

	{ &hf_smb_locking_ol,
		{ "Oplock Level", "smb.locking.oplock.level", FT_UINT8, BASE_DEC,
		VALS(locking_ol_vals), 0, "Level of existing oplock at client (if any)", HFILL }},

	{ &hf_smb_number_of_locks,
		{ "Number of Locks", "smb.locking.num_locks", FT_UINT16, BASE_DEC,
		NULL, 0, "Number of lock requests in this request", HFILL }},

	{ &hf_smb_number_of_unlocks,
		{ "Number of Unlocks", "smb.locking.num_unlocks", FT_UINT16, BASE_DEC,
		NULL, 0, "Number of unlock requests in this request", HFILL }},

	{ &hf_smb_lock_long_length,
		{ "Length", "smb.lock.length", FT_UINT64, BASE_DEC,
		NULL, 0, "Length of lock/unlock region", HFILL }},

	{ &hf_smb_lock_long_offset,
		{ "Offset", "smb.lock.offset", FT_UINT64, BASE_DEC,
		NULL, 0, "Offset in the file of lock/unlock region", HFILL }},

	{ &hf_smb_file_type,
		{ "File Type", "smb.file_type", FT_UINT16, BASE_DEC,
		VALS(filetype_vals), 0, "Type of file", HFILL }},

	{ &hf_smb_device_state,
		{ "Device State", "smb.device_state", FT_UINT16, BASE_HEX,
		NULL, 0, "Device State", HFILL }},

	{ &hf_smb_server_fid,
		{ "Server FID", "smb.server_fid", FT_UINT32, BASE_HEX,
		NULL, 0, "Server unique File ID", HFILL }},

	{ &hf_smb_open_flags_add_info,
		{ "Additional Info", "smb.open.flags.add_info", FT_BOOLEAN, 16,
		TFS(&tfs_open_flags_add_info), 0x0001, "Additional Information Requested?", HFILL }},

	{ &hf_smb_open_flags_ex_oplock,
		{ "Exclusive Oplock", "smb.open.flags.ex_oplock", FT_BOOLEAN, 16,
		TFS(&tfs_open_flags_ex_oplock), 0x0002, "Exclusive Oplock Requested?", HFILL }},

	{ &hf_smb_open_flags_batch_oplock,
		{ "Batch Oplock", "smb.open.flags.batch_oplock", FT_BOOLEAN, 16,
		TFS(&tfs_open_flags_batch_oplock), 0x0004, "Batch Oplock Requested?", HFILL }},

	{ &hf_smb_open_flags_ealen,
		{ "Total EA Len", "smb.open.flags.ealen", FT_BOOLEAN, 16,
		TFS(&tfs_open_flags_ealen), 0x0008, "Total EA Len Requested?", HFILL }},

	{ &hf_smb_open_action_open,
		{ "Open Action", "smb.open.action.open", FT_UINT16, BASE_DEC,
		VALS(oa_open_vals), 0x0003, "Open Action, how the file was opened", HFILL }},

	{ &hf_smb_open_action_lock,
		{ "Exclusive Open", "smb.open.action.lock", FT_BOOLEAN, 16,
		TFS(&tfs_oa_lock), 0x8000, "Is this file opened by another user?", HFILL }},

	{ &hf_smb_vc_num,
		{ "VC Number", "smb.vc", FT_UINT16, BASE_DEC,
		NULL, 0, "VC Number", HFILL }},

	{ &hf_smb_password_len,
		{ "Password Length", "smb.pwlen", FT_UINT16, BASE_DEC,
		NULL, 0, "Length of password", HFILL }},

	{ &hf_smb_ansi_password_len,
		{ "ANSI Password Length", "smb.ansi_pwlen", FT_UINT16, BASE_DEC,
		NULL, 0, "Length of ANSI password", HFILL }},

	{ &hf_smb_unicode_password_len,
		{ "Unicode Password Length", "smb.unicode_pwlen", FT_UINT16, BASE_DEC,
		NULL, 0, "Length of Unicode password", HFILL }},

	{ &hf_smb_account,
		{ "Account", "smb.account", FT_STRING, BASE_NONE,
		NULL, 0, "Account, username", HFILL }},

	{ &hf_smb_os,
		{ "Native OS", "smb.native_os", FT_STRING, BASE_NONE,
		NULL, 0, "Which OS we are running", HFILL }},

	{ &hf_smb_lanman,
		{ "Native LAN Manager", "smb.native_lanman", FT_STRING, BASE_NONE,
		NULL, 0, "Which LANMAN protocol we are running", HFILL }},

	{ &hf_smb_setup_action_guest,
		{ "Guest", "smb.setup.action.guest", FT_BOOLEAN, 16,
		TFS(&tfs_setup_action_guest), 0x0001, "Client logged in as GUEST?", HFILL }},

	{ &hf_smb_fs,
		{ "Native File System", "smb.native_fs", FT_STRING, BASE_NONE,
		NULL, 0, "Native File System", HFILL }},

	{ &hf_smb_connect_flags_dtid,
		{ "Disconnect TID", "smb.connect.flags.dtid", FT_BOOLEAN, 16,
		TFS(&tfs_disconnect_tid), 0x0001, "Disconnect TID?", HFILL }},

	{ &hf_smb_connect_support_search,
		{ "Search Bits", "smb.connect.support.search", FT_BOOLEAN, 16,
		TFS(&tfs_connect_support_search), 0x0001, "Exclusive Search Bits supported?", HFILL }},

	{ &hf_smb_connect_support_in_dfs,
		{ "In Dfs", "smb.connect.support.dfs", FT_BOOLEAN, 16,
		TFS(&tfs_connect_support_in_dfs), 0x0002, "Is this in a Dfs tree?", HFILL }},

	{ &hf_smb_max_setup_count,
		{ "Max Setup Count", "smb.msc", FT_UINT8, BASE_DEC,
		NULL, 0, "Maximum number of setup words to return", HFILL }},

	{ &hf_smb_total_param_count,
		{ "Total Param Count", "smb.tpc", FT_UINT32, BASE_DEC,
		NULL, 0, "Total number of parameter bytes", HFILL }},

	{ &hf_smb_total_data_count,
		{ "Total Data Count", "smb.tdc", FT_UINT32, BASE_DEC,
		NULL, 0, "Total number of data bytes", HFILL }},

	{ &hf_smb_max_param_count,
		{ "Max Param Count", "smb.mpc", FT_UINT32, BASE_DEC,
		NULL, 0, "Maximum number of parameter bytes to return", HFILL }},

	{ &hf_smb_max_data_count,
		{ "Max Data Count", "smb.mdc", FT_UINT32, BASE_DEC,
		NULL, 0, "Maximum number of data bytes to return", HFILL }},

	{ &hf_smb_param_disp32,
		{ "Param Disp", "smb.pd", FT_UINT32, BASE_DEC,
		NULL, 0, "Displacement of these parameter bytes", HFILL }},

	{ &hf_smb_param_count32,
		{ "Param Count", "smb.pc", FT_UINT32, BASE_DEC,
		NULL, 0, "Number of parameter bytes in this buffer", HFILL }},

	{ &hf_smb_param_offset32,
		{ "Param Offset", "smb.po", FT_UINT32, BASE_DEC,
		NULL, 0, "Offset (from header start) to parameters", HFILL }},

	{ &hf_smb_data_count32,
		{ "Data Count", "smb.dc", FT_UINT32, BASE_DEC,
		NULL, 0, "Number of data bytes in this buffer", HFILL }},

	{ &hf_smb_data_disp32,
		{ "Data Disp", "smb.data_disp", FT_UINT32, BASE_DEC,
		NULL, 0, "Data Displacement", HFILL }},

	{ &hf_smb_data_offset32,
		{ "Data Offset", "smb.data_offset", FT_UINT32, BASE_DEC,
		NULL, 0, "Data Offset", HFILL }},

	{ &hf_smb_setup_count,
		{ "Setup Count", "smb.sc", FT_UINT8, BASE_DEC,
		NULL, 0, "Number of setup words in this buffer", HFILL }},

	{ &hf_smb_nt_trans_subcmd,
		{ "Function", "smb.nt.function", FT_UINT16, BASE_DEC,
		VALS(nt_cmd_vals), 0, "Function for NT Transaction", HFILL }},

	{ &hf_smb_nt_ioctl_function_code,
		{ "Function", "smb.nt.ioctl.function", FT_UINT32, BASE_HEX,
		NULL, 0, "NT IOCTL function code", HFILL }},

	{ &hf_smb_nt_ioctl_isfsctl,
		{ "IsFSctl", "smb.nt.ioctl.isfsctl", FT_UINT8, BASE_DEC,
		VALS(nt_ioctl_isfsctl_vals), 0, "Is this a device IOCTL (FALSE) or FS Control (TRUE)", HFILL }},

	{ &hf_smb_nt_ioctl_flags_root_handle,
		{ "Root Handle", "smb.nt.ioctl.flags.root_handle", FT_BOOLEAN, 8,
		TFS(&tfs_nt_ioctl_flags_root_handle), NT_IOCTL_FLAGS_ROOT_HANDLE, "Apply to this share or root DFS share", HFILL }},

	{ &hf_smb_nt_ioctl_data,
		{ "IOCTL Data", "smb.nt.ioctl.data", FT_BYTES, BASE_HEX,
		NULL, 0, "Data for the IOCTL call", HFILL }},

	{ &hf_smb_nt_notify_action,
		{ "Action", "smb.nt.notify.action", FT_UINT32, BASE_DEC,
		VALS(nt_notify_action_vals), 0, "Which action caused this notify response", HFILL }},

	{ &hf_smb_nt_notify_watch_tree,
		{ "Watch Tree", "smb.nt.notify.watch_tree", FT_UINT8, BASE_DEC,
		VALS(watch_tree_vals), 0, "Should Notify watch subdirectories also?", HFILL }},

	{ &hf_smb_nt_notify_stream_write,
		{ "Stream Write", "smb.nt.notify.stream_write", FT_BOOLEAN, 32,
		TFS(&tfs_nt_notify_stream_write), NT_NOTIFY_STREAM_WRITE, "Notify on stream write?", HFILL }},

	{ &hf_smb_nt_notify_stream_size,
		{ "Stream Size Change", "smb.nt.notify.stream_size", FT_BOOLEAN, 32,
		TFS(&tfs_nt_notify_stream_size), NT_NOTIFY_STREAM_SIZE, "Notify on changes of stream size", HFILL }},

	{ &hf_smb_nt_notify_stream_name,
		{ "Stream Name Change", "smb.nt.notify.stream_name", FT_BOOLEAN, 32,
		TFS(&tfs_nt_notify_stream_name), NT_NOTIFY_STREAM_NAME, "Notify on changes to stream name?", HFILL }},

	{ &hf_smb_nt_notify_security,
		{ "Security Change", "smb.nt.notify.security", FT_BOOLEAN, 32,
		TFS(&tfs_nt_notify_security), NT_NOTIFY_SECURITY, "Notify on changes to security settings", HFILL }},

	{ &hf_smb_nt_notify_ea,
		{ "EA Change", "smb.nt.notify.ea", FT_BOOLEAN, 32,
		TFS(&tfs_nt_notify_ea), NT_NOTIFY_EA, "Notify on changes to Extended Attributes", HFILL }},

	{ &hf_smb_nt_notify_creation,
		{ "Created Change", "smb.nt.notify.creation", FT_BOOLEAN, 32,
		TFS(&tfs_nt_notify_creation), NT_NOTIFY_CREATION, "Notify on changes to creation time", HFILL }},

	{ &hf_smb_nt_notify_last_access,
		{ "Last Access Change", "smb.nt.notify.last_access", FT_BOOLEAN, 32,
		TFS(&tfs_nt_notify_last_access), NT_NOTIFY_LAST_ACCESS, "Notify on changes to last access", HFILL }},

	{ &hf_smb_nt_notify_last_write,
		{ "Last Write Change", "smb.nt.notify.last_write", FT_BOOLEAN, 32,
		TFS(&tfs_nt_notify_last_write), NT_NOTIFY_LAST_WRITE, "Notify on changes to last write", HFILL }},

	{ &hf_smb_nt_notify_size,
		{ "Size Change", "smb.nt.notify.size", FT_BOOLEAN, 32,
		TFS(&tfs_nt_notify_size), NT_NOTIFY_SIZE, "Notify on changes to size", HFILL }},

	{ &hf_smb_nt_notify_attributes,
		{ "Attribute Change", "smb.nt.notify.attributes", FT_BOOLEAN, 32,
		TFS(&tfs_nt_notify_attributes), NT_NOTIFY_ATTRIBUTES, "Notify on changes to attributes", HFILL }},

	{ &hf_smb_nt_notify_dir_name,
		{ "Directory Name Change", "smb.nt.notify.dir_name", FT_BOOLEAN, 32,
		TFS(&tfs_nt_notify_dir_name), NT_NOTIFY_DIR_NAME, "Notify on changes to directory name", HFILL }},

	{ &hf_smb_nt_notify_file_name,
		{ "File Name Change", "smb.nt.notify.file_name", FT_BOOLEAN, 32,
		TFS(&tfs_nt_notify_file_name), NT_NOTIFY_FILE_NAME, "Notify on changes to file name", HFILL }},

	{ &hf_smb_root_dir_fid,
		{ "Root FID", "smb.rfid", FT_UINT32, BASE_HEX,
		NULL, 0, "Open is relative to this FID (if nonzero)", HFILL }},

	{ &hf_smb_alloc_size64,
		{ "Allocation Size", "smb.alloc_size", FT_UINT64, BASE_DEC,
		NULL, 0, "Number of bytes to reserve on create or truncate", HFILL }},

	{ &hf_smb_nt_create_disposition,
		{ "Disposition", "smb.create.disposition", FT_UINT32, BASE_DEC,
		VALS(create_disposition_vals), 0, "Create disposition, what to do if the file does/does not exist", HFILL }},

	{ &hf_smb_nt_create_options,
		{ "Options", "smb.create.options", FT_UINT32, BASE_DEC,
		NULL, 0, "What to do if creating a file", HFILL }},

	{ &hf_smb_sd_length,
		{ "SD Length", "smb.sd.length", FT_UINT32, BASE_DEC,
		NULL, 0, "Total length of security descriptor", HFILL }},

	{ &hf_smb_ea_length,
		{ "EA Length", "smb.ea.length", FT_UINT32, BASE_DEC,
		NULL, 0, "Total EA length for opened file", HFILL }},

	{ &hf_smb_file_name_len,
		{ "File Name Len", "smb.file_name_len", FT_UINT32, BASE_DEC,
		NULL, 0, "Length of File Name", HFILL }},

	{ &hf_smb_nt_impersonation_level,
		{ "Impersonation", "smb.impersonation.level", FT_UINT32, BASE_DEC,
		VALS(impersonation_level_vals), 0, "Impersonation level", HFILL }},

	{ &hf_smb_nt_security_flags_context_tracking,
		{ "Context Tracking", "smb.security.flags.context_tracking", FT_BOOLEAN, 8,
		TFS(&tfs_nt_security_flags_context_tracking), 0x01, "Is security tracking static or dynamic?", HFILL }},

	{ &hf_smb_nt_security_flags_effective_only,
		{ "Effective Only", "smb.security.flags.effective_only", FT_BOOLEAN, 8,
		TFS(&tfs_nt_security_flags_effective_only), 0x02, "Are only enabled or all aspects uf the users SID available?", HFILL }},

	{ &hf_smb_nt_create_bits_oplock,
		{ "Exclusive Oplock", "smb.nt.create.oplock", FT_BOOLEAN, 32,
		TFS(&tfs_nt_create_bits_oplock), 0x00000002, "Is an oplock requested", HFILL }},

	{ &hf_smb_nt_create_bits_boplock,
		{ "Batch Oplock", "smb.nt.create.batch_oplock", FT_BOOLEAN, 32,
		TFS(&tfs_nt_create_bits_boplock), 0x00000004, "Is a batch oplock requested?", HFILL }},

	{ &hf_smb_nt_create_bits_dir,
		{ "Create Directory", "smb.nt.create.dir", FT_BOOLEAN, 32,
		TFS(&tfs_nt_create_bits_dir), 0x00000008, "Must target of open be a directory?", HFILL }},

	{ &hf_smb_nt_access_mask_generic_read,
		{ "Generic Read", "smb.access.generic_read", FT_BOOLEAN, 32,
		TFS(&tfs_nt_access_mask_generic_read), 0x80000000, "Is generic read allowed for this object?", HFILL }},

	{ &hf_smb_nt_access_mask_generic_write,
		{ "Generic Write", "smb.access.generic_write", FT_BOOLEAN, 32,
		TFS(&tfs_nt_access_mask_generic_write), 0x40000000, "Is generic write allowed for this object?", HFILL }},

	{ &hf_smb_nt_access_mask_generic_execute,
		{ "Generic Execute", "smb.access.generic_execute", FT_BOOLEAN, 32,
		TFS(&tfs_nt_access_mask_generic_execute), 0x20000000, "Is generic execute allowed for this object?", HFILL }},

	{ &hf_smb_nt_access_mask_generic_all,
		{ "Generic All", "smb.access.generic_all", FT_BOOLEAN, 32,
		TFS(&tfs_nt_access_mask_generic_all), 0x10000000, "Is generic all allowed for this attribute", HFILL }},

	{ &hf_smb_nt_access_mask_maximum_allowed,
		{ "Maximum Allowed", "smb.access.maximum_allowed", FT_BOOLEAN, 32,
		TFS(&tfs_nt_access_mask_maximum_allowed), 0x02000000, "?", HFILL }},

	{ &hf_smb_nt_access_mask_system_security,
		{ "System Security", "smb.access.system_security", FT_BOOLEAN, 32,
		TFS(&tfs_nt_access_mask_system_security), 0x01000000, "Access to a system ACL?", HFILL }},

	{ &hf_smb_nt_access_mask_synchronize,
		{ "Synchronize", "smb.access.synchronize", FT_BOOLEAN, 32,
		TFS(&tfs_nt_access_mask_synchronize), 0x00100000, "Windows NT: synchronize access", HFILL }},

	{ &hf_smb_nt_access_mask_write_owner,
		{ "Write Owner", "smb.access.write_owner", FT_BOOLEAN, 32,
		TFS(&tfs_nt_access_mask_write_owner), 0x00080000, "Can owner write to the object?", HFILL }},

	{ &hf_smb_nt_access_mask_write_dac,
		{ "Write DAC", "smb.access.write_dac", FT_BOOLEAN, 32,
		TFS(&tfs_nt_access_mask_write_dac), 0x00040000, "Is write allowed to the owner group or ACLs?", HFILL }},

	{ &hf_smb_nt_access_mask_read_control,
		{ "Read Control", "smb.access.read_control", FT_BOOLEAN, 32,
		TFS(&tfs_nt_access_mask_read_control), 0x00020000, "Are reads allowed of owner, group and ACL data of the SID?", HFILL }},

	{ &hf_smb_nt_access_mask_delete,
		{ "Delete", "smb.access.delete", FT_BOOLEAN, 32,
		TFS(&tfs_nt_access_mask_delete), 0x00010000, "Can object be deleted", HFILL }},

	{ &hf_smb_nt_share_access_read,
		{ "Read", "smb.share.access.read", FT_BOOLEAN, 32,
		TFS(&tfs_nt_share_access_read), 0x00000001, "Can the object be shared for reading?", HFILL }},

	{ &hf_smb_nt_share_access_write,
		{ "Write", "smb.share.access.write", FT_BOOLEAN, 32,
		TFS(&tfs_nt_share_access_write), 0x00000002, "Can the object be shared for write?", HFILL }},

	{ &hf_smb_nt_share_access_delete,
		{ "Delete", "smb.share.access.delete", FT_BOOLEAN, 32,
		TFS(&tfs_nt_share_access_delete), 0x00000004, "", HFILL }},

	{ &hf_smb_file_eattr_read_only,
		{ "Read Only", "smb.file.attribute.read_only", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_read_only), FILE_ATTRIBUTE_READ_ONLY, "READ ONLY file attribute", HFILL }},

	{ &hf_smb_file_eattr_hidden,
		{ "Hidden", "smb.file.attribute.hidden", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_hidden), FILE_ATTRIBUTE_HIDDEN, "HIDDEN file attribute", HFILL }},

	{ &hf_smb_file_eattr_system,
		{ "System", "smb.file.attribute.system", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_system), FILE_ATTRIBUTE_SYSTEM, "SYSTEM file attribute", HFILL }},

	{ &hf_smb_file_eattr_volume,
		{ "Volume ID", "smb.file.attribute.volume", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_volume), FILE_ATTRIBUTE_VOLUME, "VOLUME file attribute", HFILL }},

	{ &hf_smb_file_eattr_directory,
		{ "Directory", "smb.file.attribute.directory", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_directory), FILE_ATTRIBUTE_DIRECTORY, "DIRECTORY file attribute", HFILL }},

	{ &hf_smb_file_eattr_archive,
		{ "Archive", "smb.file.attribute.archive", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_archive), FILE_ATTRIBUTE_ARCHIVE, "ARCHIVE file attribute", HFILL }},

	{ &hf_smb_file_eattr_device,
		{ "Device", "smb.file.attribute.device", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_device), FILE_ATTRIBUTE_DEVICE, "Is this file a device?", HFILL }},

	{ &hf_smb_file_eattr_normal,
		{ "Normal", "smb.file.attribute.normal", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_normal), FILE_ATTRIBUTE_NORMAL, "Is this a normal file?", HFILL }},

	{ &hf_smb_file_eattr_temporary,
		{ "Temporary", "smb.file.attribute.temporary", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_temporary), FILE_ATTRIBUTE_TEMPORARY, "Is this a temporary file?", HFILL }},

	{ &hf_smb_file_eattr_sparse,
		{ "Sparse", "smb.file.attribute.sparse", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_sparse), FILE_ATTRIBUTE_SPARSE, "Is this a sparse file?", HFILL }},

	{ &hf_smb_file_eattr_reparse,
		{ "Reparse Point", "smb.file.attribute.reparse", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_reparse), FILE_ATTRIBUTE_REPARSE, "Does this file have an associated reparse point?", HFILL }},

	{ &hf_smb_file_eattr_compressed,
		{ "Compressed", "smb.file.attribute.compressed", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_compressed), FILE_ATTRIBUTE_COMPRESSED, "Is this file compressed?", HFILL }},

	{ &hf_smb_file_eattr_offline,
		{ "Offline", "smb.file.attribute.offline", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_offline), FILE_ATTRIBUTE_OFFLINE, "Is this file offline?", HFILL }},

	{ &hf_smb_file_eattr_not_content_indexed,
		{ "Content Indexed", "smb.file.attribute.not_content_indexed", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_not_content_indexed), FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, "May this file be indexed by the content indexing service", HFILL }},

	{ &hf_smb_file_eattr_encrypted,
		{ "Encrypted", "smb.file.attribute.encrypted", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_encrypted), FILE_ATTRIBUTE_ENCRYPTED, "Is this file encrypted?", HFILL }},

	{ &hf_smb_file_eattr_write_through,
		{ "Write Through", "smb.file.attribute.write_through", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_write_through), FILE_ATTRIBUTE_WRITE_THROUGH, "Does this object need write through?", HFILL }},

	{ &hf_smb_file_eattr_no_buffering,
		{ "No Buffering", "smb.file.attribute.no_buffering", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_no_buffering), FILE_ATTRIBUTE_NO_BUFFERING, "May the server buffer this object?", HFILL }},

	{ &hf_smb_file_eattr_random_access,
		{ "Random Access", "smb.file.attribute.random_access", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_random_access), FILE_ATTRIBUTE_RANDOM_ACCESS, "Optimize for random access", HFILL }},

	{ &hf_smb_file_eattr_sequential_scan,
		{ "Sequential Scan", "smb.file.attribute.sequential_scan", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_sequential_scan), FILE_ATTRIBUTE_SEQUENTIAL_SCAN, "Optimize for sequential scan", HFILL }},

	{ &hf_smb_file_eattr_delete_on_close,
		{ "Delete on Close", "smb.file.attribute.delete_on_close", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_delete_on_close), FILE_ATTRIBUTE_DELETE_ON_CLOSE, "Should this object be deleted on close?", HFILL }},

	{ &hf_smb_file_eattr_backup_semantics,
		{ "Backup", "smb.file.attribute.backup_semantics", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_backup_semantics), FILE_ATTRIBUTE_BACKUP_SEMANTICS, "Does this object need/support backup semantics", HFILL }},

	{ &hf_smb_file_eattr_posix_semantics,
		{ "Posix", "smb.file.attribute.posix_semantics", FT_BOOLEAN, 32,
		TFS(&tfs_file_attribute_posix_semantics), FILE_ATTRIBUTE_POSIX_SEMANTICS, "Does this object need/support POSIX semantics?", HFILL }},

	{ &hf_smb_security_descriptor_len,
		{ "Security Descriptor Length", "smb.sec_desc_len", FT_UINT32, BASE_DEC,
		NULL, 0, "Security Descriptor Length", HFILL }},

	{ &hf_smb_security_descriptor,
		{ "Security Descriptor", "smb.sec_desc", FT_BYTES, BASE_HEX,
		NULL, 0, "Security Descriptor", HFILL }},

	{ &hf_smb_nt_qsd_owner,
		{ "Owner", "smb.nt_qsd.owner", FT_BOOLEAN, 32,
		TFS(&tfs_nt_qsd_owner), NT_QSD_OWNER, "Is owner security informaton being queried?", HFILL }},

	{ &hf_smb_nt_qsd_group,
		{ "Group", "smb.nt_qsd.group", FT_BOOLEAN, 32,
		TFS(&tfs_nt_qsd_group), NT_QSD_GROUP, "Is group security informaton being queried?", HFILL }},

	{ &hf_smb_nt_qsd_dacl,
		{ "DACL", "smb.nt_qsd.dacl", FT_BOOLEAN, 32,
		TFS(&tfs_nt_qsd_dacl), NT_QSD_DACL, "Is DACL security informaton being queried?", HFILL }},

	{ &hf_smb_nt_qsd_sacl,
		{ "SACL", "smb.nt_qsd.sacl", FT_BOOLEAN, 32,
		TFS(&tfs_nt_qsd_sacl), NT_QSD_SACL, "Is SACL security informaton being queried?", HFILL }},

	{ &hf_smb_extended_attributes,
		{ "Extended Attributes", "smb.ext_attr", FT_BYTES, BASE_HEX,
		NULL, 0, "Extended Attributes", HFILL }},

	{ &hf_smb_oplock_level,
		{ "Oplock level", "smb.oplock.level", FT_UINT8, BASE_DEC,
		VALS(oplock_level_vals), 0, "Level of oplock granted", HFILL }},

	{ &hf_smb_create_action,
		{ "Create action", "smb.create.action", FT_UINT32, BASE_DEC,
		VALS(create_disposition_vals), 0, "Type of action taken", HFILL }},

	{ &hf_smb_ea_error_offset,
		{ "EA Error offset", "smb.ea.error_offset", FT_UINT32, BASE_DEC,
		NULL, 0, "Offset into EA list if EA error", HFILL }},

	{ &hf_smb_end_of_file,
		{ "End Of File", "smb.end_of_file", FT_UINT64, BASE_DEC,
		NULL, 0, "Offset to the first free byte in the file", HFILL }},

	{ &hf_smb_device_type,
		{ "Device Type", "smb.device.type", FT_UINT32, BASE_HEX,
		VALS(device_type_vals), 0, "Type of device", HFILL }},

	{ &hf_smb_is_directory,
		{ "Is Directory", "smb.is_directory", FT_UINT8, BASE_DEC,
		VALS(is_directory_vals), 0, "Is this object a directory?", HFILL }},

	{ &hf_smb_next_entry_offset,
		{ "Next Entry Offset", "smb.next_entry_offset", FT_UINT32, BASE_DEC,
		NULL, 0, "Offset to next entry", HFILL }},

	{ &hf_smb_change_time,
		{ "Change", "smb.change.time", FT_ABSOLUTE_TIME, BASE_NONE,
		NULL, 0, "Last Change Time", HFILL }},


	};
	static gint *ett[] = {
		&ett_smb,
		&ett_smb_hdr,
		&ett_smb_command,
		&ett_smb_fileattributes,
		&ett_smb_capabilities,
		&ett_smb_aflags,
		&ett_smb_dialect,
		&ett_smb_dialects,
		&ett_smb_mode,
		&ett_smb_rawmode,
		&ett_smb_flags,
		&ett_smb_flags2,
		&ett_smb_desiredaccess,
		&ett_smb_search,
		&ett_smb_file,
		&ett_smb_openfunction,
		&ett_smb_filetype,
		&ett_smb_openaction,
		&ett_smb_writemode,
		&ett_smb_lock_type,
		&ett_smb_ssetupandxaction,
		&ett_smb_optionsup,
		&ett_smb_time_date,
		&ett_smb_64bit_time,
		&ett_smb_move_flags,
		&ett_smb_file_attributes,
		&ett_smb_search_resume_key,
		&ett_smb_search_dir_info,
		&ett_smb_unlocks,
		&ett_smb_unlock,
		&ett_smb_locks,
		&ett_smb_lock,
		&ett_smb_open_flags,
		&ett_smb_open_action,
		&ett_smb_setup_action,
		&ett_smb_connect_flags,
		&ett_smb_connect_support_bits,
		&ett_smb_nt_create_bits,
		&ett_smb_nt_access_mask,
		&ett_smb_nt_share_access,
		&ett_smb_nt_security_flags,
		&ett_smb_nt_trans_setup,
		&ett_smb_nt_notify_completion_filter,
		&ett_smb_nt_ioctl_flags,
		&ett_smb_security_information_mask,
	};

	proto_smb = proto_register_protocol("SMB (Server Message Block Protocol)",
	    "SMB", "smb");

	proto_register_subtree_array(ett, array_length(ett));
	proto_register_field_array(proto_smb, hf, array_length(hf));
	register_init_routine(&smb_init_protocol);
	register_init_routine(&smb_info_init);

	register_proto_smb_browse();
	register_proto_smb_logon();
	register_proto_smb_mailslot();
	register_proto_smb_pipe();
}

void
proto_reg_handoff_smb(void)
{
	heur_dissector_add("netbios", dissect_smb, proto_smb);
}
