// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_
#define CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_

#include <list>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/common/service_worker/service_worker_provider.mojom.h"
#include "content/common/shared_worker/shared_worker.mojom.h"
#include "content/common/shared_worker/shared_worker_client.mojom.h"
#include "content/common/shared_worker/shared_worker_factory.mojom.h"
#include "content/common/shared_worker/shared_worker_host.mojom.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/mojom/shared_worker/shared_worker_main_script_load_params.mojom.h"
#include "third_party/blink/public/web/devtools_agent.mojom.h"

class GURL;

namespace blink {
class MessagePortChannel;
}

namespace content {

class AppCacheNavigationHandle;
class SharedWorkerContentSettingsProxyImpl;
class SharedWorkerInstance;
class SharedWorkerServiceImpl;
class URLLoaderFactoryBundleInfo;
struct SubresourceLoaderParams;

// The SharedWorkerHost is the interface that represents the browser side of
// the browser <-> worker communication channel. This is owned by
// SharedWorkerServiceImpl and destructed when a worker context or worker's
// message filter is closed.
class CONTENT_EXPORT SharedWorkerHost
    : public mojom::SharedWorkerHost,
      public service_manager::mojom::InterfaceProvider {
 public:
  SharedWorkerHost(SharedWorkerServiceImpl* service,
                   std::unique_ptr<SharedWorkerInstance> instance,
                   int process_id);
  ~SharedWorkerHost() override;

  // Starts the SharedWorker in the renderer process.
  //
  // S13nServiceWorker:
  // |service_worker_provider_info| is sent to the renderer process and contains
  // information about its ServiceWorkerProviderHost, the browser-side host for
  // supporting the shared worker as a service worker client.
  //
  // S13nServiceWorker (non-NetworkService):
  // |main_script_loader_factory| is sent to the renderer process and is to be
  // used to request the shared worker's main script. Currently it's only
  // non-null when S13nServiceWorker is enabled but NetworkService is disabled,
  // to allow service worker machinery to observe the request.
  //
  // NetworkService (PlzWorker):
  // |main_script_load_params| is sent to the renderer process and to be used to
  // load the shared worker main script pre-requested by the browser process.
  // This is only non-null when NetworkService is enabled.
  //
  // NetworkService:
  // |subresource_loader_factories| is sent to the renderer process and is to be
  // used to request subresources where applicable. For example, this allows the
  // shared worker to load chrome-extension:// URLs which the renderer's default
  // loader factory can't load.
  //
  // NetworkService (PlzWorker):
  // |subresource_loader_params| contains information about the default loader
  // factory for |subresource_loader_factories_| and the service worker
  // controller. The default loader factory can be associated with some request
  // interceptor like AppCacheRequestHandler. This is only non-null when
  // NetworkService is enabled.
  // When S13nServiceWorker is enabled but NetworkService is disabled, the
  // default network loader factory is created by the RenderFrameHost, and
  // service worker controller is sent via ServiceWorkerContainer#SetController.
  void Start(
      mojom::SharedWorkerFactoryPtr factory,
      mojom::ServiceWorkerProviderInfoForSharedWorkerPtr
          service_worker_provider_info,
      network::mojom::URLLoaderFactoryAssociatedPtrInfo
          main_script_loader_factory,
      blink::mojom::SharedWorkerMainScriptLoadParamsPtr main_script_load_params,
      std::unique_ptr<URLLoaderFactoryBundleInfo> subresource_loader_factories,
      base::Optional<SubresourceLoaderParams> subresource_loader_params);

  void AllowFileSystem(const GURL& url,
                       base::OnceCallback<void(bool)> callback);
  void AllowIndexedDB(const GURL& url,
                      const base::string16& name,
                      base::OnceCallback<void(bool)> callback);

  // Terminates the given worker, i.e. based on a UI action.
  void TerminateWorker();

  void AddClient(mojom::SharedWorkerClientPtr client,
                 int process_id,
                 int frame_id,
                 const blink::MessagePortChannel& port);

  void BindDevToolsAgent(blink::mojom::DevToolsAgentHostAssociatedPtrInfo host,
                         blink::mojom::DevToolsAgentAssociatedRequest request);

  void SetAppCacheHandle(
      std::unique_ptr<AppCacheNavigationHandle> appcache_handle);

  SharedWorkerInstance* instance() { return instance_.get(); }
  int process_id() const { return process_id_; }
  bool IsAvailable() const;

  base::WeakPtr<SharedWorkerHost> AsWeakPtr();

 private:
  friend class SharedWorkerHostTest;

  enum class Phase {
    kInitial,
    kStarted,
    kClosed,
    kTerminationSent,
    kTerminationSentAndClosed
  };

  class ScopedDevToolsHandle;

  struct ClientInfo {
    ClientInfo(mojom::SharedWorkerClientPtr client,
               int connection_request_id,
               int process_id,
               int frame_id);
    ~ClientInfo();
    mojom::SharedWorkerClientPtr client;
    const int connection_request_id;
    const int process_id;
    const int frame_id;
  };

  using ClientList = std::list<ClientInfo>;

  // mojom::SharedWorkerHost methods:
  void OnConnected(int connection_request_id) override;
  void OnContextClosed() override;
  void OnReadyForInspection() override;
  void OnScriptLoaded() override;
  void OnScriptLoadFailed() override;
  void OnFeatureUsed(blink::mojom::WebFeature feature) override;

  // Returns the frame ids of this worker's clients.
  std::vector<GlobalFrameRoutingId> GetRenderFrameIDsForWorker();

  void AllowFileSystemResponse(base::OnceCallback<void(bool)> callback,
                               bool allowed);
  void OnClientConnectionLost();
  void OnWorkerConnectionLost();

  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override;

  void CreateNetworkFactory(network::mojom::URLLoaderFactoryRequest request);

  void AdvanceTo(Phase phase);

  mojo::Binding<mojom::SharedWorkerHost> binding_;

  // |service_| owns |this|.
  SharedWorkerServiceImpl* service_;
  std::unique_ptr<SharedWorkerInstance> instance_;
  ClientList clients_;

  mojom::SharedWorkerRequest worker_request_;
  mojom::SharedWorkerPtr worker_;

  const int process_id_;
  int next_connection_request_id_;
  const base::TimeTicks creation_time_;
  std::unique_ptr<ScopedDevToolsHandle> devtools_handle_;

  // This is the set of features that this worker has used.
  std::set<blink::mojom::WebFeature> used_features_;

  std::unique_ptr<SharedWorkerContentSettingsProxyImpl> content_settings_;

  // This is kept alive during the lifetime of the shared worker, since it's
  // associated with Mojo interfaces (ServiceWorkerContainer and
  // URLLoaderFactory) that are needed to stay alive while the worker is
  // starting or running.
  mojom::SharedWorkerFactoryPtr factory_;

  mojo::Binding<service_manager::mojom::InterfaceProvider>
      interface_provider_binding_;

  // NetworkService:
  // The handle owns the precreated AppCacheHost until it's claimed by the
  // renderer after main script loading finishes.
  std::unique_ptr<AppCacheNavigationHandle> appcache_handle_;

  Phase phase_ = Phase::kInitial;

  base::WeakPtrFactory<SharedWorkerHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SHARED_WORKER_SHARED_WORKER_HOST_H_
