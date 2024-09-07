// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CLIENT_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CLIENT_H_

#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/back_forward_cache_metrics.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/content_export.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/service_worker_client_info.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom-forward.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_running_status_callback.mojom-forward.h"

namespace content {
class ScopedServiceWorkerClient;
class ServiceWorkerClientOwner;
class ServiceWorkerContainerHostForClient;
class ServiceWorkerContextCore;
class ServiceWorkerVersion;
class StoragePartitionImpl;
struct GlobalRenderFrameHostId;
struct PolicyContainerPolicies;

// `ServiceWorkerClient` is owned by `ServiceWorkerContextCore` and represents a
// service worker client in the spec.
// https://w3c.github.io/ServiceWorker/#dfn-service-worker-client
//
// Example:
// When a new service worker registration is created, the browser process
// iterates over all ServiceWorkerClients to find clients (frames,
// dedicated workers if PlzDedicatedWorker is enabled, and shared workers) with
// a URL inside the registration's scope, and has the container host watch the
// registration in order to resolve navigator.serviceWorker.ready once the
// registration settles, if need.
class CONTENT_EXPORT ServiceWorkerClient final
    : public ServiceWorkerRegistration::Listener {
 public:
  using ExecutionReadyCallback = base::OnceClosure;

  // Constructor for window clients.
  ServiceWorkerClient(base::WeakPtr<ServiceWorkerContextCore> context,
                      bool is_parent_frame_secure,
                      FrameTreeNodeId frame_tree_node_id);

  // Constructor for worker clients.
  ServiceWorkerClient(base::WeakPtr<ServiceWorkerContextCore> context,
                      int process_id,
                      ServiceWorkerClientInfo client_info);

  ServiceWorkerClient(const ServiceWorkerClient& other) = delete;
  ServiceWorkerClient& operator=(const ServiceWorkerClient& other) = delete;

  virtual ~ServiceWorkerClient();

  ServiceWorkerContainerHostForClient* container_host() {
    return container_host_.get();
  }
  const ServiceWorkerContainerHostForClient* container_host() const {
    return container_host_.get();
  }

  // ServiceWorkerRegistration::Listener overrides.
  void OnVersionAttributesChanged(
      ServiceWorkerRegistration* registration,
      blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask) override;
  void OnRegistrationFailed(ServiceWorkerRegistration* registration) override;
  void OnRegistrationFinishedUninstalling(
      ServiceWorkerRegistration* registration) override;
  void OnSkippedWaiting(ServiceWorkerRegistration* registration) override;

  // For service worker clients. The host keeps track of all the prospective
  // longest-matching registrations, in order to resolve .ready or respond to
  // claim() attempts.
  //
  // This is subtle: it doesn't keep all registrations (e.g., from storage) in
  // memory, but just the ones that are possibly the longest-matching one. The
  // best match from storage is added at load time. That match can't uninstall
  // while this host is a controllee, so all the other stored registrations can
  // be ignored. Only a newly installed registration can claim it, and new
  // installing registrations are added as matches.
  void AddMatchingRegistration(ServiceWorkerRegistration* registration);
  void RemoveMatchingRegistration(ServiceWorkerRegistration* registration);

  // An optimized implementation of [[Match Service Worker Registration]]
  // for the current client.
  ServiceWorkerRegistration* MatchRegistration() const;

  // For service worker clients. Called when |version| is the active worker upon
  // the main resource request for this client. Remembers |version| as needing
  // a Soft Update. To avoid affecting page load performance, the update occurs
  // when we get a HintToUpdateServiceWorker message from the renderer, or when
  // |this| is destroyed before receiving that message.
  //
  // Corresponds to the Handle Fetch algorithm:
  // "If request is a non-subresource request...invoke Soft Update algorithm
  // with registration."
  // https://w3c.github.io/ServiceWorker/#on-fetch-request-algorithm
  //
  // This can be called multiple times due to redirects during a main resource
  // load. All service workers are updated.
  void AddServiceWorkerToUpdate(scoped_refptr<ServiceWorkerVersion> version);

  // Notifies the client that its controller used a feature, for UseCounter
  // purposes. This can only be called if IsContainerForClient() is true.
  void CountFeature(blink::mojom::WebFeature feature);

  // Called when this container host's controller has been terminated and doomed
  // due to an exceptional condition like it could no longer be read from the
  // script cache.
  void NotifyControllerLost();

  // Returns the client type of this container host. Can only be called when
  // IsContainerForClient() is true.
  blink::mojom::ServiceWorkerClientType GetClientType() const;

  // Returns true if this container host is specifically for a window client.
  bool IsContainerForWindowClient() const;

  // Returns true if this container host is specifically for a worker client.
  bool IsContainerForWorkerClient() const;

  // Returns the client info for this container host.
  ServiceWorkerClientInfo GetServiceWorkerClientInfo() const;

  // Transitions to `kResponseCommitted`.
  // `rfh_id` is given only for window clients.
  // Must be called from `ScopedServiceWorkerClient` to centralize the
  // lifetime control there.
  // TODO(falken): Pass in an RenderFrameHostImpl instead of an ID. Some
  // tests use a fake id.
  blink::mojom::ServiceWorkerContainerInfoForClientPtr CommitResponse(
      base::PassKey<ScopedServiceWorkerClient>,
      std::optional<GlobalRenderFrameHostId> rfh_id,
      const PolicyContainerPolicies& policy_container_policies,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter,
      ukm::SourceId ukm_source_id);

  // For service worker window clients. Called after the navigation commits to a
  // RenderFrameHost. At this point, the previous ServiceWorkerContainerHost
  // for that RenderFrameHost no longer exists.
  void OnEndNavigationCommit();

  // Must be called before `CommitResponse()`.
  void UpdateUrls(const GURL& url,
                  const std::optional<url::Origin>& top_frame_origin,
                  const blink::StorageKey& storage_key);

  // TODO(crbug.com/336154571): For some tests that want UpdateUrls() after
  // response commit. Investigate whether this can be removed and related
  // condition checks can be turned to CHECK()s.
  void UpdateUrlsAfterCommitResponseForTesting(
      const GURL& url,
      const std::optional<url::Origin>& top_frame_origin,
      const blink::StorageKey& storage_key);

  // For service worker clients. Makes this client be controlled by
  // |registration|'s active worker, or makes this client be not
  // controlled if |registration| is null. If |notify_controllerchange| is true,
  // instructs the renderer to dispatch a 'controllerchange' event.
  void SetControllerRegistration(
      scoped_refptr<ServiceWorkerRegistration> controller_registration,
      bool notify_controllerchange);

  // Create a receiver to notice on ServiceWorker running status change.
  mojo::PendingReceiver<blink::mojom::ServiceWorkerRunningStatusCallback>
  GetRunningStatusCallbackReceiver();

  // |registration| claims the client (document, dedicated worker when
  // PlzDedicatedWorker is enabled, or shared worker) to be controlled.
  void ClaimedByRegistration(
      scoped_refptr<ServiceWorkerRegistration> registration);

  // The URL of the main resource of the client: the document URL (for
  // documents) or script URL (for workers).
  //
  // url() may be empty if loading has not started, or our custom loading
  // handler didn't see the load (because e.g. another handler did first, or the
  // initial request URL was such that OriginCanAccessServiceWorkers returned
  // false).
  //
  // The URL may also change on redirects during loading. Once
  // is_response_committed() is true, the URL should no longer change.
  const GURL& url() const { return url_; }

  // The origin of the top frame of the client. This is more specific than the
  // `top_frame_site` in the storage key, so must be passed separately.
  // For shared worker it is the origin of the document that created the worker.
  // For dedicated worker it is the top-frame origin of the document that owns
  // the worker.
  std::optional<url::Origin> top_frame_origin() const {
    return top_frame_origin_;
  }

  // The StorageKey for this context. Any service worker registrations/versions
  // that are persisted from this context (e.x., via `register()`) are
  // associated with this particular StorageKey. Note: This doesn't hold true
  // when "disable-web-security" is active, see
  // `service_worker_security_utils::GetCorrectStorageKeyForWebSecurityState()`
  // and its usages for more details.
  const blink::StorageKey& key() const { return key_; }

  // Returns whether this container host is secure enough to have a service
  // worker controller.
  // Analogous to Blink's Document::IsSecureContext. Because of how service
  // worker intercepts main resource requests, this check must be done
  // browser-side once the URL is known (see comments in
  // ServiceWorkerNetworkProviderForFrame::Create). This function uses
  // |url_| and |is_parent_frame_secure_| to determine context security, so they
  // must be set properly before calling this function.
  bool IsEligibleForServiceWorkerController() const;

  // For service worker clients. True if the response for the main resource load
  // was committed to the renderer. When this is false, the client's URL may
  // still change due to redirects.
  bool is_response_committed() const;

  // For service worker clients. |callback| is called when this client becomes
  // execution ready or if it is destroyed first.
  void AddExecutionReadyCallback(ExecutionReadyCallback callback);

  // For service worker clients. True if the client is execution ready and
  // therefore can be exposed to JavaScript. Execution ready implies response
  // committed.
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-environment-execution-ready-flag
  bool is_execution_ready() const;

  bool is_container_ready() const;

  const base::UnguessableToken& fetch_request_window_id() const {
    return fetch_request_window_id_;
  }

  base::TimeTicks create_time() const { return create_time_; }

  // For service worker window clients. The RFH ID is set only after navigation
  // commit. See also comments for RenderFrameHost::GetFrameTreeNodeId()
  // for more details.
  GlobalRenderFrameHostId GetRenderFrameHostId() const;

  // For service worker clients. For window clients, this is not populated until
  // after navigation commit.
  int GetProcessId() const;

  // For service worker window clients.
  // Returns the ongoing navigation request before the navigation commit starts.
  // Returns a nullptr if the clients was discarded, e.g., the WebContents was
  // closed.
  // Never call this function if `GetRenderFrameHostId` can return a valid
  // value, since the client can change to another FrameTreeNode(FTN) over its
  // lifetime while its RFH ID never changes, and and function uses the FTN ID
  // to find the NavigationRequest. See also comments for
  // RenderFrameHost::GetFrameTreeNodeId() for more details.
  NavigationRequest* GetOngoingNavigationRequestBeforeCommit(
      base::PassKey<StoragePartitionImpl>) const;

  // For service worker clients.
  const std::string& client_uuid() const;

  // For service worker clients. Returns this client's controller.
  ServiceWorkerVersion* controller() const;

  // For service worker clients. Returns this client's controller's
  // registration.
  ServiceWorkerRegistration* controller_registration() const;

  // BackForwardCache:
  // For service worker clients that are windows.
  bool IsInBackForwardCache() const;
  void EvictFromBackForwardCache(
      BackForwardCacheMetrics::NotRestoredReason reason);
  // Called when this container host's frame goes into BackForwardCache.
  void OnEnterBackForwardCache();
  // Called when a frame gets restored from BackForwardCache. Note that a
  // BackForwardCached frame can be deleted while in the cache but in this case
  // OnRestoreFromBackForwardCache will not be called.
  void OnRestoreFromBackForwardCache();

  bool navigation_commit_ended() const { return navigation_commit_ended_; }

  void EnterBackForwardCacheForTesting() { is_in_back_forward_cache_ = true; }
  void LeaveBackForwardCacheForTesting() { is_in_back_forward_cache_ = false; }
  bool is_in_back_forward_cache() { return is_in_back_forward_cache_; }

  // For service worker clients. Returns the URL that is used for scope matching
  // algorithm. This can be different from url() in the case of blob URL
  // workers. In that case, url() may be like "blob://https://a.test" and the
  // scope matching URL is "https://a.test", inherited from the parent container
  // host.
  const GURL& GetUrlForScopeMatch() const;

  // For service worker clients that are dedicated workers. Inherits the
  // controller of the creator document or worker. Used when the client was
  // created with a blob URL.
  void InheritControllerFrom(ServiceWorkerClient& creator_host,
                             const GURL& blob_url);

  void SetContainerReady();

  bool is_inherited() const { return is_inherited_; }
  void SetInherited() { is_inherited_ = true; }

  const base::WeakPtr<ServiceWorkerContextCore>& context() const {
    return context_;
  }
  ServiceWorkerClientOwner& owner() { return *owner_; }

  // Implements blink::mojom::ServiceWorkerContainerHost and called from
  // ServiceWorkerContainerHostForClient.
  void EnsureControllerServiceWorker(
      mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
      blink::mojom::ControllerServiceWorkerPurpose purpose);
  void HintToUpdateServiceWorker();
  void EnsureFileAccess(
      const std::vector<base::FilePath>& file_paths,
      blink::mojom::ServiceWorkerContainerHost::EnsureFileAccessCallback
          callback);
  void OnExecutionReady();

  // Sets execution ready flag and runs execution ready callbacks.
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-environment-execution-ready-flag
  void SetExecutionReady();

  base::WeakPtr<ServiceWorkerClient> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  class ServiceWorkerRunningStatusObserver;

  friend class ServiceWorkerContainerHostTest;

  void UpdateUrlsInternal(const GURL& url,
                          const std::optional<url::Origin>& top_frame_origin,
                          const blink::StorageKey& storage_key);

  // Syncs matching registrations with live registrations.
  void SyncMatchingRegistrations();

#if DCHECK_IS_ON()
  bool IsMatchingRegistration(ServiceWorkerRegistration* registration) const;
#endif  // DCHECK_IS_ON()

  // Discards all references to matching registrations.
  void RemoveAllMatchingRegistrations();

  void RunExecutionReadyCallbacks();

  // For service worker clients. The flow is:
  // - kInitial -> kResponseCommitted -> kContainerReady -> kExecutionReady
  // - kInitial -> kResponseNotCommitted (client initialization failure cases)
  // - kInitial (no transitions, client initialization failure cases)
  //
  // - kInitial: The initial phase. Container host mojo messages are buffered
  //   because the message pipe is piggy-backed and isn't associated to the
  //   existing message pipe yet.
  //
  // - kResponseCommitted: `CommitResponse()` is called, i.e. the response for
  //   the main resource is about to be committed to the renderer and container
  //   host's mojo endpoints are about to be passed to the renderer. This
  //   client's URL should no longer change. The client should immediately
  //   transition to kContainerReady and thus the kResponseCommitted state
  //   shouldn't be observed (except for the code for response commit and
  //   transitioning to kContainerReady).
  //
  // - kContainerReady: `SetContainerReady()` is called. The response commit has
  //   completed. The container host's mojo pipes are ready to use.
  //
  // - kExecutionReady: `SetExecutionReady()` is called. This client can be
  //   exposed to JavaScript as a Client object.
  //   https://html.spec.whatwg.org/multipage/webappapis.html#concept-environment-execution-ready-flag
  //
  // - kResponseNotCommitted: `CommitResponse()` is not called (and worker
  //   script loading is considered failed) but `SetExecutionReady()` is called.
  //   This can happen e.g. on COEP errors because its caller
  //   (`WorkerScriptLoader::CommitCompleted()`) doesn't take COEP errors into
  //   account. `CommitResponse()` is never called after reaching this state
  //   (i.e. it's not a race condition between `CommitResponse()` and
  //   `SetExecutionReady()`).
  enum class ClientPhase {
    kInitial,
    kResponseCommitted,
    kContainerReady,
    kExecutionReady,
    kResponseNotCommitted
  };
  void TransitionToClientPhase(ClientPhase new_phase);

  // Sets the controller to |controller_registration_->active_version()| or null
  // if there is no associated registration.
  //
  // If |notify_controllerchange| is true, instructs the renderer to dispatch a
  // 'controller' change event.
  void UpdateController(bool notify_controllerchange);

#if DCHECK_IS_ON()
  void CheckControllerConsistency(bool should_crash) const;
#endif  // DCHECK_IS_ON()

  // Flushes features stored, when it gets ready to send.
  // If it is still not ready to send, the features are buffered again.
  void FlushFeatures();

  const base::WeakPtr<ServiceWorkerContextCore> context_;

  // The `ServiceWorkerClientOwner` that owns `this`.
  // On construction, `owner_` is the same as
  // `context_->service_worker_client_owner()`.
  // After `ServiceWorkerContextWrapper::DidDeleteAndStartOver()`,
  // - `context_` is cleared (as the old `ServiceWorkerContextCore` is
  //   destroyed) so that subsequent e.g. registration fails.
  // - `owner_` remains the same and valid (the `ServiceWorkerClientOwner`
  //   is now owned by the new `ServiceWorkerContextCore`), in order to
  //   continue lifetime management of `this`.
  const raw_ref<ServiceWorkerClientOwner> owner_;

  // The corresponding container host.
  // Always valid and non-null except for initialization/destruction.
  std::unique_ptr<ServiceWorkerContainerHostForClient> container_host_;

  // The time when the container host is created.
  const base::TimeTicks create_time_;

  // See comments for the getter functions.
  GURL url_;
  std::optional<url::Origin> top_frame_origin_;
  blink::StorageKey key_;

  // For all service worker clients --------------------------------------------

  // A GUID that is web-exposed as FetchEvent.clientId.
  std::string client_uuid_;

  // |is_parent_frame_secure_| is false if the container host is created for a
  // document whose parent frame is not secure. This doesn't mean the document
  // is necessarily an insecure context, because the document may have a URL
  // whose scheme is granted an exception that allows bypassing the ancestor
  // secure context check. If the container is not created for a document, or
  // the document does not have a parent frame, is_parent_frame_secure_| is
  // true.
  const bool is_parent_frame_secure_;

  // The phase that this container host is on.
  ClientPhase client_phase_ = ClientPhase::kInitial;

  // Callbacks to run upon transition to kExecutionReady.
  std::vector<ExecutionReadyCallback> execution_ready_callbacks_;

  // The controller service worker (i.e., ServiceWorkerContainer#controller) and
  // its registration. The controller is typically the same as the
  // registration's active version, but during algorithms such as the update,
  // skipWaiting(), and claim() steps, the active version and controller may
  // temporarily differ. For example, to perform skipWaiting(), the
  // registration's active version is updated first and then the container
  // host's controller is updated to match it.
  scoped_refptr<ServiceWorkerVersion> controller_;
  scoped_refptr<ServiceWorkerRegistration> controller_registration_;

  // Keyed by registration scope URL length.
  using ServiceWorkerRegistrationMap =
      std::map<size_t, scoped_refptr<ServiceWorkerRegistration>>;
  // Contains all living registrations whose scope this client's URL starts
  // with and whose keys match this client's key, used for .ready and claim().
  // It is empty if IsEligibleForServiceWorkerController() is false. See also
  // AddMatchingRegistration().
  ServiceWorkerRegistrationMap matching_registrations_;

  // The service workers in the chain of redirects during the main resource
  // request for this client. These workers should be updated "soon". See
  // AddServiceWorkerToUpdate() documentation.
  class PendingUpdateVersion;

  base::flat_set<PendingUpdateVersion> versions_to_update_;

  // The type of client.
  std::optional<ServiceWorkerClientInfo> client_info_;

  // The URL used for service worker scope matching. It is empty except in the
  // case of a service worker client with a blob URL.
  GURL scope_match_url_for_blob_client_;

  // Become true if the container is inherited by other container.
  bool is_inherited_ = false;

  // The observer for the running status change.
  // It is used for notifying the ServiceWorker running status change to
  // the ServiceWorkerContainerHost in the renderer.
  std::unique_ptr<ServiceWorkerRunningStatusObserver> running_status_observer_;

  // Until |container_| gets associated, its method cannot be used.
  // If CountFeature() is called before |container_| gets ready, features are
  // kept here, and flushed in SetContainerReady().
  std::set<blink::mojom::WebFeature> buffered_used_features_;

  // For worker clients only ---------------------------------------------------

  // The ID of the process where the container lives for worker clients. It is
  // set during initialization. Use `GetProcessId()` instead of directly
  // accessing `process_id_for_worker_client_` to avoid using a wrong process
  // id.
  const int process_id_for_worker_client_;

  // For window clients only ---------------------------------------------------

  // A token used internally to identify this context in requests. Corresponds
  // to the Fetch specification's concept of a request's associated window:
  // https://fetch.spec.whatwg.org/#concept-request-window. This gets reset on
  // redirects, unlike |client_uuid_|.
  //
  // TODO(falken): Consider using this for |client_uuid_| as well. We can't
  // right now because this gets reset on redirects, and potentially sites rely
  // on the GUID format.
  base::UnguessableToken fetch_request_window_id_;

  // Indicates if this container host is in the back-forward cache.
  //
  // TODO(yuzus): This bit will be unnecessary once ServiceWorkerContainerHost
  // and RenderFrameHost have the same lifetime.
  bool is_in_back_forward_cache_ = false;

  // Indicates if OnEndNavigationCommit() was called on this container host.
  bool navigation_commit_ended_ = false;

  // The frame tree node ID that is set in the constructor and is reset in
  // CommitResponse().
  FrameTreeNodeId ongoing_navigation_frame_tree_node_id_;

  // For all instances --------------------------------------------------------

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ServiceWorkerClient> weak_ptr_factory_{this};
};

CONTENT_EXPORT BASE_DECLARE_FEATURE(kSharedWorkerBlobURLFix);

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CLIENT_H_
