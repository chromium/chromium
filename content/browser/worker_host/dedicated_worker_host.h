// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_HOST_H_
#define CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_HOST_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "content/browser/browser_interface_broker_impl.h"
#include "content/browser/buckets/bucket_context.h"
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
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-forward.h"
#include "third_party/blink/public/mojom/broadcastchannel/broadcast_channel.mojom.h"
#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/mojom/loader/content_security_notifier.mojom.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom-forward.h"
#include "third_party/blink/public/mojom/wake_lock/wake_lock.mojom-forward.h"
#include "third_party/blink/public/mojom/websockets/websocket_connector.mojom-forward.h"
#include "third_party/blink/public/mojom/webtransport/web_transport_connector.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host.mojom.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host_factory.mojom.h"
#include "third_party/blink/public/mojom/worker/subresource_loader_updater.mojom.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_ANDROID)
#include "third_party/blink/public/mojom/serial/serial.mojom-forward.h"
#endif

namespace network {

struct CrossOriginEmbedderPolicy;

}  // namespace network

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
// TODO(crbug.com/1273717): Align this class's lifetime with the associated
// frame.
class DedicatedWorkerHost final
    : public blink::mojom::DedicatedWorkerHost,
      public blink::mojom::BackForwardCacheControllerHost,
      public RenderProcessHostObserver,
      public BucketContext {
 public:
  // Creates a new browser-side host for a single dedicated worker.
  //
  // See the class-level comment for lifetime considerations.
  //
  // - `service` must not be nullptr and must outlive this instance.
  // - `worker_process_host` must not be nullptr and must outlive this instance.
  //   It must be initialized and not be dead - see
  //   `RenderProcessHost::IsInitializedAndNotDead()`.
  // - Exactly one of `creator_render_frame_host_id` or `creator_worker_token`
  //   must be specified.
  // - `creator_client_security_state` specifies the client security state of
  //   the creator frame or worker. It must not be nullptr.
  DedicatedWorkerHost(
      DedicatedWorkerServiceImpl* service,
      const blink::DedicatedWorkerToken& token,
      RenderProcessHost* worker_process_host,
      absl::optional<GlobalRenderFrameHostId> creator_render_frame_host_id,
      absl::optional<blink::DedicatedWorkerToken> creator_worker_token,
      GlobalRenderFrameHostId ancestor_render_frame_host_id,
      const blink::StorageKey& creator_storage_key,
      const net::IsolationInfo& isolation_info,
      network::mojom::ClientSecurityStatePtr creator_client_security_state,
      base::WeakPtr<CrossOriginEmbedderPolicyReporter> creator_coep_reporter,
      base::WeakPtr<CrossOriginEmbedderPolicyReporter> ancestor_coep_reporter,
      mojo::PendingReceiver<blink::mojom::DedicatedWorkerHost> host);

  DedicatedWorkerHost(const DedicatedWorkerHost&) = delete;
  DedicatedWorkerHost& operator=(const DedicatedWorkerHost&) = delete;

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
  void BindCacheStorageInternal(
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver,
      const storage::BucketLocator& bucket_locator);
  void CreateCodeCacheHost(
      mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver);
  void CreateBroadcastChannelProvider(
      mojo::PendingReceiver<blink::mojom::BroadcastChannelProvider> receiver);
  void CreateBlobUrlStoreProvider(
      mojo::PendingReceiver<blink::mojom::BlobURLStore> receiver);
  void CreateBucketManagerHost(
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver);

#if !BUILDFLAG(IS_ANDROID)
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

  const net::NetworkAnonymizationKey& GetNetworkAnonymizationKey() const {
    return isolation_info_.network_anonymization_key();
  }

  const base::UnguessableToken& GetReportingSource() const {
    return reporting_source_;
  }

  // Returns the client security state applied to subresource fetches.
  // May return nullptr before the script is loaded.
  const network::mojom::ClientSecurityState* client_security_state() const {
    return worker_client_security_state_.get();
  }

  const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy()
      const {
    DCHECK(worker_client_security_state_);
    return worker_client_security_state_->cross_origin_embedder_policy;
  }

  ServiceWorkerMainResourceHandle* service_worker_handle() {
    return service_worker_handle_.get();
  }

  // blink::mojom::BackForwardCacheControllerHost:
  void EvictFromBackForwardCache(
      blink::mojom::RendererEvictionReason reason) override;
  using BackForwardCacheBlockingDetails =
      std::vector<blink::mojom::BlockingDetailsPtr>;
  void DidChangeBackForwardCacheDisablingFeatures(
      BackForwardCacheBlockingDetails details) override;

  // BucketContext:
  blink::StorageKey GetBucketStorageKey() override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission_type) override;
  void BindCacheStorageForBucket(
      const storage::BucketInfo& bucket,
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) override;
  void GetSandboxedFileSystemForBucket(
      const storage::BucketInfo& bucket,
      blink::mojom::FileSystemAccessManager::GetSandboxedFileSystemCallback
          callback) override;
  GlobalRenderFrameHostId GetAssociatedRenderFrameHostId() const override;

  // Returns the features set that disable back-forward cache.
  blink::scheduler::WebSchedulerTrackedFeatures
  GetBackForwardCacheDisablingFeatures() const;

  base::WeakPtr<ServiceWorkerContainerHost> GetServiceWorkerContainerHost();

  mojo::PendingRemote<blink::mojom::BackForwardCacheControllerHost>
  BindAndPassRemoteForBackForwardCacheControllerHost();

  base::WeakPtr<DedicatedWorkerHost> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // RenderProcessHostObserver:
  void RenderProcessExited(RenderProcessHost* render_process_host,
                           const ChildProcessTerminationInfo& info) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  // Called from `WorkerScriptFetcher`. Continues starting the dedicated worker
  // in the renderer process.
  //
  // `main_script_load_params` is not nullptr iff the fetch succeeded. This is
  // sent to the renderer process and to be used to load the dedicated worker
  // main script pre-requested by the browser process.
  //
  // The following parameters are valid iff `main_script_load_params` is not
  // nullptr, i.e. iff the fetch succeeded.
  //
  // `subresource_loader_factories` is sent to the renderer process and is to be
  // used to request subresources where applicable. For example, this allows the
  // dedicated worker to load chrome-extension:// URLs which the renderer's
  // default loader factory can't load.
  //
  // `controller` contains information about the service worker controller. Once
  // a ServiceWorker object about the controller is prepared, it is registered
  // to `controller_service_worker_object_host`.
  //
  // `final_response_url` is the URL calculated from the initial request URL,
  // redirect chain, and URLs fetched via service worker.
  // https://fetch.spec.whatwg.org/#concept-response-url
  void DidStartScriptLoad(
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          subresource_loader_factories,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      blink::mojom::ControllerServiceWorkerInfoPtr controller,
      base::WeakPtr<ServiceWorkerObjectHost>
          controller_service_worker_object_host,
      const GURL& final_response_url);

  void ScriptLoadStartFailed(const GURL& url,
                             const network::URLLoaderCompletionStatus& status);

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

  // Returns whether creator and worker's COEP values are compatible.
  bool CheckCrossOriginEmbedderPolicy();

  base::WeakPtr<CrossOriginEmbedderPolicyReporter> GetWorkerCoepReporter();

  // This outlives `this` as follows:
  //  - StoragePartitionImpl owns DedicatedWorkerServiceImpl until its dtor.
  //  - StoragePartitionImpl outlives RenderProcessHostImpl.
  //  - RenderProcessHostImpl outlives DedicatedWorkerHost.
  // As the conclusion of the above, DedicatedWorkerServiceImpl outlives
  // DedicatedWorkerHost.
  const raw_ptr<DedicatedWorkerServiceImpl> service_;

  // The renderer generated ID of this worker, unique across all processes.
  const blink::DedicatedWorkerToken token_;

  // The RenderProcessHost that hosts this worker. This outlives `this`.
  const raw_ptr<RenderProcessHost> worker_process_host_;

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

  // The client security state of the creator execution context. Never nullptr.
  // Copied at construction time.
  //
  // TODO(https://crbug.com/1177652): Consider removing this member once the
  // creator always outlives this instance. In that case, we could copy the
  // creator's client security state lazily instead of eagerly.
  const network::mojom::ClientSecurityStatePtr creator_client_security_state_;

  // The client security state of this worker, used for subresource fetches.
  //
  // If PlzDedicatedWorker is disabled, it is cloned from
  // `creator_client_security_state_` at construction time.
  //
  // Otherwise, it is nullptr until the script's response head is loaded, at
  // which point it is calculated based on the response info. If the response is
  // loaded from a URL with a local scheme, then the worker inherits its
  // creator's client security state.
  network::mojom::ClientSecurityStatePtr worker_client_security_state_;

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
  mojo::Receiver<blink::mojom::BackForwardCacheControllerHost>
      back_forward_cache_controller_host_receiver_{this};

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

  blink::scheduler::WebSchedulerTrackedFeatures bfcache_disabling_features_;

  base::WeakPtrFactory<DedicatedWorkerHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_DEDICATED_WORKER_HOST_H_
