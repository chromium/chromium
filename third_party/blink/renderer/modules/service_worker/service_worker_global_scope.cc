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

#include "base/bind_helpers.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/cross_origin_embedder_policy.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/worker_timing_container.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/subresource_loader_updater.mojom.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_error.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_fetch_context.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_background_fetch_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_content_index_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_notification_event_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_payment_request_event_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
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
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
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
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_response_headers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

constexpr char kServiceWorkerGlobalScopeTraceScope[] =
    "ServiceWorkerGlobalScope";

void DidSkipWaiting(ScriptPromiseResolver* resolver, bool success) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;
  // Per spec the promise returned by skipWaiting() can never reject.
  if (!success)
    return;
  resolver->Resolve();
}

// Creates a callback which takes an |event_id| and |status|, which calls the
// given event's callback with the given status and removes it from |map|.
template <typename MapType, typename... Args>
ServiceWorkerEventQueue::AbortCallback CreateAbortCallback(MapType* map,
                                                           Args&&... args) {
  return WTF::Bind(
      [](MapType* map, Args&&... args, int event_id,
         mojom::blink::ServiceWorkerEventStatus status) {
        auto iter = map->find(event_id);
        DCHECK(iter != map->end());
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
    const ServiceWorkerToken& service_worker_token,
    ukm::SourceId ukm_source_id) {
#if DCHECK_IS_ON()
  // If the script is being loaded via script streaming, the script is not yet
  // loaded.
  if (installed_scripts_manager && installed_scripts_manager->IsScriptInstalled(
                                       creation_params->script_url)) {
    // CSP headers, referrer policy, and origin trial tokens will be provided by
    // the InstalledScriptsManager in EvaluateClassicScript().
    DCHECK(creation_params->outside_content_security_policy_headers.IsEmpty());
    DCHECK_EQ(network::mojom::ReferrerPolicy::kDefault,
              creation_params->referrer_policy);
    DCHECK(creation_params->origin_trial_tokens->IsEmpty());
  }
#endif  // DCHECK_IS_ON()

  return MakeGarbageCollected<ServiceWorkerGlobalScope>(
      std::move(creation_params), thread, std::move(installed_scripts_manager),
      std::move(cache_storage_remote), time_origin, service_worker_token,
      ukm_source_id);
}

ServiceWorkerGlobalScope::ServiceWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    ServiceWorkerThread* thread,
    std::unique_ptr<ServiceWorkerInstalledScriptsManager>
        installed_scripts_manager,
    mojo::PendingRemote<mojom::blink::CacheStorage> cache_storage_remote,
    base::TimeTicks time_origin,
    const ServiceWorkerToken& service_worker_token,
    ukm::SourceId ukm_source_id)
    : WorkerGlobalScope(std::move(creation_params),
                        thread,
                        time_origin,
                        ukm_source_id),
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
    const FetchClientSettingsObjectSnapshot& outside_settings_object,
    WorkerResourceTimingNotifier& outside_resource_timing_notifier,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK(!IsContextPaused());

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
  auto context_type = mojom::RequestContextType::SERVICE_WORKER;
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
      script_url, std::move(worker_main_script_load_params),
      mojo::NullRemote() /* resource_load_info_notifier */, context_type,
      destination, network::mojom::RequestMode::kSameOrigin,
      network::mojom::CredentialsMode::kSameOrigin,
      WTF::Bind(&ServiceWorkerGlobalScope::DidReceiveResponseForClassicScript,
                WrapWeakPersistent(this),
                WrapPersistent(classic_script_loader)),
      WTF::Bind(&ServiceWorkerGlobalScope::DidFetchClassicScript,
                WrapWeakPersistent(this), WrapPersistent(classic_script_loader),
                stack_id));
}

void ServiceWorkerGlobalScope::FetchAndRunModuleScript(
    const KURL& module_url_record,
    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params,
    const FetchClientSettingsObjectSnapshot& outside_settings_object,
    WorkerResourceTimingNotifier& outside_resource_timing_notifier,
    network::mojom::CredentialsMode credentials_mode,
    RejectCoepUnsafeNone reject_coep_unsafe_none) {
  DCHECK(IsContextThread());
  DCHECK(!reject_coep_unsafe_none);
  ModuleScriptCustomFetchType fetch_type =
      installed_scripts_manager_
          ? ModuleScriptCustomFetchType::kInstalledServiceWorker
          : ModuleScriptCustomFetchType::kWorkerConstructor;
  FetchModuleScript(module_url_record, outside_settings_object,
                    outside_resource_timing_notifier,
                    mojom::RequestContextType::SERVICE_WORKER,
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

void ServiceWorkerGlobalScope::CountWorkerScript(size_t script_size,
                                                 size_t cached_metadata_size) {
  DCHECK_EQ(GetScriptType(), mojom::ScriptType::kClassic);
  base::UmaHistogramCustomCounts(
      "ServiceWorker.ScriptSize",
      base::saturated_cast<base::Histogram::Sample>(script_size), 1000, 5000000,
      50);

  if (cached_metadata_size) {
    base::UmaHistogramCustomCounts(
        "ServiceWorker.ScriptCachedMetadataSize",
        base::saturated_cast<base::Histogram::Sample>(cached_metadata_size),
        1000, 50000000, 50);
  }

  CountScriptInternal(script_size, cached_metadata_size);
}

void ServiceWorkerGlobalScope::CountImportedScript(
    size_t script_size,
    size_t cached_metadata_size) {
  DCHECK_EQ(GetScriptType(), mojom::ScriptType::kClassic);
  CountScriptInternal(script_size, cached_metadata_size);
}

void ServiceWorkerGlobalScope::DidEvaluateScript() {
  DCHECK(!did_evaluate_script_);
  did_evaluate_script_ = true;

  event_queue_->Start();

  // Skip recording UMAs for module scripts because there're no ways to get the
  // number of static-imported scripts and the total size of the imported
  // scripts.
  if (GetScriptType() == mojom::ScriptType::kModule) {
    return;
  }

  // TODO(asamidoi,nhiroki): Record the UMAs for module scripts, or remove them
  // if they're no longer used.
  base::UmaHistogramCounts1000(
      "ServiceWorker.ScriptCount",
      base::saturated_cast<base::Histogram::Sample>(script_count_));
  base::UmaHistogramCustomCounts(
      "ServiceWorker.ScriptTotalSize",
      base::saturated_cast<base::Histogram::Sample>(script_total_size_), 1000,
      5000000, 50);
  if (script_cached_metadata_total_size_) {
    base::UmaHistogramCustomCounts(
        "ServiceWorker.ScriptCachedMetadataTotalSize",
        base::saturated_cast<base::Histogram::Sample>(
            script_cached_metadata_total_size_),
        1000, 50000000, 50);
  }
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
      classic_script_loader->ResponseAddressSpace(),
      classic_script_loader->GetContentSecurityPolicy()
          ? classic_script_loader->GetContentSecurityPolicy()->Headers()
          : Vector<CSPHeaderAndType>(),
      classic_script_loader->OriginTrialTokens(),
      classic_script_loader->SourceText(),
      classic_script_loader->ReleaseCachedMetadata(), stack_id);
}

// https://w3c.github.io/ServiceWorker/#run-service-worker-algorithm
void ServiceWorkerGlobalScope::Initialize(
    const KURL& response_url,
    network::mojom::ReferrerPolicy response_referrer_policy,
    network::mojom::IPAddressSpace response_address_space,
    const Vector<CSPHeaderAndType>& response_csp_headers,
    const Vector<String>* response_origin_trial_tokens,
    int64_t appcache_id) {
  // Step 4.5. "Set workerGlobalScope's url to serviceWorker's script url."
  InitializeURL(response_url);

  // Step 4.6. "Set workerGlobalScope's HTTPS state to serviceWorker's script
  // resource's HTTPS state."
  // This is done in the constructor of WorkerGlobalScope.

  // Step 4.7. "Set workerGlobalScope's referrer policy to serviceWorker's
  // script resource's referrer policy."
  SetReferrerPolicy(response_referrer_policy);

  // https://wicg.github.io/cors-rfc1918/#integration-html
  SetAddressSpace(response_address_space);

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
  //
  // These should be called after SetAddressSpace() to correctly override the
  // address space by the "treat-as-public-address" CSP directive.
  InitContentSecurityPolicyFromVector(response_csp_headers);
  BindContentSecurityPolicyToExecutionContext();

  OriginTrialContext::AddTokens(this, response_origin_trial_tokens);

  // TODO(nhiroki): Clarify mappings between the steps 4.8-4.11 and
  // implementation.

  // This should be called after OriginTrialContext::AddTokens() to install
  // origin trial features in JavaScript's global object.
  ScriptController()->PrepareForEvaluation();

  // Service Workers don't use application cache.
  DCHECK_EQ(appcache_id, mojom::blink::kAppCacheNoCacheId);
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

  // Construct a ContentSecurityPolicy object to convert
  // ContentSecurityPolicyResponseHeaders to CSPHeaderAndType.
  // TODO(nhiroki): Find an efficient way to do this.
  auto* content_security_policy = MakeGarbageCollected<ContentSecurityPolicy>();
  content_security_policy->DidReceiveHeaders(
      script_data->GetContentSecurityPolicyResponseHeaders());

  RunClassicScript(
      script_url, referrer_policy, script_data->GetResponseAddressSpace(),
      content_security_policy->Headers(),
      script_data->CreateOriginTrialTokens().get(),
      script_data->TakeSourceText(), script_data->TakeMetaData(), stack_id);
}

// https://w3c.github.io/ServiceWorker/#run-service-worker-algorithm
void ServiceWorkerGlobalScope::RunClassicScript(
    const KURL& response_url,
    network::mojom::ReferrerPolicy response_referrer_policy,
    network::mojom::IPAddressSpace response_address_space,
    const Vector<CSPHeaderAndType> response_csp_headers,
    const Vector<String>* response_origin_trial_tokens,
    const String& source_code,
    std::unique_ptr<Vector<uint8_t>> cached_meta_data,
    const v8_inspector::V8StackTraceId& stack_id) {
  // Step 4.5-4.11 are implemented in Initialize().
  Initialize(response_url, response_referrer_policy, response_address_space,
             response_csp_headers, response_origin_trial_tokens,
             mojom::blink::kAppCacheNoCacheId);

  // Step 4.12. "Let evaluationStatus be the result of running the classic
  // script script if script is a classic script, otherwise, the result of
  // running the module script script if script is a module script."
  EvaluateClassicScript(response_url, source_code, std::move(cached_meta_data),
                        stack_id);
}

void ServiceWorkerGlobalScope::CountScriptInternal(
    size_t script_size,
    size_t cached_metadata_size) {
  ++script_count_;
  script_total_size_ += script_size;
  script_cached_metadata_total_size_ += cached_metadata_size;
}

ServiceWorkerClients* ServiceWorkerGlobalScope::clients() {
  if (!clients_)
    clients_ = ServiceWorkerClients::Create();
  return clients_;
}

ServiceWorkerRegistration* ServiceWorkerGlobalScope::registration() {
  return registration_;
}

::blink::ServiceWorker* ServiceWorkerGlobalScope::serviceWorker() {
  return service_worker_;
}

ScriptPromise ServiceWorkerGlobalScope::skipWaiting(ScriptState* script_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  // FIXME: short-term fix, see details at:
  // https://codereview.chromium.org/535193002/.
  if (!execution_context)
    return ScriptPromise();

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  GetServiceWorkerHost()->SkipWaiting(
      WTF::Bind(&DidSkipWaiting, WrapPersistent(resolver)));
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
  DCHECK(it != pending_preload_fetch_events_.end());
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
  // Display an error message to the console, preferring the unsanitized one if
  // available.
  const WebString& error_message = error->unsanitized_message.IsEmpty()
                                       ? error->message
                                       : error->unsanitized_message;
  if (!error_message.IsEmpty()) {
    AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::ConsoleMessageSource::kWorker,
        mojom::ConsoleMessageLevel::kError, error_message));
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
  ::blink::ServiceWorker* worker = service_worker_objects_.at(info.version_id);
  if (!worker) {
    worker = ::blink::ServiceWorker::Create(this, std::move(info));
    service_worker_objects_.Set(info.version_id, worker);
  }
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
  respond_with_observer->DidDispatchEvent(dispatch_result);
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
  visitor->Trace(controller_receivers_);
  WorkerGlobalScope::Trace(visitor);
}

bool ServiceWorkerGlobalScope::HasRelatedFetchEvent(
    const KURL& request_url) const {
  auto it = unresponded_fetch_event_counts_.find(request_url);
  return it != unresponded_fetch_event_counts_.end();
}

int ServiceWorkerGlobalScope::GetOutstandingThrottledLimit() const {
  return features::kInstallingServiceWorkerOutstandingThrottledLimit.Get();
}

void ServiceWorkerGlobalScope::importScripts(const Vector<String>& urls,
                                             ExceptionState& exception_state) {
  for (const String& string_url : urls) {
    KURL completed_url = CompleteURL(string_url);
    if (installed_scripts_manager_ &&
        !installed_scripts_manager_->IsScriptInstalled(completed_url)) {
      DCHECK(installed_scripts_manager_->IsScriptInstalled(Url()));
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNetworkError,
          "Failed to import '" + completed_url.ElidedString() +
              "'. importScripts() of new scripts after service worker "
              "installation is not allowed.");
      return;
    }
  }
  WorkerGlobalScope::importScripts(urls, exception_state);
}

SingleCachedMetadataHandler*
ServiceWorkerGlobalScope::CreateWorkerScriptCachedMetadataHandler(
    const KURL& script_url,
    std::unique_ptr<Vector<uint8_t>> meta_data) {
  return MakeGarbageCollected<ServiceWorkerScriptCachedMetadataHandler>(
      this, script_url, std::move(meta_data));
}

ScriptPromise ServiceWorkerGlobalScope::fetch(ScriptState* script_state,
                                              const RequestInfo& input,
                                              const RequestInit* init,
                                              ExceptionState& exception_state) {
  return GlobalFetch::fetch(script_state, *this, input, init, exception_state);
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
  RunEventCallback(&install_event_callbacks_, event_queue_.get(),
                   install_event_id, status);
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
    const KURL& request_url,
    base::TimeTicks event_dispatch_time,
    base::TimeTicks respond_with_settled_time) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker",
      "ServiceWorkerGlobalScope::RespondToFetchEventWithNoResponse",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(fetch_event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(fetch_response_callbacks_.Contains(fetch_event_id));
  mojom::blink::ServiceWorkerFetchResponseCallback* response_callback =
      fetch_response_callbacks_.Take(fetch_event_id)->Value().get();

  auto timing = mojom::blink::ServiceWorkerFetchEventTiming::New();
  timing->dispatch_event_time = event_dispatch_time;
  timing->respond_with_settled_time = respond_with_settled_time;

  NoteRespondedToFetchEvent(request_url);

  response_callback->OnFallback(std::move(timing));
}

void ServiceWorkerGlobalScope::RespondToFetchEvent(
    int fetch_event_id,
    const KURL& request_url,
    mojom::blink::FetchAPIResponsePtr response,
    base::TimeTicks event_dispatch_time,
    base::TimeTicks respond_with_settled_time) {
  DCHECK(IsContextThread());
  TRACE_EVENT_WITH_FLOW0(
      "ServiceWorker", "ServiceWorkerGlobalScope::RespondToFetchEvent",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(fetch_event_id)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(fetch_response_callbacks_.Contains(fetch_event_id));

  mojom::blink::ServiceWorkerFetchResponseCallback* response_callback =
      fetch_response_callbacks_.Take(fetch_event_id)->Value().get();

  auto timing = mojom::blink::ServiceWorkerFetchEventTiming::New();
  timing->dispatch_event_time = event_dispatch_time;
  timing->respond_with_settled_time = respond_with_settled_time;

  NoteRespondedToFetchEvent(request_url);

  response_callback->OnResponse(std::move(response), std::move(timing));
}

void ServiceWorkerGlobalScope::RespondToFetchEventWithResponseStream(
    int fetch_event_id,
    const KURL& request_url,
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
  DCHECK(fetch_response_callbacks_.Contains(fetch_event_id));
  mojom::blink::ServiceWorkerFetchResponseCallback* response_callback =
      fetch_response_callbacks_.Take(fetch_event_id)->Value().get();

  auto timing = mojom::blink::ServiceWorkerFetchEventTiming::New();
  timing->dispatch_event_time = event_dispatch_time;
  timing->respond_with_settled_time = respond_with_settled_time;

  NoteRespondedToFetchEvent(request_url);

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
  event_queue_->EnqueueNormal(WTF::Bind(&ServiceWorkerEventQueue::EndEvent,
                                        WTF::Unretained(event_queue_.get())),
                              base::DoNothing(), base::nullopt);
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
    DCHECK(!client_info->client_uuid.IsEmpty());
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
      if (!msg.locked_agent_cluster_id ||
          GetExecutionContext()->IsSameAgentCluster(
              *msg.locked_agent_cluster_id)) {
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
    if (!msg.locked_agent_cluster_id ||
        GetExecutionContext()->IsSameAgentCluster(
            *msg.locked_agent_cluster_id)) {
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

void ServiceWorkerGlobalScope::StartFetchEvent(
    mojom::blink::DispatchFetchEventParamsPtr params,
    base::WeakPtr<CrossOriginResourcePolicyChecker> corp_checker,
    mojo::PendingRemote<mojom::blink::ServiceWorkerFetchResponseCallback>
        response_callback,
    DispatchFetchEventInternalCallback callback,
    base::Optional<base::TimeTicks> created_time,
    int event_id) {
  DCHECK(IsContextThread());
  if (created_time.has_value()) {
    RecordQueuingTime(created_time.value());
  }

  fetch_event_callbacks_.Set(event_id, std::move(callback));
  HeapMojoRemote<mojom::blink::ServiceWorkerFetchResponseCallback,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      remote(this);
  remote.Bind(std::move(response_callback),
              GetThread()->GetTaskRunner(TaskType::kNetworking));
  fetch_response_callbacks_.Set(event_id, WrapDisallowNew(std::move(remote)));

  // This TRACE_EVENT is used for perf benchmark to confirm if all of fetch
  // events have completed. (crbug.com/736697)
  TRACE_EVENT_WITH_FLOW1(
      "ServiceWorker", "ServiceWorkerGlobalScope::DispatchFetchEventInternal",
      TRACE_ID_WITH_SCOPE(kServiceWorkerGlobalScopeTraceScope,
                          TRACE_ID_LOCAL(event_id)),
      TRACE_EVENT_FLAG_FLOW_OUT, "url",
      params->request->url.ElidedString().Utf8());

  // Set up for navigation preload (FetchEvent#preloadResponse) if needed.
  const bool navigation_preload_sent = !!params->preload_handle;
  if (navigation_preload_sent) {
    To<ServiceWorkerGlobalScopeProxy>(ReportingProxy())
        .SetupNavigationPreload(event_id, params->request->url,
                                std::move(params->preload_handle));
  }

  ScriptState::Scope scope(ScriptController()->GetScriptState());
  auto* wait_until_observer = MakeGarbageCollected<WaitUntilObserver>(
      this, WaitUntilObserver::kFetch, event_id);
  auto* respond_with_observer = MakeGarbageCollected<FetchRespondWithObserver>(
      this, event_id, std::move(corp_checker), *params->request,
      wait_until_observer);
  FetchEventInit* event_init = FetchEventInit::Create();
  event_init->setCancelable(true);
  event_init->setClientId(
      params->request->is_main_resource_load ? String() : params->client_id);
  event_init->setResultingClientId(
      !params->request->is_main_resource_load ? String() : params->client_id);
  event_init->setIsReload(params->request->is_reload);

  mojom::blink::FetchAPIRequest& fetch_request = *params->request;
  auto debug_header_it =
      fetch_request.headers.find(RequestDebugHeaderScope::kHeaderName);
  auto stack_string = debug_header_it == fetch_request.headers.end()
                          ? String()
                          : debug_header_it->value;
  Request* request = Request::Create(
      ScriptController()->GetScriptState(), std::move(params->request),
      Request::ForServiceWorkerFetchEvent::kTrue);
  request->getHeaders()->SetGuard(Headers::kImmutableGuard);
  event_init->setRequest(request);

  ScriptState* script_state = ScriptController()->GetScriptState();
  FetchEvent* fetch_event = MakeGarbageCollected<FetchEvent>(
      script_state, event_type_names::kFetch, event_init, respond_with_observer,
      wait_until_observer,
      mojo::PendingRemote<mojom::blink::WorkerTimingContainer>(
          std::move(params->worker_timing_remote)),
      navigation_preload_sent);
  respond_with_observer->SetEvent(fetch_event);

  if (navigation_preload_sent) {
    // Keep |fetchEvent| until OnNavigationPreloadComplete() or
    // onNavigationPreloadError() will be called.
    pending_preload_fetch_events_.insert(event_id, fetch_event);
  }

  NoteNewFetchEvent(request->url());

  RequestDebugHeaderScope debug_header_scope(this, stack_string);
  DispatchExtendableEventWithRespondWith(fetch_event, wait_until_observer,
                                         respond_with_observer);
}

void ServiceWorkerGlobalScope::SetFetchHandlerExistence(
    FetchHandlerExistence fetch_handler_existence) {
  DCHECK(IsContextThread());
  if (fetch_handler_existence == FetchHandlerExistence::EXISTS)
    GetThread()->GetIsolate()->IsolateInForegroundNotification();
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
  if (RequestedTermination()) {
    event_queue_->EnqueuePending(
        WTF::Bind(&ServiceWorkerGlobalScope::StartFetchEvent,
                  WrapWeakPersistent(this), std::move(params),
                  std::move(corp_checker), std::move(response_callback),
                  std::move(callback), base::TimeTicks::Now()),
        CreateAbortCallback(&fetch_event_callbacks_), base::nullopt);
  } else {
    event_queue_->EnqueueNormal(
        WTF::Bind(&ServiceWorkerGlobalScope::StartFetchEvent,
                  WrapWeakPersistent(this), std::move(params),
                  std::move(corp_checker), std::move(response_callback),
                  std::move(callback), base::TimeTicks::Now()),
        CreateAbortCallback(&fetch_event_callbacks_), base::nullopt);
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
    mojom::blink::ServiceWorkerRegistrationObjectInfoPtr registration_info,
    mojom::blink::ServiceWorkerObjectInfoPtr service_worker_info,
    mojom::blink::FetchHandlerExistence fetch_hander_existence,
    std::unique_ptr<PendingURLLoaderFactoryBundle> subresource_loader_factories,
    mojo::PendingReceiver<mojom::blink::ReportingObserver>
        reporting_observer_receiver) {
  DCHECK(IsContextThread());
  DCHECK(!global_scope_initialized_);

  DCHECK(service_worker_host.is_valid());
  DCHECK(!service_worker_host_.is_bound());
  service_worker_host_.Bind(std::move(service_worker_host),
                            GetTaskRunner(TaskType::kInternalDefault));

  if (subresource_loader_factories) {
    static_cast<WebServiceWorkerFetchContext*>(web_worker_fetch_context())
        ->GetSubresourceLoaderUpdater()
        ->UpdateSubresourceLoaderFactories(
            std::move(subresource_loader_factories));
  }

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

  if (reporting_observer_receiver) {
    ReportingContext::From(this)->Bind(std::move(reporting_observer_receiver));
  }

  global_scope_initialized_ = true;
  if (!pause_evaluation_)
    ReadyToRunWorkerScript();
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
  event_queue_->EnqueueNormal(
      WTF::Bind(&ServiceWorkerGlobalScope::StartInstallEvent,
                WrapWeakPersistent(this), std::move(callback)),
      CreateAbortCallback(&install_event_callbacks_), base::nullopt);
}

void ServiceWorkerGlobalScope::StartInstallEvent(
    DispatchInstallEventCallback callback,
    int event_id) {
  DCHECK(IsContextThread());
  install_event_callbacks_.Set(event_id, std::move(callback));
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
  event_queue_->EnqueueNormal(
      WTF::Bind(&ServiceWorkerGlobalScope::StartActivateEvent,
                WrapWeakPersistent(this), std::move(callback)),
      CreateAbortCallback(&activate_event_callbacks_), base::nullopt);
}

void ServiceWorkerGlobalScope::StartActivateEvent(
    DispatchActivateEventCallback callback,
    int event_id) {
  DCHECK(IsContextThread());
  activate_event_callbacks_.Set(event_id, std::move(callback));
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
  event_queue_->EnqueueNormal(
      WTF::Bind(&ServiceWorkerGlobalScope::StartBackgroundFetchAbortEvent,
                WrapWeakPersistent(this), std::move(registration),
                std::move(callback)),
      CreateAbortCallback(&background_fetch_abort_event_callbacks_),
      base::nullopt);
}

void ServiceWorkerGlobalScope::StartBackgroundFetchAbortEvent(
    mojom::blink::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchAbortEventCallback callback,
    int event_id) {
  DCHECK(IsContextThread());
  background_fetch_abort_event_callbacks_.Set(event_id, std::move(callback));
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
  event_queue_->EnqueueNormal(
      WTF::Bind(&ServiceWorkerGlobalScope::StartBackgroundFetchClickEvent,
                WrapWeakPersistent(this), std::move(registration),
                std::move(callback)),
      CreateAbortCallback(&background_fetch_click_event_callbacks_),
      base::nullopt);
}

void ServiceWorkerGlobalScope::StartBackgroundFetchClickEvent(
    mojom::blink::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchClickEventCallback callback,
    int event_id) {
  DCHECK(IsContextThread());
  background_fetch_click_event_callbacks_.Set(event_id, std::move(callback));
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
  event_queue_->EnqueueNormal(
      WTF::Bind(&ServiceWorkerGlobalScope::StartBackgroundFetchFailEvent,
                WrapWeakPersistent(this), std::move(registration),
                std::move(callback)),
      CreateAbortCallback(&background_fetch_fail_event_callbacks_),
      base::nullopt);
}

void ServiceWorkerGlobalScope::StartBackgroundFetchFailEvent(
    mojom::blink::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchFailEventCallback callback,
    int event_id) {
  DCHECK(IsContextThread());
  background_fetch_fail_event_callbacks_.Set(event_id, std::move(callback));
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
  event_queue_->EnqueueNormal(
      WTF::Bind(&ServiceWorkerGlobalScope::StartBackgroundFetchSuccessEvent,
                WrapWeakPersistent(this), std::move(registration),
                std::move(callback)),
      CreateAbortCallback(&background_fetched_event_callbacks_), base::nullopt);
}

void ServiceWorkerGlobalScope::StartBackgroundFetchSuccessEvent(
    mojom::blink::BackgroundFetchRegistrationPtr registration,
    DispatchBackgroundFetchSuccessEventCallback callback,
    int event_id) {
  DCHECK(IsContextThread());
  background_fetched_event_callbacks_.Set(event_id, std::move(callback));
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
  event_queue_->EnqueueNormal(
      WTF::Bind(&ServiceWorkerGlobalScope::StartExtendableMessageEvent,
                WrapWeakPersistent(this), std::move(event),
                std::move(callback)),
      CreateAbortCallback(&message_event_callbacks_), base::nullopt);
}

void ServiceWorkerGlobalScope::StartExtendableMessageEvent(
    mojom::blink::ExtendableMessageEventPtr event,
    DispatchExtendableMessageEventCallback callback,
    int event_id) {
  DCHECK(IsContextThread());
  message_event_callbacks_.Set(event_id, std::move(callback));
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

  // We can use nullptr as a |corp_checker| for the main resource because it
  // must be the same origin.
  if (params->is_offline_capability_check) {
    event_queue_->EnqueueOffline(
        WTF::Bind(&ServiceWorkerGlobalScope::StartFetchEvent,
                  WrapWeakPersistent(this), std::move(params),
                  /*corp_checker=*/nullptr, std::move(response_callback),
                  std::move(callback), base::nullopt),
        CreateAbortCallback(&fetch_event_callbacks_), base::nullopt);
  } else {
    event_queue_->EnqueueNormal(
        WTF::Bind(&ServiceWorkerGlobalScope::StartFetchEvent,
                  WrapWeakPersistent(this), std::move(params),
                  /*corp_checker=*/nullptr, std::move(response_callback),
                  std::move(callback), base::TimeTicks::Now()),
        CreateAbortCallback(&fetch_event_callbacks_), base::nullopt);
  }
}

void ServiceWorkerGlobalScope::DispatchNotificationClickEvent(
    const String& notification_id,
    mojom::blink::NotificationDataPtr notification_data,
    int action_index,
    const String& reply,
    DispatchNotificationClickEventCallback callback) {
  DCHECK(IsContextThread());
  event_queue_->EnqueueNormal(
      WTF::Bind(&ServiceWorkerGlobalScope::StartNotificationClickEvent,
                WrapWeakPersistent(this), notification_id,
                std::move(notification_data), action_index, reply,
                std::move(callback)),
      CreateAbortCallback(&notification_click_event_callbacks_), base::nullopt);
}

void ServiceWorkerGlobalScope::StartNotificationClickEvent(
    String notification_id,
    mojom::blink::NotificationDataPtr notification_data,
    int action_index,
    String reply,
    DispatchNotificationClickEventCallback callback,
    int event_id) {
  notification_click_event_callbacks_.Set(event_id, std::move(callback));
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
  event_queue_->EnqueueNormal(
      WTF::Bind(&ServiceWorkerGlobalScope::StartNotificationCloseEvent,
                WrapWeakPersistent(this), notification_id,
                std::move(notification_data), std::move(callback)),
      CreateAbortCallback(&notification_close_event_callbacks_), base::nullopt);
}

void ServiceWorkerGlobalScope::StartNotificationCloseEvent(
    String notification_id,
    mojom::blink::NotificationDataPtr notification_data,
    DispatchNotificationCloseEventCallback callback,
    int event_id) {
  DCHECK(IsContextThread());
  notification_close_event_callbacks_.Set(event_id, std::move(callback));
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
  event_queue_->EnqueueNormal(
      WTF::Bind(&ServiceWorkerGlobalScope::StartPushEvent,
                WrapWeakPersistent(this), std::move(payload),
                std::move(callback)),
      CreateAbortCallback(&push_event_callbacks_),
      base::TimeDelta::FromSeconds(mojom::blink::kPushEventTimeoutSeconds));
}

void ServiceWorkerGlobalScope::StartPushEvent(
    String payload,
    DispatchPushEventCallback callback,
    int event_id) {
  DCHECK(IsContextThread());
  push_event_callbacks_.Set(event_id, std::move(callback));
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
  event_queue_->EnqueueNormal(
      WTF::Bind(&ServiceWorkerGlobalScope::StartPushSubscriptionChangeEvent,
                WrapWeakPersistent(this), std::move(old_subscription),
                std::move(new_subscription), std::move(callback)),
      CreateAbortCallback(&push_subscription_change_event_callbacks_),
      base::TimeDelta::FromSeconds(mojom::blink::kPushEventTimeoutSeconds));
}

void ServiceWorkerGlobalScope::StartPushSubscriptionChangeEvent(
    mojom::blink::PushSubscriptionPtr old_subscription,
    mojom::blink::PushSubscriptionPtr new_subscription,
    DispatchPushSubscriptionChangeEventCallback callback,
    int event_id) {
  DCHECK(IsContextThread());
  push_subscription_change_event_callbacks_.Set(event_id, std::move(callback));
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
  event_queue_->EnqueueNormal(
      WTF::Bind(&ServiceWorkerGlobalScope::StartSyncEvent,
                WrapWeakPersistent(this), std::move(tag), last_chance,
                std::move(callback)),
      CreateAbortCallback(&sync_event_callbacks_), timeout);
}

void ServiceWorkerGlobalScope::StartSyncEvent(
    String tag,
    bool last_chance,
    DispatchSyncEventCallback callback,
    int event_id) {
  DCHECK(IsContextThread());
  sync_event_callbacks_.Set(event_id, std::move(callback));
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
  event_queue_->EnqueueNormal(
      WTF::Bind(&ServiceWorkerGlobalScope::StartPeriodicSyncEvent,
                WrapWeakPersistent(this), std::move(tag), std::move(callback)),
      CreateAbortCallback(&periodic_sync_event_callbacks_), timeout);
}

void ServiceWorkerGlobalScope::StartPeriodicSyncEvent(
    String tag,
    DispatchPeriodicSyncEventCallback callback,
    int event_id) {
  DCHECK(IsContextThread());
  periodic_sync_event_callbacks_.Set(event_id, std::move(callback));
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
  event_queue_->EnqueueNormal(
      WTF::Bind(&ServiceWorkerGlobalScope::StartAbortPaymentEvent,
                WrapWeakPersistent(this), std::move(response_callback),
                std::move(callback)),
      CreateAbortCallback(&abort_payment_event_callbacks_), base::nullopt);
}

void ServiceWorkerGlobalScope::StartAbortPaymentEvent(
    mojo::PendingRemote<payments::mojom::blink::PaymentHandlerResponseCallback>
        response_callback,
    DispatchAbortPaymentEventCallback callback,
    int event_id) {
  DCHECK(IsContextThread());
  abort_payment_event_callbacks_.Set(event_id, std::move(callback));
  HeapMojoRemote<payments::mojom::blink::PaymentHandlerResponseCallback,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      remote(this);
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
  event_queue_->EnqueueNormal(
      WTF::Bind(&ServiceWorkerGlobalScope::StartCanMakePaymentEvent,
                WrapWeakPersistent(this), std::move(event_data),
                std::move(response_callback), std::move(callback)),
      CreateAbortCallback(&can_make_payment_event_callbacks_), base::nullopt);
}

void ServiceWorkerGlobalScope::StartCanMakePaymentEvent(
    payments::mojom::blink::CanMakePaymentEventDataPtr event_data,
    mojo::PendingRemote<payments::mojom::blink::PaymentHandlerResponseCallback>
        response_callback,
    DispatchCanMakePaymentEventCallback callback,
    int event_id) {
  DCHECK(IsContextThread());
  can_make_payment_event_callbacks_.Set(event_id, std::move(callback));
  HeapMojoRemote<payments::mojom::blink::PaymentHandlerResponseCallback,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      remote(this);
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
  event_queue_->EnqueueNormal(
      WTF::Bind(&ServiceWorkerGlobalScope::StartPaymentRequestEvent,
                WrapWeakPersistent(this), std::move(event_data),
                std::move(response_callback), std::move(callback)),
      CreateAbortCallback(&payment_request_event_callbacks_), base::nullopt);
}

void ServiceWorkerGlobalScope::StartPaymentRequestEvent(
    payments::mojom::blink::PaymentRequestEventDataPtr event_data,
    mojo::PendingRemote<payments::mojom::blink::PaymentHandlerResponseCallback>
        response_callback,
    DispatchPaymentRequestEventCallback callback,
    int event_id) {
  DCHECK(IsContextThread());
  payment_request_event_callbacks_.Set(event_id, std::move(callback));
  HeapMojoRemote<payments::mojom::blink::PaymentHandlerResponseCallback,
                 HeapMojoWrapperMode::kWithoutContextObserver>
      remote(this);
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

  mojo::PendingRemote<payments::mojom::blink::PaymentHandlerHost>
      payment_handler_host = std::move(event_data->payment_handler_host);
  Event* event = PaymentRequestEvent::Create(
      event_type_names::kPaymentrequest,
      PaymentEventDataConversion::ToPaymentRequestEventInit(
          ScriptController()->GetScriptState(), std::move(event_data)),
      std::move(payment_handler_host), respond_with_observer,
      wait_until_observer,
      ExecutionContext::From(ScriptController()->GetScriptState()));

  DispatchExtendableEventWithRespondWith(event, wait_until_observer,
                                         respond_with_observer);
}

void ServiceWorkerGlobalScope::DispatchCookieChangeEvent(
    network::mojom::blink::CookieChangeInfoPtr change,
    DispatchCookieChangeEventCallback callback) {
  event_queue_->EnqueueNormal(
      WTF::Bind(&ServiceWorkerGlobalScope::StartCookieChangeEvent,
                WrapWeakPersistent(this), std::move(change),
                std::move(callback)),
      CreateAbortCallback(&cookie_change_event_callbacks_), base::nullopt);
}

void ServiceWorkerGlobalScope::StartCookieChangeEvent(
    network::mojom::blink::CookieChangeInfoPtr change,
    DispatchCookieChangeEventCallback callback,
    int event_id) {
  DCHECK(IsContextThread());
  cookie_change_event_callbacks_.Set(event_id, std::move(callback));
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
  //               (changed.IsEmpty() && deleted.IsEmpty()).

  // TODO(pwnall): Investigate dispatching this on cookieStore.
  DispatchExtendableEvent(event, observer);
}

void ServiceWorkerGlobalScope::DispatchContentDeleteEvent(
    const String& id,
    DispatchContentDeleteEventCallback callback) {
  event_queue_->EnqueueNormal(
      WTF::Bind(&ServiceWorkerGlobalScope::StartContentDeleteEvent,
                WrapWeakPersistent(this), id, std::move(callback)),
      CreateAbortCallback(&content_delete_callbacks_), base::nullopt);
}

void ServiceWorkerGlobalScope::StartContentDeleteEvent(
    String id,
    DispatchContentDeleteEventCallback callback,
    int event_id) {
  DCHECK(IsContextThread());
  content_delete_callbacks_.Set(event_id, std::move(callback));
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

void ServiceWorkerGlobalScope::AddMessageToConsole(
    mojom::blink::ConsoleMessageLevel level,
    const String& message) {
  AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
      mojom::ConsoleMessageSource::kOther, level, message,
      SourceLocation::Capture(/* url= */ "", /* line_number= */ 0,
                              /* column_number= */ 0)));
}

void ServiceWorkerGlobalScope::NoteNewFetchEvent(const KURL& request_url) {
  auto it = unresponded_fetch_event_counts_.find(request_url);
  if (it == unresponded_fetch_event_counts_.end()) {
    unresponded_fetch_event_counts_.insert(request_url, 1);
  } else {
    it->value += 1;
  }
}

void ServiceWorkerGlobalScope::NoteRespondedToFetchEvent(
    const KURL& request_url) {
  auto it = unresponded_fetch_event_counts_.find(request_url);
  DCHECK_GE(it->value, 1);
  it->value -= 1;
  if (it->value == 0)
    unresponded_fetch_event_counts_.erase(it);
}

void ServiceWorkerGlobalScope::RecordQueuingTime(base::TimeTicks created_time) {
  base::UmaHistogramMediumTimes(
      "ServiceWorker.FetchEvent.QueuingTime",
      base::TimeTicks::Now() - created_time);
}

}  // namespace blink
