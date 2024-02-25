// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_RENDERER_PPAPI_HOST_IMPL_H_
#define CONTENT_RENDERER_PEPPER_RENDERER_PPAPI_HOST_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/content_renderer_pepper_host_factory.h"
#include "ppapi/host/ppapi_host.h"

namespace ppapi {

namespace proxy {
class HostDispatcher;
}

namespace thunk {
class ResourceCreationAPI;
}

}  // namespace ppapi

namespace content {

class PepperInProcessRouter;
class PepperPluginInstanceImpl;
class PluginModule;

// This class is attached to a PluginModule which manages our lifetime.
class RendererPpapiHostImpl : public RendererPpapiHost {
 public:
  RendererPpapiHostImpl(const RendererPpapiHostImpl&) = delete;
  RendererPpapiHostImpl& operator=(const RendererPpapiHostImpl&) = delete;

  ~RendererPpapiHostImpl() override;

  // Factory functions to create in process or out-of-process host impls. The
  // host will be created and associated with the given module, which must not
  // already have embedder state on it.
  //
  // The module will take ownership of the new host impl. The returned value
  // does not pass ownership, it's just for the information of the caller.
  static RendererPpapiHostImpl* CreateOnModuleForOutOfProcess(
      PluginModule* module,
      ppapi::proxy::HostDispatcher* dispatcher,
      const ppapi::PpapiPermissions& permissions);
  CONTENT_EXPORT static RendererPpapiHostImpl* CreateOnModuleForInProcess(
      PluginModule* module,
      const ppapi::PpapiPermissions& permissions);

  // Returns the RendererPpapiHostImpl associated with the given PP_Instance,
  // or NULL if the instance is invalid.
  static RendererPpapiHostImpl* GetForPPInstance(PP_Instance pp_instance);

  // Returns the router that we use for in-process IPC emulation (see the
  // pepper_in_process_router.h for more). This will be NULL when the plugin
  // is running out-of-process.
  PepperInProcessRouter* in_process_router() {
    return in_process_router_.get();
  }

  // Creates the in-process resource creation API wrapper for the given
  // plugin instance. This object will reference the host impl, so the
  // host impl should outlive the returned pointer. Since the resource
  // creation object is associated with the instance, this will generally
  // happen automatically.
  std::unique_ptr<ppapi::thunk::ResourceCreationAPI>
  CreateInProcessResourceCreationAPI(PepperPluginInstanceImpl* instance);

  PepperPluginInstanceImpl* GetPluginInstanceImpl(PP_Instance instance) const;

  bool IsExternalPluginHost() const;

  // RendererPpapiHost implementation.
  ppapi::host::PpapiHost* GetPpapiHost() override;
  bool IsValidInstance(PP_Instance instance) override;
  PepperPluginInstance* GetPluginInstance(PP_Instance instance) override;
  RenderFrame* GetRenderFrameForInstance(PP_Instance instance) override;
  blink::WebPluginContainer* GetContainerForInstance(
      PP_Instance instance) override;
  bool HasUserGesture(PP_Instance instance) override;
  int GetRoutingIDForFrame(PP_Instance instance) override;
  gfx::Point PluginPointToRenderFrame(PP_Instance instance,
                                      const gfx::Point& pt) override;
  IPC::PlatformFileForTransit ShareHandleWithRemote(
      base::PlatformFile handle,
      bool should_close_source) override;
  base::UnsafeSharedMemoryRegion ShareUnsafeSharedMemoryRegionWithRemote(
      const base::UnsafeSharedMemoryRegion& region) override;
  base::ReadOnlySharedMemoryRegion ShareReadOnlySharedMemoryRegionWithRemote(
      const base::ReadOnlySharedMemoryRegion& region) override;
  bool IsRunningInProcess() override;
  std::string GetPluginName() override;
  void SetToExternalPluginHost() override;
  void CreateBrowserResourceHosts(
      PP_Instance instance,
      const std::vector<IPC::Message>& nested_msgs,
      base::OnceCallback<void(const std::vector<int>&)> callback) override;
  GURL GetDocumentURL(PP_Instance pp_instance) override;

  // Returns whether the plugin is running in a secure context.
  bool IsSecureContext(PP_Instance pp_instance) const;

  // Returns the plugin child process ID if the plugin is running out of
  // process. Returns -1 otherwise. This is the ID that the browser process uses
  // to idetify the child process for the plugin. This isn't directly useful
  // from our process (the renderer) except in messages to the browser to
  // disambiguate plugins.
  int GetPluginChildId() const;

  void set_viewport_to_dip_scale(float viewport_to_dip_scale) {
    DCHECK_LT(0, viewport_to_dip_scale_);
    viewport_to_dip_scale_ = viewport_to_dip_scale;
  }

 private:
  RendererPpapiHostImpl(PluginModule* module,
                        ppapi::proxy::HostDispatcher* dispatcher,
                        const ppapi::PpapiPermissions& permissions);
  RendererPpapiHostImpl(PluginModule* module,
                        const ppapi::PpapiPermissions& permissions);

  // Retrieves the plugin instance object associated with the given PP_Instance
  // and validates that it is one of the instances associated with our module.
  // Returns NULL on failure.
  //
  // We use this to security check the PP_Instance values sent from a plugin to
  // make sure it's not trying to spoof another instance.
  PepperPluginInstanceImpl* GetAndValidateInstance(PP_Instance instance) const;

  raw_ptr<PluginModule> module_;  // Non-owning pointer.

  // The dispatcher we use to send messagse when the plugin is out-of-process.
  // Will be null when running in-process. Non-owning pointer.
  raw_ptr<ppapi::proxy::HostDispatcher> dispatcher_;

  std::unique_ptr<ppapi::host::PpapiHost> ppapi_host_;

  // Null when running out-of-process.
  std::unique_ptr<PepperInProcessRouter> in_process_router_;

  // Whether the plugin is running in process.
  bool is_running_in_process_;

  // Whether this is a host for external plugins.
  bool is_external_plugin_host_;

  // The scale between the viewport and dip.
  float viewport_to_dip_scale_ = 1.0f;
};

}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_RENDERER_PPAPI_HOST_IMPL_H_
