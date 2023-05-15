// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_H_

#include <string>
#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/service_worker_host.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

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
  void IncrementServiceWorkerActivity(int64_t service_worker_version_id,
                                      const std::string& request_uuid) override;
  void DecrementServiceWorkerActivity(int64_t service_worker_version_id,
                                      const std::string& request_uuid) override;
  void RequestWorker(mojom::RequestParamsPtr params) override;
  void WorkerResponseAck(int request_id,
                         int64_t service_worker_version_id) override;

 private:
  // Returns the browser context associated with the render process this
  // `ServiceWorkerHost` belongs to.
  content::BrowserContext* GetBrowserContext();

  // This is safe because ServiceWorkerHost is tied to the life time of
  // RenderProcessHost.
  const raw_ptr<content::RenderProcessHost> render_process_host_;

  // This set is maintained by `(In|De)crementServiceWorkerActivity`.
  std::unordered_set<std::string> active_request_uuids_;

  std::unique_ptr<ExtensionFunctionDispatcher> dispatcher_;

  mojo::AssociatedReceiver<mojom::ServiceWorkerHost> receiver_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_SERVICE_WORKER_HOST_H_
