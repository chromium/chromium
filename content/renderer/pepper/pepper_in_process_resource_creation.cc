// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_in_process_resource_creation.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "content/child/browser_font_resource_trusted.h"
#include "content/renderer/pepper/pepper_in_process_router.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/ppapi_preferences_builder.h"
#include "content/renderer/pepper/renderer_ppapi_host_impl.h"
#include "gpu/config/gpu_feature_info.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/file_chooser_resource.h"
#include "ppapi/proxy/file_io_resource.h"
#include "ppapi/proxy/file_ref_resource.h"
#include "ppapi/proxy/file_system_resource.h"
#include "ppapi/proxy/graphics_2d_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/printing_resource.h"
#include "ppapi/proxy/url_loader_resource.h"
#include "ppapi/proxy/url_request_info_resource.h"
#include "ppapi/proxy/url_response_info_resource.h"
#include "ppapi/proxy/websocket_resource.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/var.h"
#include "third_party/blink/public/web/web_view.h"

// Note that the code in the creation functions in this file should generally
// be the same as that in ppapi/proxy/resource_creation_proxy.cc. See
// pepper_in_process_resource_creation.h for what this file is for.

namespace content {

// PepperInProcessResourceCreation --------------------------------------------

PepperInProcessResourceCreation::PepperInProcessResourceCreation(
    RendererPpapiHostImpl* host_impl,
    PepperPluginInstanceImpl* instance)
    : ResourceCreationImpl(instance), host_impl_(host_impl) {}

PepperInProcessResourceCreation::~PepperInProcessResourceCreation() {}

PP_Resource PepperInProcessResourceCreation::CreateBrowserFont(
    PP_Instance instance,
    const PP_BrowserFont_Trusted_Description* description) {
  if (!BrowserFontResource_Trusted::IsPPFontDescriptionValid(*description))
    return 0;
  // BrowserFontResource_Trusted and in turn PPFontDescToWebFontDesc do not
  // care about preferences of GPU features, so no need to query them from
  // GPU process whether these features are blacklisted or not.
  gpu::GpuFeatureInfo gpu_feature_info;
  ppapi::Preferences prefs(PpapiPreferencesBuilder::Build(
      host_impl_->GetRenderFrameForInstance(instance)
          ->GetWebView()
          ->GetWebPreferences(),
      gpu_feature_info));
  return (new BrowserFontResource_Trusted(
              host_impl_->in_process_router()->GetPluginConnection(instance),
              instance,
              *description,
              prefs))->GetReference();
}

PP_Resource PepperInProcessResourceCreation::CreateFileChooser(
    PP_Instance instance,
    PP_FileChooserMode_Dev mode,
    const PP_Var& accept_types) {
  scoped_refptr<ppapi::StringVar> string_var =
      ppapi::StringVar::FromPPVar(accept_types);
  std::string str = string_var.get() ? string_var->value() : std::string();
  return (new ppapi::proxy::FileChooserResource(
              host_impl_->in_process_router()->GetPluginConnection(instance),
              instance,
              mode,
              str.c_str()))->GetReference();
}

PP_Resource PepperInProcessResourceCreation::CreateFileIO(
    PP_Instance instance) {
  return (new ppapi::proxy::FileIOResource(
              host_impl_->in_process_router()->GetPluginConnection(instance),
              instance))->GetReference();
}

PP_Resource PepperInProcessResourceCreation::CreateFileRef(
    PP_Instance instance,
    const ppapi::FileRefCreateInfo& create_info) {
  return ppapi::proxy::FileRefResource::CreateFileRef(
      host_impl_->in_process_router()->GetPluginConnection(instance),
      instance,
      create_info);
}

PP_Resource PepperInProcessResourceCreation::CreateFileSystem(
    PP_Instance instance,
    PP_FileSystemType type) {
  return (new ppapi::proxy::FileSystemResource(
              host_impl_->in_process_router()->GetPluginConnection(instance),
              instance,
              type))->GetReference();
}

PP_Resource PepperInProcessResourceCreation::CreateGraphics2D(
    PP_Instance instance,
    const PP_Size* size,
    PP_Bool is_always_opaque) {
  return (new ppapi::proxy::Graphics2DResource(
              host_impl_->in_process_router()->GetPluginConnection(instance),
              instance,
              *size,
              is_always_opaque))->GetReference();
}

PP_Resource PepperInProcessResourceCreation::CreatePrinting(
    PP_Instance instance) {
  return (new ppapi::proxy::PrintingResource(
              host_impl_->in_process_router()->GetPluginConnection(instance),
              instance))->GetReference();
}

PP_Resource PepperInProcessResourceCreation::CreateURLLoader(
    PP_Instance instance) {
  return (new ppapi::proxy::URLLoaderResource(
              host_impl_->in_process_router()->GetPluginConnection(instance),
              instance))->GetReference();
}

PP_Resource PepperInProcessResourceCreation::CreateURLRequestInfo(
    PP_Instance instance) {
  return (new ppapi::proxy::URLRequestInfoResource(
              host_impl_->in_process_router()->GetPluginConnection(instance),
              instance,
              ppapi::URLRequestInfoData()))->GetReference();
}

PP_Resource PepperInProcessResourceCreation::CreateWebSocket(
    PP_Instance instance) {
  return (new ppapi::proxy::WebSocketResource(
              host_impl_->in_process_router()->GetPluginConnection(instance),
              instance))->GetReference();
}

}  // namespace content
