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
#include "extensions/browser/permissions_manager.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/service_worker.mojom.h"
#include "extensions/common/mojom/service_worker_host.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
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
class ServiceWorkerHost : public base::SupportsUserData::Data,
#if !BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
                          public PermissionsManager::Observer,
#endif
                          public mojom::ServiceWorkerHost {
 public:
  explicit ServiceWorkerHost(content::RenderProcessHost* render_process_host);
  ServiceWorkerHost(const ServiceWorkerHost&) = delete;
  ServiceWorkerHost& operator=(const ServiceWorkerHost&) = delete;
  ~ServiceWorkerHost() override;

  static void BindReceiver(
      int render_process_id,
      mojo::PendingAssociatedReceiver<mojom::ServiceWorkerHost> receiver);

  // mojom::ServiceWorkerHost:
  void DidInitializeServiceWorkerContext(
      const ExtensionId& extension_id,
      int64_t service_worker_version_id,
      int worker_thread_id,
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

#if !BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
  // PermissionManager::Observer overrides.
  void OnExtensionPermissionsUpdated(
      const Extension& extension,
      const PermissionSet& permissions,
      PermissionsManager::UpdateReason reason) override;
#endif

 private:
  // Returns the browser context associated with the render process this
  // `ServiceWorkerHost` belongs to.
  content::BrowserContext* GetBrowserContext();

#if !BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
  mojom::ServiceWorker* GetServiceWorker();
#endif

  void RemoteDisconnected();

  // This is safe because ServiceWorkerHost is tied to the life time of
  // RenderProcessHost.
  const raw_ptr<content::RenderProcessHost> render_process_host_;

  std::unique_ptr<ExtensionFunctionDispatcher> dispatcher_;

  mojo::AssociatedReceiver<mojom::ServiceWorkerHost> receiver_{this};
#if !BUILDFLAG(ENABLE_EXTENSIONS_LEGACY_IPC)
  mojo::AssociatedRemote<mojom::ServiceWorker> remote_;
  int64_t service_worker_version_id_ =
      blink::mojom::kInvalidServiceWorkerVersionId;
  ExtensionId extension_id_;

  base::ScopedObservation<PermissionsManager, PermissionsManager::Observer>
      permissions_observer_{this};
#endif
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_H_
