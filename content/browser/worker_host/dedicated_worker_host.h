// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_HOST_H_
#define CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_HOST_H_

#include "build/build_config.h"
#include "content/browser/browser_interface_broker_impl.h"
#include "content/public/browser/render_process_host.h"
#include "media/mojo/mojom/video_decode_perf_history.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom-forward.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom-forward.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom-forward.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom-forward.h"
#include "third_party/blink/public/mojom/websockets/websocket_connector.mojom-forward.h"
#include "third_party/blink/public/mojom/webtransport/quic_transport_connector.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host.mojom.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host_factory.mojom.h"
#include "third_party/blink/public/mojom/worker/subresource_loader_updater.mojom.h"

#if !defined(OS_ANDROID)
#include "third_party/blink/public/mojom/serial/serial.mojom-forward.h"
#endif

namespace url {
class Origin;
}

namespace content {

class ServiceWorkerNavigationHandle;
class ServiceWorkerObjectHost;
class StoragePartitionImpl;

// Creates a host factory for a dedicated worker. This must be called on the UI
// thread.
void CreateDedicatedWorkerHostFactory(
    int creator_process_id,
    int ancestor_render_frame_id,
    int creator_render_frame_id,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHostFactory> receiver);

// A host for a single dedicated worker. Its lifetime is managed by the
// DedicatedWorkerGlobalScope of the corresponding worker in the renderer via a
// StrongBinding. This lives on the UI thread.
class DedicatedWorkerHost final
    : public service_manager::mojom::InterfaceProvider,
      public blink::mojom::DedicatedWorkerHost {
 public:
  DedicatedWorkerHost(
      int worker_process_id,
      int ancestor_render_frame_id,
      int creator_render_frame_id,
      const url::Origin& origin,
      mojo::PendingReceiver<blink::mojom::DedicatedWorkerHost> host);
  ~DedicatedWorkerHost() final;

  void BindBrowserInterfaceBrokerReceiver(
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver);

  // May return nullptr.
  RenderProcessHost* GetProcessHost() {
    return RenderProcessHost::FromID(worker_process_id_);
  }
  const url::Origin& GetOrigin() { return origin_; }

  void CreateIdleManager(
      mojo::PendingReceiver<blink::mojom::IdleManager> receiver);
  void CreateIDBFactory(
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver);
  void BindSmsReceiverReceiver(
      mojo::PendingReceiver<blink::mojom::SmsReceiver> receiver);
  void CreateWebUsbService(
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver);
  void CreateQuicTransportConnector(
      mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver);

#if !defined(OS_ANDROID)
  void BindSerialService(
      mojo::PendingReceiver<blink::mojom::SerialService> receiver);
#endif

  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override;

  // blink::mojom::DedicatedWorkerHost:
  void LifecycleStateChanged(blink::mojom::FrameLifecycleState state) override;

  // TODO(dtapuska): This state needs to be hooked up to the
  // ServiceWorkerProviderHost so the correct state is queried when looking
  // for frozen dedicated workers. crbug.com/968417
  bool is_frozen() const { return is_frozen_; }

  // PlzDedicatedWorker:
  void StartScriptLoad(
      const GURL& script_url,
      const url::Origin& request_initiator_origin,
      network::mojom::CredentialsMode credentials_mode,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
      mojo::Remote<blink::mojom::DedicatedWorkerHostFactoryClient> client);

 private:
  void RegisterMojoInterfaces();

  // Called from WorkerScriptFetchInitiator. Continues starting the dedicated
  // worker in the renderer process.
  //
  // |main_script_load_params| is sent to the renderer process and to be used to
  // load the dedicated worker main script pre-requested by the browser process.
  //
  // |subresource_loader_factories| is sent to the renderer process and is to be
  // used to request subresources where applicable. For example, this allows the
  // dedicated worker to load chrome-extension:// URLs which the renderer's
  // default loader factory can't load.
  //
  // |controller| contains information about the service worker controller. Once
  // a ServiceWorker object about the controller is prepared, it is registered
  // to |controller_service_worker_object_host|.
  void DidStartScriptLoad(
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      blink::mojom::ControllerServiceWorkerInfoPtr controller,
      base::WeakPtr<ServiceWorkerObjectHost>
          controller_service_worker_object_host,
      bool success);

  // Sets up the observer of network service crash.
  void ObserveNetworkServiceCrash(StoragePartitionImpl* storage_partition_impl);

  // Creates a network factory for subresource requests from this worker. The
  // network factory is meant to be passed to the renderer.
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateNetworkFactoryForSubresources(RenderProcessHost* worker_process_host,
                                      RenderFrameHostImpl* render_frame_host,
                                      bool* bypass_redirect_checks);

  void CreateWebSocketConnector(
      mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver);

  void CreateNestedDedicatedWorker(
      mojo::PendingReceiver<blink::mojom::DedicatedWorkerHostFactory> receiver);

  // Updates subresource loader factories. This is supposed to be called when
  // out-of-process Network Service crashes.
  void UpdateSubresourceLoaderFactories();

  // May return a nullptr.
  RenderFrameHostImpl* GetAncestorRenderFrameHost();

  // The ID of the render process host that hosts this worker.
  const int worker_process_id_;

  // The ID of the frame that owns this worker, either directly, or (in the case
  // of nested workers) indirectly via a tree of dedicated workers.
  const int ancestor_render_frame_id_;

  // The ID of the frame that directly starts this worker. This is
  // MSG_ROUTING_NONE when this worker is nested.
  const int creator_render_frame_id_;

  const url::Origin origin_;

  // The network isolation key to be used for both the worker script and the
  // worker's subresources.
  net::NetworkIsolationKey network_isolation_key_;

  // This is kept alive during the lifetime of the dedicated worker, since it's
  // associated with Mojo interfaces (ServiceWorkerContainer and
  // URLLoaderFactory) that are needed to stay alive while the worker is
  // starting or running.
  mojo::Remote<blink::mojom::DedicatedWorkerHostFactoryClient> client_;

  std::unique_ptr<ServiceWorkerNavigationHandle> service_worker_handle_;

  service_manager::BinderRegistry registry_;

  BrowserInterfaceBrokerImpl<DedicatedWorkerHost, const url::Origin&> broker_{
      this};
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> broker_receiver_{
      &broker_};
  mojo::Receiver<blink::mojom::DedicatedWorkerHost> host_receiver_;

  // Indicates if subresource loaders of this worker support file URLs.
  bool file_url_support_ = false;

  // The liveness state of the dedicated worker in the renderer.
  bool is_frozen_ = false;

  // For observing Network Service connection errors only.
  mojo::Remote<network::mojom::URLLoaderFactory>
      network_service_connection_error_handler_holder_;
  mojo::Remote<blink::mojom::SubresourceLoaderUpdater>
      subresource_loader_updater_;

  base::WeakPtrFactory<DedicatedWorkerHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DedicatedWorkerHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_HOST_H_
