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
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_container.mojom.h"
#include "content/common/service_worker/service_worker_provider.mojom.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/common/resource_type.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/request_context_frame_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/platform/modules/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/platform/web_feature.mojom.h"

namespace network {
class ResourceRequestBody;
}

namespace service_worker_object_host_unittest {
class ServiceWorkerObjectHostTest;
}

namespace storage {
class BlobStorageContext;
}

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerRegistrationObjectHost;
class ServiceWorkerRequestHandler;
class ServiceWorkerVersion;
class WebContents;

namespace service_worker_dispatcher_host_unittest {
class ServiceWorkerDispatcherHostTest;
FORWARD_DECLARE_TEST(ServiceWorkerDispatcherHostTest,
                     DispatchExtendableMessageEvent);
FORWARD_DECLARE_TEST(ServiceWorkerDispatcherHostTest,
                     DispatchExtendableMessageEvent_Fail);
}  // namespace service_worker_dispatcher_host_unittest

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
// The analogue of ServiceWorkerProviderHost ("provider host") on the renderer
// process is ServiceWorkerProviderContext ("provider"). A provider host has a
// Mojo connection to the provider in the renderer. Destruction of the host
// happens upon disconnection of the Mojo pipe.
//
// There are two general types of providers:
// 1) those for service worker clients (windows or shared workers), and
// 2) those for service workers themselves.
//
// For client providers, there is a provider per frame or shared worker in the
// renderer process. The lifetime of this host object is tied to the lifetime of
// the document or the worker.
//
// For service worker providers, there is a provider per running service worker
// in the renderer process. The lifetime of this host object is tied to the
// lifetime of the running service worker.
//
// A ServiceWorkerProviderHost is created in the following situations:
//
// 1) For a client created for a navigation (for both top-level and
// non-top-level frames), the provider host for the resulting document is
// pre-created by the browser process. Upon navigation commit, the provider is
// created on the renderer, which sends an OnProviderCreated IPC to establish
// the Mojo connection.
//
// 2) For clients created by the renderer not due to navigations (shared workers
// in the non-S13nServiceWorker case, and about:blank iframes), the provider
// host is created and the Mojo connection is established when the provider is
// created by the renderer process and sends an OnProviderCreated IPC.
//
// 3) For shared workers in the S13nServiceWorker case and for service workers,
// the provider host is pre-created by the browser process, and information
// about the host is sent in the start worker IPC message. The Mojo connection
// is established when renderer process receives the start message and creates
// the provider.
class CONTENT_EXPORT ServiceWorkerProviderHost
    : public ServiceWorkerRegistration::Listener,
      public base::SupportsWeakPtr<ServiceWorkerProviderHost>,
      public mojom::ServiceWorkerContainerHost,
      public service_manager::mojom::InterfaceProvider {
 public:
  using WebContentsGetter = base::RepeatingCallback<WebContents*()>;

  // Used to pre-create a ServiceWorkerProviderHost for a navigation. The
  // ServiceWorkerNetworkProvider will later be created in the renderer, should
  // the navigation succeed. |are_ancestors_secure| should be true for main
  // frames. Otherwise it is true iff all ancestor frames of this frame have a
  // secure origin. |web_contents_getter| indicates the tab where the navigation
  // is occurring.
  //
  // The returned host is owned by |context|. Upon successful navigation, the
  // caller should remove it from |context| and re-add it after calling
  // CompleteNavigationInitialized() to update it with the correct process id.
  // If navigation fails, the caller should remove it from |context|.
  static base::WeakPtr<ServiceWorkerProviderHost> PreCreateNavigationHost(
      base::WeakPtr<ServiceWorkerContextCore> context,
      bool are_ancestors_secure,
      WebContentsGetter web_contents_getter);

  // Used for starting a service worker. Returns a provider host for the service
  // worker and partially fills |out_provider_info|.  The host stays alive as
  // long as this info stays alive (namely, as long as
  // |out_provider_info->host_ptr_info| stays alive).
  // CompleteStartWorkerPreparation() must be called later to get a full info to
  // send to the renderer.
  static base::WeakPtr<ServiceWorkerProviderHost> PreCreateForController(
      base::WeakPtr<ServiceWorkerContextCore> context,
      scoped_refptr<ServiceWorkerVersion> version,
      mojom::ServiceWorkerProviderInfoForStartWorkerPtr* out_provider_info);

  // S13nServiceWorker:
  // Used for starting a shared worker. Returns a provider host for the shared
  // worker and fills |out_provider_info| with info to send to the renderer to
  // connect to the host. The host stays alive as long as this info stays alive
  // (namely, as long as |out_provider_info->host_ptr_info| stays alive).
  static base::WeakPtr<ServiceWorkerProviderHost> PreCreateForSharedWorker(
      base::WeakPtr<ServiceWorkerContextCore> context,
      int process_id,
      mojom::ServiceWorkerProviderInfoForSharedWorkerPtr* out_provider_info);

  // Used to create a ServiceWorkerProviderHost when the renderer-side provider
  // is created. This ProviderHost will be created for the process specified by
  // |process_id|.
  static std::unique_ptr<ServiceWorkerProviderHost> Create(
      int process_id,
      mojom::ServiceWorkerProviderHostInfoPtr info,
      base::WeakPtr<ServiceWorkerContextCore> context);

  ~ServiceWorkerProviderHost() override;

  const std::string& client_uuid() const { return client_uuid_; }
  base::TimeTicks create_time() const { return create_time_; }
  int process_id() const { return render_process_id_; }
  int provider_id() const { return info_->provider_id; }
  int frame_id() const;
  int route_id() const { return info_->route_id; }
  const WebContentsGetter& web_contents_getter() const {
    return web_contents_getter_;
  }

  bool is_parent_frame_secure() const { return info_->is_parent_frame_secure; }

  // Returns whether this provider host is secure enough to have a service
  // worker controller.
  // Analogous to Blink's Document::isSecureContext. Because of how service
  // worker intercepts main resource requests, this check must be done
  // browser-side once the URL is known (see comments in
  // ServiceWorkerNetworkProvider::CreateForNavigation). This function uses
  // |document_url_| and |is_parent_frame_secure_| to determine context
  // security, so they must be set properly before calling this function.
  bool IsContextSecureForServiceWorker() const;

  // For service worker clients. Describes whether the client has a controller
  // and if it has a fetch event handler.
  blink::mojom::ControllerServiceWorkerMode GetControllerMode() const;

  // For service worker clients. Returns this client's controller.
  ServiceWorkerVersion* controller() const {
#if DCHECK_IS_ON()
    CheckControllerConsistency();
#endif  // DCHECK_IS_ON()

    return controller_.get();
  }

  ServiceWorkerRegistration* controller_registration() const {
#if DCHECK_IS_ON()
    CheckControllerConsistency();
#endif  // DCHECK_IS_ON()

    return controller_registration_.get();
  }

  // For service worker execution contexts. The version of the service worker.
  // This is nullptr when the worker is still starting up (until
  // CompleteStartWorkerPreparation() is called).
  ServiceWorkerVersion* running_hosted_version() const {
    DCHECK(!running_hosted_version_ ||
           info_->type ==
               blink::mojom::ServiceWorkerProviderType::kForServiceWorker);
    return running_hosted_version_.get();
  }

  // S13nServiceWorker:
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
  mojom::ControllerServiceWorkerPtr GetControllerServiceWorkerPtr();

  // Sets the |document_url_|.  When this object is for a client,
  // |matching_registrations_| gets also updated to ensure that |document_url_|
  // is in scope of all |matching_registrations_|.
  // |document_url_| is the service worker script URL if this is for a
  // service worker execution context. It will be used when creating
  // ServiceWorkerObjectHost or handling ServiceWorkerRegistration#{*} calls
  // etc.
  // TODO(leonhsl): We should rename |document_url_| to something more
  // appropriate and/or split this class into one for clients vs one for service
  // workers.
  void SetDocumentUrl(const GURL& url);
  const GURL& document_url() const { return document_url_; }

  // For service worker clients. Sets the |topmost_frame_url|.
  void SetTopmostFrameUrl(const GURL& url);
  // For service worker clients, used for permission checks. Use document_url()
  // instead if |this| is for a service worker execution context.
  const GURL& topmost_frame_url() const;

  blink::mojom::ServiceWorkerProviderType provider_type() const {
    return info_->type;
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

  // For use by the ServiceWorkerControlleeRequestHandler to disallow a
  // registration claiming this host while its main resource request is
  // occurring.
  //
  // TODO(crbug.com/866353): This should be unneccessary: registration code
  // already avoids claiming clients that are not execution ready. However
  // there may be edge cases with shared workers (pre-NetS13nServiceWorker) and
  // about:blank iframes, since |is_execution_ready_| is initialized true for
  // them. Try to remove this after S13nServiceWorker.
  void AllowSetControllerRegistration(bool allow) {
    allow_set_controller_registration_ = allow;
  }
  bool IsSetControllerRegistrationAllowed() {
    return allow_set_controller_registration_;
  }

  // Returns a handler for a request. May return nullptr if the request doesn't
  // require special handling.
  std::unique_ptr<ServiceWorkerRequestHandler> CreateRequestHandler(
      network::mojom::FetchRequestMode request_mode,
      network::mojom::FetchCredentialsMode credentials_mode,
      network::mojom::FetchRedirectMode redirect_mode,
      const std::string& integrity,
      bool keepalive,
      ResourceType resource_type,
      blink::mojom::RequestContextType request_context_type,
      network::mojom::RequestContextFrameType frame_type,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context,
      scoped_refptr<network::ResourceRequestBody> body,
      bool skip_service_worker);

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

  // Returns a ServiceWorkerObjectHost instance for |version| for this provider
  // host. A new instance is created if one does not already exist.
  // ServiceWorkerObjectHost will have an ownership of the |version|.
  base::WeakPtr<ServiceWorkerObjectHost> GetOrCreateServiceWorkerObjectHost(
      scoped_refptr<ServiceWorkerVersion> version);

  // Returns true if the context referred to by this host (i.e. |context_|) is
  // still alive.
  bool IsContextAlive();

  // Dispatches message event to the document.
  void PostMessageToClient(ServiceWorkerVersion* version,
                           blink::TransferableMessage message);

  // Notifies the client that its controller used a feature, for UseCounter
  // purposes. This can only be called if IsProviderForClient() is true.
  void CountFeature(blink::mojom::WebFeature feature);

  // |registration| claims the document to be controlled.
  void ClaimedByRegistration(
      scoped_refptr<ServiceWorkerRegistration> registration);

  // For service worker clients. Completes initialization of
  // provider hosts used for navigation requests.
  void CompleteNavigationInitialized(
      int process_id,
      mojom::ServiceWorkerProviderHostInfoPtr info);

  // For service worker execution contexts. Completes initialization of this
  // provider host. It is called once a renderer process has been found to host
  // the worker. Returns the info needed for creating a provider on the renderer
  // which will be connected to this provider host.
  //
  // |provider_info| should be the info returned by PreCreateForController(),
  // which is partially filled out. This function returns it after
  // filling it out completely.
  //
  // S13nServiceWorker:
  // |loader_factory| is the factory to use for "network" requests for the
  // service worker main script and import scripts. It is possibly not the
  // simple direct network factory, since service worker scripts can have
  // non-NetworkService schemes, e.g., chrome-extension:// URLs.
  mojom::ServiceWorkerProviderInfoForStartWorkerPtr
  CompleteStartWorkerPreparation(
      int process_id,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      mojom::ServiceWorkerProviderInfoForStartWorkerPtr provider_info);

  // Called when the shared worker main script resource has finished loading.
  // After this is called, is_execution_ready() returns true.
  void CompleteSharedWorkerPreparation();

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
  bool AllowServiceWorker(const GURL& scope);

  // Called when our controller has been terminated and doomed due to an
  // exceptional condition like it could no longer be read from the script
  // cache.
  void NotifyControllerLost();

  // S13nServiceWorker:
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
  //
  // For non-S13nServiceWorker: The update logic is controlled entirely by
  // ServiceWorkerControlleeRequestHandler, which sees all resource request
  // activity and schedules an update at a convenient time.
  void AddServiceWorkerToUpdate(scoped_refptr<ServiceWorkerVersion> version);

  bool is_execution_ready() const { return is_execution_ready_; }

 private:
  friend class LinkHeaderServiceWorkerTest;
  friend class ServiceWorkerProviderHostTest;
  friend class ServiceWorkerWriteToCacheJobTest;
  friend class ServiceWorkerContextRequestHandlerTest;
  friend class service_worker_controllee_request_handler_unittest::
      ServiceWorkerControlleeRequestHandlerTest;
  friend class service_worker_object_host_unittest::ServiceWorkerObjectHostTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerWriteToCacheJobTest, Update_SameScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerWriteToCacheJobTest,
                           Update_SameSizeScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerWriteToCacheJobTest,
                           Update_TruncatedScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerWriteToCacheJobTest,
                           Update_ElongatedScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerWriteToCacheJobTest,
                           Update_EmptyScript);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_dispatcher_host_unittest::ServiceWorkerDispatcherHostTest,
      DispatchExtendableMessageEvent);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_dispatcher_host_unittest::ServiceWorkerDispatcherHostTest,
      DispatchExtendableMessageEvent_Fail);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerProviderHostTest, ContextSecurity);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest, Unregister);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest, RegisterDuplicateScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest,
                           RegisterWithDifferentUpdateViaCache);
  FRIEND_TEST_ALL_PREFIXES(BackgroundSyncManagerTest,
                           RegisterWithoutLiveSWRegistration);

  ServiceWorkerProviderHost(int process_id,
                            mojom::ServiceWorkerProviderHostInfoPtr info,
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

#if DCHECK_IS_ON()
  void CheckControllerConsistency() const;
#endif  // DCHECK_IS_ON()

  // Implements mojom::ServiceWorkerContainerHost.
  void Register(const GURL& script_url,
                blink::mojom::ServiceWorkerRegistrationOptionsPtr options,
                RegisterCallback callback) override;
  void GetRegistration(const GURL& client_url,
                       GetRegistrationCallback callback) override;
  void GetRegistrations(GetRegistrationsCallback callback) override;
  void GetRegistrationForReady(
      GetRegistrationForReadyCallback callback) override;
  void EnsureControllerServiceWorker(
      mojom::ControllerServiceWorkerRequest controller_request,
      mojom::ControllerServiceWorkerPurpose purpose) override;
  void CloneContainerHost(
      mojom::ServiceWorkerContainerHostRequest container_host_request) override;
  void Ping(PingCallback callback) override;
  void HintToUpdateServiceWorker() override;

  // Callback for ServiceWorkerContextCore::RegisterServiceWorker().
  void RegistrationComplete(RegisterCallback callback,
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
      mojom::ControllerServiceWorkerRequest controller_request,
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
  // Returns true if all checks have passed.
  // If anything looks wrong |callback| will run with an error
  // message prefixed by |error_prefix| and |args|, and false is returned.
  template <typename CallbackType, typename... Args>
  bool CanServeContainerHostMethods(CallbackType* callback,
                                    const GURL& scope,
                                    const char* error_prefix,
                                    Args... args);

  const std::string client_uuid_;
  const base::TimeTicks create_time_;
  int render_process_id_;

  // For service worker execution contexts, the id of the service worker thread
  // or |kInvalidEmbeddedWorkerThreadId| before the service worker starts up.
  // Otherwise, |kDocumentMainThreadId|.
  int render_thread_id_;

  mojom::ServiceWorkerProviderHostInfoPtr info_;

  // Only set when this object is pre-created for a navigation. It indicates the
  // tab where the navigation occurs.
  WebContentsGetter web_contents_getter_;

  GURL document_url_;
  GURL topmost_frame_url_;

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
  bool allow_set_controller_registration_ = true;

  // For service worker execution contexts. The ServiceWorkerVersion of the
  // service worker this is a provider for.
  scoped_refptr<ServiceWorkerVersion> running_hosted_version_;

  base::WeakPtr<ServiceWorkerContextCore> context_;

  // |container_| is the Mojo endpoint to the renderer-side
  // ServiceWorkerContainer that |this| is a ServiceWorkerContainerHost for.
  mojom::ServiceWorkerContainerAssociatedPtr container_;
  // |binding_| is the Mojo binding that keeps the connection to the
  // renderer-side counterpart (content::ServiceWorkerProviderContext). When the
  // connection bound on |binding_| gets killed from the renderer side, or the
  // bound |ServiceWorkerProviderInfoForStartWorker::host_ptr_info| is otherwise
  // destroyed before being passed to the renderer, this
  // content::ServiceWorkerProviderHost will be destroyed.
  mojo::AssociatedBinding<mojom::ServiceWorkerContainerHost> binding_;

  // Container host bindings other than the original |binding_|. These include
  // bindings for container host pointers used from (dedicated or shared) worker
  // threads, or from ServiceWorkerSubresourceLoaderFactory.
  mojo::BindingSet<mojom::ServiceWorkerContainerHost> additional_bindings_;

  // For service worker execution contexts.
  mojo::Binding<service_manager::mojom::InterfaceProvider>
      interface_provider_binding_;

  // For service worker clients. True if the main resource for this host has
  // finished loading. When false, the document URL may still change due to
  // redirects.
  bool is_execution_ready_ = false;

  // For service worker clients. The service workers in the chain of redirects
  // during the main resource request for this client. These workers should be
  // updated "soon". See AddServiceWorkerToUpdate() documentation.
  class PendingUpdateVersion;
  base::flat_set<PendingUpdateVersion> versions_to_update_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
