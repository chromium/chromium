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

#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/bindings/core/v8/callback_promise_adapter.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fetch/global_fetch.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/worker_inspector_controller.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/installed_scripts_manager.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worker_module_tree_client.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/service_worker/respond_with_observer.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_clients.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_global_scope_client.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_registration.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_script_cached_metadata_handler.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_thread.h"
#include "third_party/blink/renderer/modules/service_worker/wait_until_observer.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_response_headers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

namespace {

void DidSkipWaiting(ScriptPromiseResolver* resolver, bool success) {
  if (!resolver->GetExecutionContext() ||
      resolver->GetExecutionContext()->IsContextDestroyed())
    return;
  // Per spec the promise returned by skipWaiting() can never reject.
  if (!success)
    return;
  resolver->Resolve();
}

}  // namespace

ServiceWorkerGlobalScope* ServiceWorkerGlobalScope::Create(
    ServiceWorkerThread* thread,
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    mojom::blink::CacheStoragePtrInfo cache_storage_info,
    base::TimeTicks time_origin) {
  // If the script is being loaded via script streaming, the script is not yet
  // loaded.
  if (thread->GetInstalledScriptsManager() &&
      thread->GetInstalledScriptsManager()->IsScriptInstalled(
          creation_params->script_url)) {
    // CSP headers, referrer policy, and origin trial tokens will be provided by
    // the InstalledScriptsManager in EvaluateClassicScript().
    DCHECK(creation_params->content_security_policy_parsed_headers.IsEmpty());
    DCHECK_EQ(kReferrerPolicyDefault, creation_params->referrer_policy);
    DCHECK(creation_params->origin_trial_tokens->IsEmpty());
  }
  return new ServiceWorkerGlobalScope(std::move(creation_params), thread,
                                      std::move(cache_storage_info),
                                      time_origin);
}

ServiceWorkerGlobalScope::ServiceWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    ServiceWorkerThread* thread,
    mojom::blink::CacheStoragePtrInfo cache_storage_info,
    base::TimeTicks time_origin)
    : WorkerGlobalScope(std::move(creation_params), thread, time_origin),
      cache_storage_info_(std::move(cache_storage_info)) {}

ServiceWorkerGlobalScope::~ServiceWorkerGlobalScope() = default;

void ServiceWorkerGlobalScope::ReadyToEvaluateScript() {
  DCHECK(!evaluate_script_ready_);
  evaluate_script_ready_ = true;
  if (evaluate_script_)
    std::move(evaluate_script_).Run();
}

bool ServiceWorkerGlobalScope::ShouldInstallV8Extensions() const {
  return Platform::Current()->AllowScriptExtensionForServiceWorker(Url());
}

void ServiceWorkerGlobalScope::EvaluateClassicScript(
    const KURL& script_url,
    AccessControlStatus access_control_status,
    String source_code,
    std::unique_ptr<Vector<char>> cached_meta_data) {
  DCHECK(IsContextThread());

  if (!evaluate_script_ready_) {
    evaluate_script_ =
        WTF::Bind(&ServiceWorkerGlobalScope::EvaluateClassicScript,
                  WrapWeakPersistent(this), script_url, access_control_status,
                  std::move(source_code), std::move(cached_meta_data));
    return;
  }

  // Receive the main script via script streaming if needed.
  InstalledScriptsManager* installed_scripts_manager =
      GetThread()->GetInstalledScriptsManager();
  if (installed_scripts_manager &&
      installed_scripts_manager->IsScriptInstalled(script_url)) {
    // GetScriptData blocks until the script is received from the browser.
    std::unique_ptr<InstalledScriptsManager::ScriptData> script_data =
        installed_scripts_manager->GetScriptData(script_url);
    if (!script_data) {
      close();
      return;
    }

    DCHECK(source_code.IsEmpty());
    DCHECK(!cached_meta_data);
    source_code = script_data->TakeSourceText();
    cached_meta_data = script_data->TakeMetaData();

    base::Optional<ContentSecurityPolicyResponseHeaders>
        content_security_policy_raw_headers =
            script_data->GetContentSecurityPolicyResponseHeaders();
    ApplyContentSecurityPolicyFromHeaders(
        content_security_policy_raw_headers.value());

    String referrer_policy = script_data->GetReferrerPolicy();
    if (!referrer_policy.IsNull())
      ParseAndSetReferrerPolicy(referrer_policy);

    std::unique_ptr<Vector<String>> origin_trial_tokens =
        script_data->CreateOriginTrialTokens();
    OriginTrialContext::AddTokens(this, origin_trial_tokens.get());

    ReportingProxy().DidLoadInstalledScript();
  }

  WorkerGlobalScope::EvaluateClassicScript(script_url, access_control_status,
                                           source_code,
                                           std::move(cached_meta_data));
}

void ServiceWorkerGlobalScope::ImportModuleScript(
    const KURL& module_url_record,
    FetchClientSettingsObjectSnapshot* outside_settings_object,
    network::mojom::FetchCredentialsMode credentials_mode) {
  Modulator* modulator = Modulator::From(ScriptController()->GetScriptState());

  ModuleScriptCustomFetchType fetch_type =
      ModuleScriptCustomFetchType::kWorkerConstructor;
  InstalledScriptsManager* installed_scripts_manager =
      GetThread()->GetInstalledScriptsManager();
  if (installed_scripts_manager &&
      installed_scripts_manager->IsScriptInstalled(module_url_record)) {
    fetch_type = ModuleScriptCustomFetchType::kInstalledServiceWorker;
  }

  FetchModuleScript(module_url_record, outside_settings_object,
                    mojom::RequestContextType::SERVICE_WORKER, credentials_mode,
                    fetch_type, new WorkerModuleTreeClient(modulator));
}

void ServiceWorkerGlobalScope::Dispose() {
  DCHECK(IsContextThread());
  ServiceWorkerGlobalScopeClient::From(GetExecutionContext())
      ->WillDestroyWorkerContext();
  WorkerGlobalScope::Dispose();
}

void ServiceWorkerGlobalScope::CountWorkerScript(size_t script_size,
                                                 size_t cached_metadata_size) {
  DCHECK_EQ(GetScriptType(), ScriptType::kClassic);
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, script_size_histogram,
      ("ServiceWorker.ScriptSize", 1000, 5000000, 50));
  script_size_histogram.Count(
      base::saturated_cast<base::Histogram::Sample>(script_size));

  if (cached_metadata_size) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, script_cached_metadata_size_histogram,
        ("ServiceWorker.ScriptCachedMetadataSize", 1000, 50000000, 50));
    script_cached_metadata_size_histogram.Count(
        base::saturated_cast<base::Histogram::Sample>(cached_metadata_size));
  }

  CountScriptInternal(script_size, cached_metadata_size);
}

void ServiceWorkerGlobalScope::CountImportedScript(
    size_t script_size,
    size_t cached_metadata_size) {
  DCHECK_EQ(GetScriptType(), ScriptType::kClassic);
  CountScriptInternal(script_size, cached_metadata_size);
}

void ServiceWorkerGlobalScope::DidEvaluateScript() {
  DCHECK(!did_evaluate_script_);
  did_evaluate_script_ = true;

  // Skip recording UMAs for module scripts because there're no ways to get the
  // number of static-imported scripts and the total size of the imported
  // scripts.
  if (GetScriptType() == ScriptType::kModule) {
    return;
  }

  // TODO(asamidoi,nhiroki): Record the UMAs for module scripts, or remove them
  // if they're no longer used.
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, script_count_histogram,
                                  ("ServiceWorker.ScriptCount", 1, 1000, 50));
  script_count_histogram.Count(
      base::saturated_cast<base::Histogram::Sample>(script_count_));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, script_total_size_histogram,
      ("ServiceWorker.ScriptTotalSize", 1000, 5000000, 50));
  script_total_size_histogram.Count(
      base::saturated_cast<base::Histogram::Sample>(script_total_size_));
  if (script_cached_metadata_total_size_) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, cached_metadata_histogram,
        ("ServiceWorker.ScriptCachedMetadataTotalSize", 1000, 50000000, 50));
    cached_metadata_histogram.Count(
        base::saturated_cast<base::Histogram::Sample>(
            script_cached_metadata_total_size_));
  }
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

ScriptPromise ServiceWorkerGlobalScope::skipWaiting(ScriptState* script_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  // FIXME: short-term fix, see details at:
  // https://codereview.chromium.org/535193002/.
  if (!execution_context)
    return ScriptPromise();

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ServiceWorkerGlobalScopeClient::From(execution_context)
      ->SkipWaiting(WTF::Bind(&DidSkipWaiting, WrapPersistent(resolver)));
  return resolver->Promise();
}

void ServiceWorkerGlobalScope::BindServiceWorkerHost(
    mojom::blink::ServiceWorkerHostAssociatedPtrInfo service_worker_host) {
  ServiceWorkerGlobalScopeClient::From(GetExecutionContext())
      ->BindServiceWorkerHost(std::move(service_worker_host));
}

void ServiceWorkerGlobalScope::SetRegistration(
    WebServiceWorkerRegistrationObjectInfo info) {
  if (!GetExecutionContext())
    return;
  registration_ =
      new ServiceWorkerRegistration(GetExecutionContext(), std::move(info));
}

ServiceWorker* ServiceWorkerGlobalScope::GetOrCreateServiceWorker(
    WebServiceWorkerObjectInfo info) {
  if (info.version_id == mojom::blink::kInvalidServiceWorkerVersionId)
    return nullptr;
  ServiceWorker* worker = service_worker_objects_.at(info.version_id);
  if (!worker) {
    worker = new ServiceWorker(this, std::move(info));
    service_worker_objects_.Set(info.version_id, worker);
  }
  return worker;
}

bool ServiceWorkerGlobalScope::AddEventListenerInternal(
    const AtomicString& event_type,
    EventListener* listener,
    const AddEventListenerOptionsResolved& options) {
  if (did_evaluate_script_) {
    String message = String::Format(
        "Event handler of '%s' event must be added on the initial evaluation "
        "of worker script.",
        event_type.Utf8().data());
    AddConsoleMessage(ConsoleMessage::Create(kJSMessageSource,
                                             kWarningMessageLevel, message));
  }
  return WorkerGlobalScope::AddEventListenerInternal(event_type, listener,
                                                     options);
}

const AtomicString& ServiceWorkerGlobalScope::InterfaceName() const {
  return EventTargetNames::ServiceWorkerGlobalScope;
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

void ServiceWorkerGlobalScope::Trace(blink::Visitor* visitor) {
  visitor->Trace(clients_);
  visitor->Trace(registration_);
  visitor->Trace(service_worker_objects_);
  WorkerGlobalScope::Trace(visitor);
}

void ServiceWorkerGlobalScope::importScripts(const Vector<String>& urls,
                                             ExceptionState& exception_state) {
  InstalledScriptsManager* installed_scripts_manager =
      GetThread()->GetInstalledScriptsManager();
  for (auto& url : urls) {
    KURL completed_url = CompleteURL(url);
    // Bust the MemoryCache to ensure script requests reach the browser-side
    // and get added to and retrieved from the ServiceWorker's script cache.
    // FIXME: Revisit in light of the solution to crbug/388375.
    RemoveURLFromMemoryCache(completed_url);

    if (installed_scripts_manager &&
        !installed_scripts_manager->IsScriptInstalled(completed_url)) {
      DCHECK(installed_scripts_manager->IsScriptInstalled(Url()));
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
    const Vector<char>* meta_data) {
  return ServiceWorkerScriptCachedMetadataHandler::Create(this, script_url,
                                                          meta_data);
}

ScriptPromise ServiceWorkerGlobalScope::fetch(ScriptState* script_state,
                                              const RequestInfo& input,
                                              const RequestInit& init,
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

  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, script_size_histogram,
      ("ServiceWorker.CacheStorageInstalledScript.ScriptSize", 1000, 5000000,
       50));
  script_size_histogram.Count(
      base::saturated_cast<base::Histogram::Sample>(script_size));

  if (script_metadata_size) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, script_metadata_size_histogram,
        ("ServiceWorker.CacheStorageInstalledScript.CachedMetadataSize", 1000,
         50000000, 50));
    script_metadata_size_histogram.Count(
        base::saturated_cast<base::Histogram::Sample>(script_metadata_size));
  }
}

void ServiceWorkerGlobalScope::SetIsInstalling(bool is_installing) {
  is_installing_ = is_installing;
  if (is_installing)
    return;

  // Installing phase is finished; record the stats for the scripts that are
  // stored in Cache storage during installation.
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, cache_storage_installed_script_count_histogram,
      ("ServiceWorker.CacheStorageInstalledScript.Count", 1, 1000, 50));
  cache_storage_installed_script_count_histogram.Count(
      base::saturated_cast<base::Histogram::Sample>(
          cache_storage_installed_script_count_));
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      CustomCountHistogram, cache_storage_installed_script_total_size_histogram,
      ("ServiceWorker.CacheStorageInstalledScript.ScriptTotalSize", 1000,
       50000000, 50));
  cache_storage_installed_script_total_size_histogram.Count(
      base::saturated_cast<base::Histogram::Sample>(
          cache_storage_installed_script_total_size_));

  if (cache_storage_installed_script_metadata_total_size_) {
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram,
        cache_storage_installed_script_metadata_total_size_histogram,
        ("ServiceWorker.CacheStorageInstalledScript.CachedMetadataTotalSize",
         1000, 50000000, 50));
    cache_storage_installed_script_metadata_total_size_histogram.Count(
        base::saturated_cast<base::Histogram::Sample>(
            cache_storage_installed_script_metadata_total_size_));
  }
}

mojom::blink::CacheStoragePtrInfo ServiceWorkerGlobalScope::TakeCacheStorage() {
  return std::move(cache_storage_info_);
}

}  // namespace blink
