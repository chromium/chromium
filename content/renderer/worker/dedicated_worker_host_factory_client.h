// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WORKER_DEDICATED_WORKER_HOST_FACTORY_CLIENT_H_
#define CONTENT_RENDERER_WORKER_DEDICATED_WORKER_HOST_FACTORY_CLIENT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/storage_access_api/status.h"
#include "third_party/blink/public/common/renderer_preferences/renderer_preferences.h"
#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info_notifier.mojom.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom-forward.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host.mojom.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host_factory.mojom.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_dedicated_worker_host_factory_client.h"

namespace blink {
class ChildURLLoaderFactoryBundle;
class WebDedicatedOrSharedWorkerFetchContext;
class WebDedicatedWorker;
}  // namespace blink

namespace content {

class ServiceWorkerProviderContext;

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
      const blink::BrowserInterfaceBrokerProxy& interface_broker);
  ~DedicatedWorkerHostFactoryClient() override;

  // Implements blink::WebDedicatedWorkerHostFactoryClient.
  void CreateWorkerHostDeprecated(
      const blink::DedicatedWorkerToken& dedicated_worker_token,
      const blink::WebURL& script_url,
      const blink::WebSecurityOrigin& origin,
      CreateWorkerHostCallback callback) override;
  void CreateWorkerHost(
      const blink::DedicatedWorkerToken& dedicated_worker_token,
      const blink::WebURL& script_url,
      network::mojom::CredentialsMode credentials_mode,
      const blink::WebFetchClientSettingsObject& fetch_client_settings_object,
      blink::CrossVariantMojoRemote<blink::mojom::BlobURLTokenInterfaceBase>
          blob_url_token,
      net::StorageAccessApiStatus storage_access_api_status) override;
  scoped_refptr<blink::WebWorkerFetchContext> CloneWorkerFetchContext(
      blink::WebWorkerFetchContext* web_worker_fetch_context,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;

  scoped_refptr<blink::WebDedicatedOrSharedWorkerFetchContext>
  CreateWorkerFetchContext(
      const blink::RendererPreferences& renderer_preference,
      mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
          watcher_receiver,
      mojo::PendingRemote<blink::mojom::ResourceLoadInfoNotifier>
          pending_resource_load_info_notifier);

 private:
  // Implements blink::mojom::DedicatedWorkerHostFactoryClient.
  void OnWorkerHostCreated(
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker,
      mojo::PendingRemote<blink::mojom::DedicatedWorkerHost>
          dedicated_worker_host,
      const url::Origin& origin) override;
  void OnScriptLoadStarted(
      blink::mojom::ServiceWorkerContainerInfoForClientPtr
          service_worker_container_info,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          pending_subresource_loader_factory_bundle,
      mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
          subresource_loader_updater,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      mojo::PendingRemote<blink::mojom::BackForwardCacheControllerHost>
          back_forward_cache_controller_host) override;
  void OnScriptLoadStartFailed() override;

  // |worker_| owns |this|.
  raw_ptr<blink::WebDedicatedWorker> worker_;

  scoped_refptr<blink::ChildURLLoaderFactoryBundle>
      subresource_loader_factory_bundle_;
  mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
      pending_subresource_loader_updater_;

  scoped_refptr<ServiceWorkerProviderContext> service_worker_provider_context_;

  mojo::Remote<blink::mojom::DedicatedWorkerHostFactory> factory_;
  mojo::Receiver<blink::mojom::DedicatedWorkerHostFactoryClient> receiver_{
      this};
};

}  // namespace content

#endif  // CONTENT_RENDERER_WORKER_DEDICATED_WORKER_HOST_FACTORY_CLIENT_H_
