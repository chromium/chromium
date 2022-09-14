// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_module.h>
#include <ppapi/c/ppb.h>
#include <ppapi/c/ppp.h>
#include <ppapi/cpp/instance.h>
#include <ppapi/cpp/module.h>

#include "ppapi_simple/ps_interface.h"
#include "ppapi_simple/ps_internal.h"

class PSModule : public pp::Module {
 public:
  virtual pp::Instance* CreateInstance(PP_Instance instance) {
    // Should not get here.
    // This is only called by libppapi_cpp in Instance_DidCreate. That function
    // is called by the PPP_Instance handler in libppapi_cpp, but we handle all
    // plugin interfaces in ppapi_simple.
    assert(0);
    return NULL;
  }
};

static PSModule* s_module;

namespace pp {

Module* Module::Get() {
  return s_module;
}

// This shouldn't be called (it is only referenced by PPP_InitializeModule in
// ppapi_cpp, which we override), but is needed to successfully link.
Module* CreateModule() {
  assert(0);
  return NULL;
}

}  // namespace pp

int32_t PPP_InitializeModule(PP_Module module_id,
                             PPB_GetInterface get_interface) {
  g_ps_get_interface = get_interface;
  PSInterfaceInit();

  PSModule* module = new PSModule();
  if (!module->InternalInit(module_id, get_interface)) {
    delete s_module;
    return PP_ERROR_FAILED;
  }
  s_module = module;
  return PP_OK;
}

const void* PPP_GetInterface(const char* interface_name) {
  return PSGetInterfaceImplementation(interface_name);
}

void PPP_ShutdownModule(void) {
  delete s_module;
  s_module = NULL;
}
