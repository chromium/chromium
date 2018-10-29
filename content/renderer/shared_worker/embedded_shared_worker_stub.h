// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SHARED_WORKER_EMBEDDED_SHARED_WORKER_STUB_H_
#define CONTENT_RENDERER_SHARED_WORKER_EMBEDDED_SHARED_WORKER_STUB_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/unguessable_token.h"
#include "content/common/service_worker/service_worker_provider.mojom.h"
#include "content/common/shared_worker/shared_worker.mojom.h"
#include "content/common/shared_worker/shared_worker_host.mojom.h"
#include "content/common/shared_worker/shared_worker_info.mojom.h"
#include "content/public/common/renderer_preference_watcher.mojom.h"
#include "content/public/common/renderer_preferences.h"
#include "ipc/ipc_listener.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/mojom/shared_worker/shared_worker_main_script_load_params.mojom.h"
#include "third_party/blink/public/platform/web_content_security_policy.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/devtools_agent.mojom.h"
#include "third_party/blink/public/web/web_shared_worker_client.h"
#include "third_party/blink/public/web/worker_content_settings_proxy.mojom.h"
#include "url/gurl.h"

namespace blink {
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebNotificationPresenter;
class WebSharedWorker;
}

namespace blink {
class MessagePortChannel;
}

namespace content {

class HostChildURLLoaderFactoryBundle;
class URLLoaderFactoryBundleInfo;
class WebApplicationCacheHostImpl;
struct NavigationResponseOverrideParameters;

// A stub class to receive IPC from browser process and talk to
// blink::WebSharedWorker. Implements blink::WebSharedWorkerClient.
// This class is self-destructed (no one explicitly owns this). It deletes
// itself when either one of following methods is called by
// blink::WebSharedWorker:
// - WorkerScriptLoadFailed() or
// - WorkerContextDestroyed()
//
// This class owns blink::WebSharedWorker.
class EmbeddedSharedWorkerStub : public blink::WebSharedWorkerClient,
                                 public mojom::SharedWorker {
 public:
  EmbeddedSharedWorkerStub(
      mojom::SharedWorkerInfoPtr info,
      bool pause_on_start,
      const base::UnguessableToken& devtools_worker_token,
      const RendererPreferences& renderer_preferences,
      mojom::RendererPreferenceWatcherRequest preference_watcher_request,
      blink::mojom::WorkerContentSettingsProxyPtr content_settings,
      mojom::ServiceWorkerProviderInfoForSharedWorkerPtr
          service_worker_provider_info,
      int appcache_host_id,
      network::mojom::URLLoaderFactoryAssociatedPtrInfo
          main_script_loader_factory,
      blink::mojom::SharedWorkerMainScriptLoadParamsPtr main_script_load_params,
      std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loader_factories,
      mojom::ControllerServiceWorkerInfoPtr controller_info,
      mojom::SharedWorkerHostPtr host,
      mojom::SharedWorkerRequest request,
      service_manager::mojom::InterfaceProviderPtr interface_provider);
  ~EmbeddedSharedWorkerStub() override;

  // blink::WebSharedWorkerClient implementation.
  void CountFeature(blink::mojom::WebFeature feature) override;
  void WorkerContextClosed() override;
  void WorkerContextDestroyed() override;
  void WorkerReadyForInspection() override;
  void WorkerScriptLoaded() override;
  void WorkerScriptLoadFailed() override;
  void SelectAppCacheID(long long) override;
  blink::WebNotificationPresenter* NotificationPresenter() override;
  std::unique_ptr<blink::WebApplicationCacheHost> CreateApplicationCacheHost(
      blink::WebApplicationCacheHostClient*) override;
  std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
  CreateServiceWorkerNetworkProvider() override;
  std::unique_ptr<blink::WebWorkerFetchContext> CreateWorkerFetchContext(
      blink::WebServiceWorkerNetworkProvider*) override;
  void WaitForServiceWorkerControllerInfo(
      blink::WebServiceWorkerNetworkProvider* web_network_provider,
      base::OnceClosure callback) override;

 private:
  // WebSharedWorker will own |channel|.
  void ConnectToChannel(int connection_request_id,
                        blink::MessagePortChannel channel);

  // mojom::SharedWorker methods:
  void Connect(int connection_request_id,
               mojo::ScopedMessagePipeHandle port) override;
  void Terminate() override;
  void BindDevToolsAgent(
      blink::mojom::DevToolsAgentHostAssociatedPtrInfo host,
      blink::mojom::DevToolsAgentAssociatedRequest request) override;

  mojo::Binding<mojom::SharedWorker> binding_;
  mojom::SharedWorkerHostPtr host_;
  const std::string name_;
  bool running_ = false;
  GURL url_;
  RendererPreferences renderer_preferences_;
  // Set on ctor and passed to the fetch context created when
  // CreateWorkerFetchContext() is called.
  mojom::RendererPreferenceWatcherRequest preference_watcher_request_;
  std::unique_ptr<blink::WebSharedWorker> impl_;

  using PendingChannel =
      std::pair<int /* connection_request_id */, blink::MessagePortChannel>;
  std::vector<PendingChannel> pending_channels_;

  const int appcache_host_id_;
  WebApplicationCacheHostImpl* app_cache_host_ = nullptr;  // Not owned.

  // S13nServiceWorker: The info needed to connect to the
  // ServiceWorkerProviderHost on the browser.
  mojom::ServiceWorkerProviderInfoForSharedWorkerPtr
      service_worker_provider_info_;

  // NetworkService: The URLLoaderFactory used for loading the shared worker
  // main script.
  network::mojom::URLLoaderFactoryAssociatedPtrInfo main_script_loader_factory_;

  // NetworkService:
  mojom::ControllerServiceWorkerInfoPtr controller_info_;

  // S13nServiceWorker: The factory bundle used for loading subresources for
  // this shared worker.
  scoped_refptr<HostChildURLLoaderFactoryBundle> subresource_loader_factories_;

  // NetworkService (PlzWorker): The response override parameters used for
  // taking a resource pre-requested by the browser process.
  std::unique_ptr<NavigationResponseOverrideParameters> response_override_;

  // Out-of-process NetworkService:
  // Detects disconnection from the default factory of the loader factory bundle
  // used by this worker (typically the network service).
  network::mojom::URLLoaderFactoryPtr
      default_factory_connection_error_handler_holder_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedSharedWorkerStub);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SHARED_WORKER_EMBEDDED_SHARED_WORKER_STUB_H_
