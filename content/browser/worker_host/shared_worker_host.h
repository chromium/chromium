// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_HOST_H_
#define CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_HOST_H_

#include <list>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "base/unguessable_token.h"
#include "content/browser/browser_interface_broker_impl.h"
#include "content/browser/buckets/bucket_context.h"
#include "content/browser/renderer_host/code_cache_host_impl.h"
#include "content/browser/renderer_host/policy_container_host.h"
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
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/base/network_isolation_key.h"
#include "services/device/public/cpp/compute_pressure/buildflags.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-forward.h"
#include "third_party/blink/public/mojom/broadcastchannel/broadcast_channel.mojom.h"
#include "third_party/blink/public/mojom/buckets/bucket_manager_host.mojom.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom-forward.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/webtransport/web_transport_connector.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_client.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_factory.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_host.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
#include "content/browser/compute_pressure/pressure_service_for_shared_worker.h"
#include "third_party/blink/public/mojom/compute_pressure/web_pressure_manager.mojom.h"
#endif  // BUILDFLAG(ENABLE_COMPUTE_PRESSURE)

class GURL;

namespace blink {
class MessagePortChannel;
class StorageKey;
}  // namespace blink

namespace content {

class ContentBrowserClient;
class CrossOriginEmbedderPolicyReporter;
class ServiceWorkerMainResourceHandle;
class SharedWorkerContentSettingsProxyImpl;
class SharedWorkerServiceImpl;
class SiteInstanceImpl;
struct WorkerScriptFetcherResult;

// SharedWorkerHost is the browser-side host of a single shared worker running
// in the renderer. This class is owned by the SharedWorkerServiceImpl of the
// current BrowserContext.
class CONTENT_EXPORT SharedWorkerHost : public blink::mojom::SharedWorkerHost,
                                        public RenderProcessHostObserver,
                                        public BucketContext,
                                        public base::SupportsUserData {
 public:
  SharedWorkerHost(
      SharedWorkerServiceImpl* service,
      const SharedWorkerInstance& instance,
      scoped_refptr<SiteInstanceImpl> site_instance,
      std::vector<network::mojom::ContentSecurityPolicyPtr>
          content_security_policies,
      scoped_refptr<PolicyContainerHost> creator_policy_container_host);

  SharedWorkerHost(const SharedWorkerHost&) = delete;
  SharedWorkerHost& operator=(const SharedWorkerHost&) = delete;

  ~SharedWorkerHost() override;

  // Returns the RenderProcessHost where this shared worker lives.
  // SharedWorkerHost can't outlive the RenderProcessHost so this can't be null.
  RenderProcessHost* GetProcessHost() const;

  // Starts the SharedWorker in the renderer process.
  //
  // |outside_fetch_client_settings_object| is used for loading the shared
  // worker main script by the browser process, sent to the renderer process,
  // and then used to load the script.
  //
  // |client| is used to determine the IP address space of the worker if the
  // script is fetched from a URL with a special scheme known only to the
  // embedder.
  //
  // `result` contains the worker main script fetch result.
  void Start(mojo::PendingRemote<blink::mojom::SharedWorkerFactory> factory,
             blink::mojom::FetchClientSettingsObjectPtr
                 outside_fetch_client_settings_object,
             ContentBrowserClient* client,
             WorkerScriptFetcherResult result);

  void AllowFileSystem(const GURL& url,
                       base::OnceCallback<void(bool)> callback);
  void AllowIndexedDB(const GURL& url, base::OnceCallback<void(bool)> callback);
  void AllowCacheStorage(const GURL& url,
                         base::OnceCallback<void(bool)> callback);
  void AllowWebLocks(const GURL& url, base::OnceCallback<void(bool)> callback);

  void CreateWebTransportConnector(
      mojo::PendingReceiver<blink::mojom::WebTransportConnector> receiver);
  void BindCacheStorage(
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver);
  void CreateBroadcastChannelProvider(
      mojo::PendingReceiver<blink::mojom::BroadcastChannelProvider> receiver);
  void CreateBlobUrlStoreProvider(
      mojo::PendingReceiver<blink::mojom::BlobURLStore> receiver);
  void CreateBucketManagerHost(
      mojo::PendingReceiver<blink::mojom::BucketManagerHost> receiver);

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
  void BindPressureService(
      mojo::PendingReceiver<blink::mojom::WebPressureManager> receiver);
#endif  // BUILDFLAG(ENABLE_COMPUTE_PRESSURE)

  // Causes this instance to be deleted, which will terminate the worker. May
  // be done based on a UI action.
  void Destruct();

  void AddClient(mojo::PendingRemote<blink::mojom::SharedWorkerClient> client,
                 GlobalRenderFrameHostId client_render_frame_host_id,
                 const blink::MessagePortChannel& port,
                 ukm::SourceId client_ukm_source_id);

  void SetServiceWorkerHandle(
      std::unique_ptr<ServiceWorkerMainResourceHandle> service_worker_handle);

  // Removes all clients whose RenderFrameHost has been destroyed before the
  // shared worker was started.
  void PruneNonExistentClients();

  // Returns true if this worker is connected to at least one client.
  bool HasClients() const;

  // Returns the frame ids of this worker's clients.
  std::vector<GlobalRenderFrameHostId> GetRenderFrameIDsForWorker();

  SiteInstanceImpl* site_instance() { return site_instance_.get(); }

  bool started() const { return started_; }

  const GURL& final_response_url() const { return final_response_url_; }

  const blink::SharedWorkerToken& token() const { return token_; }

  const SharedWorkerInstance& instance() const { return instance_; }

  const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy()
      const {
    return worker_client_security_state_->cross_origin_embedder_policy;
  }

  const network::mojom::ClientSecurityStatePtr& client_security_state() const {
    return worker_client_security_state_;
  }

  const std::vector<network::mojom::ContentSecurityPolicyPtr>&
  content_security_policies() const {
    return content_security_policies_;
  }

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
  PressureServiceForSharedWorker* pressure_service() {
    return pressure_service_.get();
  }
#endif  // BUILDFLAG(ENABLE_COMPUTE_PRESSURE)

  // Exposed so that tests can swap the implementation and intercept calls.
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker>&
  browser_interface_broker_receiver_for_testing() {
    return broker_receiver_;
  }

  ukm::SourceId ukm_source_id() const { return ukm_source_id_; }

  const base::UnguessableToken& GetDevToolsToken() const;

  // Signals the remote worker to terminate and returns the mojo::Remote
  // instance so the caller can be notified when the connection is lost. Should
  // be called right before deleting this instance.
  mojo::Remote<blink::mojom::SharedWorker> TerminateRemoteWorkerForTesting();

  base::WeakPtr<SharedWorkerHost> AsWeakPtr();

  net::NetworkIsolationKey GetNetworkIsolationKey() const;

  net::NetworkAnonymizationKey GetNetworkAnonymizationKey() const;

  const blink::StorageKey& GetStorageKey() const;

  const base::UnguessableToken& GetReportingSource() const {
    return reporting_source_;
  }

  void ReportNoBinderForInterface(const std::string& error);

  void CreateCodeCacheHost(
      mojo::PendingReceiver<blink::mojom::CodeCacheHost> receiver);

  // Creates a network factory params for subresource requests from this worker.
  network::mojom::URLLoaderFactoryParamsPtr
  CreateNetworkFactoryParamsForSubresources();

  // BucketContext:
  blink::StorageKey GetBucketStorageKey() override;
  blink::mojom::PermissionStatus GetPermissionStatus(
      blink::PermissionType permission_type) override;
  void BindCacheStorageForBucket(
      const storage::BucketInfo& bucket,
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver) override;
  void GetSandboxedFileSystemForBucket(
      const storage::BucketInfo& bucket,
      const std::vector<std::string>& directory_path_components,
      blink::mojom::BucketHost::GetDirectoryCallback callback) override;
  storage::BucketClientInfo GetBucketClientInfo() const override;

 private:
  friend class SharedWorkerHostTest;

  class ScopedDevToolsHandle;
  class ScopedProcessHostRef;

  // Contains information about a client connecting to this shared worker.
  struct ClientInfo {
    ClientInfo(mojo::Remote<blink::mojom::SharedWorkerClient> client,
               int connection_request_id,
               GlobalRenderFrameHostId render_frame_host_id);
    ~ClientInfo();
    mojo::Remote<blink::mojom::SharedWorkerClient> client;
    const int connection_request_id;
    const GlobalRenderFrameHostId render_frame_host_id;
  };

  using ClientList = std::list<ClientInfo>;

  // Returns true if the COEP policy of the worker and the creator are
  // compatible.
  bool CheckCrossOriginEmbedderPolicy(
      network::CrossOriginEmbedderPolicy creator_cross_origin_embedder_policy,
      network::CrossOriginEmbedderPolicy worker_cross_origin_embedder_policy);

  // blink::mojom::SharedWorkerHost methods:
  void OnConnected(int connection_request_id) override;
  void OnContextClosed() override;
  void OnReadyForInspection(
      mojo::PendingRemote<blink::mojom::DevToolsAgent>,
      mojo::PendingReceiver<blink::mojom::DevToolsAgentHost>) override;
  void OnScriptLoadFailed(const std::string& error_message) override;
  void OnFeatureUsed(blink::mojom::WebFeature feature) override;

  // RenderProcessHostObserver methods:
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  void AllowFileSystemResponse(base::OnceCallback<void(bool)> callback,
                               bool allowed);
  void OnClientConnectionLost();
  void OnWorkerConnectionLost();

  void BindCacheStorageInternal(
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver,
      const storage::BucketLocator& bucket_locator);

  // Creates a network factory for subresource requests from this worker. The
  // network factory is meant to be passed to the renderer.
  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateNetworkFactoryForSubresources(bool* bypass_redirect_checks);

  mojo::Receiver<blink::mojom::SharedWorkerHost> receiver_{this};

  // |service_| owns |this|.
  const raw_ptr<SharedWorkerServiceImpl> service_;

  // An identifier for this worker that is unique across all workers. This is
  // generated by this object in the browser process.
  const blink::SharedWorkerToken token_;

  // This holds information used to match a shared worker connection request to
  // this shared worker.
  SharedWorkerInstance instance_;
  ClientList clients_;

  std::vector<network::mojom::ContentSecurityPolicyPtr>
      content_security_policies_;

  mojo::PendingReceiver<blink::mojom::SharedWorker> worker_receiver_;
  mojo::Remote<blink::mojom::SharedWorker> worker_;

  // A SiteInstance whose process the shared worker runs in.
  const scoped_refptr<SiteInstanceImpl> site_instance_;

  // Keep alive the renderer process that will be hosting the shared worker.
  std::unique_ptr<ScopedProcessHostRef> scoped_process_host_ref_;

  int next_connection_request_id_;

  std::unique_ptr<ScopedDevToolsHandle> devtools_handle_;

  // This is the set of features that this worker has used.
  std::set<blink::mojom::WebFeature> used_features_;

  std::unique_ptr<SharedWorkerContentSettingsProxyImpl> content_settings_;

#if BUILDFLAG(ENABLE_COMPUTE_PRESSURE)
  std::unique_ptr<PressureServiceForSharedWorker> pressure_service_;
#endif  // BUILDFLAG(ENABLE_COMPUTE_PRESSURE)

  // This is kept alive during the lifetime of the shared worker, since it's
  // associated with Mojo interfaces (ServiceWorkerContainer and
  // URLLoaderFactory) that are needed to stay alive while the worker is
  // starting or running.
  mojo::Remote<blink::mojom::SharedWorkerFactory> factory_;

  BrowserInterfaceBrokerImpl<SharedWorkerHost, const url::Origin&> broker_{
      this};
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> broker_receiver_{
      &broker_};

  std::unique_ptr<ServiceWorkerMainResourceHandle> service_worker_handle_;

  // CodeCacheHost processes requests to fetch / write generated code for
  // JavaScript / WebAssembly resources.
  CodeCacheHostImpl::ReceiverSet code_cache_host_receivers_;

  // Indicates if Start() was invoked on this instance.
  bool started_ = false;

  GURL final_response_url_;

  const ukm::SourceId ukm_source_id_;

  const base::UnguessableToken reporting_source_;

  // Set at construction time and should not change afterwards.
  const scoped_refptr<PolicyContainerHost> creator_policy_container_host_;

  // The worker's own client security state, applied to subresource fetches.
  // This is nullptr until it is computed in `DidStartScriptLoad()`.
  network::mojom::ClientSecurityStatePtr worker_client_security_state_;

  std::unique_ptr<CrossOriginEmbedderPolicyReporter> coep_reporter_;

  base::WeakPtrFactory<SharedWorkerHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_HOST_H_
