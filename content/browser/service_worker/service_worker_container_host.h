// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTAINER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTAINER_HOST_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "content/browser/renderer_host/back_forward_cache_metrics.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/service_worker_client_info.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cookies/site_for_cookies.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom-forward.h"

namespace content {

namespace service_worker_object_host_unittest {
class ServiceWorkerObjectHostTest;
}

struct GlobalRenderFrameHostId;
class ServiceWorkerContextCore;
class ServiceWorkerHost;
class ServiceWorkerObjectHost;
class ServiceWorkerRegistrationObjectHost;
class ServiceWorkerVersion;

// ServiceWorkerContainerHost is the host of a service worker client (a window,
// dedicated worker, or shared worker) or service worker execution context in
// the renderer process.
//
// Most of its functionality helps implement the web-exposed
// ServiceWorkerContainer interface (navigator.serviceWorker). The long-term
// goal is for it to be the host of ServiceWorkerContainer in the renderer,
// although currently only windows support ServiceWorkerContainers (see
// https://crbug.com/371690).
//
// ServiceWorkerContainerHost is also responsible for handling service worker
// related things in the execution context where the container lives. For
// example, the container host manages service worker (registration) JavaScript
// object hosts, delivers messages to/from the service worker, and dispatches
// events on the container.
//
// Ownership model and responsibilities of ServiceWorkerContainerHost are
// slightly different based on the type of the execution context that the
// container host serves:
//
// For service worker clients, ServiceWorkerContainerHost is owned by
// ServiceWorkerContextCore. The container host has a Mojo connection to the
// container in the renderer, and destruction of the container host happens upon
// disconnection of the Mojo pipe.
//
// For service worker clients, the container host works as a source of truth of
// a service worker client.
//
// Example:
// When a new service worker registration is created, the browser process
// iterates over all ServiceWorkerContainerHosts to find clients (frames,
// dedicated workers if PlzDedicatedWorker is enabled, and shared workers) with
// a URL inside the registration's scope, and has the container host watch the
// registration in order to resolve navigator.serviceWorker.ready once the
// registration settles, if need.
//
// For service worker execution contexts, ServiceWorkerContainerHost is owned
// by ServiceWorkerHost, which in turn is owned by ServiceWorkerVersion. The
// container host and worker host are destructed when the service worker is
// stopped.
class CONTENT_EXPORT ServiceWorkerContainerHost final
    : public blink::mojom::ServiceWorkerContainerHost,
      public ServiceWorkerRegistration::Listener {
 public:
  using ExecutionReadyCallback = base::OnceClosure;

  // Constructor for service worker.
  explicit ServiceWorkerContainerHost(
      base::WeakPtr<ServiceWorkerContextCore> context);

  // Constructor for window clients.
  ServiceWorkerContainerHost(
      base::WeakPtr<ServiceWorkerContextCore> context,
      bool is_parent_frame_secure,
      mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
          container_remote,
      int frame_tree_node_id);

  // Constructor for worker clients.
  ServiceWorkerContainerHost(
      base::WeakPtr<ServiceWorkerContextCore> context,
      int process_id,
      mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
          container_remote,
      ServiceWorkerClientInfo client_info);

  ~ServiceWorkerContainerHost() override;

  ServiceWorkerContainerHost(const ServiceWorkerContainerHost& other) = delete;
  ServiceWorkerContainerHost& operator=(
      const ServiceWorkerContainerHost& other) = delete;
  ServiceWorkerContainerHost(ServiceWorkerContainerHost&& other) = delete;
  ServiceWorkerContainerHost& operator=(ServiceWorkerContainerHost&& other) =
      delete;

  // Implements blink::mojom::ServiceWorkerContainerHost.
  void Register(const GURL& script_url,
                blink::mojom::ServiceWorkerRegistrationOptionsPtr options,
                blink::mojom::FetchClientSettingsObjectPtr
                    outside_fetch_client_settings_object,
                RegisterCallback callback) override;
  void GetRegistration(const GURL& client_url,
                       GetRegistrationCallback callback) override;
  void GetRegistrations(GetRegistrationsCallback callback) override;
  void GetRegistrationForReady(
      GetRegistrationForReadyCallback callback) override;
  void EnsureControllerServiceWorker(
      mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
      blink::mojom::ControllerServiceWorkerPurpose purpose) override;
  void CloneContainerHost(
      mojo::PendingReceiver<blink::mojom::ServiceWorkerContainerHost> receiver)
      override;
  void HintToUpdateServiceWorker() override;
  void EnsureFileAccess(const std::vector<base::FilePath>& file_paths,
                        EnsureFileAccessCallback callback) override;
  void OnExecutionReady() override;
  void GetRunningStatus(GetRunningStatusCallback callback) override;

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

  // Dispatches message event to the client (document, dedicated worker when
  // PlzDedicatedWorker is enabled, or shared worker).
  void PostMessageToClient(ServiceWorkerVersion* version,
                           blink::TransferableMessage message);

  // Notifies the client that its controller used a feature, for UseCounter
  // purposes. This can only be called if IsContainerForClient() is true.
  void CountFeature(blink::mojom::WebFeature feature);

  // Sends information about the controller to the container of the service
  // worker clients in the renderer. If |notify_controllerchange| is true,
  // instructs the renderer to dispatch a 'controllerchange' event.
  void SendSetControllerServiceWorker(bool notify_controllerchange);

  // Called when this container host's controller has been terminated and doomed
  // due to an exceptional condition like it could no longer be read from the
  // script cache.
  void NotifyControllerLost();

  // Returns an object info representing |registration|. The object info holds a
  // Mojo connection to the ServiceWorkerRegistrationObjectHost for the
  // |registration| to ensure the host stays alive while the object info is
  // alive. A new ServiceWorkerRegistrationObjectHost instance is created if one
  // can not be found in |registration_object_hosts_|.
  //
  // NOTE: The registration object info should be sent over Mojo in the same
  // task with calling this method. Otherwise, some Mojo calls to
  // blink::mojom::ServiceWorkerRegistrationObject or
  // blink::mojom::ServiceWorkerObject may happen before establishing the
  // connections, and they'll end up with crashes.
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
  CreateServiceWorkerRegistrationObjectInfo(
      scoped_refptr<ServiceWorkerRegistration> registration);

  // Removes the ServiceWorkerRegistrationObjectHost corresponding to
  // |registration_id|.
  void RemoveServiceWorkerRegistrationObjectHost(int64_t registration_id);

  // For service worker execution contexts.
  // Returns an object info representing |self.serviceWorker|. The object
  // info holds a Mojo connection to the ServiceWorkerObjectHost for the
  // |serviceWorker| to ensure the host stays alive while the object info is
  // alive. See documentation.
  blink::mojom::ServiceWorkerObjectInfoPtr CreateServiceWorkerObjectInfoToSend(
      scoped_refptr<ServiceWorkerVersion> version);

  // Returns a ServiceWorkerObjectHost instance for |version| for this
  // container host. A new instance is created if one does not already exist.
  // ServiceWorkerObjectHost will have an ownership of the |version|.
  base::WeakPtr<ServiceWorkerObjectHost> GetOrCreateServiceWorkerObjectHost(
      scoped_refptr<ServiceWorkerVersion> version);

  // Removes the ServiceWorkerObjectHost corresponding to |version_id|.
  void RemoveServiceWorkerObjectHost(int64_t version_id);

  // Returns true if this container host is for a service worker.
  bool IsContainerForServiceWorker() const;

  // Returns true if this container host is for a service worker client.
  bool IsContainerForClient() const;

  // Returns the client type of this container host. Can only be called when
  // IsContainerForClient() is true.
  blink::mojom::ServiceWorkerClientType GetClientType() const;

  // Returns true if this container host is specifically for a window client.
  bool IsContainerForWindowClient() const;

  // Returns true if this container host is specifically for a worker client.
  bool IsContainerForWorkerClient() const;

  // Returns the client info for this container host.
  ServiceWorkerClientInfo GetServiceWorkerClientInfo() const;

  // For service worker window clients. Called when the navigation is ready to
  // commit. Updates this host with information about the frame committed to.
  // After this is called, is_response_committed() and is_execution_ready()
  // return true.
  //
  // TODO(falken): Pass in an RenderFrameHostImpl instead of an ID. Some
  // tests use a fake id.
  void OnBeginNavigationCommit(
      const GlobalRenderFrameHostId& rfh_id,
      const PolicyContainerPolicies& policy_container_policies,
      mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
          coep_reporter,
      ukm::SourceId document_ukm_source_id);

  // For service worker window clients. Called after the navigation commits to a
  // RenderFrameHost. At this point, the previous ServiceWorkerContainerHost
  // for that RenderFrameHost no longer exists.
  void OnEndNavigationCommit();

  // For service worker clients that are shared workers or dedicated workers.
  // Called when the web worker main script resource has finished loading.
  // Updates this host with information about the worker.
  // After this is called, is_response_committed() and is_execution_ready()
  // return true.
  void CompleteWebWorkerPreparation(
      const PolicyContainerPolicies& cross_origin_embedder_policy,
      ukm::SourceId worker_ukm_source_id);

  // Sets the URL and storage key for the owner of this container.
  //
  // For a ServiceWorkerContainerHost representing a service worker
  // (IsContainerForServiceWorker()), the URL is the service worker's script;
  // for all other clients (IsContainerForClient()), it is the main resource.
  //
  // The top_frame_origin is the origin of the top frame of the client, or for a
  // service worker the origin of the service worker's scope URL. This is more
  // specific than the `top_frame_site` in the storage key, so must be passed
  // separately.
  void UpdateUrls(const GURL& url,
                  const absl::optional<url::Origin>& top_frame_origin,
                  const blink::StorageKey& storage_key);

  // For service worker clients. Makes this client be controlled by
  // |registration|'s active worker, or makes this client be not
  // controlled if |registration| is null. If |notify_controllerchange| is true,
  // instructs the renderer to dispatch a 'controllerchange' event.
  void SetControllerRegistration(
      scoped_refptr<ServiceWorkerRegistration> controller_registration,
      bool notify_controllerchange);

  // For service worker clients. Similar to EnsureControllerServiceWorker, but
  // this returns a bound Mojo ptr which is supposed to be sent to clients. The
  // controller ptr passed to the clients will be used to intercept requests
  // from them.
  // It is invalid to call this when controller_ is null.
  //
  // This method can be called in one of the following cases:
  //
  // - During navigation, right after a request handler for the main resource
  //   has found the matching registration and has started the worker.
  // - When a controller is updated by UpdateController() (e.g.
  //   by OnSkippedWaiting() or SetControllerRegistration()).
  //   In some cases the controller worker may not be started yet.
  //
  // This may return nullptr if the controller service worker does not have a
  // fetch handler, i.e. when the renderer does not need the controller ptr.
  //
  // WARNING:
  // Unlike EnsureControllerServiceWorker, this method doesn't guarantee that
  // the controller worker is running because this method can be called in some
  // situations where the worker isn't running yet. When the returned ptr is
  // stored somewhere and intended to use later, clients need to make sure
  // that the worker is eventually started to use the ptr.
  // Currently all the callsites do this, i.e. they start the worker before
  // or after calling this, but there's no mechanism to prevent future breakage.
  // TODO(crbug.com/827935): Figure out a way to prevent misuse of this method.
  // TODO(crbug.com/827935): Make sure the connection error handler fires in
  // ControllerServiceWorkerConnector (so that it can correctly call
  // EnsureControllerServiceWorker later) if the worker gets killed before
  // events are dispatched.
  //
  // TODO(kinuko): revisit this if we start to use the ControllerServiceWorker
  // for posting messages.
  // TODO(hayato): Return PendingRemote, instead of Remote. Binding to Remote
  // as late as possible is more idiomatic for new Mojo types.
  mojo::Remote<blink::mojom::ControllerServiceWorker>
  GetRemoteControllerServiceWorker();

  // Get an associated cache storage interface.
  mojo::PendingRemote<blink::mojom::CacheStorage> GetRemoteCacheStorage();

  // |registration| claims the client (document, dedicated worker when
  // PlzDedicatedWorker is enabled, or shared worker) to be controlled.
  void ClaimedByRegistration(
      scoped_refptr<ServiceWorkerRegistration> registration);

  // The URL of this context. For service worker clients, this is the document
  // URL (for documents) or script URL (for workers). For service worker
  // execution contexts, this is the script URL.
  //
  // For clients, url() may be empty if loading has not started, or our custom
  // loading handler didn't see the load (because e.g. another handler did
  // first, or the initial request URL was such that
  // OriginCanAccessServiceWorkers returned false).
  //
  // The URL may also change on redirects during loading. Once
  // is_response_committed() is true, the URL should no longer change.
  const GURL& url() const { return url_; }

  // This returns the first party for cookies as derived from the storage key.
  // For information on how this may differ from the SiteForCookies in the frame
  // context please see the comments above StorageKey::ToNetSiteForCookies.
  // For service worker execution contexts, site_for_cookies() always
  // corresponds to the service worker script URL.
  const net::SiteForCookies site_for_cookies() const {
    // TODO(crbug.com/1159586): Once partitioning is on by default calling
    // ToNetSiteForCookies will be sufficient.
    return key_.CopyWithForceEnabledThirdPartyStoragePartitioning()
        .ToNetSiteForCookies();
  }

  // The URL representing the first-party site for this context.
  // For service worker execution contexts, top_frame_origin() always
  // returns the origin of the service worker scope's URL.
  // For shared worker it is the origin of the document that created the worker.
  // For dedicated worker it is the top-frame origin of the document that owns
  // the worker.
  absl::optional<url::Origin> top_frame_origin() const {
    return top_frame_origin_;
  }

  // The StorageKey for this context. Any service worker registrations/versions
  // that are persisted from this context (e.x., via `register()`) are
  // associated with this particular StorageKey. Note: This doesn't hold true
  // when "disable-web-security" is active, see
  // `GetCorrectStorageKeyForWebSecurityState()` and its usages for more
  // details.
  const blink::StorageKey& key() const { return key_; }

  // This function returns the correct StorageKey depending on the state of the
  // "disable-web-security" flag.
  //
  // If web security is disabled then it's possible for the `url` to be
  // cross-origin from `this`'s origin. In that case we need to make a new key
  // with the `url`'s origin, otherwise we might access the wrong storage
  // partition.
  blink::StorageKey GetCorrectStorageKeyForWebSecurityState(
      const GURL& url) const;

  // Calls ContentBrowserClient::AllowServiceWorker(). Returns true if content
  // settings allows service workers to run at |scope|. If this container is for
  // a window client, the check involves the topmost frame url as well as
  // |scope|, and may display tab-level UI.
  // If non-empty, |script_url| is the script the service worker will run.
  bool AllowServiceWorker(const GURL& scope, const GURL& script_url);

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

  // For service worker clients. Describes whether the client has a controller
  // and if it has a fetch event handler.
  blink::mojom::ControllerServiceWorkerMode GetControllerMode() const;

  // For service worker clients. Returns this client's controller.
  ServiceWorkerVersion* controller() const;

  // For service worker clients. Returns this client's controller's
  // registration.
  ServiceWorkerRegistration* controller_registration() const;

  // For service worker execution contexts.
  void set_service_worker_host(ServiceWorkerHost* service_worker_host);
  ServiceWorkerHost* service_worker_host();

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

  // For service worker clients. Returns the URL that is used for scope matching
  // algorithm. This can be different from url() in the case of blob URL
  // workers. In that case, url() may be like "blob://https://a.test" and the
  // scope matching URL is "https://a.test", inherited from the parent container
  // host.
  const GURL& GetUrlForScopeMatch() const;

  // For service worker clients that are dedicated workers. Inherits the
  // controller of the creator document or worker. Used when the client was
  // created with a blob URL.
  void InheritControllerFrom(ServiceWorkerContainerHost& creator_host,
                             const GURL& blob_url);

  base::WeakPtr<ServiceWorkerContainerHost> GetWeakPtr();

  ukm::SourceId ukm_source_id() const { return ukm_source_id_; }

 private:
  friend class ServiceWorkerContainerHostTest;
  friend class service_worker_object_host_unittest::ServiceWorkerObjectHostTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest, Unregister);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest, RegisterDuplicateScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerUpdateJobTest,
                           RegisterWithDifferentUpdateViaCache);
  FRIEND_TEST_ALL_PREFIXES(BackgroundSyncManagerTest,
                           RegisterWithoutLiveSWRegistration);

  // Syncs matching registrations with live registrations.
  void SyncMatchingRegistrations();

#if DCHECK_IS_ON()
  bool IsMatchingRegistration(ServiceWorkerRegistration* registration) const;
#endif  // DCHECK_IS_ON()

  // Discards all references to matching registrations.
  void RemoveAllMatchingRegistrations();

  void ReturnRegistrationForReadyIfNeeded();

  // Sets |execution_ready_| and runs execution ready callbacks.
  void SetExecutionReady();

  void RunExecutionReadyCallbacks();

  // For service worker clients. The flow is kInitial -> kResponseCommitted ->
  // kExecutionReady.
  //
  // - kInitial: The initial phase.
  // - kResponseCommitted: The response for the main resource has been
  //   committed to the renderer. This client's URL should no longer change.
  // - kExecutionReady: This client can be exposed to JavaScript as a Client
  //   object.
  enum class ClientPhase { kInitial, kResponseCommitted, kExecutionReady };
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

  // Callback for ServiceWorkerVersion::RunAfterStartWorker()
  void StartControllerComplete(
      mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
      blink::ServiceWorkerStatusCode status);

  // Callback for ServiceWorkerContextCore::RegisterServiceWorker().
  void RegistrationComplete(const GURL& script_url,
                            const GURL& scope,
                            RegisterCallback callback,
                            int64_t trace_id,
                            mojo::ReportBadMessageCallback bad_message_callback,
                            blink::ServiceWorkerStatusCode status,
                            const std::string& status_message,
                            int64_t registration_id);
  // Callback for ServiceWorkerRegistry::FindRegistrationForClientUrl().
  void GetRegistrationComplete(
      GetRegistrationCallback callback,
      int64_t trace_id,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  // Callback for ServiceWorkerStorage::GetRegistrationsForOrigin().
  void GetRegistrationsComplete(
      GetRegistrationsCallback callback,
      int64_t trace_id,
      blink::ServiceWorkerStatusCode status,
      const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
          registrations);

  bool IsValidGetRegistrationMessage(const GURL& client_url,
                                     std::string* out_error) const;
  bool IsValidGetRegistrationsMessage(std::string* out_error) const;
  bool IsValidGetRegistrationForReadyMessage(std::string* out_error) const;

  // Perform common checks that need to run before ContainerHost methods that
  // come from a child process are handled.
  // |scope| is checked if it is allowed to run a service worker.
  // If non-empty, |script_url| is the script associated with the service
  // worker.
  // Returns true if all checks have passed.
  // If anything looks wrong |callback| will run with an error
  // message prefixed by |error_prefix| and |args|, and false is returned.
  template <typename CallbackType, typename... Args>
  bool CanServeContainerHostMethods(CallbackType* callback,
                                    const GURL& scope,
                                    const GURL& script_url,
                                    const char* error_prefix,
                                    Args... args);

  base::WeakPtr<ServiceWorkerContextCore> context_;

  // The time when the container host is created.
  const base::TimeTicks create_time_;

  // See comments for the getter functions.
  GURL url_;
  absl::optional<url::Origin> top_frame_origin_;
  blink::StorageKey key_;

  // Contains all ServiceWorkerRegistrationObjectHost instances corresponding to
  // the service worker registration JavaScript objects for the hosted execution
  // context (service worker global scope or service worker client) in the
  // renderer process.
  std::map<int64_t /* registration_id */,
           std::unique_ptr<ServiceWorkerRegistrationObjectHost>>
      registration_object_hosts_;

  // Contains all ServiceWorkerObjectHost instances corresponding to
  // the service worker JavaScript objects for the hosted execution
  // context (service worker global scope or service worker client) in the
  // renderer process.
  std::map<int64_t /* version_id */, std::unique_ptr<ServiceWorkerObjectHost>>
      service_worker_object_hosts_;

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
  const bool is_parent_frame_secure_ = true;

  // The phase that this container host is on.
  ClientPhase client_phase_ = ClientPhase::kInitial;

  // Callbacks to run upon transition to kExecutionReady.
  std::vector<ExecutionReadyCallback> execution_ready_callbacks_;

  // The ready() promise is only allowed to be created once.
  // |get_ready_callback_| has three states:
  // 1. |get_ready_callback_| is null when ready() has not yet been called.
  // 2. |*get_ready_callback_| is a valid OnceCallback after ready() has been
  //    called and the callback has not yet been run.
  // 3. |*get_ready_callback_| is a null OnceCallback after the callback has
  //    been run.
  std::unique_ptr<GetRegistrationForReadyCallback> get_ready_callback_;

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

  // Mojo endpoint which will be be sent to the service worker just before
  // the response is committed, where |cross_origin_embedder_policy_| is ready.
  // We need to store this here because navigation code depends on having a
  // mojo::Remote<ControllerServiceWorker> for making a SubresourceLoaderParams,
  // which is created before the response header is ready.
  mojo::PendingReceiver<blink::mojom::ControllerServiceWorker>
      pending_controller_receiver_;

  // Container host receivers other than the original |receiver_|. These include
  // receivers used from (dedicated or shared) worker threads, or from
  // ServiceWorkerSubresourceLoaderFactory.
  mojo::ReceiverSet<blink::mojom::ServiceWorkerContainerHost>
      additional_receivers_;

  // |container_| is the remote renderer-side ServiceWorkerContainer that |this|
  // is hosting.
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainer> container_;

  // The type of client.
  absl::optional<ServiceWorkerClientInfo> client_info_;

  // The source id of the client's ExecutionContext, set on response commit.
  ukm::SourceId ukm_source_id_ = ukm::kInvalidSourceId;

  // The URL used for service worker scope matching. It is empty except in the
  // case of a service worker client with a blob URL.
  GURL scope_match_url_for_blob_client_;

  // For worker clients only ---------------------------------------------------

  // The ID of the process where the container lives for worker clients. It is
  // set during initialization. Use `GetProcessId()` instead of directly
  // accessing `process_id_for_worker_client_` to avoid using a wrong process
  // id.
  const int process_id_for_worker_client_ = ChildProcessHost::kInvalidUniqueID;

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

  // The policy container policies of the client. Set on response commit.
  absl::optional<PolicyContainerPolicies> policy_container_policies_;

  // An endpoint connected to the COEP reporter. A clone of this connection is
  // passed to the service worker. Bound on response commit.
  mojo::Remote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter_;

  // Indicates if this container host is in the back-forward cache.
  //
  // TODO(yuzus): This bit will be unnecessary once ServiceWorkerContainerHost
  // and RenderFrameHost have the same lifetime.
  bool is_in_back_forward_cache_ = false;

  // Indicates if OnEndNavigationCommit() was called on this container host.
  bool navigation_commit_ended_ = false;

  // The frame tree node ID that is set in the constructor and is reset in
  // OnBeginNavigationCommit().
  int ongoing_navigation_frame_tree_node_id_ =
      RenderFrameHost::kNoFrameTreeNodeId;

  // For service worker execution contexts -------------------------------------

  // The ServiceWorkerHost that owns |this|.
  raw_ptr<ServiceWorkerHost> service_worker_host_ = nullptr;

  // For all instances --------------------------------------------------------

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ServiceWorkerContainerHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTAINER_HOST_H_
