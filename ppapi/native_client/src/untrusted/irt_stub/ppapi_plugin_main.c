/*
 * Copyright 2011 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ppapi/native_client/src/shared/ppapi_proxy/ppruntime.h"

#include "ppapi/native_client/src/untrusted/irt_stub/ppapi_start.h"

/*
 * These are dangling references to functions that the application must define.
 */
static const struct PP_StartFunctions ppapi_app_start_callbacks = {
  PPP_InitializeModule,
  PPP_ShutdownModule,
  PPP_GetInterface
};

/*
 * The application's main (or the one supplied in this library) calls this
 * to start the PPAPI world.
 */
int PpapiPluginMain(void) {
  return PpapiPluginStart(&ppapi_app_start_callbacks);
}
