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
#include "base/strings/string16.h"
#include "base/unguessable_token.h"
#include "content/browser/browser_interface_broker_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/shared_worker_instance.h"
#include "media/mojo/mojom/video_decode_perf_history.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
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
class URLLoaderFactoryBundleInfo;
}

namespace content {

class AppCacheNavigationHandle;
class ServiceWorkerNavigationHandle;
class ServiceWorkerObjectHost;
class SharedWorkerContentSettingsProxyImpl;
class SharedWorkerServiceImpl;

// The SharedWorkerHost is the interface that represents the browser side of
// the browser <-> worker communication channel. This is owned by
// SharedWorkerServiceImpl and destructed when a worker context or worker's
// message filter is closed.
class CONTENT_EXPORT SharedWorkerHost
    : public blink::mojom::SharedWorkerHost,
      public service_manager::mojom::InterfaceProvider {
 public:
  SharedWorkerHost(SharedWorkerServiceImpl* service,
                   const SharedWorkerInstance& instance,
                   int worker_process_id);
  ~SharedWorkerHost() override;

  // May return nullptr.
  RenderProcessHost* GetProcessHost() {
    return RenderProcessHost::FromID(worker_process_id_);
  }

  // Allows overriding the URLLoaderFactory creation for subresources.
  // Passing a null callback will restore the default behavior.
  // This method must be called either on the UI thread or before threads start.
  // This callback is run on the UI thread.
  using CreateNetworkFactoryCallback = base::RepeatingCallback<void(
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
      int worker_process_id,
      mojo::PendingRemote<network::mojom::URLLoaderFactory> original_factory)>;
  static void SetNetworkFactoryForSubresourcesForTesting(
      const CreateNetworkFactoryCallback& url_loader_factory_callback);

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
  void Start(
      mojo::PendingRemote<blink::mojom::SharedWorkerFactory> factory,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories,
      blink::mojom::ControllerServiceWorkerInfoPtr controller,
      base::WeakPtr<ServiceWorkerObjectHost>
          controller_service_worker_object_host);

  void AllowFileSystem(const GURL& url,
                       base::OnceCallback<void(bool)> callback);
  void AllowIndexedDB(const GURL& url, base::OnceCallback<void(bool)> callback);
  void AllowCacheStorage(const GURL& url,
                         base::OnceCallback<void(bool)> callback);
  void AllowWebLocks(const GURL& url, base::OnceCallback<void(bool)> callback);

  void CreateAppCacheBackend(
      mojo::PendingReceiver<blink::mojom::AppCacheBackend> receiver);
  void CreateIDBFactory(
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver);
  void CreateQuicTransportConnector(
      mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver);

  // Causes this instance to be deleted, which will terminate the worker. May
  // be done based on a UI action.
  void Destruct();

  void AddClient(mojo::PendingRemote<blink::mojom::SharedWorkerClient> client,
                 int client_process_id,
                 int frame_id,
                 const blink::MessagePortChannel& port);

  void SetAppCacheHandle(
      std::unique_ptr<AppCacheNavigationHandle> appcache_handle);
  void SetServiceWorkerHandle(
      std::unique_ptr<ServiceWorkerNavigationHandle> service_worker_handle);

  // Returns true if this worker is connected to at least one client.
  bool HasClients() const;

  const SharedWorkerInstance& instance() const { return instance_; }
  int worker_process_id() const { return worker_process_id_; }

  // Signals the remote worker to terminate and returns the mojo::Remote
  // instance so the caller can be notified when the connection is lost. Should
  // be called right before deleting this instance.
  mojo::Remote<blink::mojom::SharedWorker> TerminateRemoteWorkerForTesting();

  base::WeakPtr<SharedWorkerHost> AsWeakPtr();

 private:
  friend class SharedWorkerHostTest;

  class ScopedDevToolsHandle;

  // Contains information about a client connecting to this shared worker.
  struct ClientInfo {
    ClientInfo(mojo::Remote<blink::mojom::SharedWorkerClient> client,
               int connection_request_id,
               int client_process_id,
               int frame_id);
    ~ClientInfo();
    mojo::Remote<blink::mojom::SharedWorkerClient> client;
    const int connection_request_id;
    const int client_process_id;
    const int frame_id;
  };

  using ClientList = std::list<ClientInfo>;

  // blink::mojom::SharedWorkerHost methods:
  void OnConnected(int connection_request_id) override;
  void OnContextClosed() override;
  void OnReadyForInspection(
      mojo::PendingRemote<blink::mojom::DevToolsAgent>,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>) override;
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

  // Creates a network factory for subresource requests from this worker. The
  // network factory is meant to be passed to the renderer.
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateNetworkFactoryForSubresources(bool* bypass_redirect_checks);

  mojo::Receiver<blink::mojom::SharedWorkerHost> receiver_{this};

  // |service_| owns |this|.
  SharedWorkerServiceImpl* service_;
  SharedWorkerInstance instance_;
  ClientList clients_;

  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver_;
  mojo::Remote<blink::mojom::SharedWorker> worker_;

  const int worker_process_id_;
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

  mojo::Binding<service_manager::mojom::InterfaceProvider>
      interface_provider_binding_;

  BrowserInterfaceBrokerImpl<SharedWorkerHost, const url::Origin&> broker_{
      this};
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> broker_receiver_{
      &broker_};

  // The handle owns the precreated AppCacheHost until it's claimed by the
  // renderer after main script loading finishes.
  std::unique_ptr<AppCacheNavigationHandle> appcache_handle_;

  std::unique_ptr<ServiceWorkerNavigationHandle> service_worker_handle_;

  // Indicates if Start() was invoked on this instance.
  bool started_ = false;

  base::WeakPtrFactory<SharedWorkerHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_HOST_H_
