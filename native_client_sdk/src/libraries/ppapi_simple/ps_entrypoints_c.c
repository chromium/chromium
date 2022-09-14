/* Copyright 2015 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_module.h>
#include <ppapi/c/ppb.h>

#include "ppapi_simple/ps_interface.h"
#include "ppapi_simple/ps_internal.h"

int32_t PPP_InitializeModule(PP_Module module, PPB_GetInterface get_interface) {
  g_ps_get_interface = get_interface;
  PSInterfaceInit();
  return PP_OK;
}

const void* PPP_GetInterface(const char* interface_name) {
  return PSGetInterfaceImplementation(interface_name);
}

void PPP_ShutdownModule(void) {
}
