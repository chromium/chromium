/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From pp_directory_entry.idl modified Tue Apr 30 05:44:50 2013. */

#ifndef PPAPI_C_PP_DIRECTORY_ENTRY_H_
#define PPAPI_C_PP_DIRECTORY_ENTRY_H_

#include "ppapi/c/pp_file_info.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

/**
 * @file
 *
 * This file defines the <code>PP_DirectoryEntry</code> struct.
 */


/**
 * @addtogroup Structs
 * @{
 */
struct PP_DirectoryEntry {
  PP_Resource file_ref;
  PP_FileType file_type;
};
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_DirectoryEntry, 8);
/**
 * @}
 */

#endif  /* PPAPI_C_PP_DIRECTORY_ENTRY_H_ */

