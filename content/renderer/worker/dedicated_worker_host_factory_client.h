// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WORKER_DEDICATED_WORKER_HOST_FACTORY_CLIENT_H_
#define CONTENT_RENDERER_WORKER_DEDICATED_WORKER_HOST_FACTORY_CLIENT_H_

#include <memory>
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host.mojom.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host_factory.mojom.h"
#include "third_party/blink/public/platform/web_dedicated_worker_host_factory_client.h"

namespace blink {
class WebDedicatedWorker;
class WebWorkerFetchContext;
}  // namespace blink

namespace content {

class ChildURLLoaderFactoryBundle;
class ServiceWorkerProviderContext;
class WebWorkerFetchContextImpl;
struct NavigationResponseOverrideParameters;

// DedicatedWorkerHostFactoryClient intermediates between
// blink::(Web)DedicatedWorker and content::DedicatedWorkerHostFactory. This
// is bound with the thread where the execution context creating this worker
// lives (i.e., the main thread or a worker thread for nested workers). This is
// owned by blink::(Web)DedicatedWorker.
class DedicatedWorkerHostFactoryClient final
    : public blink::WebDedicatedWorkerHostFactoryClient,
      public blink::mojom::DedicatedWorkerHostFactoryClient {
 public:
  DedicatedWorkerHostFactoryClient(
      blink::WebDedicatedWorker* worker,
      service_manager::InterfaceProvider* interface_provider);
  ~DedicatedWorkerHostFactoryClient() override;

  // Implements blink::WebDedicatedWorkerHostFactoryClient.
  void CreateWorkerHostDeprecated(
      const blink::WebSecurityOrigin& script_origin) override;
  void CreateWorkerHost(
      const blink::WebURL& script_url,
      const blink::WebSecurityOrigin& script_origin,
      network::mojom::CredentialsMode credentials_mode,
      const blink::WebSecurityOrigin& fetch_client_security_origin,
      const blink::WebFetchClientSettingsObject& fetch_client_settings_object,
      mojo::ScopedMessagePipeHandle blob_url_token) override;
  scoped_refptr<blink::WebWorkerFetchContext> CloneWorkerFetchContext(
      blink::WebWorkerFetchContext* web_worker_fetch_context,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  void LifecycleStateChanged(blink::mojom::FrameLifecycleState state) override;

  scoped_refptr<WebWorkerFetchContextImpl> CreateWorkerFetchContext(
      blink::mojom::RendererPreferences renderer_preference,
      mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
          watcher_receiver);

 private:
  // Implements blink::mojom::DedicatedWorkerHostFactoryClient.
  void OnWorkerHostCreated(
      service_manager::mojom::InterfaceProviderPtr interface_provider,
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker) override;
  void OnScriptLoadStarted(
      blink::mojom::ServiceWorkerProviderInfoForClientPtr
          service_worker_provider_info,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factory_bundle_info,
      mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
          subresource_loader_updater,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info) override;
  void OnScriptLoadStartFailed() override;

  // |worker_| owns |this|.
  blink::WebDedicatedWorker* worker_;

  scoped_refptr<ChildURLLoaderFactoryBundle> subresource_loader_factory_bundle_;
  mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
      pending_subresource_loader_updater_;

  scoped_refptr<ServiceWorkerProviderContext> service_worker_provider_context_;
  std::unique_ptr<NavigationResponseOverrideParameters>
      response_override_for_main_script_;

  mojo::Remote<blink::mojom::DedicatedWorkerHostFactory> factory_;
  mojo::Receiver<blink::mojom::DedicatedWorkerHostFactoryClient> receiver_{
      this};
  mojo::Remote<blink::mojom::DedicatedWorkerHost> remote_host_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WORKER_DEDICATED_WORKER_HOST_FACTORY_CLIENT_H_
