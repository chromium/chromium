/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppp_pexe_stream_handler.idl,
 *   modified Wed Aug  6 13:11:06 2014.
 */

#ifndef PPAPI_C_PRIVATE_PPP_PEXE_STREAM_HANDLER_H_
#define PPAPI_C_PRIVATE_PPP_PEXE_STREAM_HANDLER_H_

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

#define PPP_PEXESTREAMHANDLER_INTERFACE_1_0 "PPP_PexeStreamHandler;1.0"
#define PPP_PEXESTREAMHANDLER_INTERFACE PPP_PEXESTREAMHANDLER_INTERFACE_1_0

/**
 * @file
 * This file contains NaCl private interfaces. This interface is not versioned
 * and is for internal Chrome use. It may change without notice. */


#include "ppapi/c/private/pp_file_handle.h"

/**
 * @addtogroup Interfaces
 * @{
 */
struct PPP_PexeStreamHandler_1_0 {
  /**
   * Invoked as a result of a cache hit for a translated pexe.
   */
  void (*DidCacheHit)(void* user_data, PP_FileHandle nexe_file_handle);
  /**
   * Invoked as a result of a cache miss for a translated pexe.
   * Provides the expected length of the pexe, as read from HTTP headers.
   */
  void (*DidCacheMiss)(void* user_data,
                       int64_t expected_total_length,
                       PP_FileHandle temp_nexe_file);
  /**
   * Invoked when a block of data has been downloaded.
   * Only invoked after DidCacheMiss().
   */
  void (*DidStreamData)(void* user_data, const void* data, int32_t length);
  /**
   * Invoked when the stream has finished downloading, regardless of whether it
   * succeeded. Not invoked if DidCacheHit() was called.
   */
  void (*DidFinishStream)(void* user_data, int32_t pp_error);
};

typedef struct PPP_PexeStreamHandler_1_0 PPP_PexeStreamHandler;
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPP_PEXE_STREAM_HANDLER_H_ */

