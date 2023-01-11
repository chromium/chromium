// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/renderer_ppapi_host_impl.h"

#include <memory>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/process/process_handle.h"
#include "base/task/single_thread_task_runner.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_browser_connection.h"
#include "content/renderer/pepper/pepper_graphics_2d_host.h"
#include "content/renderer/pepper/pepper_in_process_resource_creation.h"
#include "content/renderer/pepper/pepper_in_process_router.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/render_frame_impl.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_element.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "ui/gfx/geometry/point.h"

namespace content {

// static
RendererPpapiHost* RendererPpapiHost::GetForPPInstance(PP_Instance instance) {
  return RendererPpapiHostImpl::GetForPPInstance(instance);
}

// Out-of-process constructor.
RendererPpapiHostImpl::RendererPpapiHostImpl(
    PluginModule* module,
    ppapi::proxy::HostDispatcher* dispatcher,
    const ppapi::PpapiPermissions& permissions)
    : module_(module),
      dispatcher_(dispatcher),
      is_external_plugin_host_(false) {
  // Hook the PpapiHost up to the dispatcher for out-of-process communication.
  ppapi_host_ =
      std::make_unique<ppapi::host::PpapiHost>(dispatcher, permissions);
  ppapi_host_->AddHostFactoryFilter(std::unique_ptr<ppapi::host::HostFactory>(
      new ContentRendererPepperHostFactory(this)));
  dispatcher->AddFilter(ppapi_host_.get());
  is_running_in_process_ = false;
}

// In-process constructor.
RendererPpapiHostImpl::RendererPpapiHostImpl(
    PluginModule* module,
    const ppapi::PpapiPermissions& permissions)
    : module_(module), dispatcher_(nullptr), is_external_plugin_host_(false) {
  // Hook the host up to the in-process router.
  in_process_router_ = std::make_unique<PepperInProcessRouter>(this);
  ppapi_host_ = std::make_unique<ppapi::host::PpapiHost>(
      in_process_router_->GetRendererToPluginSender(), permissions);
  ppapi_host_->AddHostFactoryFilter(std::unique_ptr<ppapi::host::HostFactory>(
      new ContentRendererPepperHostFactory(this)));
  is_running_in_process_ = true;
}

RendererPpapiHostImpl::~RendererPpapiHostImpl() {
  // Delete the host explicitly first. This shutdown will destroy the
  // resources, which may want to do cleanup in their destructors and expect
  // their pointers to us to be valid.
  ppapi_host_.reset();
}

// static
RendererPpapiHostImpl* RendererPpapiHostImpl::CreateOnModuleForOutOfProcess(
    PluginModule* module,
    ppapi::proxy::HostDispatcher* dispatcher,
    const ppapi::PpapiPermissions& permissions) {
  DCHECK(!module->renderer_ppapi_host());
  RendererPpapiHostImpl* result =
      new RendererPpapiHostImpl(module, dispatcher, permissions);

  // Takes ownership of pointer.
  module->SetRendererPpapiHost(std::unique_ptr<RendererPpapiHostImpl>(result));

  return result;
}

// static
RendererPpapiHostImpl* RendererPpapiHostImpl::CreateOnModuleForInProcess(
    PluginModule* module,
    const ppapi::PpapiPermissions& permissions) {
  DCHECK(!module->renderer_ppapi_host());
  RendererPpapiHostImpl* result =
      new RendererPpapiHostImpl(module, permissions);

  // Takes ownership of pointer.
  module->SetRendererPpapiHost(std::unique_ptr<RendererPpapiHostImpl>(result));

  return result;
}

// static
RendererPpapiHostImpl* RendererPpapiHostImpl::GetForPPInstance(
    PP_Instance pp_instance) {
  PepperPluginInstanceImpl* instance =
      HostGlobals::Get()->GetInstance(pp_instance);
  if (!instance)
    return nullptr;

  // All modules created by content will have their embedder state be the
  // host impl.
  return instance->module()->renderer_ppapi_host();
}

std::unique_ptr<ppapi::thunk::ResourceCreationAPI>
RendererPpapiHostImpl::CreateInProcessResourceCreationAPI(
    PepperPluginInstanceImpl* instance) {
  return std::unique_ptr<ppapi::thunk::ResourceCreationAPI>(
      new PepperInProcessResourceCreation(this, instance));
}

PepperPluginInstanceImpl* RendererPpapiHostImpl::GetPluginInstanceImpl(
    PP_Instance instance) const {
  return GetAndValidateInstance(instance);
}

bool RendererPpapiHostImpl::IsExternalPluginHost() const {
  return is_external_plugin_host_;
}

ppapi::host::PpapiHost* RendererPpapiHostImpl::GetPpapiHost() {
  return ppapi_host_.get();
}

RenderFrame* RendererPpapiHostImpl::GetRenderFrameForInstance(
    PP_Instance instance) {
  PepperPluginInstanceImpl* instance_object = GetAndValidateInstance(instance);
  if (!instance_object)
    return nullptr;

  // Since we're the embedder, we can make assumptions about the helper on
  // the instance and get back to our RenderFrame.
  return instance_object->render_frame();
}

bool RendererPpapiHostImpl::IsValidInstance(PP_Instance instance) {
  return !!GetAndValidateInstance(instance);
}

PepperPluginInstance* RendererPpapiHostImpl::GetPluginInstance(
    PP_Instance instance) {
  return GetAndValidateInstance(instance);
}

blink::WebPluginContainer* RendererPpapiHostImpl::GetContainerForInstance(
    PP_Instance instance) {
  PepperPluginInstanceImpl* instance_object = GetAndValidateInstance(instance);
  if (!instance_object)
    return nullptr;
  return instance_object->container();
}

bool RendererPpapiHostImpl::HasUserGesture(PP_Instance instance) {
  PepperPluginInstanceImpl* instance_object = GetAndValidateInstance(instance);
  if (!instance_object)
    return false;

  if (instance_object->module()->permissions().HasPermission(
          ppapi::PERMISSION_BYPASS_USER_GESTURE))
    return true;
  return instance_object->HasTransientUserActivation();
}

int RendererPpapiHostImpl::GetRoutingIDForFrame(PP_Instance instance) {
  PepperPluginInstanceImpl* plugin_instance = GetAndValidateInstance(instance);
  if (!plugin_instance)
    return 0;
  return GetRenderFrameForInstance(instance)->GetRoutingID();
}

gfx::Point RendererPpapiHostImpl::PluginPointToRenderFrame(
    PP_Instance instance,
    const gfx::Point& pt) {
  PepperPluginInstanceImpl* plugin_instance = GetAndValidateInstance(instance);
  if (!plugin_instance)
    return pt;
  return gfx::Point((pt.x() + plugin_instance->view_data().rect.point.x) /
                        viewport_to_dip_scale_,
                    (pt.y() + plugin_instance->view_data().rect.point.y) /
                        viewport_to_dip_scale_);
}

IPC::PlatformFileForTransit RendererPpapiHostImpl::ShareHandleWithRemote(
    base::PlatformFile handle,
    bool should_close_source) {
  if (!dispatcher_) {
    DCHECK(is_running_in_process_);
    // Duplicate the file handle for in process mode so this function
    // has the same semantics for both in process mode and out of
    // process mode (i.e., the remote side must cloes the handle).
    return IPC::GetPlatformFileForTransit(handle, should_close_source);
  }
  return dispatcher_->ShareHandleWithRemote(handle, should_close_source);
}

base::UnsafeSharedMemoryRegion
RendererPpapiHostImpl::ShareUnsafeSharedMemoryRegionWithRemote(
    const base::UnsafeSharedMemoryRegion& region) {
  if (!dispatcher_) {
    DCHECK(is_running_in_process_);
    return region.Duplicate();
  }
  return dispatcher_->ShareUnsafeSharedMemoryRegionWithRemote(region);
}

base::ReadOnlySharedMemoryRegion
RendererPpapiHostImpl::ShareReadOnlySharedMemoryRegionWithRemote(
    const base::ReadOnlySharedMemoryRegion& region) {
  if (!dispatcher_) {
    DCHECK(is_running_in_process_);
    return region.Duplicate();
  }
  return dispatcher_->ShareReadOnlySharedMemoryRegionWithRemote(region);
}

bool RendererPpapiHostImpl::IsRunningInProcess() {
  return is_running_in_process_;
}

std::string RendererPpapiHostImpl::GetPluginName() {
  return module_->name();
}

void RendererPpapiHostImpl::SetToExternalPluginHost() {
  is_external_plugin_host_ = true;
}

void RendererPpapiHostImpl::CreateBrowserResourceHosts(
    PP_Instance instance,
    const std::vector<IPC::Message>& nested_msgs,
    base::OnceCallback<void(const std::vector<int>&)> callback) {
  RenderFrame* render_frame = GetRenderFrameForInstance(instance);
  PepperBrowserConnection* browser_connection =
      PepperBrowserConnection::Get(render_frame);
  if (!browser_connection) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::vector<int>(nested_msgs.size(), 0)));
  } else {
    browser_connection->SendBrowserCreate(module_->GetPluginChildId(), instance,
                                          nested_msgs, std::move(callback));
  }
}

GURL RendererPpapiHostImpl::GetDocumentURL(PP_Instance pp_instance) {
  PepperPluginInstanceImpl* instance = GetAndValidateInstance(pp_instance);
  if (!instance)
    return GURL();
  return instance->document_url();
}

bool RendererPpapiHostImpl::IsSecureContext(PP_Instance pp_instance) const {
  PepperPluginInstanceImpl* instance = GetAndValidateInstance(pp_instance);
  if (!instance)
    return false;
  return instance->GetContainer()->GetDocument().IsSecureContext() &&
         network::IsUrlPotentiallyTrustworthy(instance->GetPluginURL());
}

int RendererPpapiHostImpl::GetPluginChildId() const {
  return module_->GetPluginChildId();
}

PepperPluginInstanceImpl* RendererPpapiHostImpl::GetAndValidateInstance(
    PP_Instance pp_instance) const {
  PepperPluginInstanceImpl* instance =
      HostGlobals::Get()->GetInstance(pp_instance);
  if (!instance)
    return nullptr;
  if (!instance->IsValidInstanceOf(module_))
    return nullptr;
  return instance;
}

}  // namespace content
