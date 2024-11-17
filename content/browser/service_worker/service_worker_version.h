// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_

#include <stdint.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/id_map.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/uuid.h"
#include "components/services/storage/public/mojom/service_worker_storage_control.mojom.h"
#include "content/browser/renderer_host/back_forward_cache_metrics.h"
#include "content/browser/renderer_host/policy_container_host.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/service_worker_client_utils.h"
#include "content/browser/service_worker/service_worker_metrics.h"
#include "content/browser/service_worker/service_worker_ping_controller.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_script_cache_map.h"
#include "content/browser/service_worker/service_worker_update_checker.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_router_evaluator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/service_worker_client_info.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/cpp/document_isolation_policy.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom-forward.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/common/service_worker/embedded_worker_status.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/associated_interfaces/associated_interfaces.mojom.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_event_status.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

class ServiceWorkerClient;
class ServiceWorkerContextCore;
class ServiceWorkerHost;
class ServiceWorkerInstalledScriptsSender;
struct ServiceWorkerVersionInfo;

namespace service_worker_controllee_request_handler_unittest {
class ServiceWorkerControlleeRequestHandlerTest;
FORWARD_DECLARE_TEST(ServiceWorkerControlleeRequestHandlerTest,
                     ActivateWaitingVersion);
FORWARD_DECLARE_TEST(ServiceWorkerControlleeRequestHandlerTest,
                     FallbackWithNoFetchHandler);
}  // namespace service_worker_controllee_request_handler_unittest

namespace service_worker_version_unittest {
class ServiceWorkerVersionTest;
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, FailToStart_Timeout);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, IdleTimeout);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, MixedRequestTimeouts);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, PendingExternalRequest);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, RequestCustomizedTimeout);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, RequestNowTimeout);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, RequestTimeout);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, RestartWorker);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, RequestNowTimeoutKill);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, SetDevToolsAttached);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, StaleUpdate_DoNotDeferTimer);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, StaleUpdate_FreshWorker);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, StaleUpdate_NonActiveWorker);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, StaleUpdate_RunningWorker);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, StaleUpdate_StartWorker);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest,
                     StallInStopping_DetachThenRestart);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, StallInStopping_DetachThenStart);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, StartRequestWithNullContext);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest,
                     WorkerLifetimeWithExternalRequest);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, DevToolsAttachThenDetach);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest,
                     DefaultTimeoutRequestDoesNotAffectMaxTimeoutRequest);
FORWARD_DECLARE_TEST(ServiceWorkerVersionTest, Doom);
}  // namespace service_worker_version_unittest

FORWARD_DECLARE_TEST(ServiceWorkerRegistryTest, ScriptResponseTime);

namespace service_worker_main_resource_loader_unittest {
class ServiceWorkerMainResourceLoaderTest;
}  // namespace service_worker_main_resource_loader_unittest

// This class corresponds to a specific version of a ServiceWorker
// script for a given scope. When a script is upgraded, there may be
// more than one ServiceWorkerVersion "running" at a time, but only
// one of them is activated. This class connects the actual script with a
// running worker.
//
// Unless otherwise noted, all methods of this class run on the UI thread.
class CONTENT_EXPORT ServiceWorkerVersion
    : public blink::mojom::ServiceWorkerHost,
      public blink::mojom::AssociatedInterfaceProvider,
      public base::RefCounted<ServiceWorkerVersion>,
      public EmbeddedWorkerInstance::Listener {
 public:
  using StatusCallback =
      base::OnceCallback<void(blink::ServiceWorkerStatusCode)>;
  using SimpleEventCallback =
      base::OnceCallback<void(blink::mojom::ServiceWorkerEventStatus)>;
  using FetchHandlerExistence = blink::mojom::FetchHandlerExistence;
  using FetchHandlerType = blink::mojom::ServiceWorkerFetchHandlerType;
  using FetchHandlerBypassOption =
      blink::mojom::ServiceWorkerFetchHandlerBypassOption;

  // Current version status; some of the status (e.g. INSTALLED and ACTIVATED)
  // should be persisted unlike running status.
  enum Status {
    NEW,         // The version is just created.
    INSTALLING,  // Install event is dispatched and being handled.
    INSTALLED,   // Install event is finished and is ready to be activated.
    ACTIVATING,  // Activate event is dispatched and being handled.
    ACTIVATED,   // Activation is finished and can run as activated.
    REDUNDANT,   // The version is no longer running as activated, due to
                 // unregistration or replace.
  };

  // Behavior when a request times out.
  enum TimeoutBehavior {
    KILL_ON_TIMEOUT,     // Kill the worker if this request times out.
    CONTINUE_ON_TIMEOUT  // Keep the worker alive, only abandon the request that
                         // timed out.
  };

  // Contains a subset of the main script's response information.
  struct CONTENT_EXPORT MainScriptResponse {
    explicit MainScriptResponse(
        const network::mojom::URLResponseHead& response_head);
    ~MainScriptResponse();

    base::Time response_time;
    base::Time last_modified;
    // These are used for all responses sent back from a service worker, as
    // effective security of these responses is equivalent to that of the
    // service worker.
    scoped_refptr<net::HttpResponseHeaders> headers;
    std::optional<net::SSLInfo> ssl_info;
  };

  class Observer {
   public:
    virtual void OnRunningStateChanged(ServiceWorkerVersion* version) {}
    virtual void OnVersionStateChanged(ServiceWorkerVersion* version) {}
    virtual void OnDevToolsRoutingIdChanged(ServiceWorkerVersion* version) {}
    virtual void OnErrorReported(ServiceWorkerVersion* version,
                                 const std::u16string& error_message,
                                 int line_number,
                                 int column_number,
                                 const GURL& source_url) {}
    virtual void OnReportConsoleMessage(
        ServiceWorkerVersion* version,
        blink::mojom::ConsoleMessageSource source,
        blink::mojom::ConsoleMessageLevel message_level,
        const std::u16string& message,
        int line_number,
        const GURL& source_url) {}
    virtual void OnCachedMetadataUpdated(ServiceWorkerVersion* version,
                                         size_t size) {}
    virtual void OnNoWork(ServiceWorkerVersion* version) {}

   protected:
    virtual ~Observer() {}
  };

  // The constructor should be called only from ServiceWorkerRegistry other than
  // tests.
  ServiceWorkerVersion(
      ServiceWorkerRegistration* registration,
      const GURL& script_url,
      blink::mojom::ScriptType script_type,
      int64_t version_id,
      mojo::PendingRemote<storage::mojom::ServiceWorkerLiveVersionRef>
          remote_reference,
      base::WeakPtr<ServiceWorkerContextCore> context);

  ServiceWorkerVersion(const ServiceWorkerVersion&) = delete;
  ServiceWorkerVersion& operator=(const ServiceWorkerVersion&) = delete;

  int64_t version_id() const { return version_id_; }
  int64_t registration_id() const { return registration_id_; }
  const GURL& script_url() const { return script_url_; }
  const blink::StorageKey& key() const { return key_; }
  const GURL& scope() const { return scope_; }
  blink::mojom::ScriptType script_type() const { return script_type_; }
  blink::EmbeddedWorkerStatus running_status() const {
    return embedded_worker_->status();
  }
  ServiceWorkerVersionInfo GetInfo();
  Status status() const { return status_; }
  ukm::SourceId ukm_source_id() const { return ukm_source_id_; }
  const base::UnguessableToken& reporting_source() const {
    return reporting_source_;
  }

  // This status is set to EXISTS or DOES_NOT_EXIST when the install event has
  // been executed in a new version or when an installed version is loaded from
  // the storage. When a new version is not installed yet, it is UNKNOWN.
  FetchHandlerExistence fetch_handler_existence() const;
  // Returns the fetch handler type if set.  Otherwise, kNoHandler.
  FetchHandlerType fetch_handler_type() const;
  void set_fetch_handler_type(FetchHandlerType fetch_handler_type);

  // Return the option indicating how the fetch handler should be bypassed.
  // This is used to let the renderer know to bypass fetch handlers for
  // subresources.
  FetchHandlerBypassOption fetch_handler_bypass_option() {
    return fetch_handler_bypass_option_;
  }
  void set_fetch_handler_bypass_option(
      FetchHandlerBypassOption fetch_handler_bypass_option) {
    fetch_handler_bypass_option_ = fetch_handler_bypass_option;
  }

  bool has_hid_event_handlers() const { return has_hid_event_handlers_; }
  void set_has_hid_event_handlers(bool has_hid_event_handlers);

  bool has_usb_event_handlers() const { return has_usb_event_handlers_; }
  void set_has_usb_event_handlers(bool has_usb_event_handlers);

  // Returns `ServiceWorkerRouterEvaluatorErrorEnums::kNoError` on setup
  // success.
  // Otherwise, we encountered an setup error with the returned error code,
  // and subsequent `router_evaluator()` will return `std::nullptr`.
  ServiceWorkerRouterEvaluatorErrorEnums SetupRouterEvaluator(
      const blink::ServiceWorkerRouterRules& rules);
  const ServiceWorkerRouterEvaluator* router_evaluator() const {
    return router_evaluator_.get();
  }

  base::TimeDelta TimeSinceNoControllees() const {
    return GetTickDuration(no_controllees_time_);
  }

  base::TimeDelta TimeSinceSkipWaiting() const {
    return GetTickDuration(skip_waiting_time_);
  }

  // Meaningful only if this version is active.
  const blink::mojom::NavigationPreloadState& navigation_preload_state() const {
    DCHECK(status_ == ACTIVATING || status_ == ACTIVATED) << status_;
    return navigation_preload_state_;
  }
  // Only intended for use by ServiceWorkerRegistration. Generally use
  // ServiceWorkerRegistration::EnableNavigationPreload or
  // ServiceWorkerRegistration::SetNavigationPreloadHeader instead of this
  // function.
  void SetNavigationPreloadState(
      const blink::mojom::NavigationPreloadState& state);

  // Only intended for use by ServiceWorkerRegistration. Generally use
  // ServiceWorkerRegistration::status() instead of this function.
  void SetRegistrationStatus(
      ServiceWorkerRegistration::Status registration_status);

  // This sets the new status and also run status change callbacks
  // if there're any (see RegisterStatusChangeCallback).
  void SetStatus(Status status);

  // Registers status change callback. (This is for one-off observation,
  // the consumer needs to re-register if it wants to continue observing
  // status changes)
  void RegisterStatusChangeCallback(base::OnceClosure callback);

  // Starts an embedded worker for this version.
  // This returns OK (success) if the worker is already running.
  // |purpose| is recorded in UMA.
  void StartWorker(ServiceWorkerMetrics::EventType purpose,
                   StatusCallback callback);

  // Stops an embedded worker for this version.
  void StopWorker(base::OnceClosure callback);

  // Asks the renderer to notify the browser that it becomes idle as soon as
  // possible, and it results in letting idle termination occur earlier. This is
  // typically used for activation. An active worker needs to be swapped out
  // soon after the service worker becomes idle if a waiting worker exists.
  void TriggerIdleTerminationAsap();

  // Called when the renderer notifies the browser that the worker is now idle.
  // Returns true if the worker will be terminated and the worker should not
  // handle any events dispatched directly from clients (e.g. FetchEvents for
  // subresources).
  bool OnRequestTermination();

  // Schedules an update to be run 'soon'.
  void ScheduleUpdate();

  // Starts an update now.
  void StartUpdate();

  // Starts the worker if it isn't already running. Calls |callback| with
  // blink::ServiceWorkerStatusCode::kOk when the worker started
  // up successfully or if it is already running. Otherwise, calls |callback|
  // with an error code. If the worker is already running, |callback| is
  // executed synchronously (before this method returns). |purpose| is used for
  // UMA.
  void RunAfterStartWorker(ServiceWorkerMetrics::EventType purpose,
                           StatusCallback callback);

  // Call this while the worker is running before dispatching an event to the
  // worker. This informs ServiceWorkerVersion about the event in progress. The
  // worker attempts to keep running until the event finishes.
  //
  // Returns a request id, which must later be passed to FinishRequest when the
  // event finished. The caller is responsible for ensuring FinishRequest is
  // called. If FinishRequest is not called the request will eventually time
  // out and the worker will be forcibly terminated.
  //
  // `error_callback` is called if either ServiceWorkerVersion decides the
  // event is taking too long, or if for some reason the worker stops or is
  // killed before the request finishes. In this case, the caller should not
  // call FinishRequest. EXCEPTION: If CreateSimpleEventCallback() is used,
  // `error_callback` is always called, even in the case of success.
  // TODO(http://crbug.com/1251834): Clean up this exception.
  int StartRequest(ServiceWorkerMetrics::EventType event_type,
                   StatusCallback error_callback);

  // Same as StartRequest, but allows the caller to specify a custom timeout for
  // the event, as well as the behavior for when the request times out.
  //
  // S13nServiceWorker: |timeout| and |timeout_behavior| don't have any effect.
  // They are just ignored. Timeouts can be added to the
  // blink::mojom::ServiceWorker interface instead (see DispatchSyncEvent for an
  // example).
  int StartRequestWithCustomTimeout(ServiceWorkerMetrics::EventType event_type,
                                    StatusCallback error_callback,
                                    const base::TimeDelta& timeout,
                                    TimeoutBehavior timeout_behavior);

  // Starts a request of type EventType::EXTERNAL_REQUEST.
  // Provides a mechanism to external clients to keep the worker running.
  // |request_uuid| is a GUID for clients to identify the request.
  // |timeout_type| is to specfiy request timeout behaviour of the worker.
  // Returns true if the request was successfully scheduled to starrt.
  ServiceWorkerExternalRequestResult StartExternalRequest(
      const base::Uuid& request_uuid,
      ServiceWorkerExternalRequestTimeoutType timeout_type);

  // Informs ServiceWorkerVersion that an event has finished being dispatched.
  // Returns false if no inflight requests with the provided id exist, for
  // example if the request has already timed out.
  // Pass the result of the event to |was_handled|, which is used to record
  // statistics based on the event status.
  // TODO(mek): Use something other than a bool for event status.
  bool FinishRequest(int request_id, bool was_handled);

  // Like FinishRequest(), but includes a count of how many fetches were
  // performed by the script while handling the event.
  bool FinishRequestWithFetchCount(int request_id,
                                   bool was_handled,
                                   uint32_t fetch_count);

  // Finishes an external request that was started by StartExternalRequest().
  ServiceWorkerExternalRequestResult FinishExternalRequest(
      const base::Uuid& request_uuid);

  // Creates a callback that is to be used for marking simple events dispatched
  // through blink::mojom::ServiceWorker as finished for the |request_id|.
  // Simple event means those events expecting a response with only a status
  // code and the dispatch time. See service_worker.mojom.
  SimpleEventCallback CreateSimpleEventCallback(int request_id);

  // This must be called when is_endpoint_ready() returns true, which is after
  // InitializeGlobalScope() is called.
  blink::mojom::ServiceWorker* endpoint() {
    DCHECK(running_status() == blink::EmbeddedWorkerStatus::kStarting ||
           running_status() == blink::EmbeddedWorkerStatus::kRunning);
    DCHECK(service_worker_remote_.is_bound());
    return service_worker_remote_.get();
  }

  bool is_endpoint_ready() const { return is_endpoint_ready_; }

  // Returns the 'controller' interface ptr of this worker. It is expected that
  // the worker is already starting or running, or is going to be started soon.
  // TODO(kinuko): Relying on the callsites to start the worker when it's
  // not running is a bit sketchy, maybe this should queue a task to check
  // if the pending request is pending too long? https://crbug.com/797222
  blink::mojom::ControllerServiceWorker* controller() {
    if (!remote_controller_.is_bound()) {
      DCHECK(!controller_receiver_.is_valid());
      controller_receiver_ = remote_controller_.BindNewPipeAndPassReceiver();
    }
    return remote_controller_.get();
  }

  // Adds and removes the specified host as a controllee of this service worker.
  void AddControllee(ServiceWorkerClient* service_worker_client);
  void RemoveControllee(const std::string& client_uuid);

  // Called when the navigation for a window client commits to a render frame
  // host.
  void OnControlleeNavigationCommitted(const std::string& client_uuid,
                                       const GlobalRenderFrameHostId& rfh_id);

  // Called when a controllee goes into back-forward cache.
  void MoveControlleeToBackForwardCacheMap(const std::string& client_uuid);
  // Called when a back-forward cached controllee is restored.
  void RestoreControlleeFromBackForwardCacheMap(const std::string& client_uuid);
  // Called when a back-forward cached controllee is evicted or destroyed.
  void RemoveControlleeFromBackForwardCacheMap(const std::string& client_uuid);
  // Called when this version should no longer be the controller of this client.
  // Called when the controllee is destroyed or it changes controller. Removes
  // controllee from whichever map it belongs to, or do nothing when it is
  // already removed. This function is different from RemoveController(), which
  // can only be called if the controllee is not in the back-forward cache map.
  void Uncontrol(const std::string& client_uuid);

  // Returns true if this version has a controllee.
  // Note regarding BackForwardCache:
  // Clients in back-forward cache don't count as controllees.
  bool HasControllee() const { return !controllee_map_.empty(); }
  const std::map<std::string, base::WeakPtr<ServiceWorkerClient>>&
  controllee_map() const {
    return controllee_map_;
  }
  // Returns true if |uuid| is captured by BFCache.
  bool BFCacheContainsControllee(const std::string& uuid) const;

  // BackForwardCache:
  // Evicts all the controllees from back-forward cache. The controllees in
  // |bfcached_controllee_map_| will be removed asynchronously as a result of
  // eviction.
  void EvictBackForwardCachedControllees(
      BackForwardCacheMetrics::NotRestoredReason reason);
  void EvictBackForwardCachedControllee(
      ServiceWorkerClient* controllee,
      BackForwardCacheMetrics::NotRestoredReason reason);

  // The worker host hosting this version. Only valid while the version is
  // running.
  content::ServiceWorkerHost* worker_host() {
    CHECK(worker_host_);
    return worker_host_.get();
  }

  base::WeakPtr<ServiceWorkerContextCore> context() const { return context_; }

  // Adds and removes Observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  ServiceWorkerScriptCacheMap* script_cache_map() { return &script_cache_map_; }
  EmbeddedWorkerInstance* embedded_worker() { return embedded_worker_.get(); }

  // Reports the error message to |observers_|.
  void ReportError(blink::ServiceWorkerStatusCode status,
                   const std::string& status_message);

  void ReportForceUpdateToDevTools();

  // Sets the status code to pass to StartWorker callbacks if start fails.
  void SetStartWorkerStatusCode(blink::ServiceWorkerStatusCode status);

  // Sets this version's status to REDUNDANT and deletes its resources.
  void Doom();
  bool is_redundant() const { return status_ == REDUNDANT; }

  bool skip_waiting() const { return skip_waiting_; }
  void set_skip_waiting(bool skip_waiting) { skip_waiting_ = skip_waiting; }

  bool skip_recording_startup_time() const {
    return skip_recording_startup_time_;
  }

  bool force_bypass_cache_for_scripts() const {
    return force_bypass_cache_for_scripts_;
  }
  void set_force_bypass_cache_for_scripts(bool force_bypass_cache_for_scripts) {
    force_bypass_cache_for_scripts_ = force_bypass_cache_for_scripts;
  }

  void set_main_script_load_params(
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params) {
    main_script_load_params_ = std::move(main_script_load_params);
  }

  void set_outside_fetch_client_settings_object(
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object) {
    DCHECK(!outside_fetch_client_settings_object_);
    outside_fetch_client_settings_object_ =
        std::move(outside_fetch_client_settings_object);
  }

  // Returns the reason the embedded worker failed to start, using internal
  // information that may not be available to the caller. Returns
  // |default_code| if it can't deduce a reason.
  blink::ServiceWorkerStatusCode DeduceStartWorkerFailureReason(
      blink::ServiceWorkerStatusCode default_code);

  // Gets the main script net::Error. If there isn't an error, returns net::OK.
  net::Error GetMainScriptNetError();

  // Returns nullptr if the main script is not loaded yet and:
  //  1) The worker is a new one.
  //  OR
  //  2) The worker is an existing one but the entry in ServiceWorkerDatabase
  //     was written by old version of Chrome (< M56), so |origin_trial_tokens|
  //     wasn't set in the entry.
  const blink::TrialTokenValidator::FeatureToTokensMap* origin_trial_tokens()
      const {
    return origin_trial_tokens_.get();
  }
  // Set valid tokens in |tokens|. Invalid tokens in |tokens| are ignored.
  void SetValidOriginTrialTokens(
      const blink::TrialTokenValidator::FeatureToTokensMap& tokens);

  void SetDevToolsAttached(bool attached);

  // Sets the response information used to load the main script.
  void SetMainScriptResponse(std::unique_ptr<MainScriptResponse> response);
  const MainScriptResponse* GetMainScriptResponse();

  // Simulate ping timeout. Should be used for tests-only.
  void SimulatePingTimeoutForTesting();

  // Used to allow tests to change time for testing.
  void SetTickClockForTesting(const base::TickClock* tick_clock);

  // Run user tasks for testing. Used for stopping a service worker.
  void RunUserTasksForTesting();

  // Returns true when the service worker isn't handling any events or stream
  // responses, initiated from either the browser or the renderer.
  bool HasNoWork() const;

  // Returns the number of pending external request count of this worker.
  size_t GetExternalRequestCountForTest() const {
    return external_request_uuid_to_request_id_.size();
  }

  // Returns the amount of time left until the request with the latest
  // expiration time expires.
  base::TimeDelta remaining_timeout() const {
    return max_request_expiration_time_ - tick_clock_->NowTicks();
  }

  void CountFeature(blink::mojom::WebFeature feature);
  void set_used_features(std::set<blink::mojom::WebFeature> used_features) {
    used_features_ = std::move(used_features);
  }
  const std::set<blink::mojom::WebFeature>& used_features() const {
    return used_features_;
  }

  // Returns the COEP value stored in `client_security_state()`.
  // Returns `kNone` if `client_security_state()` is nullptr.
  network::mojom::CrossOriginEmbedderPolicyValue
  cross_origin_embedder_policy_value() const;

  // Returns a pointer to the COEP stored in `client_security_state()`.
  // Returns nullptr if `client_security_state()` is nullptr.
  const network::CrossOriginEmbedderPolicy* cross_origin_embedder_policy()
      const;
  const scoped_refptr<PolicyContainerHost> policy_container_host() const {
    return policy_container_host_;
  }

  // Returns a pointer to the DIP stored in `client_security_state()`.
  // Returns nullptr if `client_security_state()` is nullptr.
  const network::DocumentIsolationPolicy* document_isolation_policy() const;

  // Returns a client security state built from this service worker's policy
  // container policies.
  const network::mojom::ClientSecurityStatePtr BuildClientSecurityState() const;

  void set_script_response_time_for_devtools(base::Time response_time) {
    script_response_time_for_devtools_ = std::move(response_time);
  }

  static bool IsInstalled(ServiceWorkerVersion::Status status);
  static std::string VersionStatusToString(ServiceWorkerVersion::Status status);

  // For scheduling Soft Update after main resource requests. We schedule
  // a Soft Update to happen "soon" after each main resource request, attempting
  // to do the update after the page load finished. The renderer sends a hint
  // when it's a good time to update. This is a count of outstanding expected
  // hints, to handle multiple main resource requests occurring near the same
  // time.
  //
  // On each request that dispatches a fetch event to this worker (or would
  // have, in the case of a no-fetch event worker), this count is incremented.
  // When the browser-side worker host receives a hint from the renderer that
  // it is a good time to update the service worker, the count is decremented.
  // It is also decremented when if the worker host is destroyed before
  // receiving the hint.
  //
  // When the count transitions from 1 to 0, update is scheduled.
  void IncrementPendingUpdateHintCount();
  void DecrementPendingUpdateHintCount();

  // Called on versions created for an update check. Called if the check
  // determined an update exists before starting the worker for an install
  // event.
  void PrepareForUpdate(
      std::map<GURL, ServiceWorkerUpdateChecker::ComparedScriptInfo>
          compared_script_info_map,
      const GURL& updated_script_url,
      scoped_refptr<PolicyContainerHost> policy_container_host);
  const std::map<GURL, ServiceWorkerUpdateChecker::ComparedScriptInfo>&
  compared_script_info_map() const;
  ServiceWorkerUpdateChecker::ComparedScriptInfo TakeComparedScriptInfo(
      const GURL& script_url);

  // Called by the EmbeddedWorkerInstance to determine if its worker process
  // should be kept at foreground priority.
  bool ShouldRequireForegroundPriority(int worker_process_id) const;

  // Called when a controlled client's state changes in a way that might effect
  // whether the service worker should be kept at foreground priority.
  void UpdateForegroundPriority();

  // Adds a message to the service worker's DevTools console.
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message);

  // Adds a message to service worker internals UI page if the internal page is
  // opened. Use this method only for events which can't be logged on the
  // worker's DevTools console, e.g., the worker is not responding. For regular
  // events use AddMessageToConsole().
  void MaybeReportConsoleMessageToInternals(
      blink::mojom::ConsoleMessageLevel message_level,
      const std::string& message);

  // Rebinds the mojo remote to the Storage Service. Called during a recovery
  // step of the Storage Service.
  storage::mojom::ServiceWorkerLiveVersionInfoPtr RebindStorageReference();

  mojo::AssociatedReceiver<blink::mojom::ServiceWorkerHost>&
  service_worker_host_receiver_for_testing() {
    return receiver_;
  }

  void set_reporting_observer_receiver(
      mojo::PendingReceiver<blink::mojom::ReportingObserver>
          reporting_observer_receiver) {
    reporting_observer_receiver_ = std::move(reporting_observer_receiver);
  }

  void set_policy_container_host(
      scoped_refptr<PolicyContainerHost> policy_container_host) {
    policy_container_host_ = std::move(policy_container_host);
  }

  // Initializes the global scope of the ServiceWorker on the renderer side.
  void InitializeGlobalScope();

  // Returns true if |process_id| is a controllee process ID of this version.
  bool IsControlleeProcessID(int process_id) const;

  // Executes the given `script` in the associated worker. If `callback` is
  // non-empty, invokes `callback` with the result of the script after
  // execution. See also service_worker.mojom.
  void ExecuteScriptForTest(const std::string& script,
                            ServiceWorkerScriptExecutionCallback callback);

  // Returns true if this SW is going to warm-up. This function can be called
  // anytime. If the `running_status()` is `RUNNING`, `STOPPING` or `STOPPED`,
  // this function returns false.
  bool IsWarmingUp() const;

  // Returns true if this SW already warmed-up. This function can be called
  // anytime. If the `running_status()` is `RUNNING`, `STOPPING` or `STOPPED`,
  // this function returns false.
  bool IsWarmedUp() const;

  blink::mojom::AncestorFrameType ancestor_frame_type() const {
    return ancestor_frame_type_;
  }

  // Used when loading an existing version. Sets ServiceWorkerResourceRecord to
  // |script_cache_map_|, then updates |sha256_script_checksum_|.
  void SetResources(
      const std::vector<storage::mojom::ServiceWorkerResourceRecordPtr>&
          resources);

  std::optional<std::string> sha256_script_checksum() {
    return sha256_script_checksum_;
  }

  blink::AssociatedInterfaceProvider* associated_interface_provider() {
    return associated_interface_provider_.get();
  }

  // Check if the static router API is enabled. It checks if the feature flag is
  // enabled or having a valid trial token.
  bool IsStaticRouterEnabled();

  // Check if the static router should be evaluated.
  bool NeedRouterEvaluate() const;

  // Get an associated cache storage interface.
  mojo::PendingRemote<blink::mojom::CacheStorage> GetRemoteCacheStorage();

  // Describes whether the client has a controller and if it has a fetch event
  // handler.
  blink::mojom::ControllerServiceWorkerMode GetControllerMode() const;

  // Timeout for a request to be handled.
  static constexpr base::TimeDelta kRequestTimeout = base::Minutes(5);

  base::WeakPtr<ServiceWorkerVersion> GetWeakPtr();

 private:
  friend class base::RefCounted<ServiceWorkerVersion>;
  friend class EmbeddedWorkerTestHelper;
  friend class ServiceWorkerPingController;
  friend class ServiceWorkerContainerHostTest;
  friend class ServiceWorkerReadFromCacheJobTest;
  friend class ServiceWorkerVersionBrowserTest;
  friend class ServiceWorkerActivationTest;
  friend class service_worker_version_unittest::ServiceWorkerVersionTest;
  friend class service_worker_main_resource_loader_unittest::
      ServiceWorkerMainResourceLoaderTest;

  FRIEND_TEST_ALL_PREFIXES(service_worker_controllee_request_handler_unittest::
                               ServiceWorkerControlleeRequestHandlerTest,
                           ActivateWaitingVersion);
  FRIEND_TEST_ALL_PREFIXES(service_worker_controllee_request_handler_unittest::
                               ServiceWorkerControlleeRequestHandlerTest,
                           FallbackWithNoFetchHandler);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      PendingExternalRequest);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest, Register);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      IdleTimeout);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      SetDevToolsAttached);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      StaleUpdate_FreshWorker);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      StaleUpdate_NonActiveWorker);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      StaleUpdate_StartWorker);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      StaleUpdate_RunningWorker);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      StaleUpdate_DoNotDeferTimer);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      StartRequestWithNullContext);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      FailToStart_Timeout);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionBrowserTest,
                           TimeoutStartingWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerVersionBrowserTest,
                           TimeoutWorkerInEvent);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      StallInStopping_DetachThenStart);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      StallInStopping_DetachThenRestart);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      RequestNowTimeout);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      RequestTimeout);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      RestartWorker);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      RequestNowTimeoutKill);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      RequestCustomizedTimeout);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      MixedRequestTimeouts);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      WorkerLifetimeWithExternalRequest);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      DefaultTimeoutRequestDoesNotAffectMaxTimeoutRequest);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      DevToolsAttachThenDetach);
  FRIEND_TEST_ALL_PREFIXES(
      service_worker_version_unittest::ServiceWorkerVersionTest,
      Doom);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerRegistryTest, ScriptResponseTime);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerBrowserTest,
                           WarmUpAndStartServiceWorker);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerBrowserTest, WarmUpWorkerAndTimeout);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerBrowserTest, WarmUpWorkerTwice);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerFetchDispatcherBrowserTest,
                           FetchEventTimeout);

  // Contains timeout info for InflightRequest.
  struct InflightRequestTimeoutInfo {
    InflightRequestTimeoutInfo(int id,
                               ServiceWorkerMetrics::EventType event_type,
                               const base::TimeTicks& expiration_time,
                               TimeoutBehavior timeout_behavior);
    ~InflightRequestTimeoutInfo();
    // Compares |expiration_time|, or |id| if |expiration_time| is the same.
    bool operator<(const InflightRequestTimeoutInfo& other) const;

    const int id;
    const ServiceWorkerMetrics::EventType event_type;
    const base::TimeTicks expiration_time;
    const TimeoutBehavior timeout_behavior;
  };

  // Keeps track of the status of each request, which starts at StartRequest()
  // and ends at FinishRequest().
  struct InflightRequest {
    InflightRequest(StatusCallback error_callback,
                    base::Time time,
                    const base::TimeTicks& time_ticks,
                    ServiceWorkerMetrics::EventType event_type);
    ~InflightRequest();

    StatusCallback error_callback;
    base::Time start_time;
    base::TimeTicks start_time_ticks;
    ServiceWorkerMetrics::EventType event_type;
    // Points to this request's entry in |request_timeouts_|.
    std::set<InflightRequestTimeoutInfo>::iterator timeout_iter;
  };

  // The timeout timer interval.
  static constexpr base::TimeDelta kTimeoutTimerDelay = base::Seconds(30);
  // Timeout for a new worker to start.
  static constexpr base::TimeDelta kStartNewWorkerTimeout = base::Minutes(5);
  // Timeout for the worker to stop.
  static constexpr base::TimeDelta kStopWorkerTimeout = base::Seconds(5);

  ~ServiceWorkerVersion() override;

  // The following methods all rely on the internal |tick_clock_| for the
  // current time.
  void RestartTick(base::TimeTicks* time) const;
  bool RequestExpired(const base::TimeTicks& expiration_time) const;
  base::TimeDelta GetTickDuration(const base::TimeTicks& time) const;

  // EmbeddedWorkerInstance::Listener overrides:
  void OnScriptEvaluationStart() override;
  void OnScriptLoaded() override;
  void OnProcessAllocated() override;
  void OnStarting() override;
  void OnStarted(blink::mojom::ServiceWorkerStartStatus status,
                 FetchHandlerType new_fetch_handler_type,
                 bool new_has_hid_event_handlers,
                 bool new_has_usb_event_handlers) override;
  void OnStopping() override;
  void OnStopped(blink::EmbeddedWorkerStatus old_status) override;
  void OnDetached(blink::EmbeddedWorkerStatus old_status) override;
  void OnRegisteredToDevToolsManager() override;
  void OnReportException(const std::u16string& error_message,
                         int line_number,
                         int column_number,
                         const GURL& source_url) override;
  void OnReportConsoleMessage(blink::mojom::ConsoleMessageSource source,
                              blink::mojom::ConsoleMessageLevel message_level,
                              const std::u16string& message,
                              int line_number,
                              const GURL& source_url) override;

  void OnStartSent(blink::ServiceWorkerStatusCode status);

  // Implements blink::mojom::ServiceWorkerHost.
  void SetCachedMetadata(const GURL& url,
                         base::span<const uint8_t> data) override;
  void ClearCachedMetadata(const GURL& url) override;
  void ClaimClients(ClaimClientsCallback callback) override;
  void GetClients(blink::mojom::ServiceWorkerClientQueryOptionsPtr options,
                  GetClientsCallback callback) override;
  void GetClient(const std::string& client_uuid,
                 GetClientCallback callback) override;
  void GetClientInternal(const std::string& client_uuid,
                         GetClientCallback callback);
  void OpenNewTab(const GURL& url, OpenNewTabCallback callback) override;
  void OpenPaymentHandlerWindow(
      const GURL& url,
      OpenPaymentHandlerWindowCallback callback) override;
  void PostMessageToClient(const std::string& client_uuid,
                           blink::TransferableMessage message) override;
  void FocusClient(const std::string& client_uuid,
                   FocusClientCallback callback) override;
  void NavigateClient(const std::string& client_uuid,
                      const GURL& url,
                      NavigateClientCallback callback) override;
  void SkipWaiting(SkipWaitingCallback callback) override;
  void AddRoutes(const blink::ServiceWorkerRouterRules& rules,
                 AddRoutesCallback callback) override;

  // Implements blink::mojom::AssociatedInterfaceProvider.
  void GetAssociatedInterface(
      const std::string& name,
      mojo::PendingAssociatedReceiver<blink::mojom::AssociatedInterface>
          receiver) override;

  void OnSetCachedMetadataFinished(int64_t callback_id,
                                   size_t size,
                                   int result);
  void OnClearCachedMetadataFinished(int64_t callback_id, int result);
  void OpenWindow(GURL url,
                  service_worker_client_utils::WindowType type,
                  OpenNewTabCallback callback);

  void OnPongFromWorker();

  void DidEnsureLiveRegistrationForStartWorker(
      ServiceWorkerMetrics::EventType purpose,
      Status prestart_status,
      bool is_browser_startup_complete,
      StatusCallback callback,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  void StartWorkerInternal();

  // Returns true if the service worker is known to have work to do because the
  // browser process initiated a request to the service worker which isn't done
  // yet.
  //
  // Note that this method may return false even when the service worker still
  // has work to do; clients may dispatch events to the service worker directly.
  // You can ensure no inflight requests exist when HasWorkInBrowser() returns
  // false and |worker_is_idle_on_renderer_| is true, or when the worker is
  // stopped.
  bool HasWorkInBrowser() const;

  // Callback function for simple events dispatched through mojo interface
  // blink::mojom::ServiceWorker. Use CreateSimpleEventCallback() to
  // create a callback for a given |request_id|.
  void OnSimpleEventFinished(int request_id,
                             blink::mojom::ServiceWorkerEventStatus status);

  // The timeout timer periodically calls OnTimeoutTimer, which stops the worker
  // if it is excessively idle or unresponsive to ping.
  void StartTimeoutTimer();
  void StopTimeoutTimer();
  void OnTimeoutTimer();
  void SetTimeoutTimerInterval(base::TimeDelta interval);

  // Called by ServiceWorkerPingController for ping protocol.
  void PingWorker();
  void OnPingTimeout();

  // RecordStartWorkerResult is added as a start callback by StartTimeoutTimer
  // and records metrics about startup.
  void RecordStartWorkerResult(ServiceWorkerMetrics::EventType purpose,
                               Status prestart_status,
                               int trace_id,
                               bool is_browser_startup_complete,
                               blink::ServiceWorkerStatusCode status);

  // The caller of MaybeTimeoutRequest must increase reference count of |this|
  // to avoid it deleted during the execution.
  bool MaybeTimeoutRequest(const InflightRequestTimeoutInfo& info);
  void SetAllRequestExpirations(const base::TimeTicks& expiration_time);

  // Sets |stale_time_| if this worker is stale, causing an update to eventually
  // occur once the worker stops or is running too long.
  void MarkIfStale();

  void FoundRegistrationForUpdate(
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);

  void OnStoppedInternal(blink::EmbeddedWorkerStatus old_status);

  // Fires and clears all start callbacks.
  void FinishStartWorker(blink::ServiceWorkerStatusCode status);

  // Removes any pending external request that has Uuid of |request_uuid|.
  void CleanUpExternalRequest(const base::Uuid& request_uuid,
                              blink::ServiceWorkerStatusCode status);

  // Called if no inflight events exist on the browser process. Triggers
  // OnNoWork() if the renderer-side idle timeout has been fired or the worker
  // has been stopped.
  void OnNoWorkInBrowser();

  bool IsStartWorkerAllowed() const;

  void NotifyControlleeAdded(const std::string& uuid,
                             const ServiceWorkerClientInfo& info);
  void NotifyControlleeRemoved(const std::string& uuid);
  void NotifyControlleeNavigationCommitted(
      const std::string& uuid,
      GlobalRenderFrameHostId render_frame_host_id);
  void NotifyWindowOpened(const GURL& script_url, const GURL& url);
  void NotifyClientNavigated(const GURL& script_url, const GURL& url);

  void GetClientOnExecutionReady(const std::string& client_uuid,
                                 GetClientCallback callback,
                                 bool success);

  const int64_t version_id_;
  const int64_t registration_id_;
  const GURL script_url_;
  // `key_` is computed from `scope_`. Warning: The `script_url_`'s origin
  // and `key_` may be different in some scenarios e.g.
  // --disable-web-security.
  const blink::StorageKey key_;
  const GURL scope_;
  // A service worker has an associated type which is either
  // "classic" or "module". Unless stated otherwise, it is "classic".
  // https://w3c.github.io/ServiceWorker/#dfn-type
  const blink::mojom::ScriptType script_type_;
  std::optional<FetchHandlerType> fetch_handler_type_;

  FetchHandlerBypassOption fetch_handler_bypass_option_ =
      FetchHandlerBypassOption::kDefault;

  // Whether the associated script has any HID event handlers after the script
  // is evaluated.
  bool has_hid_event_handlers_ = false;

  // Whether the associated script has any USB event handlers after the script
  // is evaluated.
  bool has_usb_event_handlers_ = false;

  // The source of truth for navigation preload state is the
  // ServiceWorkerRegistration. |navigation_preload_state_| is essentially a
  // cached value because it must be looked up quickly and a live registration
  // doesn't necessarily exist whenever there is a live version.
  blink::mojom::NavigationPreloadState navigation_preload_state_;

  // A copy of ServiceWorkerRegistration::status(). Cached for the same reason
  // as `navigation_preload_state_`: A live registration doesn't necessarily
  // exist whenever there is a live version, but `registation_status_` is needed
  // to check if the registration is already deleted or not.
  ServiceWorkerRegistration::Status registration_status_;

  // A copy of ServiceWorkerRegistration::ancestor_frame_type(). Cached for the
  // same reason as `navigation_preload_state_`: A live registration doesn't
  // necessarily exist whenever there is a live version, but
  // `ancestor_frame_type_` is needed to check if it was registered in fenced
  // frame or not.
  const blink::mojom::AncestorFrameType ancestor_frame_type_;

  // The client security state passed to the network URL loader factory used to
  // fetch service worker subresources.
  //
  // For brand new service workers fetched from the network, this is set by
  // `ServiceWorkerNewScriptLoader` once the script headers have been fetched.
  // For service worker script updates, this is set by `PrepareForUpdate()` once
  // the updated script headers have been fetched.
  // For service workers loaded from disk, this is restored from disk.
  //
  // TODO(crbug.com/40056874): Set all of this, not just COEP, on script
  // updates.
  // TODO(crbug.com/40056874): Persist all of this to disk, not just the
  // COEP field.
  network::mojom::ClientSecurityStatePtr client_security_state_;

  Status status_ = NEW;
  std::unique_ptr<EmbeddedWorkerInstance> embedded_worker_;
  // True if endpoint() is ready to dispatch events, which means
  // InitializeGlobalScope() is already called.
  bool is_endpoint_ready_ = false;
  // True while running `start_callbacks_`. When true, StartWorker() will be
  // delayed until all `start_callbacks_` are executed. This prevents callbacks
  // from calling nested StartWorker(). A nested StartWorker() call makes `this`
  // enter an invalid state (i.e., `start_callbacks_` is empty even when
  // `running_status()` is STARTING) so it should not happen.
  // TODO(crbug.com/40739069): Figure out a way to disallow a callback to
  // re-enter StartWorker().
  bool is_running_start_callbacks_ = false;
  std::vector<StatusCallback> start_callbacks_;
  std::vector<StatusCallback> warm_up_callbacks_;
  std::vector<base::OnceClosure> stop_callbacks_;
  std::vector<base::OnceClosure> status_change_callbacks_;

  // Holds in-flight requests, including requests due to outstanding push,
  // fetch, sync, etc. events.
  base::IDMap<std::unique_ptr<InflightRequest>> inflight_requests_;

  // Keeps track of in-flight requests for timeout purposes. Requests are sorted
  // by their expiration time (soonest to expire at the beginning of the
  // set). The timeout timer periodically checks |request_timeouts_| for entries
  // that should time out.
  std::set<InflightRequestTimeoutInfo> request_timeouts_;

  // Container for pending external requests for this service worker.
  // (key, value): (request uuid, request id).
  using RequestUUIDToRequestIDMap = std::map<base::Uuid, int>;
  RequestUUIDToRequestIDMap external_request_uuid_to_request_id_;

  // External request infos that were issued before this worker reached RUNNING.
  // Info contains UUID and timeout type.
  std::map<base::Uuid, ServiceWorkerExternalRequestTimeoutType>
      pending_external_requests_;

  // Connected to ServiceWorkerContextClient while the worker is running.
  mojo::Remote<blink::mojom::ServiceWorker> service_worker_remote_;

  // Connection to the controller service worker.
  // |controller_receiver_| is non-null only when the |remote_controller_| is
  // requested before the worker is started, it is passed to the worker (and
  // becomes null) once it's started.
  mojo::Remote<blink::mojom::ControllerServiceWorker> remote_controller_;
  mojo::PendingReceiver<blink::mojom::ControllerServiceWorker>
      controller_receiver_;

  std::unique_ptr<ServiceWorkerInstalledScriptsSender>
      installed_scripts_sender_;

  std::vector<SkipWaitingCallback> pending_skip_waiting_requests_;
  base::TimeTicks skip_waiting_time_;
  base::TimeTicks no_controllees_time_;

  mojo::AssociatedReceiver<blink::mojom::ServiceWorkerHost> receiver_{this};
  mojo::AssociatedReceiver<blink::mojom::AssociatedInterfaceProvider>
      associated_interface_receiver_{this};

  // Set to true if the worker has no inflight events and the idle timer has
  // been triggered. Set back to false if another event starts since the worker
  // is no longer idle.
  bool worker_is_idle_on_renderer_ = true;

  // Set to true when the worker needs to be terminated as soon as possible
  // (e.g. activation).
  bool needs_to_be_terminated_asap_ = false;

  // The host for this version's running service worker. |worker_host_| is
  // always valid as long as this version is running.
  std::unique_ptr<content::ServiceWorkerHost> worker_host_;

  // |controllee_map_| and |bfcached_controllee_map_| should not share the same
  // controllee.  ServiceWorkerClient in the controllee maps should be
  // non-null.
  // TODO(crbug.com/40199210): Fix cases where hosts can become nullptr while
  //                          stored in the maps.
  std::map<std::string, base::WeakPtr<ServiceWorkerClient>> controllee_map_;
  std::map<std::string, base::WeakPtr<ServiceWorkerClient>>
      bfcached_controllee_map_;

  // Keeps track of the |client_uuid| of ServiceWorkerClient that is being
  // evicted, and the reason why it is evicted. Once eviction is complete, the
  // entry will be removed.
  // TODO(crbug.com/40657227): Remove this once we fix the crash.
  std::map<std::string, BackForwardCacheMetrics::NotRestoredReason>
      controllees_to_be_evicted_;

  // Will be null while shutting down.
  base::WeakPtr<ServiceWorkerContextCore> context_;
  base::ObserverList<Observer>::Unchecked observers_;
  ServiceWorkerScriptCacheMap script_cache_map_;
  base::OneShotTimer update_timer_;

  // For scheduling Soft Update after main resource requests. See
  // IncrementPendingUpdateHintCount() documentation.
  int pending_update_hint_count_ = 0;

  // Starts running in StartWorker and continues until the worker is stopped.
  base::RepeatingTimer timeout_timer_;
  // Holds the time that the outstanding StartWorker() request started.
  base::TimeTicks start_time_;
  // Holds the time the worker entered STOPPING status. This is also used as a
  // trace event id.
  base::TimeTicks stop_time_;
  // Holds the time the worker was detected as stale and needs updating. We try
  // to update once the worker stops, but will also update if it stays alive too
  // long.
  base::TimeTicks stale_time_;
  // The latest expiration time of all requests that have ever been started. In
  // particular this is not just the maximum of the expiration times of all
  // currently existing requests, but also takes into account the former
  // expiration times of finished requests.
  base::TimeTicks max_request_expiration_time_;

  bool skip_waiting_ = false;
  bool skip_recording_startup_time_ = false;
  bool force_bypass_cache_for_scripts_ = false;
  bool is_update_scheduled_ = false;
  bool in_dtor_ = false;

  // If true, warms up service worker after service worker is stopped.
  // (https://crbug.com/1431792).
  bool will_warm_up_on_stopped_ = false;

  // Populated via network::mojom::URLResponseHead of the main script.
  std::unique_ptr<MainScriptResponse> main_script_response_;

  // DevTools requires each service worker's script receive time, even for
  // the ones that haven't started. However, a ServiceWorkerVersion's field
  // |main_script_http_info_| is not set until starting up. Rather than
  // reading URLResponseHead for all service workers from disk cache and
  // populating |main_script_http_info_| just in order to expose that timestamp,
  // we provide that timestamp here.
  base::Time script_response_time_for_devtools_;

  std::unique_ptr<blink::TrialTokenValidator::FeatureToTokensMap>
      origin_trial_tokens_;

  // If not OK, the reason that StartWorker failed. Used for
  // running |start_callbacks_|.
  blink::ServiceWorkerStatusCode start_worker_status_ =
      blink::ServiceWorkerStatusCode::kOk;

  // The clock used to vend tick time.
  raw_ptr<const base::TickClock> tick_clock_;

  // The clock used for actual (wall clock) time
  const raw_ptr<base::Clock> clock_;

  ServiceWorkerPingController ping_controller_;

  bool stop_when_devtools_detached_ = false;

  bool is_stopping_warmed_up_worker_ = false;

  // This is the set of features that were used up until installation of this
  // version completed, or used during the lifetime of |this|.
  std::set<blink::mojom::WebFeature> used_features_;

  blink::TrialTokenValidator const validator_;

  // Stores the result of byte-to-byte update check for each script.
  std::map<GURL, ServiceWorkerUpdateChecker::ComparedScriptInfo>
      compared_script_info_map_;

  // If this version was created for an update check that found an update,
  // |updated_script_url_| is the URL of the script for which a byte-for-byte
  // change was found. Otherwise, it's the empty GURL.
  GURL updated_script_url_;

  blink::mojom::FetchClientSettingsObjectPtr
      outside_fetch_client_settings_object_;

  // Parameter used for starting a new worker with the worker script loaded in
  // the browser process beforehand. This is valid only when it's a new worker
  // that is going to be registered from now on.
  blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params_;

  mojo::PendingReceiver<blink::mojom::ReportingObserver>
      reporting_observer_receiver_;

  // Lives while the ServiceWorkerVersion is alive.
  // See comments at the definition of storage::mojom::ServiceWorkerVersionRef
  // for more details.
  mojo::Remote<storage::mojom::ServiceWorkerLiveVersionRef> remote_reference_;

  // Identifier for UKM recording in the service worker thread. Stored here so
  // it can be associated with clients' source IDs.
  const ukm::SourceId ukm_source_id_;

  scoped_refptr<PolicyContainerHost> policy_container_host_;

  base::UnguessableToken reporting_source_;

  // The checksum hash string, which is calculated from each checksum string in
  // |script_cache_map_|'s resources. This will be used to decide if the main
  // resource request is bypassed or not in the experiment (crbug.com/1371756).
  // This field should be set before starting the service worker when the
  // service worker starts with an existing version. But the field will be set
  // after the worker has started when there is a change in the script and new
  // version is created.
  std::optional<std::string> sha256_script_checksum_;

  std::unique_ptr<content::ServiceWorkerRouterEvaluator> router_evaluator_;

  std::unique_ptr<blink::AssociatedInterfaceRegistry> associated_registry_;
  std::unique_ptr<blink::AssociatedInterfaceProvider>
      associated_interface_provider_;

  base::WeakPtrFactory<ServiceWorkerVersion> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_VERSION_H_
