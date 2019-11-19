// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_RENDERER_PPAPI_HOST_H_
#define CONTENT_PUBLIC_RENDERER_RENDERER_PPAPI_HOST_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/process/process.h"
#include "content/common/content_export.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_instance.h"
#include "url/gurl.h"

namespace gfx {
class Point;
}

namespace IPC {
class Message;
}

namespace ppapi {
namespace host {
class PpapiHost;
}
}

namespace blink {
class WebPluginContainer;
}

namespace content {
class PepperPluginInstance;
class RenderFrame;
class RenderView;

// Interface that allows components in the embedder app to talk to the
// PpapiHost in the renderer process.
//
// There will be one of these objects in the renderer per plugin module.
class RendererPpapiHost {
 public:
  // Returns the RendererPpapiHost associated with the given PP_Instance,
  // or NULL if the instance is invalid.
  //
  // Do NOT use this when dealing with an "external plugin" that serves as a
  // bootstrap to load a second plugin. This is because the two will share a
  // PP_Instance, and the RendererPpapiHost* for the second plugin will be
  // returned after we switch the proxy on.
  CONTENT_EXPORT static RendererPpapiHost* GetForPPInstance(
      PP_Instance instance);

  // Returns the PpapiHost object.
  virtual ppapi::host::PpapiHost* GetPpapiHost() = 0;

  // Returns true if the given PP_Instance is valid and belongs to the
  // plugin associated with this host.
  virtual bool IsValidInstance(PP_Instance instance) = 0;

  // Returns the PluginInstance for the given PP_Instance, or NULL if the
  // PP_Instance is invalid (the common case this will be invalid is during
  // plugin teardown when resource hosts are being force-freed).
  virtual PepperPluginInstance* GetPluginInstance(PP_Instance instance) = 0;

  // Returns the RenderFrame for the given plugin instance, or NULL if the
  // instance is invalid.
  virtual RenderFrame* GetRenderFrameForInstance(PP_Instance instance) = 0;

  // Returns the RenderView for the given plugin instance, or NULL if the
  // instance is invalid.
  virtual RenderView* GetRenderViewForInstance(PP_Instance instance) = 0;

  // Returns the WebPluginContainer for the given plugin instance, or NULL if
  // the instance is invalid.
  virtual blink::WebPluginContainer* GetContainerForInstance(
      PP_Instance instance) = 0;

  // Returns true if the given instance is considered to be currently
  // processing a user gesture or the plugin module has the "override user
  // gesture" flag set (in which case it can always do things normally
  // restricted by user gestures). Returns false if the instance is invalid or
  // if there is no current user gesture.
  virtual bool HasUserGesture(PP_Instance instance) = 0;

  // Returns the routing ID for the render widget containing the given
  // instance. This will take into account the current Flash fullscreen state,
  // so if there is a Flash fullscreen instance active, this will return the
  // routing ID of the fullscreen widget. Returns 0 on failure.
  virtual int GetRoutingIDForWidget(PP_Instance instance) = 0;

  // Converts the given plugin coordinate to the containing RenderFrame. This
  // will take into account the current Flash fullscreen state so will use
  // the fullscreen widget if it's displayed.
  virtual gfx::Point PluginPointToRenderFrame(PP_Instance instance,
                                              const gfx::Point& pt) = 0;

  // Shares a file handle (HANDLE / file descriptor) with the remote side. It
  // returns a handle that should be sent in exactly one IPC message. Upon
  // receipt, the remote side then owns that handle. Note: if sending the
  // message fails, the returned handle is properly closed by the IPC system. If
  // |should_close_source| is set to true, the original handle is closed by this
  // operation and should not be used again.
  virtual IPC::PlatformFileForTransit ShareHandleWithRemote(
      base::PlatformFile handle,
      bool should_close_source) = 0;

  // Shares a shared memory handle with the remote side. It
  // returns a handle that should be sent in exactly one IPC message. Upon
  // receipt, the remote side then owns that handle. Note: if sending the
  // message fails, the returned handle is properly closed by the IPC system.
  virtual base::UnsafeSharedMemoryRegion
  ShareUnsafeSharedMemoryRegionWithRemote(
      const base::UnsafeSharedMemoryRegion& region) = 0;
  virtual base::ReadOnlySharedMemoryRegion
  ShareReadOnlySharedMemoryRegionWithRemote(
      const base::ReadOnlySharedMemoryRegion& region) = 0;

  // Returns true if the plugin is running in process.
  virtual bool IsRunningInProcess() = 0;

  virtual std::string GetPluginName() = 0;

  // Used by the embedder to inform this RendererPpapiHost that the associated
  // plugin module is a host for "external plugins."
  //
  // An embedder may, at the time a plugin module is created, configure it to
  // be a host for external plugins. Instances of such plugins go through two
  // two stages of initialization; the first stage initializes a host plugin
  // instance, which then loads and initializes a child plugin which takes
  // over control. These are treated as one Pepper Instance, because despite the
  // two-stage initialization process, the host and child appear to blink as
  // one plugin instance.
  //
  // The host plugin appears as an in-process plugin, while we interact with the
  // child plugin via the Pepper proxy.
  virtual void SetToExternalPluginHost() = 0;

  // There are times when the renderer needs to create a ResourceHost in the
  // browser. This function does so asynchronously. |nested_msgs| is a list of
  // resource host creation messages and |instance| is the PP_Instance which
  // the resource will belong to. |callback| will be called asynchronously with
  // the pending host IDs when the ResourceHosts have been created. This can be
  // passed back to the plugin to attach to the ResourceHosts. Pending IDs of 0
  // will be passed to the callback if a ResourceHost fails to be created.
  virtual void CreateBrowserResourceHosts(
      PP_Instance instance,
      const std::vector<IPC::Message>& nested_msgs,
      base::OnceCallback<void(const std::vector<int>&)> callback) = 0;

  // Gets the URL of the document containing the given PP_Instance.
  // Returns an empty URL if the instance is invalid.
  // TODO(yzshen): Some methods such as this one don't need to be pure virtual.
  // Instead, they could be directly implemented using other methods in this
  // interface. Consider changing them to static helpers.
  virtual GURL GetDocumentURL(PP_Instance instance) = 0;

 protected:
  virtual ~RendererPpapiHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_RENDERER_PPAPI_HOST_H_
