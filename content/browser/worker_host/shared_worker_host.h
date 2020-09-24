// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_HOST_H_
#define CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_HOST_H_

#include <list>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string16.h"
#include "base/unguessable_token.h"
#include "content/browser/browser_interface_broker_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/shared_worker_instance.h"
#include "media/mojo/mojom/video_decode_perf_history.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/webtransport/quic_transport_connector.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_client.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_factory.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_host.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"

class GURL;

namespace blink {
class MessagePortChannel;
class PendingURLLoaderFactoryBundle;
}  // namespace blink

namespace content {

class AppCacheNavigationHandle;
class ServiceWorkerMainResourceHandle;
class ServiceWorkerObjectHost;
class SharedWorkerContentSettingsProxyImpl;
class SharedWorkerServiceImpl;

// SharedWorkerHost is the browser-side host of a single shared worker running
// in the renderer. This class is owned by the SharedWorkerServiceImpl of the
// current BrowserContext.
class CONTENT_EXPORT SharedWorkerHost : public blink::mojom::SharedWorkerHost,
                                        public RenderProcessHostObserver {
 public:
  SharedWorkerHost(SharedWorkerServiceImpl* service,
                   const SharedWorkerInstance& instance,
                   RenderProcessHost* worker_process_host);
  ~SharedWorkerHost() override;

  RenderProcessHost* GetProcessHost() { return worker_process_host_; }

  // Starts the SharedWorker in the renderer process.
  //
  // |main_script_load_params| is sent to the renderer process and to be used to
  // load the shared worker main script pre-requested by the browser process.
  //
  // |subresource_loader_factories| is sent to the renderer process and is to be
  // used to request subresources where applicable. For example, this allows the
  // shared worker to load chrome-extension:// URLs which the renderer's default
  // loader factory can't load.
  //
  // |controller| contains information about the service worker controller. Once
  // a ServiceWorker object about the controller is prepared, it is registered
  // to |controller_service_worker_object_host|.
  //
  // |outside_fetch_client_settings_object| is used for loading the shared
  // worker main script by the browser process, sent to the renderer process,
  // and then used to load the script.
  void Start(
      mojo::PendingRemote<blink::mojom::SharedWorkerFactory> factory,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      blink::mojom::ControllerServiceWorkerInfoPtr controller,
      base::WeakPtr<ServiceWorkerObjectHost>
          controller_service_worker_object_host,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      const GURL& final_response_url);

  void AllowFileSystem(const GURL& url,
                       base::OnceCallback<void(bool)> callback);
  void AllowIndexedDB(const GURL& url, base::OnceCallback<void(bool)> callback);
  void AllowCacheStorage(const GURL& url,
                         base::OnceCallback<void(bool)> callback);
  void AllowWebLocks(const GURL& url, base::OnceCallback<void(bool)> callback);

  void CreateAppCacheBackend(
      mojo::PendingReceiver<blink::mojom::AppCacheBackend> receiver);
  void CreateQuicTransportConnector(
      mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver);
  void BindCacheStorage(
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver);

  // Causes this instance to be deleted, which will terminate the worker. May
  // be done based on a UI action.
  void Destruct();

  void AddClient(mojo::PendingRemote<blink::mojom::SharedWorkerClient> client,
                 GlobalFrameRoutingId client_render_frame_host_id,
                 const blink::MessagePortChannel& port,
                 ukm::SourceId client_ukm_source_id);

  void SetAppCacheHandle(
      std::unique_ptr<AppCacheNavigationHandle> appcache_handle);
  void SetServiceWorkerHandle(
      std::unique_ptr<ServiceWorkerMainResourceHandle> service_worker_handle);

  // Removes all clients whose RenderFrameHost has been destroyed before the
  // shared worker was started.
  void PruneNonExistentClients();

  // Returns true if this worker is connected to at least one client.
  bool HasClients() const;

  bool started() const { return started_; }

  const GURL& final_response_url() const { return final_response_url_; }

  const blink::SharedWorkerToken& token() const { return token_; }

  const SharedWorkerInstance& instance() const { return instance_; }

  const base::UnguessableToken& GetDevToolsToken() const;

  // Signals the remote worker to terminate and returns the mojo::Remote
  // instance so the caller can be notified when the connection is lost. Should
  // be called right before deleting this instance.
  mojo::Remote<blink::mojom::SharedWorker> TerminateRemoteWorkerForTesting();

  base::WeakPtr<SharedWorkerHost> AsWeakPtr();

  void ReportNoBinderForInterface(const std::string& error);

  // Creates a network factory params for subresource requests from this worker.
  network::mojom::URLLoaderFactoryParamsPtr
  CreateNetworkFactoryParamsForSubresources();

 private:
  friend class SharedWorkerHostTest;

  class ScopedDevToolsHandle;
  class ScopedProcessHostRef;

  // Contains information about a client connecting to this shared worker.
  struct ClientInfo {
    ClientInfo(mojo::Remote<blink::mojom::SharedWorkerClient> client,
               int connection_request_id,
               GlobalFrameRoutingId render_frame_host_id);
    ~ClientInfo();
    mojo::Remote<blink::mojom::SharedWorkerClient> client;
    const int connection_request_id;
    const GlobalFrameRoutingId render_frame_host_id;
  };

  using ClientList = std::list<ClientInfo>;

  // blink::mojom::SharedWorkerHost methods:
  void OnConnected(int connection_request_id) override;
  void OnContextClosed() override;
  void OnReadyForInspection(
      mojo::PendingRemote<blink::mojom::DevToolsAgent>,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>) override;
  void OnScriptLoadFailed(const std::string& error_message) override;
  void OnFeatureUsed(blink::mojom::WebFeature feature) override;

  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* render_process_host,
                           const ChildProcessTerminationInfo& info) override;

  // Returns the frame ids of this worker's clients.
  std::vector<GlobalFrameRoutingId> GetRenderFrameIDsForWorker();

  void AllowFileSystemResponse(base::OnceCallback<void(bool)> callback,
                               bool allowed);
  void OnClientConnectionLost();
  void OnWorkerConnectionLost();

  // Creates a network factory for subresource requests from this worker. The
  // network factory is meant to be passed to the renderer.
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateNetworkFactoryForSubresources(bool* bypass_redirect_checks);

  mojo::Receiver<blink::mojom::SharedWorkerHost> receiver_{this};

  // |service_| owns |this|.
  SharedWorkerServiceImpl* const service_;

  // An identifier for this worker that is unique across all workers. This is
  // generated by this object in the browser process.
  const blink::SharedWorkerToken token_;

  // This holds information used to match a shared worker connection request to
  // this shared worker.
  SharedWorkerInstance instance_;
  ClientList clients_;

  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver_;
  mojo::Remote<blink::mojom::SharedWorker> worker_;

  // The host of the process on which this shared worker lives.
  RenderProcessHost* const worker_process_host_;

  // Keep alive the renderer process that will be hosting the shared worker.
  std::unique_ptr<ScopedProcessHostRef> scoped_process_host_ref_;

  // Observe the destruction of |worker_process_host_|.
  ScopedObserver<RenderProcessHost, RenderProcessHostObserver>
      scoped_process_host_observer_;

  int next_connection_request_id_;

  std::unique_ptr<ScopedDevToolsHandle> devtools_handle_;

  // This is the set of features that this worker has used.
  std::set<blink::mojom::WebFeature> used_features_;

  std::unique_ptr<SharedWorkerContentSettingsProxyImpl> content_settings_;

  // This is kept alive during the lifetime of the shared worker, since it's
  // associated with Mojo interfaces (ServiceWorkerContainer and
  // URLLoaderFactory) that are needed to stay alive while the worker is
  // starting or running.
  mojo::Remote<blink::mojom::SharedWorkerFactory> factory_;

  BrowserInterfaceBrokerImpl<SharedWorkerHost, const url::Origin&> broker_{
      this};
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> broker_receiver_{
      &broker_};

  // The handle owns the precreated AppCacheHost until it's claimed by the
  // renderer after main script loading finishes.
  std::unique_ptr<AppCacheNavigationHandle> appcache_handle_;

  std::unique_ptr<ServiceWorkerMainResourceHandle> service_worker_handle_;

  // Indicates if Start() was invoked on this instance.
  bool started_ = false;

  GURL final_response_url_;

  const ukm::SourceId ukm_source_id_;

  base::WeakPtrFactory<SharedWorkerHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_HOST_H_
