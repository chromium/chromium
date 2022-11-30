// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// When used in conjunction with module_embedder.h, this gives a default
// implementation of ppp.h for clients of the ppapi C++ interface.  Most
// plugin implementors can export their derivation of Module by just
// linking to this implementation.

#include <stdint.h>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppp.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_embedder.h"

static pp::Module* g_module_singleton = NULL;
static PP_GetInterface_Func g_broker_get_interface = NULL;

namespace pp {

// Give a default implementation of Module::Get().  See module.cc for details.
pp::Module* Module::Get() {
  return g_module_singleton;
}

void SetBrokerGetInterfaceFunc(PP_GetInterface_Func broker_get_interface) {
  g_broker_get_interface = broker_get_interface;
}

}  // namespace pp

// Global PPP functions --------------------------------------------------------

PP_EXPORT int32_t PPP_InitializeModule(PP_Module module_id,
                                       PPB_GetInterface get_browser_interface) {
  pp::Module* module = pp::CreateModule();
  if (!module)
    return PP_ERROR_FAILED;

  if (!module->InternalInit(module_id, get_browser_interface)) {
    delete module;
    return PP_ERROR_FAILED;
  }
  g_module_singleton = module;
  return PP_OK;
}

PP_EXPORT void PPP_ShutdownModule() {
  delete g_module_singleton;
  g_module_singleton = NULL;
}

PP_EXPORT const void* PPP_GetInterface(const char* interface_name) {
  if (g_module_singleton)
    return g_module_singleton->GetPluginInterface(interface_name);
  if (g_broker_get_interface)
    return g_broker_get_interface(interface_name);
  return NULL;
}
