// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_FRAME_HOST_H_
#define EXTENSIONS_BROWSER_EXTENSION_FRAME_HOST_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/frame.mojom.h"
#include "extensions/common/mojom/injection_type.mojom-shared.h"
#include "extensions/common/mojom/run_location.mojom-shared.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom-forward.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {

class Extension;
class ProcessManager;

// Implements the mojo interface of extensions::mojom::LocalFrameHost.
// ExtensionWebContentsObserver creates and owns this class and it's destroyed
// when WebContents is destroyed.
class ExtensionFrameHost : public mojom::LocalFrameHost {
 public:
  explicit ExtensionFrameHost(content::WebContents* web_contents);
  ExtensionFrameHost(const ExtensionFrameHost&) = delete;
  ExtensionFrameHost& operator=(const ExtensionFrameHost&) = delete;
  ~ExtensionFrameHost() override;

  void BindLocalFrameHost(
      mojo::PendingAssociatedReceiver<mojom::LocalFrameHost> receiver,
      content::RenderFrameHost* render_frame_host);

  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host);

  content::RenderFrameHostReceiverSet<mojom::LocalFrameHost>&
  receivers_for_testing() {
    return receivers_;
  }

  // mojom::LocalFrameHost:
  void RequestScriptInjectionPermission(
      const ExtensionId& extension_id,
      mojom::InjectionType script_type,
      mojom::RunLocation run_location,
      RequestScriptInjectionPermissionCallback callback) override;
  void GetAppInstallState(const GURL& requestor_url,
                          GetAppInstallStateCallback callback) override;
  void Request(mojom::RequestParamsPtr params,
               RequestCallback callback) override;
  void ResponseAck(const base::Uuid& request_uuid) override;
  void WatchedPageChange(
      const std::vector<std::string>& css_selectors) override;
  void DetailedConsoleMessageAdded(
      const std::u16string& message,
      const std::u16string& source,
      const StackTrace& stack_trace,
      blink::mojom::ConsoleMessageLevel level) override;
  void ContentScriptsExecuting(
      const base::flat_map<std::string, std::vector<std::string>>&
          extension_id_to_scripts,
      const GURL& frame_url) override;
  void IncrementLazyKeepaliveCount() override;
  void DecrementLazyKeepaliveCount() override;
  void AppWindowReady() override;
  void OpenChannelToExtension(
      extensions::mojom::ExternalConnectionInfoPtr info,
      extensions::mojom::ChannelType channel_type,
      const std::string& channel_name,
      const PortId& port_id,
      mojo::PendingAssociatedRemote<extensions::mojom::MessagePort> port,
      mojo::PendingAssociatedReceiver<extensions::mojom::MessagePortHost>
          port_host) override;
  void OpenChannelToNativeApp(
      const std::string& native_app_name,
      const PortId& port_id,
      mojo::PendingAssociatedRemote<extensions::mojom::MessagePort> port,
      mojo::PendingAssociatedReceiver<extensions::mojom::MessagePortHost>
          port_host) override;
  void OpenChannelToTab(
      int32_t tab_id,
      int32_t frame_id,
      const std::optional<std::string>& document_id,
      extensions::mojom::ChannelType channel_type,
      const std::string& channel_name,
      const PortId& port_id,
      mojo::PendingAssociatedRemote<extensions::mojom::MessagePort> port,
      mojo::PendingAssociatedReceiver<extensions::mojom::MessagePortHost>
          port_host) override;

 protected:
  const Extension* GetExtension(ProcessManager* process_manager,
                                content::RenderFrameHost* frame);

  // This raw pointer is safe to use because ExtensionWebContentsObserver whose
  // lifetime is tied to the WebContents owns this instance.
  raw_ptr<content::WebContents> web_contents_;
  content::RenderFrameHostReceiverSet<mojom::LocalFrameHost> receivers_;

 private:
  // Data tracked for each active frame that has incremented keepalive count via
  // IPC. This structure allows `ExtensionFrameHost` to deduplicate multiple IPC
  // keepalives from the same frame into a single keepalive in `ProcessManager`,
  // preventing memory bloat and protecting against malicious counter
  // underflows.
  struct FrameIpcKeepaliveData {
    ExtensionId extension_id;
    // Unique string identifying the frame (e.g. "ipc:<child_id>:<routing_id>")
    // used to strictly match keepalive decrements in `ProcessManager`.
    std::string activity_data;
    // Number of active IPC keepalive requests from this frame.
    int count = 0;
  };

  // Decrements the `ProcessManager` keepalive count for the specified data.
  void ReleaseIpcKeepaliveData(const FrameIpcKeepaliveData& data);

  // Decrements the `ProcessManager` keepalive count if the specified frame has
  // any remaining IPC keepalive counts, then removes the tracking entry. Called
  // when a frame is deleted or when `ExtensionFrameHost` is destroyed.
  void ReleaseIpcKeepaliveForFrame(
      const content::GlobalRenderFrameHostId& frame_id);

  raw_ptr<content::BrowserContext> browser_context_;

  // Maps global frame IDs to their current keepalive tracking data.
  base::flat_map<content::GlobalRenderFrameHostId, FrameIpcKeepaliveData>
      frame_ipc_keepalives_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_FRAME_HOST_H_
