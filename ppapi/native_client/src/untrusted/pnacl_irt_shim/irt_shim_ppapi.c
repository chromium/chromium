/*
 * Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "ppapi/native_client/src/untrusted/pnacl_irt_shim/irt_shim_ppapi.h"

#include <stdint.h>

#include "native_client/src/untrusted/irt/irt.h"
#include "ppapi/nacl_irt/irt_ppapi.h"
#include "ppapi/nacl_irt/public/irt_ppapi.h"
#include "ppapi/native_client/src/untrusted/pnacl_irt_shim/pnacl_shim.h"

/*
 * Defines a version of the version irt_ppapi_start and of the irt_ppapihook
 * that returns a shimmed GetInterface for PNaCl.
 *
 * The hook will be linked into the IRT but it is considered unstable.
 * Stable nexes should not use that IRT hook (and a filter prevents
 * it from being used). Instead PNaCl nexes should embed the
 * irt_shim_ppapi_start and the shim functions directly into the nexe
 * for ABI stability.
 */

static struct PP_StartFunctions g_user_start_functions;

static int32_t shim_PPPInitializeModule(PP_Module module_id,
                                        PPB_GetInterface get_browser_intf) {
  /* Record the original PPB_GetInterface and provide a shimmed one. */
  __set_real_Pnacl_PPBGetInterface(get_browser_intf);
  return (*g_user_start_functions.PPP_InitializeModule)(
      module_id,
      &__Pnacl_PPBGetInterface);
}

static void shim_PPPShutdownModule(void) {
  (*g_user_start_functions.PPP_ShutdownModule)();
}

#ifdef PNACL_SHIM_AOT
 /*
  * This will be discovered and set by the shim, since we cannot link
  * against the IRT directly in the AOT library.
  */
int (*real_irt_ppapi_start)(const struct PP_StartFunctions *) = NULL;
#else
 /*
  * Otherwise, when linking directly into the IRT, we can refer to the
  * real irt_ppapi_start from irt_ppapi.
  */
static int (* const real_irt_ppapi_start)(const struct PP_StartFunctions *) =
    &irt_ppapi_start;
#endif

int irt_shim_ppapi_start(const struct PP_StartFunctions *funcs) {
  g_user_start_functions = *funcs;
  /*
   * Record the original PPP_GetInterface and provide a shimmed one
   * via wrapped_ppapi_methods.
   */
  const struct PP_StartFunctions wrapped_ppapi_methods = {
    shim_PPPInitializeModule,
    shim_PPPShutdownModule,
    __Pnacl_PPPGetInterface
  };
  __set_real_Pnacl_PPPGetInterface(g_user_start_functions.PPP_GetInterface);
  /*
   * Invoke the IRT's ppapi_start interface with the wrapped interface.
   */
  return (*real_irt_ppapi_start)(&wrapped_ppapi_methods);
}

#ifndef PNACL_SHIM_AOT
const struct nacl_irt_ppapihook nacl_irt_ppapihook_pnacl_private = {
  irt_shim_ppapi_start,
  PpapiPluginRegisterThreadCreator,
};
#endif
