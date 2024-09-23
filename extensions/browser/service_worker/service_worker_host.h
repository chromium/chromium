// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_H_

#include <string>
#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host_observer.h"
#include "extensions/browser/permissions_manager.h"
#include "extensions/browser/service_worker/worker_id.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/message_port.mojom.h"
#include "extensions/common/mojom/service_worker.mojom.h"
#include "extensions/common/mojom/service_worker_host.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_database.mojom.h"

class GURL;

namespace base {
class UnguessableToken;
}

namespace content {
class BrowserContext;
class RenderProcessHost;
}  // namespace content

namespace extensions {

class ExtensionFunctionDispatcher;

// This class is the host of service worker execution context for extension
// in the renderer process. Lives on the UI thread.
class ServiceWorkerHost :
    public PermissionsManager::Observer,
    public mojom::ServiceWorkerHost,
    public content::RenderProcessHostObserver {
 public:
  explicit ServiceWorkerHost(
      content::RenderProcessHost* render_process_host,
      mojo::PendingAssociatedReceiver<mojom::ServiceWorkerHost> receiver);
  ServiceWorkerHost(const ServiceWorkerHost&) = delete;
  ServiceWorkerHost& operator=(const ServiceWorkerHost&) = delete;
  ~ServiceWorkerHost() override;

  static ServiceWorkerHost* GetWorkerFor(const WorkerId& worker);
  static void BindReceiver(
      int render_process_id,
      mojo::PendingAssociatedReceiver<mojom::ServiceWorkerHost> receiver);

  // mojom::ServiceWorkerHost:
  void DidInitializeServiceWorkerContext(
      const ExtensionId& extension_id,
      int64_t service_worker_version_id,
      int worker_thread_id,
      const blink::ServiceWorkerToken& service_worker_token,
      mojo::PendingAssociatedRemote<mojom::EventDispatcher> event_dispatcher)
      override;
  void DidStartServiceWorkerContext(
      const ExtensionId& extension_id,
      const base::UnguessableToken& activation_token,
      const GURL& service_worker_scope,
      int64_t service_worker_version_id,
      int worker_thread_id) override;
  void DidStopServiceWorkerContext(
      const ExtensionId& extension_id,
      const base::UnguessableToken& activation_token,
      const GURL& service_worker_scope,
      int64_t service_worker_version_id,
      int worker_thread_id) override;
  void RequestWorker(mojom::RequestParamsPtr params,
                     RequestWorkerCallback callback) override;
  void WorkerResponseAck(const base::Uuid& request_uuid) override;
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

  // PermissionManager::Observer overrides.
  void OnExtensionPermissionsUpdated(
      const Extension& extension,
      const PermissionSet& permissions,
      PermissionsManager::UpdateReason reason) override;

  // Returns the mojo channel to the service worker. It may be null
  // if the service worker doesn't have a live service worker matching
  // the version id.
  mojom::ServiceWorker* GetServiceWorker();

  mojo::AssociatedReceiver<mojom::ServiceWorkerHost>& receiver_for_testing() {
    return receiver_;
  }

  // content::RenderProcessHostObserver implementation.
  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override;

 private:
  // Returns the browser context associated with the render process this
  // `ServiceWorkerHost` belongs to.
  content::BrowserContext* GetBrowserContext();

  void RemoteDisconnected();

  // Destroys this instance by removing it from the ServiceWorkerHostList.
  void Destroy();

  // This is safe because ServiceWorkerHost is tied to the life time of
  // RenderProcessHost.
  const raw_ptr<content::RenderProcessHost> render_process_host_;

  std::unique_ptr<ExtensionFunctionDispatcher> dispatcher_;

  mojo::AssociatedReceiver<mojom::ServiceWorkerHost> receiver_{this};
  mojo::AssociatedRemote<mojom::ServiceWorker> remote_;
  WorkerId worker_id_;

  base::ScopedObservation<PermissionsManager, PermissionsManager::Observer>
      permissions_observer_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_H_
