// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/browser/browser_interface_broker_impl.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/resource_type.h"
#include "media/mojo/mojom/video_decode_perf_history.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"
#include "third_party/blink/public/mojom/locks/lock_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom-forward.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "third_party/blink/public/mojom/webtransport/quic_transport_connector.mojom.h"
#include "url/origin.h"

namespace service_worker_object_host_unittest {
class ServiceWorkerObjectHostTest;
}

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerRegistrationObjectHost;
class ServiceWorkerVersion;
class WebContents;

// ServiceWorkerProviderHost is the browser-process representation of a
// renderer-process entity that can involve service workers. Currently, these
// entities are frames or workers. So basically, one ServiceWorkerProviderHost
// instance is the browser process's source of truth about one frame/worker in a
// renderer process, which the browser process uses when performing operations
// involving service workers.
//
// ServiceWorkerProviderHost lives on the IO thread, since all nearly all
// browser process service worker machinery lives on the IO thread.
//
// Example:
// * A new service worker registration is created. The browser process loops
// over all ServiceWorkerProviderHosts to find clients (frames and shared
// workers) with a URL inside the registration's scope, and has the provider
// host watch the registration in order to resolve navigator.serviceWorker.ready
// once the registration settles, if neeed.
//
// "Provider" is a somewhat tricky term. The idea is that a provider is what
// attaches to a frame/worker and "provides" it with functionality related to
// service workers. This functionality is mostly granted by creating the
// ServiceWorkerProviderHost for this frame/worker, which, again, makes the
// frame/worker alive in the browser's service worker world.
//
// A provider host has a Mojo connection to the provider in the renderer.
// Destruction of the host happens upon disconnection of the Mojo pipe.
//
// There are two general types of providers:
// 1) those for service worker clients (windows or shared workers), and
// 2) those for service workers themselves.
//
// For client providers, there is a provider (ServiceWorkerProviderContext) per
// frame or shared worker in the renderer process. The lifetime of this host
// object is tied to the lifetime of the document or the worker.
//
// For service worker providers, there is a provider
// (ServiceWorkerContextClient) per running service worker in the renderer
// process. The lifetime of this host object is tied to the lifetime of the
// running service worker.
//
// A ServiceWorkerProviderHost is created in the following situations:
//
// 1) For a client created for a navigation (for both top-level and
// non-top-level frames), the provider host for the resulting document is
// pre-created by the browser process and the provider info is sent in the
// navigation commit IPC.
//
// 2) For web workers and for service workers, the provider host is
// created by the browser process and the provider info is sent in the start
// worker IPC message.
class CONTENT_EXPORT ServiceWorkerProviderHost
    : public ServiceWorkerRegistration::Listener,
      public base::SupportsWeakPtr<ServiceWorkerProviderHost>,
      public blink::mojom::ServiceWorkerContainerHost,
      public service_manager::mojom::InterfaceProvider {
 public:
  using WebContentsGetter = base::RepeatingCallback<WebContents*()>;
  using ExecutionReadyCallback = base::OnceClosure;

  // Used to pre-create a ServiceWorkerProviderHost for a navigation. The
  // ServiceWorkerProviderContext will later be created in the renderer, should
  // the navigation succeed. |are_ancestors_secure| should be true for main
  // frames. Otherwise it is true iff all ancestor frames of this frame have a
  // secure origin. |frame_tree_node_id| is FrameTreeNode
  // id. |web_contents_getter| indicates the tab where the navigation is
  // occurring.
  //
  // The returned host stays alive as long as the corresponding host ptr for
  // |host_request| stays alive.
  static base::WeakPtr<ServiceWorkerProviderHost> PreCreateNavigationHost(
      base::WeakPtr<ServiceWorkerContextCore> context,
      bool are_ancestors_secure,
      int frame_tree_node_id,
      mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
          host_receiver,
      mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
          client_remote);

  // Used for starting a service worker. Returns a provider host for the service
  // worker and partially fills |out_provider_info|.  The host stays alive as
  // long as this info stays alive (namely, as long as
  // |out_provider_info->host_remote| stays alive).
  // CompleteStartWorkerPreparation() must be called later to get a full info to
  // send to the renderer.
  static base::WeakPtr<ServiceWorkerProviderHost> CreateForServiceWorker(
      base::WeakPtr<ServiceWorkerContextCore> context,
      scoped_refptr<ServiceWorkerVersion> version,
      blink::mojom::ServiceWorkerProviderInfoForStartWorkerPtr*
          out_provider_info);

  // Used for starting a web worker (dedicated worker or shared worker). Returns
  // a provider host for the worker. The host stays alive as long as the
  // corresponding host for |host_receiver| stays alive.
  static base::WeakPtr<ServiceWorkerProviderHost> CreateForWebWorker(
      base::WeakPtr<ServiceWorkerContextCore> context,
      int process_id,
      blink::mojom::ServiceWorkerProviderType provider_type,
      mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
          host_receiver,
      mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
          client_remote);

  ~ServiceWorkerProviderHost() override;

  // May return nullptr.
  RenderProcessHost* GetProcessHost() {
    return RenderProcessHost::FromID(render_process_id_);
  }

  const std::string& client_uuid() const { return client_uuid_; }
  const base::UnguessableToken& fetch_request_window_id() const {
    return fetch_request_window_id_;
  }
  base::TimeTicks create_time() const { return create_time_; }
  int process_id() const { return render_process_id_; }
  int provider_id() const { return provider_id_; }
  int frame_id() const { return frame_id_; }
  const WebContentsGetter& web_contents_getter() const {
    return web_contents_getter_;
  }
  int frame_tree_node_id() const { return frame_tree_node_id_; }

  bool is_parent_frame_secure() const { return is_parent_frame_secure_; }

  // Returns whether this provider host is secure enough to have a service
  // worker controller.
  // Analogous to Blink's Document::IsSecureContext. Because of how service
  // worker intercepts main resource requests, this check must be done
  // browser-side once the URL is known (see comments in
  // ServiceWorkerNetworkProviderForFrame::Create). This function uses
  // |url_| and |is_parent_frame_secure_| to determine context security, so they
  // must be set properly before calling this function.
  bool IsContextSecureForServiceWorker() const;

  // For service worker clients. Describes whether the client has a controller
  // and if it has a fetch event handler.
  blink::mojom::ControllerServiceWorkerMode GetControllerMode() const;

  // For service worker clients. Returns this client's controller.
  ServiceWorkerVersion* controller() const;

  ServiceWorkerRegistration* controller_registration() const {
#if DCHECK_IS_ON()
    CheckControllerConsistency(false);
#endif  // DCHECK_IS_ON()

    return controller_registration_.get();
  }

  // For service worker execution contexts. The version of the service worker.
  // This is nullptr when the worker is still starting up (until
  // CompleteStartWorkerPreparation() is called).
  ServiceWorkerVersion* running_hosted_version() const {
    DCHECK(!running_hosted_version_ ||
           type_ == blink::mojom::ServiceWorkerProviderType::kForServiceWorker);
    return running_hosted_version_.get();
  }

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

  // For service worker clients. Sets |url_|, |site_for_cookies_| and
  // |top_frame_origin_| and updates the client uuid if it's a cross-origin
  // transition.
  void UpdateUrls(const GURL& url,
                  const GURL& site_for_cookies,
                  const base::Optional<url::Origin>& top_frame_origin);

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
  const GURL& url() const;

  // The URL representing the site_for_cookies for this context. See
  // |URLRequest::site_for_cookies()| for details.
  // For service worker execution contexts, site_for_cookies() always
  // returns the service worker script URL.
  const GURL& site_for_cookies() const;

  // The URL representing the first-party site for this context.
  // For service worker execution contexts, top_frame_origin() always
  // returns the origin of the service worker script URL.
  // For shared worker it is the origin of the document that created the worker.
  // For dedicated worker it is the top-frame origin of the document that owns
  // the worker.
  base::Optional<url::Origin> top_frame_origin() const;

  blink::mojom::ServiceWorkerProviderType provider_type() const {
    return type_;
  }
  bool IsProviderForServiceWorker() const;
  bool IsProviderForClient() const;
  // Can only be called when IsProviderForClient() is true.
  blink::mojom::ServiceWorkerClientType client_type() const;

  // For service worker clients. Makes this client be controlled by
  // |registration|'s active worker, or makes this client be not
  // controlled if |registration| is null. If |notify_controllerchange| is true,
  // instructs the renderer to dispatch a 'controllerchange' event.
  void SetControllerRegistration(
      scoped_refptr<ServiceWorkerRegistration> controller_registration,
      bool notify_controllerchange);

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

  // For service worker execution contexts.
  // Returns an object info representing |self.serviceWorker|. The object
  // info holds a Mojo connection to the ServiceWorkerObjectHost for the
  // |serviceWorker| to ensure the host stays alive while the object info is
  // alive. See documentation.
  blink::mojom::ServiceWorkerObjectInfoPtr CreateServiceWorkerObjectInfoToSend(
      scoped_refptr<ServiceWorkerVersion> version);

  // Returns a ServiceWorkerObjectHost instance for |version| for this provider
  // host. A new instance is created if one does not already exist.
  // ServiceWorkerObjectHost will have an ownership of the |version|.
  base::WeakPtr<ServiceWorkerObjectHost> GetOrCreateServiceWorkerObjectHost(
      scoped_refptr<ServiceWorkerVersion> version);

  // May return nullptr if the context has shut down.
  base::WeakPtr<ServiceWorkerContextCore> context() { return context_; }

  // Dispatches message event to the document.
  void PostMessageToClient(ServiceWorkerVersion* version,
                           blink::TransferableMessage message);

  // Notifies the client that its controller used a feature, for UseCounter
  // purposes. This can only be called if IsProviderForClient() is true.
  void CountFeature(blink::mojom::WebFeature feature);

  // |registration| claims the document to be controlled.
  void ClaimedByRegistration(
      scoped_refptr<ServiceWorkerRegistration> registration);

  // For service worker window clients. Called when the navigation is ready to
  // commit. Updates this host with information about the frame committed to.
  // After this is called, is_response_committed() and is_execution_ready()
  // return true.
  void OnBeginNavigationCommit(
      int render_process_id,
      int render_frame_id,
      network::mojom::CrossOriginEmbedderPolicy cross_origin_embedder_policy);

  // For service worker execution contexts. Completes initialization of this
  // provider host. It is called once a renderer process has been found to host
  // the worker.
  void CompleteStartWorkerPreparation(
      int process_id,
      service_manager::mojom::InterfaceProviderRequest
          interface_provider_request,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          broker_receiver);

  // For service worker clients that are shared workers or dedicated workers.
  // Called when the web worker main script resource has finished loading.
  // Updates this host with information about the worker.
  // After this is called, is_response_committed() and is_execution_ready()
  // return true.
  void CompleteWebWorkerPreparation(
      network::mojom::CrossOriginEmbedderPolicy cross_origin_embedder_policy);

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
  // for current document.
  ServiceWorkerRegistration* MatchRegistration() const;

  // Removes the ServiceWorkerRegistrationObjectHost corresponding to
  // |registration_id|.
  void RemoveServiceWorkerRegistrationObjectHost(int64_t registration_id);

  // Removes the ServiceWorkerObjectHost corresponding to |version_id|.
  void RemoveServiceWorkerObjectHost(int64_t version_id);

  // Calls ContentBrowserClient::AllowServiceWorker(). Returns true if content
  // settings allows service workers to run at |scope|. If this provider is for
  // a window client, the check involves the topmost frame url as well as
  // |scope|, and may display tab-level UI.
  // If non-empty, |script_url| is the script the service worker will run.
  bool AllowServiceWorker(const GURL& scope, const GURL& script_url);

  // Called when our controller has been terminated and doomed due to an
  // exceptional condition like it could no longer be read from the script
  // cache.
  void NotifyControllerLost();

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

  // For service worker clients. |callback| is called when this client becomes
  // execution ready or if it is destroyed first.
  void AddExecutionReadyCallback(ExecutionReadyCallback callback);

  // For service worker clients. True if the response for the main resource load
  // was committed to the renderer. When this is false, the client's URL may
  // still change due to redirects.
  bool is_response_committed() const;

  // For service worker clients. True if the client is execution ready and
  // therefore can be exposed to JavaScript. Execution ready implies response
  // committed.
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-environment-execution-ready-flag
  bool is_execution_ready() const;

  // For service worker execution contexts. Forwards |receiver| to the process
  // host on the UI thread.
  void CreateLockManager(
      mojo::PendingReceiver<blink::mojom::LockManager> receiver);
  void CreateIDBFactory(
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver);
  void CreateQuicTransportConnector(
      mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver);

  // BackForwardCache:
  // For service worker clients that are windows.
  bool IsInBackForwardCache() const;
  void EvictFromBackForwardCache();
  // Called when this provider host's frame goes into BackForwardCache.
  void OnEnterBackForwardCache();
  // Called when a frame gets restored from BackForwardCache. Note that a
  // BackForwardCached frame can be deleted while in the cache but in this case
  // OnRestoreFromBackForwardCache will not be called.
  void OnRestoreFromBackForwardCache();

 private:
  // For service worker clients. The flow is kInitial -> kResponseCommitted ->
  // kExecutionReady.
  //
  // - kInitial: The initial phase.
  // - kResponseCommitted: The response for the main resource has been
  //   committed to the renderer. This client's URL should no longer change.
  // - kExecutionReady: This client can be exposed to JavaScript as a Client
  //   object.
  enum class ClientPhase { kInitial, kResponseCommitted, kExecutionReady };

  friend class ServiceWorkerProviderHostTest;
  friend class service_worker_object_host_unittest::ServiceWorkerObjectHostTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest, Unregister);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest, RegisterDuplicateScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerUpdateJobTest,
                           RegisterWithDifferentUpdateViaCache);
  FRIEND_TEST_ALL_PREFIXES(BackgroundSyncManagerTest,
                           RegisterWithoutLiveSWRegistration);

  static void RegisterToContextCore(
      base::WeakPtr<ServiceWorkerContextCore> context,
      std::unique_ptr<ServiceWorkerProviderHost> host);

  ServiceWorkerProviderHost(
      blink::mojom::ServiceWorkerProviderType type,
      bool is_parent_frame_secure,
      int frame_tree_node_id,
      mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
          host_receiver,
      mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
          client_remote,
      base::WeakPtr<ServiceWorkerContextCore> context);

  // ServiceWorkerRegistration::Listener overrides.
  void OnVersionAttributesChanged(
      ServiceWorkerRegistration* registration,
      blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask,
      const ServiceWorkerRegistrationInfo& info) override;
  void OnRegistrationFailed(ServiceWorkerRegistration* registration) override;
  void OnRegistrationFinishedUninstalling(
      ServiceWorkerRegistration* registration) override;
  void OnSkippedWaiting(ServiceWorkerRegistration* registration) override;

  // Sets the controller to |controller_registration_->active_version()| or null
  // if there is no associated registration.
  //
  // If |notify_controllerchange| is true, instructs the renderer to dispatch a
  // 'controller' change event.
  void UpdateController(bool notify_controllerchange);

  // Syncs matching registrations with live registrations.
  void SyncMatchingRegistrations();

#if DCHECK_IS_ON()
  bool IsMatchingRegistration(ServiceWorkerRegistration* registration) const;
#endif  // DCHECK_IS_ON()

  // Discards all references to matching registrations.
  void RemoveAllMatchingRegistrations();

  void ReturnRegistrationForReadyIfNeeded();

  // Sends information about the controller to the providers of the service
  // worker clients in the renderer. If |notify_controllerchange| is true,
  // instructs the renderer to dispatch a 'controllerchange' event.
  void SendSetControllerServiceWorker(bool notify_controllerchange);

  // For service worker clients. Returns false if it's not yet time to send the
  // renderer information about the controller. Basically returns false if this
  // client is still loading so due to potential redirects the initial
  // controller has not yet been decided.
  bool IsControllerDecided() const;

#if DCHECK_IS_ON()
  void CheckControllerConsistency(bool should_crash) const;
#endif  // DCHECK_IS_ON()

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
  void OnExecutionReady() override;

  // Callback for ServiceWorkerContextCore::RegisterServiceWorker().
  void RegistrationComplete(const GURL& script_url,
                            const GURL& scope,
                            RegisterCallback callback,
                            int64_t trace_id,
                            mojo::ReportBadMessageCallback bad_message_callback,
                            blink::ServiceWorkerStatusCode status,
                            const std::string& status_message,
                            int64_t registration_id);
  // Callback for ServiceWorkerStorage::FindRegistrationForDocument().
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

  // Callback for ServiceWorkerVersion::RunAfterStartWorker()
  void StartControllerComplete(
      mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
      blink::ServiceWorkerStatusCode status);

  bool IsValidGetRegistrationMessage(const GURL& client_url,
                                     std::string* out_error) const;
  bool IsValidGetRegistrationsMessage(std::string* out_error) const;
  bool IsValidGetRegistrationForReadyMessage(std::string* out_error) const;

  // service_manager::mojom::InterfaceProvider:
  // For service worker execution contexts.
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override;

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

  // Sets |execution_ready_| and runs execution ready callbacks.
  void SetExecutionReady();

  void RunExecutionReadyCallbacks();

  void TransitionToClientPhase(ClientPhase new_phase);

  void SetRenderProcessId(int process_id);

  // Unique among all provider hosts.
  const int provider_id_;

  const blink::mojom::ServiceWorkerProviderType type_;

  // A GUID that is web-exposed as FetchEvent.clientId.
  std::string client_uuid_;

  // For window clients. A token used internally to identify this context in
  // requests. Corresponds to the Fetch specification's concept of a request's
  // associated window: https://fetch.spec.whatwg.org/#concept-request-window
  // This gets reset on redirects, unlike |client_uuid_|.
  //
  // TODO(falken): Consider using this for |client_uuid_| as well. We can't
  // right now because this gets reset on redirects, and potentially sites rely
  // on the GUID format.
  base::UnguessableToken fetch_request_window_id_;

  const base::TimeTicks create_time_;
  int render_process_id_;

  // The window's RenderFrame id, if this is a service worker window client.
  // Otherwise, |MSG_ROUTING_NONE|.
  int frame_id_;

  // |is_parent_frame_secure_| is false if the provider host is created for a
  // document whose parent frame is not secure. This doesn't mean the document
  // is necessarily an insecure context, because the document may have a URL
  // whose scheme is granted an exception that allows bypassing the ancestor
  // secure context check. If the provider is not created for a document, or the
  // document does not have a parent frame, is_parent_frame_secure_| is true.
  const bool is_parent_frame_secure_;

  // FrameTreeNode id if this is a service worker window client.
  // Otherwise, |FrameTreeNode::kFrameTreeNodeInvalidId|.
  const int frame_tree_node_id_;

  // Only set when this object is pre-created for a navigation. It indicates the
  // tab where the navigation occurs. Otherwise, a null callback.
  const WebContentsGetter web_contents_getter_;

  // For service worker clients. See comments for the getter functions.
  GURL url_;
  GURL site_for_cookies_;
  base::Optional<url::Origin> top_frame_origin_;

  // Keyed by registration scope URL length.
  using ServiceWorkerRegistrationMap =
      std::map<size_t, scoped_refptr<ServiceWorkerRegistration>>;
  // Contains all living registrations whose scope this document's URL
  // starts with, used for .ready and claim(). It is empty if
  // IsContextSecureForServiceWorker() is false. See also
  // AddMatchingRegistration().
  ServiceWorkerRegistrationMap matching_registrations_;

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

  // The ready() promise is only allowed to be created once.
  // |get_ready_callback_| has three states:
  // 1. |get_ready_callback_| is null when ready() has not yet been called.
  // 2. |*get_ready_callback_| is a valid OnceCallback after ready() has been
  //    called and the callback has not yet been run.
  // 3. |*get_ready_callback_| is a null OnceCallback after the callback has
  //    been run.
  std::unique_ptr<GetRegistrationForReadyCallback> get_ready_callback_;

  // For service worker clients. The controller service worker (i.e.,
  // ServiceWorkerContainer#controller) and its registration. The controller is
  // typically the same as the registration's active version, but during
  // algorithms such as the update, skipWaiting(), and claim() steps, the active
  // version and controller may temporarily differ. For example, to perform
  // skipWaiting(), the registration's active version is updated first and then
  // the provider host's controller is updated to match it.
  scoped_refptr<ServiceWorkerVersion> controller_;
  scoped_refptr<ServiceWorkerRegistration> controller_registration_;

  // For service worker execution contexts. The ServiceWorkerVersion of the
  // service worker this is a provider for.
  scoped_refptr<ServiceWorkerVersion> running_hosted_version_;

  // |context_| owns |this| but if the context is destroyed and a new one is
  // created, the provider host becomes owned by the new context, while this
  // |context_| is reset to null.
  // TODO(https://crbug.com/877356): Don't support copying context, so this can
  // just be a raw ptr that is never null.
  base::WeakPtr<ServiceWorkerContextCore> context_;

  // |container_| is the remote renderer-side ServiceWorkerContainer that |this|
  // is a ServiceWorkerContainerHost for.
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainer> container_;
  // |receiver_| keeps the connection to the renderer-side counterpart
  // (content::ServiceWorkerProviderContext). When the connection bound on
  // |receiver_| gets killed from the renderer side, or the bound
  // |ServiceWorkerProviderInfoForStartWorker::host_remote| is otherwise
  // destroyed before being passed to the renderer, this
  // content::ServiceWorkerProviderHost will be destroyed.
  mojo::AssociatedReceiver<blink::mojom::ServiceWorkerContainerHost> receiver_{
      this};

  // Container host receivers other than the original |receiver_|. These include
  // receivers used from (dedicated or shared) worker threads, or from
  // ServiceWorkerSubresourceLoaderFactory.
  mojo::ReceiverSet<blink::mojom::ServiceWorkerContainerHost>
      additional_receivers_;

  // For service worker execution contexts.
  mojo::Binding<service_manager::mojom::InterfaceProvider>
      interface_provider_binding_;
  BrowserInterfaceBrokerImpl<ServiceWorkerProviderHost,
                             const ServiceWorkerVersionInfo&>
      broker_{this};
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> broker_receiver_{
      &broker_};

  // For service worker clients.
  ClientPhase client_phase_ = ClientPhase::kInitial;

  // For service worker clients. Callbacks to run upon transition to
  // kExecutionReady.
  std::vector<ExecutionReadyCallback> execution_ready_callbacks_;

  // For service worker clients. The service workers in the chain of redirects
  // during the main resource request for this client. These workers should be
  // updated "soon". See AddServiceWorkerToUpdate() documentation.
  class PendingUpdateVersion;
  base::flat_set<PendingUpdateVersion> versions_to_update_;

  // Mojo endpoint which will be be sent to the service worker just before
  // the response is committed, where |cross_origin_embedder_policy_| is ready.
  // We need to store this here because navigation code depends on having a
  // mojo::Remote<ControllerServiceWorker> for making a SubresourceLoaderParams,
  // which is created before the response header is ready.
  mojo::PendingReceiver<blink::mojom::ControllerServiceWorker>
      pending_controller_receiver_;

  // For service worker clients. The embedder policy of the client. Set on
  // response commit.
  base::Optional<network::mojom::CrossOriginEmbedderPolicy>
      cross_origin_embedder_policy_;

  // TODO(yuzus): This bit will be unnecessary once ServiceWorkerProviderHost
  // and RenderFrameHost have the same lifetime.
  bool is_in_back_forward_cache_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
