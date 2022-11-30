/*
 * Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef PPAPI_NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PPRUNTIME_H_
#define PPAPI_NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PPRUNTIME_H_

#include "native_client/src/include/portability.h"
#include "ppapi/nacl_irt/public/irt_ppapi.h"

EXTERN_C_BEGIN

// The entry point for the main thread of the PPAPI plugin process.
int PpapiPluginMain(void);

EXTERN_C_END

#endif  // PPAPI_NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PPRUNTIME_H_
