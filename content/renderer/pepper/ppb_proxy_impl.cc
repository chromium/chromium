// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/ppb_proxy_impl.h"

#include "ppapi/c/private/ppb_proxy_private.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_image_data_api.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/plugin_module.h"

using ppapi::PpapiGlobals;
using ppapi::thunk::EnterResource;
using ppapi::thunk::PPB_URLLoader_API;

namespace content {

namespace {

void PluginCrashed(PP_Module module) {
  PluginModule* plugin_module = HostGlobals::Get()->GetModule(module);
  if (plugin_module)
    plugin_module->PluginCrashed();
}

PP_Instance GetInstanceForResource(PP_Resource resource) {
  ppapi::Resource* obj =
      PpapiGlobals::Get()->GetResourceTracker()->GetResource(resource);
  if (!obj)
    return 0;
  return obj->pp_instance();
}

void SetReserveInstanceIDCallback(PP_Module module,
                                  PP_Bool (*reserve)(PP_Module, PP_Instance)) {
  PluginModule* plugin_module = HostGlobals::Get()->GetModule(module);
  if (plugin_module)
    plugin_module->SetReserveInstanceIDCallback(reserve);
}

void AddRefModule(PP_Module module) {
  PluginModule* plugin_module = HostGlobals::Get()->GetModule(module);
  if (plugin_module)
    plugin_module->AddRef();
}

void ReleaseModule(PP_Module module) {
  PluginModule* plugin_module = HostGlobals::Get()->GetModule(module);
  if (plugin_module)
    plugin_module->Release();
}

PP_Bool IsInModuleDestructor(PP_Module module) {
  PluginModule* plugin_module = HostGlobals::Get()->GetModule(module);
  if (plugin_module)
    return PP_FromBool(plugin_module->is_in_destructor());
  return PP_FALSE;
}

const PPB_Proxy_Private ppb_proxy = {
    &PluginCrashed, &GetInstanceForResource, &SetReserveInstanceIDCallback,
    &AddRefModule,  &ReleaseModule,          &IsInModuleDestructor};

}  // namespace

// static
const PPB_Proxy_Private* PPB_Proxy_Impl::GetInterface() { return &ppb_proxy; }

}  // namespace content
