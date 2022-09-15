/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/pp_file_handle.idl modified Fri Jul 27 17:01:41 2012. */

#ifndef PPAPI_C_PRIVATE_PP_FILE_HANDLE_H_
#define PPAPI_C_PRIVATE_PP_FILE_HANDLE_H_

#include "ppapi/c/pp_macros.h"

/**
 * @file
 * This file provides support for native OS file handles.
 */



#ifdef _WIN32
#include<windows.h>
typedef HANDLE PP_FileHandle;
static const PP_FileHandle PP_kInvalidFileHandle = NULL;

#else
typedef int PP_FileHandle;
static const PP_FileHandle PP_kInvalidFileHandle = -1;
#endif

#endif  /* PPAPI_C_PRIVATE_PP_FILE_HANDLE_H_ */

