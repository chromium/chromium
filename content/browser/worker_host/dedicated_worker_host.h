// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_HOST_H_
#define CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_HOST_H_

#include <memory>

#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "content/browser/browser_interface_broker_impl.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/base/isolation_info.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/mojom/loader/content_security_notifier.mojom.h"
#include "third_party/blink/public/mojom/sms/webotp_service.mojom-forward.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom-forward.h"
#include "third_party/blink/public/mojom/wake_lock/wake_lock.mojom-forward.h"
#include "third_party/blink/public/mojom/websockets/websocket_connector.mojom-forward.h"
#include "third_party/blink/public/mojom/webtransport/web_transport_connector.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host.mojom.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host_factory.mojom.h"
#include "third_party/blink/public/mojom/worker/subresource_loader_updater.mojom.h"
#include "url/origin.h"

#if !defined(OS_ANDROID)
#include "third_party/blink/public/mojom/serial/serial.mojom-forward.h"
#endif

namespace content {

class ServiceWorkerContainerHost;
class ServiceWorkerRegistration;
class DedicatedWorkerServiceImpl;
class ServiceWorkerMainResourceHandle;
class ServiceWorkerObjectHost;
class StoragePartitionImpl;
class CrossOriginEmbedderPolicyReporter;

// A host for a single dedicated worker. It deletes itself upon Mojo
// disconnection from the worker in the renderer or when the RenderProcessHost
// of the worker is destroyed. This lives on the UI thread.
// TODO(crbug.com/1177652): Align this class's lifetime with the associated
// frame.
class DedicatedWorkerHost final : public blink::mojom::DedicatedWorkerHost,
                                  public RenderProcessHostObserver {
 public:
  DedicatedWorkerHost(
      DedicatedWorkerServiceImpl* service,
      const blink::DedicatedWorkerToken& token,
      RenderProcessHost* worker_process_host,
      absl::optional<GlobalRenderFrameHostId> creator_render_frame_host_id,
      absl::optional<blink::DedicatedWorkerToken> creator_worker_token,
      GlobalRenderFrameHostId ancestor_render_frame_host_id,
      const blink::StorageKey& creator_storage_key,
      const net::IsolationInfo& isolation_info,
      const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
      base::WeakPtr<CrossOriginEmbedderPolicyReporter> creator_coep_reporter,
      base::WeakPtr<CrossOriginEmbedderPolicyReporter> ancestor_coep_reporter,
      mojo::PendingReceiver<blink::mojom::DedicatedWorkerHost> host);
  ~DedicatedWorkerHost() final;

  void BindBrowserInterfaceBrokerReceiver(
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> receiver);

  const blink::DedicatedWorkerToken& GetToken() const { return token_; }
  RenderProcessHost* GetProcessHost() const { return worker_process_host_; }
  const blink::StorageKey& GetStorageKey() const { return storage_key_; }
  const GlobalRenderFrameHostId& GetAncestorRenderFrameHostId() const {
    return ancestor_render_frame_host_id_;
  }
  const absl::optional<GURL>& GetFinalResponseURL() const {
    return final_response_url_;
  }

  void CreateContentSecurityNotifier(
      mojo::PendingReceiver<blink::mojom::ContentSecurityNotifier> receiver);
  void CreateIdleManager(
      mojo::PendingReceiver<blink::mojom::IdleManager> receiver);
  void CreateNestedDedicatedWorker(
      mojo::PendingReceiver<blink::mojom::DedicatedWorkerHostFactory> receiver);
  void BindWebOTPServiceReceiver(
      mojo::PendingReceiver<blink::mojom::WebOTPService> receiver);
  void CreateWebUsbService(
      mojo::PendingReceiver<blink::mojom::WebUsbService> receiver);
  void CreateWebSocketConnector(
      mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver);
  void CreateWebTransportConnector(
      mojo::PendingReceiver<blink::mojom::WebTransportConnector> receiver);
  void CreateWakeLockService(
      mojo::PendingReceiver<blink::mojom::WakeLockService> receiver);
  void BindCacheStorage(
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver);
  void CreateCodeCacheHost(
      mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver);

#if !defined(OS_ANDROID)
  void BindSerialService(
      mojo::PendingReceiver<blink::mojom::SerialService> receiver);
#endif

  // PlzDedicatedWorker:
  void StartScriptLoad(
      const GURL& script_url,
      network::mojom::CredentialsMode credentials_mode,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
      mojo::Remote<blink::mojom::DedicatedWorkerHostFactoryClient> client);

  void ReportNoBinderForInterface(const std::string& error);

  // TODO(crbug.com/906991): Remove this method once PlzDedicatedWorker is
  // enabled by default.
  void MaybeCountWebFeature(const GURL& script_url);
  // TODO(crbug.com/906991): Remove this method once PlzDedicatedWorker is
  // enabled by default.
  void ContinueOnMaybeCountWebFeature(
      const GURL& script_url,
      base::WeakPtr<ServiceWorkerContainerHost> container_host,
      blink::ServiceWorkerStatusCode status,
      const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
          registrations);

  const net::NetworkIsolationKey& GetNetworkIsolationKey() const {
    return isolation_info_.network_isolation_key();
  }

  const base::UnguessableToken& GetReportingSource() const {
    return reporting_source_;
  }

  const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy()
      const {
    DCHECK(worker_cross_origin_embedder_policy_.has_value());
    return worker_cross_origin_embedder_policy_.value();
  }

  ServiceWorkerMainResourceHandle* service_worker_handle() {
    return service_worker_handle_.get();
  }

 private:
  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* render_process_host,
                           const ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  // Called from WorkerScriptFetchInitiator. Continues starting the dedicated
  // worker in the renderer process.
  //
  // |success| is true only when the script fetch succeeded.
  //
  // Note: None of the following parameters are valid if |success| is false.
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
  //
  // |final_response_url| is the URL calculated from the initial request URL,
  // redirect chain, and URLs fetched via service worker.
  // https://fetch.spec.whatwg.org/#concept-response-url
  void DidStartScriptLoad(
      bool success,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      blink::mojom::ControllerServiceWorkerInfoPtr controller,
      base::WeakPtr<ServiceWorkerObjectHost>
          controller_service_worker_object_host,
      const GURL& final_response_url);

  // Sets up the observer of network service crash.
  void ObserveNetworkServiceCrash(StoragePartitionImpl* storage_partition_impl);

  // Creates a network factory for subresource requests from this worker. The
  // network factory is meant to be passed to the renderer.
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateNetworkFactoryForSubresources(
      RenderFrameHostImpl* ancestor_render_frame_host,
      bool* bypass_redirect_checks);

  // Updates subresource loader factories. This is supposed to be called when
  // out-of-process Network Service crashes.
  void UpdateSubresourceLoaderFactories();

  void OnMojoDisconnect();

  // Returns true if creator and worker's COEP values are valid.
  bool CheckCrossOriginEmbedderPolicy(
      network::CrossOriginEmbedderPolicy creator_cross_origin_embedder_policy,
      network::CrossOriginEmbedderPolicy worker_cross_origin_embedder_policy);

  base::WeakPtr<CrossOriginEmbedderPolicyReporter> GetWorkerCoepReporter();

  // This outlives `this` as follows:
  //  - StoragePartitionImpl owns DedicatedWorkerServiceImpl until its dtor.
  //  - StoragePartitionImpl outlives RenderProcessHostImpl.
  //  - RenderProcessHostImpl outlives DedicatedWorkerHost.
  // As the conclusion of the above, DedicatedWorkerServiceImpl outlives
  // DedicatedWorkerHost.
  DedicatedWorkerServiceImpl* const service_;

  // The renderer generated ID of this worker, unique across all processes.
  const blink::DedicatedWorkerToken token_;

  // The RenderProcessHost that hosts this worker. This outlives `this`.
  RenderProcessHost* const worker_process_host_;

  base::ScopedObservation<RenderProcessHost, RenderProcessHostObserver>
      scoped_process_host_observation_{this};

  // The ID of the frame that directly starts this worker. This is absl::nullopt
  // when this worker is nested.
  const absl::optional<GlobalRenderFrameHostId> creator_render_frame_host_id_;

  // The token of the dedicated worker that directly starts this worker. This is
  // absl::nullopt when this worker is created from a frame.
  const absl::optional<blink::DedicatedWorkerToken> creator_worker_token_;

  // The ID of the frame that owns this worker, either directly, or (in the case
  // of nested workers) indirectly via a tree of dedicated workers.
  const GlobalRenderFrameHostId ancestor_render_frame_host_id_;

  // The origin of the frame or dedicated worker that starts this worker.
  const url::Origin creator_origin_;

  // The storage key of this worker. This is used for storage partitioning and
  // for retrieving the origin of this worker
  // (https://html.spec.whatwg.org/C/#concept-settings-object-origin).
  const blink::StorageKey storage_key_;

  // The IsolationInfo associated with this worker. Same as that of the
  // frame or the worker that created this worker.
  const net::IsolationInfo isolation_info_;

  const base::UnguessableToken reporting_source_;

  // The frame/worker's Cross-Origin-Embedder-Policy (COEP) that directly starts
  // this worker.
  const network::CrossOriginEmbedderPolicy
      creator_cross_origin_embedder_policy_;

  // The DedicatedWorker's Cross-Origin-Embedder-Policy (COEP). This is set when
  // the script's response head is loaded.
  absl::optional<network::CrossOriginEmbedderPolicy>
      worker_cross_origin_embedder_policy_;

  // This is kept alive during the lifetime of the dedicated worker, since it's
  // associated with Mojo interfaces (ServiceWorkerContainer and
  // URLLoaderFactory) that are needed to stay alive while the worker is
  // starting or running.
  mojo::Remote<blink::mojom::DedicatedWorkerHostFactoryClient> client_;

  std::unique_ptr<ServiceWorkerMainResourceHandle> service_worker_handle_;

  // BrowserInterfaceBroker implementation through which this
  // DedicatedWorkerHost exposes worker-scoped Mojo services to the
  // corresponding worker in the renderer.
  //
  // The interfaces that can be requested from this broker are defined in the
  // content/browser/browser_interface_binders.cc file, in the functions which
  // take a `DedicatedWorkerHost*` parameter.
  BrowserInterfaceBrokerImpl<DedicatedWorkerHost, const url::Origin&> broker_{
      this};
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> broker_receiver_{
      &broker_};
  mojo::Receiver<blink::mojom::DedicatedWorkerHost> host_receiver_;

  // Indicates if subresource loaders of this worker support file URLs.
  bool file_url_support_ = false;

  // For observing Network Service connection errors only.
  mojo::Remote<network::mojom::URLLoaderFactory>
      network_service_connection_error_handler_holder_;
  mojo::Remote<blink::mojom::SubresourceLoaderUpdater>
      subresource_loader_updater_;

  // For the PlzDedicatedWorker case. `coep_reporter_` is valid after
  // DidStartScriptLoad() and remains non-null for the lifetime of `this`.
  std::unique_ptr<CrossOriginEmbedderPolicyReporter> coep_reporter_;
  // TODO(crbug.com/1177652): Remove `creator_coep_reporter_` after this class's
  // lifetime is aligned with the associated frame.
  base::WeakPtr<CrossOriginEmbedderPolicyReporter> creator_coep_reporter_;

  // For the non-PlzDedicatedWorker case. Sending reports to the ancestor frame
  // is not the behavior defined in the spec, but keep the current behavior and
  // not to lose reports.
  // TODO(crbug.com/906991): Remove `ancestor_coep_reporter_` once
  // PlzDedicatedWorker is enabled by default.
  base::WeakPtr<CrossOriginEmbedderPolicyReporter> ancestor_coep_reporter_;

  // Will be set once the worker script started loading.
  absl::optional<GURL> final_response_url_;

  // CodeCacheHost processes requests to fetch / write generated code for
  // JavaScript / WebAssembly resources.
  CodeCacheHostImpl::ReceiverSet code_cache_host_receivers_;

  base::WeakPtrFactory<DedicatedWorkerHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DedicatedWorkerHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_HOST_H_
