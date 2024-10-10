/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope.h"

#include <memory>
#include <utility>

#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/mojom/cookie_manager.mojom-blink.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/notifications/notification.mojom-blink.h"
#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_fetch_response_callback.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_stream_handle.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/subresource_loader_updater.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_v8_value_converter.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/js_based_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_background_fetch_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_content_index_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_notification_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_request_event_init.h"
#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/global_fetch.h"
#include "third_party/blink/renderer/core/frame/reporting_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/request_debug_header_scope.h"
#include "third_party/blink/renderer/core/inspector/worker_inspector_controller.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/loader/worker_resource_timing_notifier_impl.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_classic_script_loader.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_event.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_registration.h"
#include "third_party/blink/renderer/modules/background_fetch/background_fetch_update_ui_event.h"
#include "third_party/blink/renderer/modules/background_sync/periodic_sync_event.h"
#include "third_party/blink/renderer/modules/background_sync/sync_event.h"
#include "third_party/blink/renderer/modules/content_index/content_index_event.h"
#include "third_party/blink/renderer/modules/cookie_store/cookie_change_event.h"
#include "third_party/blink/renderer/modules/cookie_store/extendable_cookie_change_event.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/hid/hid.h"
#include "third_party/blink/renderer/modules/notifications/notification.h"
#include "third_party/blink/renderer/modules/notifications/notification_event.h"
#include "third_party/blink/renderer/modules/payments/abort_payment_event.h"
#include "third_party/blink/renderer/modules/payments/abort_payment_respond_with_observer.h"
#include "third_party/blink/renderer/modules/payments/can_make_payment_event.h"
#include "third_party/blink/renderer/modules/payments/can_make_payment_respond_with_observer.h"
#include "third_party/blink/renderer/modules/payments/payment_event_data_conversion.h"
#include "third_party/blink/renderer/modules/payments/payment_request_event.h"
#include "third_party/blink/renderer/modules/payments/payment_request_respond_with_observer.h"
#include "third_party/blink/renderer/modules/push_messaging/push_event.h"
#include "third_party/blink/renderer/modules/push_messaging/push_message_data.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription_change_event.h"
#include "third_party/blink/renderer/modules/service_worker/cross_origin_resource_policy_checker.h"
#include "third_party/blink/renderer/modules/service_worker/extendable_event.h"
#include "third_party/blink/renderer/modules/service_worker/extendable_message_event.h"
#include "third_party/blink/renderer/modules/service_worker/fetch_event.h"
#include "third_party/blink/renderer/modules/service_worker/fetch_respond_with_observer.h"
#include "third_party/blink/renderer/modules/service_worker/install_event.h"
#include "third_party/blink/renderer/modules/service_worker/respond_with_observer.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_client.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_clients.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope_proxy.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_module_tree_client.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_script_cached_metadata_handler.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_thread.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_window_client.h"
#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"
#include "third_party/blink/renderer/modules/service_worker/web_service_worker_fetch_context_impl.h"
#include "third_party/blink/renderer/modules/webusb/usb.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_response_headers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

constexpr char kServiceWorkerGlobalScopeTraceScope[] =
    "ServiceWorkerGlobalScope";

void DidSkipWaiting(ScriptPromiseResolver<IDLUndefined>* resolver,
                    bool success) {
  // Per spec the promise returned by skipWaiting() can never reject.
  if (!success) {
    resolver->Detach();
    return;
  }
  resolver->Resolve();
}

// Creates a callback which takes an |event_id| and |status|, which calls the
// given event's callback with the given status and removes it from |map|.
template <typename MapType, typename... Args>
ServiceWorkerEventQueue::AbortCallback CreateAbortCallback(MapType* map,
                                                           Args&&... args) {
  return WTF::BindOnce(
      [](MapType* map, Args&&... args, int event_id,
         mojom::blink::ServiceWorkerEventStatus status) {
        auto iter = map->find(event_id);
        CHECK(iter != map->end(), base::NotFatalUntil::M130);
        std::move(iter->value).Run(status, std::forward<Args>(args)...);
        map->erase(iter);
      },
      WTF::Unretained(map), std::forward<Args>(args)...);
}

// Finds an event callback keyed by |event_id| from |map|, and runs the callback
// with |args|. Returns true if the callback was found and called, otherwise
// returns false.
template <typename MapType, typename... Args>
bool RunEventCallback(MapType* map,
                      ServiceWorkerEventQueue* event_queue,
                      int event_id,
                      Args&&... args) {
  auto iter = map->find(event_id);
  // The event may have been aborted.
  if (iter == map->end())
    return false;
  std::move(iter->value).Run(std::forward<Args>(args)...);
  map->erase(iter);
  event_queue->EndEvent(event_id);
  return true;
}

template <typename T>
static std::string MojoEnumToString(T mojo_enum) {
  std::ostringstream oss;
  oss << mojo_enum;
  return oss.str();
}

}  // namespace

ServiceWorkerGlobalScope* ServiceWorkerGlobalScope::Create(
    ServiceWorkerThread* thread,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    std::unique_ptr<ServiceWorkerInstalledScriptsManager>
        installed_scripts_manager,
    mojo::PendingRemote<mojom::blink::CacheStorage> cache_storage_remote,
    base::TimeTicks time_origin,
    const ServiceWorkerToken& service_worker_token) {
#if DCHECK_IS_ON()
  // If the script is being loaded via script streaming, the script is not yet
  // loaded.
  if (installed_scripts_manager && installed_scripts_manager->IsScriptInstalled(
                                       creation_params->script_url)) {
    // CSP headers, referrer policy, and origin trial tokens will be provided by
    // the InstalledScriptsManager in EvaluateClassicScript().
    DCHECK(creation_params->outside_content_security_policies.empty());
    DCHECK_EQ(network::mojom::ReferrerPolicy::kDefault,
              creation_params->referrer_policy);
    DCHECK(creation_params->inherited_trial_features->empty());
  }
#endif  // DCHECK_IS_ON()

  InterfaceRegistry* interface_registry = creation_params->interface_registry;
  return MakeGarbageCollected<ServiceWorkerGlobalScope>(
      std::move(creation_params), thread, std::move(installed_scripts_manager),
      std::move(cache_storage_remote), time_origin, service_worker_token,
      interface_registry);
}

ServiceWorkerGlobalScope::ServiceWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    ServiceWorkerThread* thread,
    std::unique_ptr<ServiceWorkerInstalledScriptsManager>
        installed_scripts_manager,
    mojo::PendingRemote<mojom::blink::CacheStorage> cache_storage_remote,
    base::TimeTicks time_origin,
    const ServiceWorkerToken& service_worker_token,
    InterfaceRegistry* interface_registry)
    : WorkerGlobalScope(std::move(creation_params), thread, time_origin, true),
      interface_registry_(interface_registry),
      installed_scripts_manager_(std::move(installed_scripts_manager)),
      cache_storage_remote_(std::move(cache_storage_remote)),
      token_(service_worker_token) {
  // Create the event queue. At this point its timer is not started. It will be
  // started by DidEvaluateScript().
  //
  // We are using TaskType::kInternalDefault for the idle callback, and it can
  // be paused or throttled. This should work for now because we don't throttle
  // or pause service worker threads, while it may cause not calling idle
  // callback. We need to revisit this once we want to implement pausing
  // service workers, but basically that won't be big problem because we have
  // ping-pong timer and that will kill paused service workers.
  event_queue_ = std::make_unique<ServiceWorkerEventQueue>(
      WTF::BindRepeating(&ServiceWorkerGlobalScope::OnBeforeStartEvent,
                         WrapWeakPersistent(this)),
      WTF::BindRepeating(&ServiceWorkerGlobalScope::OnIdleTimeout,
                         WrapWeakPersistent(this)),
      GetTaskRunner(TaskType::kInternalDefault));

  CoreInitializer::GetInstance().InitServiceWorkerGlobalScope(*this);
}

ServiceWorkerGlobalScope::~ServiceWorkerGlobalScope() = default;

bool ServiceWorkerGlobalScope::ShouldInstallV8Extensions() const {
  return Platform::Current()->AllowScriptExtensionForServiceWorker(
      WebSecurityOrigin(GetSecurityOrigin()));
}

// https://w3c.github.io/ServiceWorker/#update
void ServiceWorkerGlobalScope::FetchAndRunClassicScript(
    const KURL& script_url,
    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params,
    std::unique_ptr<PolicyContainer> policy_container,
    const FetchClientSettingsObjectSnapshot& outside_settings_object,
    WorkerResourceTimingNotifier& outside_resource_timing_notifier,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK(!IsContextPaused());

  // policy_container_host could be null for registration restored from old DB
  if (policy_container)
    SetPolicyContainer(std::move(policy_container));

  if (installed_scripts_manager_) {
    // This service worker is installed. Load and run the installed script.
    LoadAndRunInstalledClassicScript(script_url, stack_id);
    return;
  }

  // Step 9. "Switching on job's worker type, run these substeps with the
  // following options:"
  // "classic: Fetch a classic worker script given job's serialized script url,
  // job's client, "serviceworker", and the to-be-created environment settings
  // object for this service worker."
  auto context_type = mojom::blink::RequestContextType::SERVICE_WORKER;
  auto destination = network::mojom::RequestDestination::kServiceWorker;

  // "To perform the fetch given request, run the following steps:"
  // Step 9.1. "Append `Service-Worker`/`script` to request's header list."
  // Step 9.2. "Set request's cache mode to "no-cache" if any of the following
  // are true:"
  // Step 9.3. "Set request's service-workers mode to "none"."
  // The browser process takes care of these steps.

  // Step 9.4. "If the is top-level flag is unset, then return the result of
  // fetching request."
  // This step makes sense only when the worker type is "module". For classic
  // script fetch, the top-level flag is always set.

  // Step 9.5. "Set request's redirect mode to "error"."
  // The browser process takes care of this step.

  // Step 9.6. "Fetch request, and asynchronously wait to run the remaining
  // steps as part of fetch's process response for the response response."
  WorkerClassicScriptLoader* classic_script_loader =
      MakeGarbageCollected<WorkerClassicScriptLoader>();
  classic_script_loader->LoadTopLevelScriptAsynchronously(
      *this,
      CreateOutsideSettingsFetcher(outside_settings_object,
                                   outside_resource_timing_notifier),
      script_url, std::move(worker_main_script_load_params), context_type,
      destination, network::mojom::RequestMode::kSameOrigin,
      network::mojom::CredentialsMode::kSameOrigin,
      WTF::BindOnce(
          &ServiceWorkerGlobalScope::DidReceiveResponseForClassicScript,
          WrapWeakPersistent(this), WrapPersistent(classic_script_loader)),
      WTF::BindOnce(&ServiceWorkerGlobalScope::DidFetchClassicScript,
                    WrapWeakPersistent(this),
                    WrapPersistent(classic_script_loader), stack_id),
      RejectCoepUnsafeNone(false), {}, CreateUniqueIdentifier());
}

void ServiceWorkerGlobalScope::FetchAndRunModuleScript(
    const KURL& module_url_record,
    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params,
    std::unique_ptr<PolicyContainer> policy_container,
    const FetchClientSettingsObjectSnapshot& outside_settings_object,
    WorkerResourceTimingNotifier& outside_resource_timing_notifier,
    network::mojom::CredentialsMode credentials_mode,
    RejectCoepUnsafeNone reject_coep_unsafe_none) {
  DCHECK(IsContextThread());
  DCHECK(!reject_coep_unsafe_none);

  // policy_container_host could be null for registration restored from old DB
  if (policy_container)
    SetPolicyContainer(std::move(policy_container));

  if (worker_main_script_load_params) {
    SetWorkerMainScriptLoadingParametersForModules(
        std::move(worker_main_script_load_params));
  }
  ModuleScriptCustomFetchType fetch_type =
      installed_scripts_manager_
          ? ModuleScriptCustomFetchType::kInstalledServiceWorker
          : ModuleScriptCustomFetchType::kWorkerConstructor;

  // Count instantiation of a service worker using a module script as a proxy %
  // of page loads use a service worker with a module script.
  CountWebDXFeature(WebDXFeature::kJsModulesServiceWorkers);

  FetchModuleScript(module_url_record, outside_settings_object,
                    outside_resource_timing_notifier,
                    mojom::blink::RequestContextType::SERVICE_WORKER,
                    network::mojom::RequestDestination::kServiceWorker,
                    credentials_mode, fetch_type,
                    MakeGarbageCollected<ServiceWorkerModuleTreeClient>(
                        ScriptController()->GetScriptState()));
}

void ServiceWorkerGlobalScope::Dispose() {
  DCHECK(IsContextThread());
  controller_receivers_.Clear();
  event_queue_.reset();
  service_worker_host_.reset();
  receiver_.reset();
  WorkerGlobalScope::Dispose();
}

InstalledScriptsManager*
ServiceWorkerGlobalScope::GetInstalledScriptsManager() {
  return installed_scripts_manager_.get();
}

void ServiceWorkerGlobalScope::GetAssociatedInterface(
    const String& name,
    mojo::PendingAssociatedReceiver<mojom::blink::AssociatedInterface>
        receiver) {
  mojo::ScopedInterfaceEndpointHandle handle = receiver.PassHandle();
  associated_inteface_registy_.TryBindInterface(name.Utf8(), &handle);
}

void ServiceWorkerGlobalScope::DidEvaluateScript() {
  DCHECK(!did_evaluate_script_);
  did_evaluate_script_ = true;

  int number_of_fetch_handlers =
      NumberOfEventListeners(event_type_names::kFetch);
  if (number_of_fetch_handlers > 1) {
    UseCounter::Count(this, WebFeature::kMultipleFetchHandlersInServiceWorker);
  }
  base::UmaHistogramCounts1000("ServiceWorker.NumberOfRegisteredFetchHandlers",
                               number_of_fetch_handlers);
  event_queue_->Start();
}

AssociatedInterfaceRegistry&
ServiceWorkerGlobalScope::GetAssociatedInterfaceRegistry() {
  return associated_inteface_registy_;
}

void ServiceWorkerGlobalScope::DidReceiveResponseForClassicScript(
    WorkerClassicScriptLoader* classic_script_loader) {
  DCHECK(IsContextThread());
  probe::DidReceiveScriptResponse(this, classic_script_loader->Identifier());
}

// https://w3c.github.io/ServiceWorker/#update
void ServiceWorkerGlobalScope::DidFetchClassicScript(
    WorkerClassicScriptLoader* classic_script_loader,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK(IsContextThread());

  // Step 9. "If the algorithm asynchronously completes with null, then:"
  if (classic_script_loader->Failed()) {
    // Step 9.1. "Invoke Reject Job Promise with job and TypeError."
    // Step 9.2. "If newestWorker is null, invoke Clear Registration algorithm
    // passing registration as its argument."
    // Step 9.3. "Invoke Finish Job with job and abort these steps."
    // The browser process takes care of these steps.
    ReportingProxy().DidFailToFetchClassicScript();
    // Close the worker global scope to terminate the thread.
    close();
    return;
  }
  // The app cache ID is not used.
  ReportingProxy().DidFetchScript();
  probe::ScriptImported(this, classic_script_loader->Identifier(),
                        classic_script_loader->SourceText());

  // Step 10. "If hasUpdatedResources is false, then:"
  //   Step 10.1. "Invoke Resolve Job Promise with job and registration."
  //   Steo 10.2. "Invoke Finish Job with job and abort these steps."
  // Step 11. "Let worker be a new service worker."
  // Step 12. "Set worker's script url to job's script url, worker's script
  // resource to script, worker's type to job's worker type, and worker's
  // script resource map to updatedResourceMap."
  // Step 13. "Append url to worker's set of used scripts."
  // The browser process takes care of these steps.

  // Step 14. "Set worker's script resource's HTTPS state to httpsState."
  // This is done in the constructor of WorkerGlobalScope.

  // Step 15. "Set worker's script resource's referrer policy to
  // referrerPolicy."
  auto referrer_policy = network::mojom::ReferrerPolicy::kDefault;
  if (!classic_script_loader->GetReferrerPolicy().IsNull()) {
    SecurityPolicy::ReferrerPolicyFromHeaderValue(
        classic_script_loader->GetReferrerPolicy(),
        kDoNotSupportReferrerPolicyLegacyKeywords, &referrer_policy);
  }

  // Step 16. "Invoke Run Service Worker algorithm given worker, with the force
  // bypass cache for importscripts flag set if job’s force bypass cache flag
  // is set, and with the following callback steps given evaluationStatus:"
  RunClassicScript(
      classic_script_loader->ResponseURL(), referrer_policy,
      classic_script_loader->GetContentSecurityPolicy()
          ? mojo::Clone(classic_script_loader->GetContentSecurityPolicy()
                            ->GetParsedPolicies())
          : Vector<network::mojom::blink::ContentSecurityPolicyPtr>(),
      classic_script_loader->OriginTrialTokens(),
      classic_script_loader->SourceText(),
      classic_script_loader->ReleaseCachedMetadata(), stack_id);
}

// https://w3c.github.io/ServiceWorker/#run-service-worker-algorithm
void ServiceWorkerGlobalScope::Initialize(
    const KURL& response_url,
    network::mojom::ReferrerPolicy response_referrer_policy,
    Vector<network::mojom::blink::ContentSecurityPolicyPtr> response_csp,
    const Vector<String>* response_origin_trial_tokens) {
  // Step 4.5. "Set workerGlobalScope's url to serviceWorker's script url."
  InitializeURL(response_url);

  // Step 4.6. "Set workerGlobalScope's HTTPS state to serviceWorker's script
  // resource's HTTPS state."
  // This is done in the constructor of WorkerGlobalScope.

  // Step 4.7. "Set workerGlobalScope's referrer policy to serviceWorker's
  // script resource's referrer policy."
  SetReferrerPolicy(response_referrer_policy);

  // This is quoted from the "Content Security Policy" algorithm in the service
  // workers spec:
  // "Whenever a user agent invokes Run Service Worker algorithm with a service
  // worker serviceWorker:
  // - If serviceWorker's script resource was delivered with a
  //   Content-Security-Policy HTTP header containing the value policy, the
  //   user agent must enforce policy for serviceWorker.
  // - If serviceWorker's script resource was delivered with a
  //   Content-Security-Policy-Report-Only HTTP header containing the value
  //   policy, the user agent must monitor policy for serviceWorker."
  InitContentSecurityPolicyFromVector(std::move(response_csp));
  BindContentSecurityPolicyToExecutionContext();

  OriginTrialContext::AddTokens(this, response_origin_trial_tokens);

  // TODO(nhiroki): Clarify mappings between the steps 4.8-4.11 and
  // implementation.

  // This should be called after OriginTrialContext::AddTokens() to install
  // origin trial features in JavaScript's global object.
  ScriptController()->PrepareForEvaluation();
}

void ServiceWorkerGlobalScope::LoadAndRunInstalledClassicScript(
    const KURL& script_url,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK(IsContextThread());

  DCHECK(installed_scripts_manager_);
  DCHECK(installed_scripts_manager_->IsScriptInstalled(script_url));

  // GetScriptData blocks until the script is received from the browser.
  std::unique_ptr<InstalledScriptsManager::ScriptData> script_data =
      installed_scripts_manager_->GetScriptData(script_url);
  if (!script_data) {
    ReportingProxy().DidFailToFetchClassicScript();
    // This will eventually initiate worker thread termination. See
    // ServiceWorkerGlobalScopeProxy::DidCloseWorkerGlobalScope() for details.
    close();
    return;
  }
  ReportingProxy().DidLoadClassicScript();

  auto referrer_policy = network::mojom::ReferrerPolicy::kDefault;
  if (!script_data->GetReferrerPolicy().IsNull()) {
    SecurityPolicy::ReferrerPolicyFromHeaderValue(
        script_data->GetReferrerPolicy(),
        kDoNotSupportReferrerPolicyLegacyKeywords, &referrer_policy);
  }

  RunClassicScript(script_url, referrer_policy,
                   ParseContentSecurityPolicyHeaders(
                       script_data->GetContentSecurityPolicyResponseHeaders()),
                   script_data->CreateOriginTrialTokens().get(),
                   script_data->TakeSourceText(), script_data->TakeMetaData(),
                   stack_id);
}

// https://w3c.github.io/ServiceWorker/#run-service-worker-algorithm
void ServiceWorkerGlobalScope::RunClassicScript(
    const KURL& response_url,
    network::mojom::ReferrerPolicy response_referrer_policy,
    Vector<network::mojom::blink::ContentSecurityPolicyPtr> response_csp,
    const Vector<String>* response_origin_trial_tokens,
    const String& source_code,
    std::unique_ptr<Vector<uint8_t>> cached_meta_data,
    const v8_inspector::V8StackTraceId& stack_id) {
  // Step 4.5-4.11 are implemented in Initialize().
  Initialize(response_url, response_referrer_policy, std::move(response_csp),
             response_origin_trial_tokens);

  // Step 4.12. "Let evaluationStatus be the result of running the classic
  // script script if script is a classic script, otherwise, the result of
  // running the module script script if script is a module script."
  EvaluateClassicScript(response_url, source_code, std::move(cached_meta_data),
                        stack_id);
}

ServiceWorkerClients* ServiceWorkerGlobalScope::clients() {
  if (!clients_)
    clients_ = ServiceWorkerClients::Create();
  return clients_.Get();
}

ServiceWorkerRegistration* ServiceWorkerGlobalScope::registration() {
  return registration_.Get();
}

::blink::ServiceWorker* ServiceWorkerGlobalScope::serviceWorker() {
  return service_worker_.Get();
}

ScriptPromise<IDLUndefined> ServiceWorkerGlobalScope::skipWaiting(
    ScriptState* script_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  // FIXME: short-term fix, see details at:
  // https://codereview.chromium.org/535193002/.
  if (!execution_context)
    return EmptyPromise();

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  GetServiceWorkerHost()->SkipWaiting(
      WTF::BindOnce(&DidSkipWaiting, WrapPersistent(resolver)));
  return resolver->Promise();
}

void ServiceWorkerGlobalScope::BindServiceWorker(
    mojo::PendingReceiver<mojom::blink::ServiceWorker> receiver) {
  DCHECK(IsContextThread());
  DCHECK(!receiver_.is_bound());
  // TODO(falken): Consider adding task types for "the handle fetch task source"
  // and "handle functional event task source" defined in the service worker
  // spec and use them when dispatching events.
  receiver_.Bind(std::move(receiver),
                 GetThread()->GetTaskRunner(TaskType::kInternalDefault));
}

void ServiceWorkerGlobalScope::BindControllerServiceWorker(
    mojo::PendingReceiver<mojom::blink::ControllerServiceWorker> receiver) {
  DCHECK(IsContextThread());
  DCHECK(controller_receivers_.empty());
  // This receiver won't get any FetchEvents because it's used only for
  // bootstrapping, and the actual clients connect over Clone() later. kNone is
  // passed as COEP value as a placeholder.
  //
  // TODO(falken): Consider adding task types for "the handle fetch task source"
  // and "handle functional event task source" defined in the service worker
  // spec and use them when dispatching events.
  controller_receivers_.Add(
      std::move(receiver), /*context=*/nullptr,
      GetThread()->GetTaskRunner(TaskType::kInternalDefault));
}

void ServiceWorkerGlobalScope::OnNavigationPreloadResponse(
    int fetch_event_id,
    std::unique_ptr<WebURLResponse> response,
    mojo::ScopedDataPipeConsumerHandle data_pipe) {
  DCHECK(IsContextThread());
  auto it = pending_preload_fetch_events_.find(fetch_event_id);
  CHECK(it != pending_preload_fetch_events_.end(), base::NotFatalUntil::M130);
  FetchEvent* fetch_event = it->value.Get();
  DCHECK(fetch_event);
  fetch_event->OnNavigationPreloadResponse(ScriptController()->GetScriptState(),
                                           std::move(response),
                                           std::move(data_pipe));
}

void ServiceWorkerGlobalScope::OnNavigationPreloadError(
    int fetch_event_id,
    std::unique_ptr<WebServiceWorkerError> error) {
  DCHECK(IsContextThread());
  FetchEvent* fetch_event = pending_preload_fetch_events_.Take(fetch_event_id);
  DCHECK(fetch_event);
  // Display an error message to the console directly.
  if (error->mode == WebServiceWorkerError::Mode::kShownInConsole) {
    AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kWorker,
        mojom::ConsoleMessageLevel::kError, error->message));
  }
  // Reject the preloadResponse promise.
  fetch_event->OnNavigationPreloadError(ScriptController()->GetScriptState(),
                                        std::move(error));
}

void ServiceWorkerGlobalScope::OnNavigationPreloadComplete(
    int fetch_event_id,
    base::TimeTicks completion_time,
    int64_t encoded_data_length,
    int64_t encoded_body_length,
    int64_t decoded_body_length) {
  DCHECK(IsContextThread());
  FetchEvent* fetch_event = pending_preload_fetch_events_.Take(fetch_event_id);
  DCHECK(fetch_event);
  fetch_event->OnNavigationPreloadComplete(
      this, completion_time, encoded_data_length, encoded_body_length,
      decoded_body_length);
}

std::unique_ptr<ServiceWorkerEventQueue::StayAwakeToken>
ServiceWorkerGlobalScope::CreateStayAwakeToken() {
  return event_queue_->CreateStayAwakeToken();
}

ServiceWorker* ServiceWorkerGlobalScope::GetOrCreateServiceWorker(
    WebServiceWorkerObjectInfo info) {
  if (info.version_id == mojom::blink::kInvalidServiceWorkerVersionId)
    return nullptr;

  auto it = service_worker_objects_.find(info.version_id);
  if (it != service_worker_objects_.end())
    return it->value.Get();

  const int64_t version_id = info.version_id;
  ::blink::ServiceWorker* worker =
      ::blink::ServiceWorker::Create(this, std::move(info));
  service_worker_objects_.Set(version_id, worker);
  return worker;
}

bool ServiceWorkerGlobalScope::AddEventListenerInternal(
    const AtomicString& event_type,
    EventListener* listener,
    const AddEventListenerOptionsResolved* options) {
  if (did_evaluate_script_) {
    String message = String::Format(
        "Event handler of '%s' event must be added on the initial evaluation "
        "of worker script.",
        event_type.Utf8().c_str());
    AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kJavaScript,
        mojom::ConsoleMessageLevel::kWarning, message));
    // Count the update of fetch handlers after the initial evaluation.
    if (event_type == event_type_names::kFetch) {
      UseCounter::Count(
          this, WebFeature::kServiceWorkerFetchHandlerAddedAfterInitialization);
    }
    UseCounter::Count(
        this, WebFeature::kServiceWorkerEventHandlerAddedAfterInitialization);
  }
  return WorkerGlobalScope::AddEventListenerInternal(event_type, listener,
                                                     options);
}

bool ServiceWorkerGlobalScope::FetchClassicImportedScript(
    const KURL& script_url,
    KURL* out_response_url,
    String* out_source_code,
    std::unique_ptr<Vector<uint8_t>>* out_cached_meta_data) {
  // InstalledScriptsManager is used only for starting installed service
  // workers.
  if (installed_scripts_manager_) {
    // All imported scripts must be installed. This is already checked in
    // ServiceWorkerGlobalScope::importScripts().
    DCHECK(installed_scripts_manager_->IsScriptInstalled(script_url));
    std::unique_ptr<InstalledScriptsManager::ScriptData> script_data =
        installed_scripts_manager_->GetScriptData(script_url);
    if (!script_data)
      return false;
    *out_response_url = script_url;
    *out_source_code = script_data->TakeSourceText();
    *out_cached_meta_data = script_data->TakeMetaData();
    // TODO(shimazu): Add appropriate probes for inspector.
    return true;
  }
  // This is a new service worker. Proceed with importing scripts and installing
  // them.
  return WorkerGlobalScope::FetchClassicImportedScript(
      script_url, out_response_url, out_source_code, out_cached_meta_data);
}

ResourceLoadScheduler::ThrottleOptionOverride
ServiceWorkerGlobalScope::GetThrottleOptionOverride() const {
  if (is_installing_ && base::FeatureList::IsEnabled(
                            features::kThrottleInstallingServiceWorker)) {
    return ResourceLoadScheduler::ThrottleOptionOverride::
        kStoppableAsThrottleable;
  }
  return ResourceLoadScheduler::ThrottleOptionOverride::kNone;
}

const AtomicString& ServiceWorkerGlobalScope::InterfaceName() const {
  return event_target_names::kServiceWorkerGlobalScope;
}

void ServiceWorkerGlobalScope::DispatchExtendableEvent(
    Event* event,
    WaitUntilObserver* observer) {
  observer->WillDispatchEvent();
  DispatchEvent(*event);

  // Check if the worker thread is forcibly terminated during the event
  // because of timeout etc.
  observer->DidDispatchEvent(GetThread()->IsForciblyTerminated());
}

void ServiceWorkerGlobalScope::DispatchExtendableEventWithRespondWith(
    Event* event,
    WaitUntilObserver* wait_until_observer,
    RespondWithObserver* respond_with_observer) {
  wait_until_observer->WillDispatchEvent();
  respond_with_observer->WillDispatchEvent();
  DispatchEventResult dispatch_result = DispatchEvent(*event);
  respond_with_observer->DidDispatchEvent(ScriptController()->GetScriptState(),
                                          dispatch_result);
  // false is okay because waitUntil() for events with respondWith() doesn't
  // care about the promise rejection or an uncaught runtime script error.
  wait_until_observer->DidDispatchEvent(false /* event_dispatch_failed */);
}

void ServiceWorkerGlobalScope::Trace(Visitor* visitor) const {
  visitor->Trace(clients_);
  visitor->Trace(registration_);
  visitor->Trace(service_worker_);
  visitor->Trace(service_worker_objects_);
  visitor->Trace(service_worker_host_);
  visitor->Trace(receiver_);
  visitor->Trace(abort_payment_result_callbacks_);
  visitor->Trace(can_make_payment_result_callbacks_);
  visitor->Trace(payment_response_callbacks_);
  visitor->Trace(fetch_response_callbacks_);
  visitor->Trace(pending_preload_fetch_events_);
  visitor->Trace(pending_streaming_upload_fetch_events_);
  visitor->Trace(controller_receivers_);
  visitor->Trace(remote_associated_interfaces_);
  visitor->Trace(associated_interfaces_receiver_);
  WorkerGlobalScope::Trace(visitor);
}

bool ServiceWorkerGlobalScope::HasRelatedFetchEvent(
    const KURL& request_url) const {
  auto it = unresponded_fetch_event_counts_.find(request_url);
  return it != unresponded_fetch_event_counts_.end();
}

bool ServiceWorkerGlobalScope::HasRangeFetchEvent(
    const KURL& request_url) const {
  auto it = unresponded_fetch_event_counts_.find(request_url);
  return it != unresponded_fetch_event_counts_.end() &&
         it->value.range_count > 0;
}

int ServiceWorkerGlobalScope::GetOutstandingThrottledLimit() const {
  return features::kInstallingServiceWorkerOutstandingThrottledLimit.Get();
}

// Note that ServiceWorkers can be for cross-origin iframes, and that it might
// look like an escape from the Permissions-Policy enforced on documents. It is
// safe however, even on platforms without OOPIF  because a ServiceWorker
// controlling a cross-origin iframe would be put in  a different process from
// the page, due to an origin mismatch in their cross-origin isolation.
// See https://crbug.com/1290224 for details.
bool ServiceWorkerGlobalScope::CrossOriginIsolatedCapability() const {
  return Agent::IsCrossOriginIsolated();
}

bool ServiceWorkerGlobalScope::IsIsolatedContext() const {
  // TODO(mkwst): Make a decision here, and spec it.
  return false;
}

void ServiceWorkerGlobalScope::importScripts(const Vector<String>& urls) {
  for (const String& string_url : urls) {
    KURL completed_url = CompleteURL(string_url);
    if (installed_scripts_manager_ &&
        !installed_scripts_manager_->IsScriptInstalled(completed_url)) {
      DCHECK(installed_scripts_manager_->IsScriptInstalled(Url()));
      v8::Isolate* isolate = GetThread()->GetIsolate();
      V8ThrowException::ThrowException(
          isolate,
          V8ThrowDOMException::CreateOrEmpty(
              isolate, DOMExceptionCode::kNetworkError,
              "Failed to import '" + completed_url.ElidedString() +
                  "'. importScripts() of new scripts after service worker "
                  "installation is not allowed."));
      return;
    }
  }
  WorkerGlobalScope::importScripts(urls);
}

CachedMetadataHandler*
ServiceWorkerGlobalScope::CreateWorkerScriptCachedMetadataHandler(
    const KURL& script_url,
    std::unique_ptr<Vector<uint8_t>> meta_data) {
  return MakeGarbageCollected<ServiceWorkerScriptCachedMetadataHandler>(
      this, script_url, std::move(meta_data));
}

void ServiceWorkerGlobalScope::ExceptionThrown(ErrorEvent* event) {
  WorkerGlobalScope::ExceptionThrown(event);
  if (WorkerThreadDebugger* debugger =
          WorkerThreadDebugger::From(GetThread()->GetIsolate()))
    debugger->ExceptionThrown(GetThread(), event);
}

void ServiceWorkerGlobalScope::CountCacheStorageInstalledScript(
    uint64_t script_size,
    uint64_t script_metadata_size) {
  ++cache_storage_installed_script_count_;
  cache_storage_installed_script_total_size_ += script_size;
  cache_storage_installed_script_metadata_total_size_ += script_metadata_size;

  base::UmaHistogramCustomCounts(
      "ServiceWorker.CacheStorageInstalledScript.ScriptSize",
      base::saturated_cast<base::Histogram::Sample>(script_size), 1000, 5000000,
      50);

  if (script_metadata_size) {
    base::UmaHistogramCustomCounts(
        "ServiceWorker.CacheStorageInstalledScript.CachedMetadataSize",
        base::saturated_cast<base::Histogram::Sample>(script_metadata_size),
        1000, 50000000, 50);
  }
}

void ServiceWorkerGlobalScope::DidHandleInstallEvent(
    int install_event_id,
    mojom::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  SetFetchHandlerExistence(HasEventListeners(event_type_names::kFetch)
                               ? FetchHandlerExistence::EXISTS
                               : FetchHandlerExistence::DOES_NOT_EXIST);
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerGlobalScope::DidHandleInstallEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(install_event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));
  GlobalFetch::ScopedFetcher* fetcher = GlobalFetch::ScopedFetcher::From(*this);
  RunEventCallback(&install_event_callbacks_, event_queue_.get(),
                   install_event_id, status, fetcher->FetchCount());
}

void ServiceWorkerGlobalScope::DidHandleActivateEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerGlobalScope::DidHandleActivateEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));
  RunEventCallback(&activate_event_callbacks_, event_queue_.get(), event_id,
                   status);
}

void ServiceWorkerGlobalScope::DidHandleBackgroundFetchAbortEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::DidHandleBackgroundFetchAbortEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));
  RunEventCallback(&background_fetch_abort_event_callbacks_, event_queue_.get(),
                   event_id, status);
}

void ServiceWorkerGlobalScope::DidHandleBackgroundFetchClickEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::DidHandleBackgroundFetchClickEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));
  RunEventCallback(&background_fetch_click_event_callbacks_, event_queue_.get(),
                   event_id, status);
}

void ServiceWorkerGlobalScope::DidHandleBackgroundFetchFailEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status) {
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::DidHandleBackgroundFetchFailEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));
  RunEventCallback(&background_fetch_fail_event_callbacks_, event_queue_.get(),
                   event_id, status);
}

void ServiceWorkerGlobalScope::DidHandleBackgroundFetchSuccessEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::DidHandleBackgroundFetchSuccessEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));
  RunEventCallback(&background_fetched_event_callbacks_, event_queue_.get(),
                   event_id, status);
}

void ServiceWorkerGlobalScope::DidHandleExtendableMessageEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::DidHandleExtendableMessageEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));
  RunEventCallback(&message_event_callbacks_, event_queue_.get(), event_id,
                   status);
}

void ServiceWorkerGlobalScope::RespondToFetchEventWithNoResponse(
    int fetch_event_id,
    FetchEvent* fetch_event,
    const KURL& request_url,
    bool range_request,
    std::optional<network::DataElementChunkedDataPipe> request_body,
    base::TimeTicks event_dispatch_time,
    base::TimeTicks respond_with_settled_time) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::RespondToFetchEventWithNoResponse",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(fetch_event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  // `fetch_response_callbacks_` does not have the entry when the event timed
  // out.
  if (!fetch_response_callbacks_.Contains(fetch_event_id))
    return;
  mojom::blink::ServiceWorkerFetchResponseCallback* response_callback =
      fetch_response_callbacks_.Take(fetch_event_id)->Value().get();

  auto timing = mojom::blink::ServiceWorkerFetchEventTiming::New();
  timing->dispatch_event_time = event_dispatch_time;
  timing->respond_with_settled_time = respond_with_settled_time;

  NoteRespondedToFetchEvent(request_url, range_request);

  if (request_body) {
    pending_streaming_upload_fetch_events_.insert(fetch_event_id, fetch_event);
  }

  response_callback->OnFallback(std::move(request_body), std::move(timing));
}
void ServiceWorkerGlobalScope::OnStreamingUploadCompletion(int fetch_event_id) {
  pending_streaming_upload_fetch_events_.erase(fetch_event_id);
}

void ServiceWorkerGlobalScope::RespondToFetchEvent(
    int fetch_event_id,
    const KURL& request_url,
    bool range_request,
    mojom::blink::FetchAPIResponsePtr response,
    base::TimeTicks event_dispatch_time,
    base::TimeTicks respond_with_settled_time) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerGlobalScope::RespondToFetchEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(fetch_event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  // `fetch_response_callbacks_` does not have the entry when the event timed
  // out.
  if (!fetch_response_callbacks_.Contains(fetch_event_id))
    return;

  mojom::blink::ServiceWorkerFetchResponseCallback* response_callback =
      fetch_response_callbacks_.Take(fetch_event_id)->Value().get();

  auto timing = mojom::blink::ServiceWorkerFetchEventTiming::New();
  timing->dispatch_event_time = event_dispatch_time;
  timing->respond_with_settled_time = respond_with_settled_time;

  NoteRespondedToFetchEvent(request_url, range_request);

  response_callback->OnResponse(std::move(response), std::move(timing));
}

void ServiceWorkerGlobalScope::RespondToFetchEventWithResponseStream(
    int fetch_event_id,
    const KURL& request_url,
    bool range_request,
    mojom::blink::FetchAPIResponsePtr response,
    mojom::blink::ServiceWorkerStreamHandlePtr body_as_stream,
    base::TimeTicks event_dispatch_time,
    base::TimeTicks respond_with_settled_time) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::RespondToFetchEventWithResponseStream",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(fetch_event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  // `fetch_response_callbacks_` does not have the entry when the event timed
  // out.
  if (!fetch_response_callbacks_.Contains(fetch_event_id))
    return;
  mojom::blink::ServiceWorkerFetchResponseCallback* response_callback =
      fetch_response_callbacks_.Take(fetch_event_id)->Value().get();

  auto timing = mojom::blink::ServiceWorkerFetchEventTiming::New();
  timing->dispatch_event_time = event_dispatch_time;
  timing->respond_with_settled_time = respond_with_settled_time;

  NoteRespondedToFetchEvent(request_url, range_request);

  response_callback->OnResponseStream(
      std::move(response), std::move(body_as_stream), std::move(timing));
}

void ServiceWorkerGlobalScope::DidHandleFetchEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  // This TRACE_EVENT is used for perf benchmark to confirm if all of fetch
  // events have completed. (crbug.com/736697)
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerGlobalScope::DidHandleFetchEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));

  // Delete the URLLoaderFactory for the RaceNetworkRequest if it's not used.
  RemoveItemFromRaceNetworkRequests(event_id);

  if (!RunEventCallback(&fetch_event_callbacks_, event_queue_.get(), event_id,
                        status)) {
    // The event may have been aborted. Its response callback also needs to be
    // deleted.
    fetch_response_callbacks_.erase(event_id);
  } else {
    // |fetch_response_callback| should be used before settling a promise for
    // waitUntil().
    DCHECK(!fetch_response_callbacks_.Contains(event_id));
  }
}

void ServiceWorkerGlobalScope::DidHandleNotificationClickEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::DidHandleNotificationClickEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));
  RunEventCallback(&notification_click_event_callbacks_, event_queue_.get(),
                   event_id, status);
}

void ServiceWorkerGlobalScope::DidHandleNotificationCloseEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::DidHandleNotificationCloseEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));
  RunEventCallback(&notification_close_event_callbacks_, event_queue_.get(),
                   event_id, status);
}

void ServiceWorkerGlobalScope::DidHandlePushEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerGlobalScope::DidHandlePushEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));
  RunEventCallback(&push_event_callbacks_, event_queue_.get(), event_id,
                   status);
}

void ServiceWorkerGlobalScope::DidHandlePushSubscriptionChangeEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::DidHandlePushSubscriptionChangeEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));
  RunEventCallback(&push_subscription_change_event_callbacks_,
                   event_queue_.get(), event_id, status);
}

void ServiceWorkerGlobalScope::DidHandleSyncEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerGlobalScope::DidHandleSyncEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));
  RunEventCallback(&sync_event_callbacks_, event_queue_.get(), event_id,
                   status);
}

void ServiceWorkerGlobalScope::DidHandlePeriodicSyncEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerGlobalScope::DidHandlePeriodicSyncEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));
  RunEventCallback(&periodic_sync_event_callbacks_, event_queue_.get(),
                   event_id, status);
}

void ServiceWorkerGlobalScope::RespondToAbortPaymentEvent(
    int event_id,
    bool payment_aborted) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerGlobalScope::RespondToAbortPaymentEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(abort_payment_result_callbacks_.Contains(event_id));
  payments::mojom::blink::PaymentHandlerResponseCallback* result_callback =
      abort_payment_result_callbacks_.Take(event_id)->Value().get();
  result_callback->OnResponseForAbortPayment(payment_aborted);
}

void ServiceWorkerGlobalScope::DidHandleAbortPaymentEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerGlobalScope::DidHandleAbortPaymentEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));
  if (RunEventCallback(&abort_payment_event_callbacks_, event_queue_.get(),
                       event_id, status)) {
    abort_payment_result_callbacks_.erase(event_id);
  }
}

void ServiceWorkerGlobalScope::RespondToCanMakePaymentEvent(
    int event_id,
    payments::mojom::blink::CanMakePaymentResponsePtr response) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerGlobalScope::RespondToCanMakePaymentEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(can_make_payment_result_callbacks_.Contains(event_id));
  payments::mojom::blink::PaymentHandlerResponseCallback* result_callback =
      can_make_payment_result_callbacks_.Take(event_id)->Value().get();
  result_callback->OnResponseForCanMakePayment(std::move(response));
}

void ServiceWorkerGlobalScope::DidHandleCanMakePaymentEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerGlobalScope::DidHandleCanMakePaymentEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));
  if (RunEventCallback(&can_make_payment_event_callbacks_, event_queue_.get(),
                       event_id, status)) {
    can_make_payment_result_callbacks_.erase(event_id);
  }
}

void ServiceWorkerGlobalScope::RespondToPaymentRequestEvent(
    int payment_event_id,
    payments::mojom::blink::PaymentHandlerResponsePtr response) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerGlobalScope::RespondToPaymentRequestEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(payment_event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(payment_response_callbacks_.Contains(payment_event_id));
  payments::mojom::blink::PaymentHandlerResponseCallback* response_callback =
      payment_response_callbacks_.Take(payment_event_id)->Value().get();
  response_callback->OnResponseForPaymentRequest(std::move(response));
}

void ServiceWorkerGlobalScope::DidHandlePaymentRequestEvent(
    int payment_event_id,
    mojom::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerGlobalScope::DidHandlePaymentRequestEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(payment_event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));
  if (RunEventCallback(&payment_request_event_callbacks_, event_queue_.get(),
                       payment_event_id, status)) {
    payment_response_callbacks_.erase(payment_event_id);
  }
}

void ServiceWorkerGlobalScope::DidHandleCookieChangeEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerGlobalScope::DidHandleCookieChangeEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));
  RunEventCallback(&cookie_change_event_callbacks_, event_queue_.get(),
                   event_id, status);
}

void ServiceWorkerGlobalScope::DidHandleContentDeleteEvent(
    int event_id,
    mojom::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerGlobalScope::DidHandleContentDeleteEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_IN, "status", MojoEnumToString(status));
  RunEventCallback(&content_delete_callbacks_, event_queue_.get(), event_id,
                   status);
}

void ServiceWorkerGlobalScope::SetIsInstalling(bool is_installing) {
  is_installing_ = is_installing;
  UpdateFetcherThrottleOptionOverride();
  if (is_installing) {
    // Mark the scheduler as "hidden" to enable network throttling while the
    // service worker is installing.
    if (base::FeatureList::IsEnabled(
            features::kThrottleInstallingServiceWorker)) {
      GetThread()->GetScheduler()->OnLifecycleStateChanged(
          scheduler::SchedulingLifecycleState::kHidden);
    }
    return;
  }

  // Disable any network throttling that was enabled while the service worker
  // was in the installing state.
  if (base::FeatureList::IsEnabled(
          features::kThrottleInstallingServiceWorker)) {
    GetThread()->GetScheduler()->OnLifecycleStateChanged(
        scheduler::SchedulingLifecycleState::kNotThrottled);
  }

  // Installing phase is finished; record the stats for the scripts that are
  // stored in Cache storage during installation.
  base::UmaHistogramCounts1000(
      "ServiceWorker.CacheStorageInstalledScript.Count",
      base::saturated_cast<base::Histogram::Sample>(
          cache_storage_installed_script_count_));
  base::UmaHistogramCustomCounts(
      "ServiceWorker.CacheStorageInstalledScript.ScriptTotalSize",
      base::saturated_cast<base::Histogram::Sample>(
          cache_storage_installed_script_total_size_),
      1000, 50000000, 50);

  if (cache_storage_installed_script_metadata_total_size_) {
    base::UmaHistogramCustomCounts(
        "ServiceWorker.CacheStorageInstalledScript.CachedMetadataTotalSize",
        base::saturated_cast<base::Histogram::Sample>(
            cache_storage_installed_script_metadata_total_size_),
        1000, 50000000, 50);
  }
}

mojo::PendingRemote<mojom::blink::CacheStorage>
ServiceWorkerGlobalScope::TakeCacheStorage() {
  return std::move(cache_storage_remote_);
}

mojom::blink::ServiceWorkerHost*
ServiceWorkerGlobalScope::GetServiceWorkerHost() {
  DCHECK(service_worker_host_.is_bound());
  return service_worker_host_.get();
}

void ServiceWorkerGlobalScope::OnBeforeStartEvent(bool is_offline_event) {
  DCHECK(IsContextThread());
  SetIsOfflineMode(is_offline_event);
}

void ServiceWorkerGlobalScope::OnIdleTimeout() {
  DCHECK(IsContextThread());
  // RequestedTermination() returns true if ServiceWorkerEventQueue agrees
  // we should request the host to terminate this worker now.
  DCHECK(RequestedTermination());
  // We use CrossThreadBindOnce() here because the callback may be destroyed on
  // the main thread if the worker thread has already terminated.
  To<ServiceWorkerGlobalScopeProxy>(ReportingProxy())
      .RequestTermination(
          CrossThreadBindOnce(&ServiceWorkerGlobalScope::OnRequestedTermination,
                              WrapCrossThreadWeakPersistent(this)));
}

void ServiceWorkerGlobalScope::OnRequestedTermination(bool will_be_terminated) {
  DCHECK(IsContextThread());
  // This worker will be terminated soon. Ignore the message.
  if (will_be_terminated)
    return;

  // Push a dummy task to run all of queued tasks. This updates the
  // idle timer too.
  event_queue_->EnqueueNormal(
      event_queue_->NextEventId(),
      WTF::BindOnce(&ServiceWorkerEventQueue::EndEvent,
                    WTF::Unretained(event_queue_.get())),
      base::DoNothing(), std::nullopt);
}

bool ServiceWorkerGlobalScope::RequestedTermination() const {
  DCHECK(IsContextThread());
  return event_queue_->did_idle_timeout();
}

void ServiceWorkerGlobalScope::DispatchExtendableMessageEventInternal(
    int event_id,
    mojom::blink::ExtendableMessageEventPtr event) {
  BlinkTransferableMessage msg = std::move(event->message);
  MessagePortArray* ports =
      MessagePort::EntanglePorts(*this, std::move(msg.ports));
  String origin;
  if (!event->source_origin->IsOpaque())
    origin = event->source_origin->ToString();
  WaitUntilObserver* observer = nullptr;
  Event* event_to_dispatch = nullptr;

  if (event->source_info_for_client) {
    mojom::blink::ServiceWorkerClientInfoPtr client_info =
        std::move(event->source_info_for_client);
    DCHECK(!client_info->client_uuid.empty());
    ServiceWorkerClient* source = nullptr;
    if (client_info->client_type == mojom::ServiceWorkerClientType::kWindow)
      source = MakeGarbageCollected<ServiceWorkerWindowClient>(*client_info);
    else
      source = MakeGarbageCollected<ServiceWorkerClient>(*client_info);
    // TODO(crbug.com/1018092): Factor out these security checks so they aren't
    // duplicated in so many places.
    if (msg.message->IsOriginCheckRequired()) {
      const SecurityOrigin* target_origin =
          GetExecutionContext()->GetSecurityOrigin();
      if (!msg.sender_origin ||
          !msg.sender_origin->IsSameOriginWith(target_origin)) {
        observer = MakeGarbageCollected<WaitUntilObserver>(
            this, WaitUntilObserver::kMessageerror, event_id);
        event_to_dispatch = ExtendableMessageEvent::CreateError(
            origin, ports, source, observer);
      }
    }
    if (!event_to_dispatch) {
      if (!msg.locked_to_sender_agent_cluster ||
          GetExecutionContext()->IsSameAgentCluster(
              msg.sender_agent_cluster_id)) {
        observer = MakeGarbageCollected<WaitUntilObserver>(
            this, WaitUntilObserver::kMessage, event_id);
        event_to_dispatch = ExtendableMessageEvent::Create(
            std::move(msg.message), origin, ports, source, observer);
      } else {
        observer = MakeGarbageCollected<WaitUntilObserver>(
            this, WaitUntilObserver::kMessageerror, event_id);
        event_to_dispatch = ExtendableMessageEvent::CreateError(
            origin, ports, source, observer);
      }
    }
    DispatchExtendableEvent(event_to_dispatch, observer);
    return;
  }

  DCHECK_NE(event->source_info_for_service_worker->version_id,
            mojom::blink::kInvalidServiceWorkerVersionId);
  ::blink::ServiceWorker* source = ::blink::ServiceWorker::From(
      GetExecutionContext(), std::move(event->source_info_for_service_worker));
  // TODO(crbug.com/1018092): Factor out these security checks so they aren't
  // duplicated in so many places.
  if (msg.message->IsOriginCheckRequired()) {
    const SecurityOrigin* target_origin =
        GetExecutionContext()->GetSecurityOrigin();
    if (!msg.sender_origin ||
        !msg.sender_origin->IsSameOriginWith(target_origin)) {
      observer = MakeGarbageCollected<WaitUntilObserver>(
          this, WaitUntilObserver::kMessageerror, event_id);
      event_to_dispatch =
          ExtendableMessageEvent::CreateError(origin, ports, source, observer);
    }
  }
  if (!event_to_dispatch) {
    DCHECK(!msg.locked_to_sender_agent_cluster || msg.sender_agent_cluster_id);
    if (!msg.locked_to_sender_agent_cluster ||
        GetExecutionContext()->IsSameAgentCluster(
            msg.sender_agent_cluster_id)) {
      observer = MakeGarbageCollected<WaitUntilObserver>(
          this, WaitUntilObserver::kMessage, event_id);
      event_to_dispatch = ExtendableMessageEvent::Create(
          std::move(msg.message), origin, ports, source, observer);
    } else {
      observer = MakeGarbageCollected<WaitUntilObserver>(
          this, WaitUntilObserver::kMessageerror, event_id);
      event_to_dispatch =
          ExtendableMessageEvent::CreateError(origin, ports, source, observer);
    }
  }
  DispatchExtendableEvent(event_to_dispatch, observer);
}

void ServiceWorkerGlobalScope::AbortCallbackForFetchEvent(
    int event_id,
    mojom::blink::ServiceWorkerEventStatus status) {
  // Discard a callback for an inflight respondWith() if it still exists.
  auto response_callback_iter = fetch_response_callbacks_.find(event_id);
  if (response_callback_iter != fetch_response_callbacks_.end()) {
    response_callback_iter->value->TakeValue().reset();
    fetch_response_callbacks_.erase(response_callback_iter);
  }
  RemoveItemFromRaceNetworkRequests(event_id);

  // Run the event callback with the error code.
  auto event_callback_iter = fetch_event_callbacks_.find(event_id);
  std::move(event_callback_iter->value).Run(status);
  fetch_event_callbacks_.erase(event_callback_iter);
}

void ServiceWorkerGlobalScope::StartFetchEvent(
    mojom::blink::DispatchFetchEventParamsPtr params,
    base::WeakPtr<CrossOriginResourcePolicyChecker> corp_checker,
    std::optional<base::TimeTicks> created_time,
    int event_id) {
  DCHECK(IsContextThread());
  if (created_time.has_value()) {
    RecordQueuingTime(created_time.value());
  }

  // This TRACE_EVENT is used for perf benchmark to confirm if all of fetch
  // events have completed. (crbug.com/736697)
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerGlobalScope::DispatchFetchEventInternal",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT, "url",
      params->request->url.ElidedString().Utf8());

  // Set up for navigation preload (FetchEvent#preloadResponse) if needed.
  bool navigation_preload_sent = !!params->preload_url_loader_client_receiver;
  if (navigation_preload_sent) {
    To<ServiceWorkerGlobalScopeProxy>(ReportingProxy())
        .SetupNavigationPreload(
            event_id, params->request->url,
            std::move(params->preload_url_loader_client_receiver));
  }

  ScriptState::Scope scope(ScriptController()->GetScriptState());
  auto* wait_until_observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kFetch, event_id);
  auto* respond_with_observer = MakeGarbageCollected<FetchRespondWithObserver>(
      this, event_id, std::move(corp_checker), *params->request,
      wait_until_observer);
  FetchEventInit* event_init = FetchEventInit::Create();
  event_init->setCancelable(true);
  // Note on how clientId / resultingClientID works.
  //
  // Legacy behavior:
  // main resource load -> only resultingClientId.
  // sub resource load -> only clientId.
  // worker script load -> only clientId. (treated as subresource)
  // * PlzDecicatedWorker makes this as main resource load.
  //   We should fix this.
  //
  // Expected behavior:
  // main resource load -> clientId and resultingClientId.
  // sub resource load -> only clientId.
  // worker script load -> clientId and resultingClientId.
  //                       (treated as main resource)
  // * We need to plumb a proper client ID to realize this.
  if (base::FeatureList::IsEnabled(
          features::kServiceWorkerClientIdAlignedWithSpec)) {
    // TODO(crbug.com/1520512): set the meaningful client_id for main resource.
    event_init->setClientId(params->client_id);
    event_init->setResultingClientId(params->request->is_main_resource_load
                                         ? params->resulting_client_id
                                         : String());
  } else {
    bool is_main_resource_load = params->request->is_main_resource_load;
    if (is_main_resource_load &&
        params->request->destination ==
            network::mojom::RequestDestination::kWorker) {
      CHECK(base::FeatureList::IsEnabled(features::kPlzDedicatedWorker));
      is_main_resource_load = false;
    }
    event_init->setClientId(is_main_resource_load ? String()
                                                  : params->client_id);
    event_init->setResultingClientId(
        is_main_resource_load ? params->resulting_client_id : String());
  }
  event_init->setIsReload(params->request->is_reload);

  mojom::blink::FetchAPIRequest& fetch_request = *params->request;
  auto stack_string = fetch_request.devtools_stack_id;

  NoteNewFetchEvent(fetch_request);

  if (params->race_network_request_loader_factory &&
      params->request->service_worker_race_network_request_token) {
    InsertNewItemToRaceNetworkRequests(
        event_id,
        params->request->service_worker_race_network_request_token.value(),
        std::move(params->race_network_request_loader_factory),
        params->request->url);
  }

  Request* request = Request::Create(
      ScriptController()->GetScriptState(), std::move(params->request),
      Request::ForServiceWorkerFetchEvent::kTrue);
  request->getHeaders()->SetGuard(Headers::kImmutableGuard);
  event_init->setRequest(request);

  ScriptState* script_state = ScriptController()->GetScriptState();
  FetchEvent* fetch_event = MakeGarbageCollected<FetchEvent>(
      script_state, event_type_names::kFetch, event_init, respond_with_observer,
      wait_until_observer, navigation_preload_sent);
  respond_with_observer->SetEvent(fetch_event);

  if (navigation_preload_sent) {
    // Keep |fetchEvent| until OnNavigationPreloadComplete() or
    // onNavigationPreloadError() will be called.
    pending_preload_fetch_events_.insert(event_id, fetch_event);
  }

  RequestDebugHeaderScope debug_header_scope(this, stack_string);
  DispatchExtendableEventWithRespondWith(fetch_event, wait_until_observer,
                                         respond_with_observer);
}

void ServiceWorkerGlobalScope::SetFetchHandlerExistence(
    FetchHandlerExistence fetch_handler_existence) {
  DCHECK(IsContextThread());
  if (fetch_handler_existence == FetchHandlerExistence::EXISTS) {
    GetThread()->GetWorkerBackingThread().SetForegrounded();
  }
}

void ServiceWorkerGlobalScope::DispatchFetchEventForSubresource(
    mojom::blink::DispatchFetchEventParamsPtr params,
    mojo::PendingRemote<mojom::blink::ServiceWorkerFetchResponseCallback>
        response_callback,
    DispatchFetchEventForSubresourceCallback callback) {
  DCHECK(IsContextThread());
  TRACE_EVENT2("ServiceWorker",
               "ServiceWorkerGlobalScope::DispatchFetchEventForSubresource",
               "url", params->request->url.ElidedString().Utf8(), "queued",
               RequestedTermination() ? "true" : "false");
  base::WeakPtr<CrossOriginResourcePolicyChecker> corp_checker =
      controller_receivers_.current_context()->GetWeakPtr();

  const int event_id = event_queue_->NextEventId();
  fetch_event_callbacks_.Set(event_id, std::move(callback));
  HeapMojoRemote<mojom::blink::ServiceWorkerFetchResponseCallback> remote(this);
  remote.Bind(std::move(response_callback),
              GetThread()->GetTaskRunner(TaskType::kNetworking));
  fetch_response_callbacks_.Set(event_id, WrapDisallowNew(std::move(remote)));

  if (RequestedTermination()) {
    event_queue_->EnqueuePending(
        event_id,
        WTF::BindOnce(&ServiceWorkerGlobalScope::StartFetchEvent,
                      WrapWeakPersistent(this), std::move(params),
                      std::move(corp_checker), base::TimeTicks::Now()),
        WTF::BindOnce(&ServiceWorkerGlobalScope::AbortCallbackForFetchEvent,
                      WrapWeakPersistent(this)),
        std::nullopt);
  } else {
    event_queue_->EnqueueNormal(
        event_id,
        WTF::BindOnce(&ServiceWorkerGlobalScope::StartFetchEvent,
                      WrapWeakPersistent(this), std::move(params),
                      std::move(corp_checker), base::TimeTicks::Now()),
        WTF::BindOnce(&ServiceWorkerGlobalScope::AbortCallbackForFetchEvent,
                      WrapWeakPersistent(this)),
        std::nullopt);
  }
}

void ServiceWorkerGlobalScope::Clone(
    mojo::PendingReceiver<mojom::blink::ControllerServiceWorker> receiver,
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<
        network::mojom::blink::CrossOriginEmbedderPolicyReporter>
        coep_reporter) {
  DCHECK(IsContextThread());
  auto checker = std::make_unique<CrossOriginResourcePolicyChecker>(
      cross_origin_embedder_policy, std::move(coep_reporter));

  controller_receivers_.Add(
      std::move(receiver), std::move(checker),
      GetThread()->GetTaskRunner(TaskType::kInternalDefault));
}

void ServiceWorkerGlobalScope::InitializeGlobalScope(
    mojo::PendingAssociatedRemote<mojom::blink::ServiceWorkerHost>
        service_worker_host,
    mojo::PendingAssociatedRemote<mojom::blink::AssociatedInterfaceProvider>
        associated_interfaces_from_browser,
    mojo::PendingAssociatedReceiver<mojom::blink::AssociatedInterfaceProvider>
        associated_interfaces_to_browser,
    mojom::blink::ServiceWorkerRegistrationObjectInfoPtr registration_info,
    mojom::blink::ServiceWorkerObjectInfoPtr service_worker_info,
    mojom::blink::FetchHandlerExistence fetch_hander_existence,
    mojo::PendingReceiver<mojom::blink::ReportingObserver>
        reporting_observer_receiver,
    mojom::blink::AncestorFrameType ancestor_frame_type,
    const blink::BlinkStorageKey& storage_key) {
  DCHECK(IsContextThread());
  DCHECK(!global_scope_initialized_);

  DCHECK(service_worker_host.is_valid());
  DCHECK(!service_worker_host_.is_bound());
  service_worker_host_.Bind(std::move(service_worker_host),
                            GetTaskRunner(TaskType::kInternalDefault));

  remote_associated_interfaces_.Bind(
      std::move(associated_interfaces_from_browser),
      GetTaskRunner(TaskType::kInternalDefault));
  associated_interfaces_receiver_.Bind(
      std::move(associated_interfaces_to_browser),
      GetTaskRunner(TaskType::kInternalDefault));

  // Set ServiceWorkerGlobalScope#registration.
  DCHECK_NE(registration_info->registration_id,
            mojom::blink::kInvalidServiceWorkerRegistrationId);
  DCHECK(registration_info->host_remote.is_valid());
  DCHECK(registration_info->receiver.is_valid());
  registration_ = MakeGarbageCollected<ServiceWorkerRegistration>(
      GetExecutionContext(), std::move(registration_info));

  // Set ServiceWorkerGlobalScope#serviceWorker.
  DCHECK_NE(service_worker_info->version_id,
            mojom::blink::kInvalidServiceWorkerVersionId);
  DCHECK(service_worker_info->host_remote.is_valid());
  DCHECK(service_worker_info->receiver.is_valid());
  service_worker_ = ::blink::ServiceWorker::From(
      GetExecutionContext(), std::move(service_worker_info));

  SetFetchHandlerExistence(fetch_hander_existence);

  ancestor_frame_type_ = ancestor_frame_type;

  if (reporting_observer_receiver) {
    ReportingContext::From(this)->Bind(std::move(reporting_observer_receiver));
  }

  global_scope_initialized_ = true;
  if (!pause_evaluation_)
    ReadyToRunWorkerScript();

  storage_key_ = storage_key;
}

void ServiceWorkerGlobalScope::PauseEvaluation() {
  DCHECK(IsContextThread());
  DCHECK(!global_scope_initialized_);
  DCHECK(!pause_evaluation_);
  pause_evaluation_ = true;
}

void ServiceWorkerGlobalScope::ResumeEvaluation() {
  DCHECK(IsContextThread());
  DCHECK(pause_evaluation_);
  pause_evaluation_ = false;
  if (global_scope_initialized_)
    ReadyToRunWorkerScript();
}

void ServiceWorkerGlobalScope::DispatchInstallEvent(
    DispatchInstallEventCallback callback) {
  DCHECK(IsContextThread());
  const int event_id = event_queue_->NextEventId();
  install_event_callbacks_.Set(event_id, std::move(callback));

  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartInstallEvent,
                    WrapWeakPersistent(this)),
      WTF::BindOnce(&ServiceWorkerGlobalScope::AbortInstallEvent,
                    WrapWeakPersistent(this)),
      std::nullopt);
}

void ServiceWorkerGlobalScope::AbortInstallEvent(
    int event_id,
    mojom::blink::ServiceWorkerEventStatus status) {
  DCHECK(IsContextThread());
  auto iter = install_event_callbacks_.find(event_id);
  CHECK(iter != install_event_callbacks_.end(), base::NotFatalUntil::M130);
  GlobalFetch::ScopedFetcher* fetcher = GlobalFetch::ScopedFetcher::From(*this);
  std::move(iter->value).Run(status, fetcher->FetchCount());
  install_event_callbacks_.erase(iter);
}

void ServiceWorkerGlobalScope::StartInstallEvent(int event_id) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerGlobalScope::DispatchInstallEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  auto* observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kInstall, event_id);
  Event* event =
      InstallEvent::Create(event_type_names::kInstall,
                           ExtendableEventInit::Create(), event_id, observer);
  SetIsInstalling(true);
  DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScope::DispatchActivateEvent(
    DispatchActivateEventCallback callback) {
  DCHECK(IsContextThread());
  const int event_id = event_queue_->NextEventId();
  activate_event_callbacks_.Set(event_id, std::move(callback));

  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartActivateEvent,
                    WrapWeakPersistent(this)),
      CreateAbortCallback(&activate_event_callbacks_), std::nullopt);
}

void ServiceWorkerGlobalScope::StartActivateEvent(int event_id) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerGlobalScope::DispatchActivateEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  auto* observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kActivate, event_id);
  Event* event = ExtendableEvent::Create(
      event_type_names::kActivate, ExtendableEventInit::Create(), observer);
  DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScope::DispatchBackgroundFetchAbortEvent(
    mojom::blink::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchAbortEventCallback callback) {
  DCHECK(IsContextThread());
  const int event_id = event_queue_->NextEventId();
  background_fetch_abort_event_callbacks_.Set(event_id, std::move(callback));

  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartBackgroundFetchAbortEvent,
                    WrapWeakPersistent(this), std::move(registration)),
      CreateAbortCallback(&background_fetch_abort_event_callbacks_),
      std::nullopt);
}

void ServiceWorkerGlobalScope::StartBackgroundFetchAbortEvent(
    mojom::blink::BackgroundFetchRegistrationPtr registration,
    int event_id) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::DispatchBackgroundFetchAbortEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  auto* observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kBackgroundFetchAbort, event_id);
  ScriptState* script_state = ScriptController()->GetScriptState();

  // Do not remove this, |scope| is needed by
  // BackgroundFetchEvent::Create which eventually calls ToV8.
  ScriptState::Scope scope(script_state);

  BackgroundFetchEventInit* init = BackgroundFetchEventInit::Create();
  init->setRegistration(MakeGarbageCollected<BackgroundFetchRegistration>(
      registration_, std::move(registration)));

  BackgroundFetchEvent* event = BackgroundFetchEvent::Create(
      event_type_names::kBackgroundfetchabort, init, observer);

  DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScope::DispatchBackgroundFetchClickEvent(
    mojom::blink::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchClickEventCallback callback) {
  DCHECK(IsContextThread());
  const int event_id = event_queue_->NextEventId();
  background_fetch_click_event_callbacks_.Set(event_id, std::move(callback));

  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartBackgroundFetchClickEvent,
                    WrapWeakPersistent(this), std::move(registration)),
      CreateAbortCallback(&background_fetch_click_event_callbacks_),
      std::nullopt);
}

void ServiceWorkerGlobalScope::StartBackgroundFetchClickEvent(
    mojom::blink::BackgroundFetchRegistrationPtr registration,
    int event_id) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::DispatchBackgroundFetchClickEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  auto* observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kBackgroundFetchClick, event_id);

  BackgroundFetchEventInit* init = BackgroundFetchEventInit::Create();
  init->setRegistration(MakeGarbageCollected<BackgroundFetchRegistration>(
      registration_, std::move(registration)));

  BackgroundFetchEvent* event = BackgroundFetchEvent::Create(
      event_type_names::kBackgroundfetchclick, init, observer);

  DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScope::DispatchBackgroundFetchFailEvent(
    mojom::blink::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchFailEventCallback callback) {
  DCHECK(IsContextThread());
  const int event_id = event_queue_->NextEventId();
  background_fetch_fail_event_callbacks_.Set(event_id, std::move(callback));

  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartBackgroundFetchFailEvent,
                    WrapWeakPersistent(this), std::move(registration)),
      CreateAbortCallback(&background_fetch_fail_event_callbacks_),
      std::nullopt);
}

void ServiceWorkerGlobalScope::StartBackgroundFetchFailEvent(
    mojom::blink::BackgroundFetchRegistrationPtr registration,
    int event_id) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::DispatchBackgroundFetchFailEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  auto* observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kBackgroundFetchFail, event_id);

  ScriptState* script_state = ScriptController()->GetScriptState();

  // Do not remove this, |scope| is needed by
  // BackgroundFetchSettledEvent::Create which eventually calls ToV8.
  ScriptState::Scope scope(script_state);

  BackgroundFetchEventInit* init = BackgroundFetchEventInit::Create();
  init->setRegistration(MakeGarbageCollected<BackgroundFetchRegistration>(
      registration_, std::move(registration)));

  BackgroundFetchUpdateUIEvent* event = BackgroundFetchUpdateUIEvent::Create(
      event_type_names::kBackgroundfetchfail, init, observer, registration_);

  DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScope::DispatchBackgroundFetchSuccessEvent(
    mojom::blink::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchSuccessEventCallback callback) {
  DCHECK(IsContextThread());
  const int event_id = event_queue_->NextEventId();
  background_fetched_event_callbacks_.Set(event_id, std::move(callback));

  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartBackgroundFetchSuccessEvent,
                    WrapWeakPersistent(this), std::move(registration)),
      CreateAbortCallback(&background_fetched_event_callbacks_), std::nullopt);
}

void ServiceWorkerGlobalScope::StartBackgroundFetchSuccessEvent(
    mojom::blink::BackgroundFetchRegistrationPtr registration,
    int event_id) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::DispatchBackgroundFetchSuccessEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  auto* observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kBackgroundFetchSuccess, event_id);

  ScriptState* script_state = ScriptController()->GetScriptState();

  // Do not remove this, |scope| is needed by
  // BackgroundFetchSettledEvent::Create which eventually calls ToV8.
  ScriptState::Scope scope(script_state);

  BackgroundFetchEventInit* init = BackgroundFetchEventInit::Create();
  init->setRegistration(MakeGarbageCollected<BackgroundFetchRegistration>(
      registration_, std::move(registration)));

  BackgroundFetchUpdateUIEvent* event = BackgroundFetchUpdateUIEvent::Create(
      event_type_names::kBackgroundfetchsuccess, init, observer, registration_);

  DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScope::DispatchExtendableMessageEvent(
    mojom::blink::ExtendableMessageEventPtr event,
    DispatchExtendableMessageEventCallback callback) {
  DCHECK(IsContextThread());
  const int event_id = event_queue_->NextEventId();
  message_event_callbacks_.Set(event_id, std::move(callback));

  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartExtendableMessageEvent,
                    WrapWeakPersistent(this), std::move(event)),
      CreateAbortCallback(&message_event_callbacks_), std::nullopt);
}

void ServiceWorkerGlobalScope::StartExtendableMessageEvent(
    mojom::blink::ExtendableMessageEventPtr event,
    int event_id) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::DispatchExtendableMessageEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);
  DispatchExtendableMessageEventInternal(event_id, std::move(event));
}

void ServiceWorkerGlobalScope::DispatchFetchEventForMainResource(
    mojom::blink::DispatchFetchEventParamsPtr params,
    mojo::PendingRemote<mojom::blink::ServiceWorkerFetchResponseCallback>
        response_callback,
    DispatchFetchEventForMainResourceCallback callback) {
  DCHECK(IsContextThread());

  const int event_id = event_queue_->NextEventId();
  fetch_event_callbacks_.Set(event_id, std::move(callback));

  HeapMojoRemote<mojom::blink::ServiceWorkerFetchResponseCallback> remote(this);
  remote.Bind(std::move(response_callback),
              GetThread()->GetTaskRunner(TaskType::kNetworking));
  fetch_response_callbacks_.Set(event_id, WrapDisallowNew(std::move(remote)));

  // We can use nullptr as a |corp_checker| for the main resource because it
  // must be the same origin.
  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartFetchEvent,
                    WrapWeakPersistent(this), std::move(params),
                    /*corp_checker=*/nullptr, base::TimeTicks::Now()),
      WTF::BindOnce(&ServiceWorkerGlobalScope::AbortCallbackForFetchEvent,
                    WrapWeakPersistent(this)),
      std::nullopt);
}

void ServiceWorkerGlobalScope::DispatchNotificationClickEvent(
    const String& notification_id,
    mojom::blink::NotificationDataPtr notification_data,
    int action_index,
    const String& reply,
    DispatchNotificationClickEventCallback callback) {
  DCHECK(IsContextThread());

  const int event_id = event_queue_->NextEventId();
  notification_click_event_callbacks_.Set(event_id, std::move(callback));

  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartNotificationClickEvent,
                    WrapWeakPersistent(this), notification_id,
                    std::move(notification_data), action_index, reply),
      CreateAbortCallback(&notification_click_event_callbacks_), std::nullopt);
}

void ServiceWorkerGlobalScope::StartNotificationClickEvent(
    String notification_id,
    mojom::blink::NotificationDataPtr notification_data,
    int action_index,
    String reply,
    int event_id) {
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::DispatchNotificationClickEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  auto* observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kNotificationClick, event_id);
  NotificationEventInit* event_init = NotificationEventInit::Create();
  if (notification_data->actions.has_value() && 0 <= action_index &&
      action_index < static_cast<int>(notification_data->actions->size())) {
    event_init->setAction((*notification_data->actions)[action_index]->action);
  }
  event_init->setNotification(Notification::Create(
      this, notification_id, std::move(notification_data), true /* showing */));
  event_init->setReply(reply);
  Event* event = NotificationEvent::Create(event_type_names::kNotificationclick,
                                           event_init, observer);
  DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScope::DispatchNotificationCloseEvent(
    const String& notification_id,
    mojom::blink::NotificationDataPtr notification_data,
    DispatchNotificationCloseEventCallback callback) {
  DCHECK(IsContextThread());
  const int event_id = event_queue_->NextEventId();
  notification_close_event_callbacks_.Set(event_id, std::move(callback));

  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartNotificationCloseEvent,
                    WrapWeakPersistent(this), notification_id,
                    std::move(notification_data)),
      CreateAbortCallback(&notification_close_event_callbacks_), std::nullopt);
}

void ServiceWorkerGlobalScope::StartNotificationCloseEvent(
    String notification_id,
    mojom::blink::NotificationDataPtr notification_data,
    int event_id) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::DispatchNotificationCloseEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);
  auto* observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kNotificationClose, event_id);
  NotificationEventInit* event_init = NotificationEventInit::Create();
  event_init->setAction(WTF::String());  // initialize as null.
  event_init->setNotification(Notification::Create(this, notification_id,
                                                   std::move(notification_data),
                                                   false /* showing */));
  Event* event = NotificationEvent::Create(event_type_names::kNotificationclose,
                                           event_init, observer);
  DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScope::DispatchPushEvent(
    const String& payload,
    DispatchPushEventCallback callback) {
  DCHECK(IsContextThread());
  const int event_id = event_queue_->NextEventId();
  push_event_callbacks_.Set(event_id, std::move(callback));

  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartPushEvent,
                    WrapWeakPersistent(this), std::move(payload)),
      CreateAbortCallback(&push_event_callbacks_),
      base::Seconds(mojom::blink::kPushEventTimeoutSeconds));
}

void ServiceWorkerGlobalScope::StartPushEvent(String payload, int event_id) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerGlobalScope::DispatchPushEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  auto* observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kPush, event_id);
  Event* event = PushEvent::Create(event_type_names::kPush,
                                   PushMessageData::Create(payload), observer);
  DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScope::DispatchPushSubscriptionChangeEvent(
    mojom::blink::PushSubscriptionPtr old_subscription,
    mojom::blink::PushSubscriptionPtr new_subscription,
    DispatchPushSubscriptionChangeEventCallback callback) {
  DCHECK(IsContextThread());
  const int event_id = event_queue_->NextEventId();
  push_subscription_change_event_callbacks_.Set(event_id, std::move(callback));

  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartPushSubscriptionChangeEvent,
                    WrapWeakPersistent(this), std::move(old_subscription),
                    std::move(new_subscription)),
      CreateAbortCallback(&push_subscription_change_event_callbacks_),
      base::Seconds(mojom::blink::kPushEventTimeoutSeconds));
}

void ServiceWorkerGlobalScope::StartPushSubscriptionChangeEvent(
    mojom::blink::PushSubscriptionPtr old_subscription,
    mojom::blink::PushSubscriptionPtr new_subscription,
    int event_id) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::DispatchPushSubscriptionChangeEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  auto* observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kPushSubscriptionChange, event_id);
  Event* event = PushSubscriptionChangeEvent::Create(
      event_type_names::kPushsubscriptionchange,
      (new_subscription)
          ? PushSubscription::Create(std::move(new_subscription), registration_)
          : nullptr /* new_subscription*/,
      (old_subscription)
          ? PushSubscription::Create(std::move(old_subscription), registration_)
          : nullptr /* old_subscription*/,
      observer);
  DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScope::DispatchSyncEvent(
    const String& tag,
    bool last_chance,
    base::TimeDelta timeout,
    DispatchSyncEventCallback callback) {
  DCHECK(IsContextThread());
  const int event_id = event_queue_->NextEventId();
  sync_event_callbacks_.Set(event_id, std::move(callback));

  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartSyncEvent,
                    WrapWeakPersistent(this), std::move(tag), last_chance),
      CreateAbortCallback(&sync_event_callbacks_), timeout);
}

void ServiceWorkerGlobalScope::StartSyncEvent(String tag,
                                              bool last_chance,
                                              int event_id) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerGlobalScope::DispatchSyncEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  auto* observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kSync, event_id);
  Event* event =
      SyncEvent::Create(event_type_names::kSync, tag, last_chance, observer);
  DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScope::DispatchPeriodicSyncEvent(
    const String& tag,
    base::TimeDelta timeout,
    DispatchPeriodicSyncEventCallback callback) {
  DCHECK(IsContextThread());
  const int event_id = event_queue_->NextEventId();
  periodic_sync_event_callbacks_.Set(event_id, std::move(callback));

  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartPeriodicSyncEvent,
                    WrapWeakPersistent(this), std::move(tag)),
      CreateAbortCallback(&periodic_sync_event_callbacks_), timeout);
}

void ServiceWorkerGlobalScope::StartPeriodicSyncEvent(String tag,
                                                      int event_id) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerGlobalScope::DispatchPeriodicSyncEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  auto* observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kPeriodicSync, event_id);
  Event* event =
      PeriodicSyncEvent::Create(event_type_names::kPeriodicsync, tag, observer);
  DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScope::DispatchAbortPaymentEvent(
    mojo::PendingRemote<payments::mojom::blink::PaymentHandlerResponseCallback>
        response_callback,
    DispatchAbortPaymentEventCallback callback) {
  DCHECK(IsContextThread());
  const int event_id = event_queue_->NextEventId();
  abort_payment_event_callbacks_.Set(event_id, std::move(callback));

  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartAbortPaymentEvent,
                    WrapWeakPersistent(this), std::move(response_callback)),
      CreateAbortCallback(&abort_payment_event_callbacks_), std::nullopt);
}

void ServiceWorkerGlobalScope::StartAbortPaymentEvent(
    mojo::PendingRemote<payments::mojom::blink::PaymentHandlerResponseCallback>
        response_callback,
    int event_id) {
  DCHECK(IsContextThread());
  HeapMojoRemote<payments::mojom::blink::PaymentHandlerResponseCallback> remote(
      this);
  // Payment task need to be processed on the user interaction task
  // runner (TaskType::kUserInteraction).
  // See:
  // https://www.w3.org/TR/payment-request/#user-aborts-the-payment-request-algorithm
  remote.Bind(std::move(response_callback),
              GetThread()->GetTaskRunner(TaskType::kUserInteraction));
  abort_payment_result_callbacks_.Set(event_id,
                                      WrapDisallowNew(std::move(remote)));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerGlobalScope::DispatchAbortPaymentEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  auto* wait_until_observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kAbortPayment, event_id);
  AbortPaymentRespondWithObserver* respond_with_observer =
      MakeGarbageCollected<AbortPaymentRespondWithObserver>(
          this, event_id, wait_until_observer);

  Event* event = AbortPaymentEvent::Create(
      event_type_names::kAbortpayment, ExtendableEventInit::Create(),
      respond_with_observer, wait_until_observer);

  DispatchExtendableEventWithRespondWith(event, wait_until_observer,
                                         respond_with_observer);
}

void ServiceWorkerGlobalScope::DispatchCanMakePaymentEvent(
    payments::mojom::blink::CanMakePaymentEventDataPtr event_data,
    mojo::PendingRemote<payments::mojom::blink::PaymentHandlerResponseCallback>
        response_callback,
    DispatchCanMakePaymentEventCallback callback) {
  DCHECK(IsContextThread());
  const int event_id = event_queue_->NextEventId();
  can_make_payment_event_callbacks_.Set(event_id, std::move(callback));

  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartCanMakePaymentEvent,
                    WrapWeakPersistent(this), std::move(event_data),
                    std::move(response_callback)),
      CreateAbortCallback(&can_make_payment_event_callbacks_), std::nullopt);
}

void ServiceWorkerGlobalScope::StartCanMakePaymentEvent(
    payments::mojom::blink::CanMakePaymentEventDataPtr event_data,
    mojo::PendingRemote<payments::mojom::blink::PaymentHandlerResponseCallback>
        response_callback,
    int event_id) {
  DCHECK(IsContextThread());
  HeapMojoRemote<payments::mojom::blink::PaymentHandlerResponseCallback> remote(
      this);
  // Payment task need to be processed on the user interaction task
  // runner (TaskType::kUserInteraction).
  // See:
  // https://www.w3.org/TR/payment-request/#canmakepayment-method
  remote.Bind(std::move(response_callback),
              GetThread()->GetTaskRunner(TaskType::kUserInteraction));
  can_make_payment_result_callbacks_.Set(event_id,
                                         WrapDisallowNew(std::move(remote)));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerGlobalScope::DispatchCanMakePaymentEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  auto* wait_until_observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kCanMakePayment, event_id);
  CanMakePaymentRespondWithObserver* respond_with_observer =
      MakeGarbageCollected<CanMakePaymentRespondWithObserver>(
          this, event_id, wait_until_observer);

  Event* event = CanMakePaymentEvent::Create(
      event_type_names::kCanmakepayment,
      PaymentEventDataConversion::ToCanMakePaymentEventInit(
          ScriptController()->GetScriptState(), std::move(event_data)),
      respond_with_observer, wait_until_observer);

  DispatchExtendableEventWithRespondWith(event, wait_until_observer,
                                         respond_with_observer);
}

void ServiceWorkerGlobalScope::DispatchPaymentRequestEvent(
    payments::mojom::blink::PaymentRequestEventDataPtr event_data,
    mojo::PendingRemote<payments::mojom::blink::PaymentHandlerResponseCallback>
        response_callback,
    DispatchPaymentRequestEventCallback callback) {
  DCHECK(IsContextThread());
  const int event_id = event_queue_->NextEventId();
  payment_request_event_callbacks_.Set(event_id, std::move(callback));

  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartPaymentRequestEvent,
                    WrapWeakPersistent(this), std::move(event_data),
                    std::move(response_callback)),
      CreateAbortCallback(&payment_request_event_callbacks_), std::nullopt);
}

void ServiceWorkerGlobalScope::StartPaymentRequestEvent(
    payments::mojom::blink::PaymentRequestEventDataPtr event_data,
    mojo::PendingRemote<payments::mojom::blink::PaymentHandlerResponseCallback>
        response_callback,
    int event_id) {
  DCHECK(IsContextThread());
  HeapMojoRemote<payments::mojom::blink::PaymentHandlerResponseCallback> remote(
      this);
  // Payment task need to be processed on the user interaction task
  // runner (TaskType::kUserInteraction).
  // See:
  // https://www.w3.org/TR/payment-request/#user-accepts-the-payment-request-algorithm
  remote.Bind(std::move(response_callback),
              GetThread()->GetTaskRunner(TaskType::kUserInteraction));
  payment_response_callbacks_.Set(event_id, WrapDisallowNew(std::move(remote)));
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerGlobalScope::DispatchPaymentRequestEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  auto* wait_until_observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kPaymentRequest, event_id);
  PaymentRequestRespondWithObserver* respond_with_observer =
      PaymentRequestRespondWithObserver::Create(this, event_id,
                                                wait_until_observer);

  // Update respond_with_observer to check for required information specified in
  // the event_data during response validation.
  if (event_data->payment_options) {
    respond_with_observer->set_should_have_payer_name(
        event_data->payment_options->request_payer_name);
    respond_with_observer->set_should_have_payer_email(
        event_data->payment_options->request_payer_email);
    respond_with_observer->set_should_have_payer_phone(
        event_data->payment_options->request_payer_phone);
    respond_with_observer->set_should_have_shipping_info(
        event_data->payment_options->request_shipping);
  }

  // Count standardized payment method identifiers, such as "basic-card" or
  // "tokenized-card". Omit counting the URL-based payment method identifiers,
  // such as "https://bobpay.xyz".
  if (base::ranges::any_of(
          event_data->method_data,
          [](const payments::mojom::blink::PaymentMethodDataPtr& datum) {
            return datum && !datum->supported_method.StartsWith("http");
          })) {
    UseCounter::Count(
        this, WebFeature::kPaymentHandlerStandardizedPaymentMethodIdentifier);
  }

  mojo::PendingRemote<payments::mojom::blink::PaymentHandlerHost>
      payment_handler_host = std::move(event_data->payment_handler_host);
  Event* event = PaymentRequestEvent::Create(
      event_type_names::kPaymentrequest,
      PaymentEventDataConversion::ToPaymentRequestEventInit(
          ScriptController()->GetScriptState(), std::move(event_data)),
      std::move(payment_handler_host), respond_with_observer,
      wait_until_observer, this);

  DispatchExtendableEventWithRespondWith(event, wait_until_observer,
                                         respond_with_observer);
}

void ServiceWorkerGlobalScope::DispatchCookieChangeEvent(
    network::mojom::blink::CookieChangeInfoPtr change,
    DispatchCookieChangeEventCallback callback) {
  const int event_id = event_queue_->NextEventId();
  cookie_change_event_callbacks_.Set(event_id, std::move(callback));

  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartCookieChangeEvent,
                    WrapWeakPersistent(this), std::move(change)),
      CreateAbortCallback(&cookie_change_event_callbacks_), std::nullopt);
}

void ServiceWorkerGlobalScope::StartCookieChangeEvent(
    network::mojom::blink::CookieChangeInfoPtr change,
    int event_id) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerGlobalScope::DispatchCookieChangeEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  auto* observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kCookieChange, event_id);

  HeapVector<Member<CookieListItem>> changed;
  HeapVector<Member<CookieListItem>> deleted;
  CookieChangeEvent::ToEventInfo(change, changed, deleted);
  Event* event = ExtendableCookieChangeEvent::Create(
      event_type_names::kCookiechange, std::move(changed), std::move(deleted),
      observer);

  // TODO(pwnall): Handle handle the case when
  //               (changed.empty() && deleted.empty()).

  // TODO(pwnall): Investigate dispatching this on cookieStore.
  DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScope::DispatchContentDeleteEvent(
    const String& id,
    DispatchContentDeleteEventCallback callback) {
  const int event_id = event_queue_->NextEventId();
  content_delete_callbacks_.Set(event_id, std::move(callback));

  event_queue_->EnqueueNormal(
      event_id,
      WTF::BindOnce(&ServiceWorkerGlobalScope::StartContentDeleteEvent,
                    WrapWeakPersistent(this), id),
      CreateAbortCallback(&content_delete_callbacks_), std::nullopt);
}

void ServiceWorkerGlobalScope::StartContentDeleteEvent(String id,
                                                       int event_id) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerGlobalScope::DispatchContentDeleteEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT);

  auto* observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kContentDelete, event_id);

  auto* init = ContentIndexEventInit::Create();
  init->setId(id);

  auto* event = MakeGarbageCollected<ContentIndexEvent>(
      event_type_names::kContentdelete, init, observer);

  DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScope::Ping(PingCallback callback) {
  DCHECK(IsContextThread());
  std::move(callback).Run();
}

void ServiceWorkerGlobalScope::SetIdleDelay(base::TimeDelta delay) {
  DCHECK(IsContextThread());
  DCHECK(event_queue_);
  event_queue_->SetIdleDelay(delay);
}

void ServiceWorkerGlobalScope::AddKeepAlive() {
  DCHECK(IsContextThread());
  DCHECK(event_queue_);

  // TODO(richardzh): refactor with RAII pattern, as explained in crbug/1399324
  event_queue_->ResetIdleTimeout();
}

void ServiceWorkerGlobalScope::ClearKeepAlive() {
  DCHECK(IsContextThread());
  DCHECK(event_queue_);

  // TODO(richardzh): refactor with RAII pattern, as explained in crbug/1399324
  event_queue_->ResetIdleTimeout();
  event_queue_->CheckEventQueue();
}

void ServiceWorkerGlobalScope::AddMessageToConsole(
    mojom::blink::ConsoleMessageLevel level,
    const String& message) {
  AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kOther, level, message,
      CaptureSourceLocation(/* url= */ "", /* line_number= */ 0,
                            /* column_number= */ 0)));
}

void ServiceWorkerGlobalScope::ExecuteScriptForTest(
    const String& javascript,
    bool wants_result,
    ExecuteScriptForTestCallback callback) {
  ScriptState* script_state = ScriptController()->GetScriptState();
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  // It's safe to use `kDoNotSanitize` because this method is for testing only.
  ClassicScript* script = ClassicScript::CreateUnspecifiedScript(
      javascript, ScriptSourceLocationType::kUnknown,
      SanitizeScriptErrors::kDoNotSanitize);

  v8::TryCatch try_catch(isolate);
  ScriptEvaluationResult result = script->RunScriptOnScriptStateAndReturnValue(
      script_state, ExecuteScriptPolicy::kDoNotExecuteScriptWhenScriptsDisabled,
      V8ScriptRunner::RethrowErrorsOption::Rethrow(String()));

  // If the script throws an error, the returned value is a stringification of
  // the error message.
  if (try_catch.HasCaught()) {
    String exception_string;
    if (try_catch.Message().IsEmpty() || try_catch.Message()->Get().IsEmpty()) {
      exception_string = "Unknown exception while executing script.";
    } else {
      exception_string =
          ToCoreStringWithNullCheck(isolate, try_catch.Message()->Get());
    }
    std::move(callback).Run(base::Value(), std::move(exception_string));
    return;
  }

  // If the script didn't want a result, just return immediately.
  if (!wants_result) {
    std::move(callback).Run(base::Value(), String());
    return;
  }

  // Otherwise, the script should have succeeded, and we return the value from
  // the execution.
  DCHECK_EQ(ScriptEvaluationResult::ResultType::kSuccess,
            result.GetResultType());

  v8::Local<v8::Value> v8_result = result.GetSuccessValue();
  DCHECK(!v8_result.IsEmpty());

  // Only convert the value to a base::Value if it's not null or undefined.
  // Null and undefined are valid results, but will fail to convert using the
  // WebV8ValueConverter. They are accurately represented (though no longer
  // distinguishable) by the empty base::Value.
  if (v8_result->IsNullOrUndefined()) {
    std::move(callback).Run(base::Value(), String());
    return;
  }

  base::Value value;
  String exception;

  // TODO(devlin): Is this thread-safe? Platform::Current() is set during
  // blink initialization and the created V8ValueConverter is constructed
  // without any special access, but it's *possible* a future implementation
  // here would be thread-unsafe (if it relied on member data in Platform).
  std::unique_ptr<WebV8ValueConverter> converter =
      Platform::Current()->CreateWebV8ValueConverter();
  converter->SetDateAllowed(true);
  converter->SetRegExpAllowed(true);

  std::unique_ptr<base::Value> converted_value =
      converter->FromV8Value(v8_result, script_state->GetContext());
  if (!converted_value) {
    std::move(callback).Run(base::Value(),
                            "Failed to convert V8 result from script");
    return;
  }

  std::move(callback).Run(std::move(*converted_value), String());
}

void ServiceWorkerGlobalScope::NoteNewFetchEvent(
    const mojom::blink::FetchAPIRequest& request) {
  int range_increment = request.headers.Contains(http_names::kRange) ? 1 : 0;
  auto it = unresponded_fetch_event_counts_.find(request.url);
  if (it == unresponded_fetch_event_counts_.end()) {
    unresponded_fetch_event_counts_.insert(
        request.url, FetchEventCounts(1, range_increment));
  } else {
    it->value.total_count += 1;
    it->value.range_count += range_increment;
  }
}

void ServiceWorkerGlobalScope::NoteRespondedToFetchEvent(
    const KURL& request_url,
    bool range_request) {
  auto it = unresponded_fetch_event_counts_.find(request_url);
  DCHECK_GE(it->value.total_count, 1);
  it->value.total_count -= 1;
  if (range_request) {
    DCHECK_GE(it->value.range_count, 1);
    it->value.range_count -= 1;
  }
  if (it->value.total_count == 0)
    unresponded_fetch_event_counts_.erase(it);
}

void ServiceWorkerGlobalScope::RecordQueuingTime(base::TimeTicks created_time) {
  base::UmaHistogramMediumTimes("ServiceWorker.FetchEvent.QueuingTime",
                                base::TimeTicks::Now() - created_time);
}

bool ServiceWorkerGlobalScope::IsInFencedFrame() const {
  return GetAncestorFrameType() ==
         mojom::blink::AncestorFrameType::kFencedFrame;
}

void ServiceWorkerGlobalScope::NotifyWebSocketActivity() {
  CHECK(IsContextThread());
  CHECK(event_queue_);

  ScriptState* script_state = ScriptController()->GetScriptState();
  CHECK(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> v8_context = script_state->GetContext();

  bool notify = To<ServiceWorkerGlobalScopeProxy>(ReportingProxy())
                    .ShouldNotifyServiceWorkerOnWebSocketActivity(v8_context);

  if (notify) {
    // TODO(crbug/1399324): refactor with RAII pattern.
    event_queue_->ResetIdleTimeout();
    event_queue_->CheckEventQueue();
  }
}

mojom::blink::ServiceWorkerFetchHandlerType
ServiceWorkerGlobalScope::FetchHandlerType() {
  EventListenerVector* elv = GetEventListeners(event_type_names::kFetch);
  if (!elv) {
    return mojom::blink::ServiceWorkerFetchHandlerType::kNoHandler;
  }

  ScriptState* script_state = ScriptController()->GetScriptState();
  // Do not remove this, |scope| is needed by `GetListenerObject`.
  ScriptState::Scope scope(script_state);

  // TODO(crbug.com/1349613): revisit the way to implement this.
  // The following code returns kEmptyFetchHandler if all handlers are nop.
  for (RegisteredEventListener* e : *elv) {
    EventTarget* et = EventTarget::Create(script_state);
    v8::Local<v8::Value> v =
        To<JSBasedEventListener>(e->Callback())->GetListenerObject(*et);
    if (v.IsEmpty() || !v->IsFunction() ||
        !v.As<v8::Function>()->Experimental_IsNopFunction()) {
      return mojom::blink::ServiceWorkerFetchHandlerType::kNotSkippable;
    }
  }
  AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::blink::ConsoleMessageSource::kJavaScript,
      mojom::blink::ConsoleMessageLevel::kWarning,
      "Fetch event handler is recognized as no-op. "
      "No-op fetch handler may bring overhead during navigation. "
      "Consider removing the handler if possible."));
  return mojom::blink::ServiceWorkerFetchHandlerType::kEmptyFetchHandler;
}

bool ServiceWorkerGlobalScope::HasHidEventHandlers() {
  HID* hid = Supplement<NavigatorBase>::From<HID>(*navigator());
  return hid ? hid->HasEventListeners() : false;
}

bool ServiceWorkerGlobalScope::HasUsbEventHandlers() {
  USB* usb = Supplement<NavigatorBase>::From<USB>(*navigator());
  return usb ? usb->HasEventListeners() : false;
}

void ServiceWorkerGlobalScope::GetRemoteAssociatedInterface(
    const String& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  remote_associated_interfaces_->GetAssociatedInterface(
      name, mojo::PendingAssociatedReceiver<mojom::blink::AssociatedInterface>(
                std::move(handle)));
}

bool ServiceWorkerGlobalScope::SetAttributeEventListener(
    const AtomicString& event_type,
    EventListener* listener) {
  // Count the modification of fetch handlers after the initial evaluation.
  if (did_evaluate_script_) {
    if (event_type == event_type_names::kFetch) {
      UseCounter::Count(
          this,
          WebFeature::kServiceWorkerFetchHandlerModifiedAfterInitialization);
    }
    UseCounter::Count(
        this,
        WebFeature::kServiceWorkerEventHandlerModifiedAfterInitialization);
  }
  return WorkerGlobalScope::SetAttributeEventListener(event_type, listener);
}

std::optional<mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>>
ServiceWorkerGlobalScope::FindRaceNetworkRequestURLLoaderFactory(
    const base::UnguessableToken& token) {
  std::unique_ptr<RaceNetworkRequestInfo> result =
      race_network_requests_.Take(String(token.ToString()));
  if (result) {
    race_network_request_fetch_event_ids_.erase(result->fetch_event_id);
    return std::optional<
        mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>>(
        std::move(result->url_loader_factory));
  }
  return std::nullopt;
}

void ServiceWorkerGlobalScope::InsertNewItemToRaceNetworkRequests(
    int fetch_event_id,
    const base::UnguessableToken& token,
    mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
        url_loader_factory,
    const KURL& request_url) {
  auto race_network_request_token = String(token.ToString());
  auto info = std::make_unique<RaceNetworkRequestInfo>(
      fetch_event_id, race_network_request_token,
      std::move(url_loader_factory));
  race_network_request_fetch_event_ids_.insert(fetch_event_id, info.get());
  auto insert_result = race_network_requests_.insert(race_network_request_token,
                                                     std::move(info));

  // DumpWithoutCrashing if the token is empty, or not inserted as a new entry
  // to |race_network_request_loader_factories_|.
  // TODO(crbug.com/1492640) Remove DumpWithoutCrashing once we collect data
  // and identify the cause.
  static bool has_dumped_without_crashing_for_empty_token = false;
  static bool has_dumped_without_crashing_for_not_new_entry = false;
  if (!has_dumped_without_crashing_for_empty_token && token.is_empty()) {
    has_dumped_without_crashing_for_empty_token = true;
    SCOPED_CRASH_KEY_BOOL("SWGlobalScope", "empty_race_token",
                          token.is_empty());
    SCOPED_CRASH_KEY_STRING64("SWGlobalScope", "race_token_string",
                              token.ToString());
    SCOPED_CRASH_KEY_BOOL("SWGlobalScope", "race_insert_new_entry",
                          insert_result.is_new_entry);
    SCOPED_CRASH_KEY_STRING256("SWGlobalScope", "race_request_url",
                               request_url.GetString().Utf8());
    base::debug::DumpWithoutCrashing();
  }
  if (!has_dumped_without_crashing_for_not_new_entry &&
      !insert_result.is_new_entry) {
    has_dumped_without_crashing_for_not_new_entry = true;
    SCOPED_CRASH_KEY_BOOL("SWGlobalScope", "empty_race_token",
                          token.is_empty());
    SCOPED_CRASH_KEY_STRING64("SWGlobalScope", "race_token_string",
                              token.ToString());
    SCOPED_CRASH_KEY_BOOL("SWGlobalScope", "race_insert_new_entry",
                          insert_result.is_new_entry);
    SCOPED_CRASH_KEY_STRING256("SWGlobalScope", "race_request_url",
                               request_url.GetString().Utf8());
    base::debug::DumpWithoutCrashing();
  }
}

void ServiceWorkerGlobalScope::RemoveItemFromRaceNetworkRequests(
    int fetch_event_id) {
  RaceNetworkRequestInfo* info =
      race_network_request_fetch_event_ids_.Take(fetch_event_id);
  if (info) {
    race_network_requests_.erase(info->token);
  }
}

}  // namespace blink
