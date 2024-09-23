/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/workers/worker_global_scope.h"

#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/typed_macros.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "third_party/blink/public/common/privacy_budget/identifiability_study_settings.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom-blink.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_function.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/css/font_face_set_worker.h"
#include "third_party/blink/renderer/core/css/offscreen_font_selector.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_type_names.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/font_matching_metrics.h"
#include "third_party/blink/renderer/core/frame/user_activation.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/inspector/worker_inspector_controller.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script_url.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy_factory.h"
#include "third_party/blink/renderer/core/workers/custom_event_message.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/installed_scripts_manager.h"
#include "third_party/blink/renderer/core/workers/worker_classic_script_loader.h"
#include "third_party/blink/renderer/core/workers/worker_location.h"
#include "third_party/blink/renderer/core/workers/worker_navigator.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_parsers.h"
#include "third_party/blink/renderer/platform/scheduler/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/virtual_time_controller.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_args.h"

namespace blink {
namespace {

void RemoveURLFromMemoryCacheInternal(const KURL& url) {
  MemoryCache::Get()->RemoveURLFromCache(url);
}

scoped_refptr<SecurityOrigin> CreateSecurityOrigin(
    GlobalScopeCreationParams* creation_params,
    bool is_service_worker_global_scope) {
  // A worker environment settings object's origin must be set as follows:
  //
  // - DedicatedWorkers and SharedWorkers
  // https://html.spec.whatwg.org/C/#set-up-a-worker-environment-settings-object
  // Step 2: Let inherited origin be outside settings's origin.
  // Step 6: Let settings object be a new environment settings object whose
  // algorithms are defined as follows:
  // The origin -> Return a unique opaque origin if worker global scope's url's
  // scheme is "data", and inherited origin otherwise. [spec text]
  //
  // - ServiceWorkers
  // https://w3c.github.io/ServiceWorker/#run-service-worker-algorithm
  // Step 7.4: Let settingsObject be a new environment settings object whose
  // algorithms are defined as follows:
  // The origin -> Return its registering service worker client's origin.
  // [spec text]
  //
  // The algorithm in ServiceWorkers differ from DedicatedWorkers and
  // SharedWorkers when worker global scope's url's scheme is "data", but
  // "data" url script is not allowed for ServiceWorkers, so all workers' origin
  // can be calculated in the same way.
  // https://w3c.github.io/ServiceWorker/#start-register
  // Step 3: If scriptURL’s scheme is not one of "http" and "https", reject
  // promise with a TypeError and abort these steps. [spec text]
  DCHECK(!is_service_worker_global_scope ||
         !KURL(creation_params->script_url).ProtocolIsData());

  scoped_refptr<SecurityOrigin> security_origin;
  if (KURL(creation_params->script_url).ProtocolIsData()) {
    // Workers with data: URL should use a new, unique opaque origin per spec:
    // https://html.spec.whatwg.org/multipage/workers.html#script-settings-for-workers:concept-settings-object-origin-2
    // We use the `origin_to_use`, which is pre-calculated and passed down from
    // the browser process instead of newly calculated here, since the browser
    // side needs to know the exact opaque origin used in the renderer.
    CHECK(creation_params->origin_to_use);
    security_origin = creation_params->origin_to_use->IsolatedCopy();
  } else {
    // TODO(https://crbug.com/1058305) Inherit |agent_cluster_id_| for dedicated
    // workers. DO NOT inherit for shared workers and service workers.
    //
    // Create a new SecurityOrigin via CreateFromUrlOrigin() so that worker's
    // origin can avoid inheriting unnecessary capabilities from the starter
    // origin, while the worker's origin inherits url:Origin's internal nonce.
    security_origin = SecurityOrigin::CreateFromUrlOrigin(
        creation_params->starter_origin->ToUrlOrigin());
  }

  if (creation_params->starter_origin) {
    security_origin->TransferPrivilegesFrom(
        creation_params->starter_origin->CreatePrivilegeData());
  }
  return security_origin;
}

}  // namespace

FontFaceSet* WorkerGlobalScope::fonts() {
  return FontFaceSetWorker::From(*this);
}

WorkerGlobalScope::~WorkerGlobalScope() {
  DCHECK(!ScriptController());
  InstanceCounters::DecrementCounter(
      InstanceCounters::kWorkerGlobalScopeCounter);
}

NOINLINE const KURL& WorkerGlobalScope::Url() const {
  CHECK(url_.IsValid());
  return url_;
}

KURL WorkerGlobalScope::CompleteURL(const String& url) const {
  // Always return a null URL when passed a null string.
  // FIXME: Should we change the KURL constructor to have this behavior?
  if (url.IsNull())
    return KURL();
  // Always use UTF-8 in Workers.
  return KURL(BaseURL(), url);
}

const KURL& WorkerGlobalScope::BaseURL() const {
  return Url();
}

scheduler::WorkerScheduler* WorkerGlobalScope::GetScheduler() {
  DCHECK(IsContextThread());
  return GetThread()->GetScheduler();
}

void WorkerGlobalScope::Dispose() {
  DCHECK(IsContextThread());
  loading_virtual_time_pauser_ = WebScopedVirtualTimePauser();
  closing_ = true;
  if (font_matching_metrics_) {
    font_matching_metrics_->PublishAllMetrics();
  }
  WorkerOrWorkletGlobalScope::Dispose();
}

const base::UnguessableToken& WorkerGlobalScope::GetDevToolsToken() const {
  return GetThread()->GetDevToolsWorkerToken();
}

void WorkerGlobalScope::ExceptionUnhandled(int exception_id) {
  ErrorEvent* event = pending_error_events_.Take(exception_id);
  DCHECK(event);
  if (WorkerThreadDebugger* debugger =
          WorkerThreadDebugger::From(GetThread()->GetIsolate()))
    debugger->ExceptionThrown(thread_, event);
}

WorkerLocation* WorkerGlobalScope::location() const {
  if (!location_)
    location_ = MakeGarbageCollected<WorkerLocation>(Url());
  return location_.Get();
}

WorkerNavigator* WorkerGlobalScope::navigator() const {
  if (!navigator_)
    navigator_ = MakeGarbageCollected<WorkerNavigator>(GetExecutionContext());
  return navigator_.Get();
}

void WorkerGlobalScope::close() {
  // Let current script run to completion, but tell the worker micro task
  // runner to tear down the thread after this task.
  closing_ = true;
}

String WorkerGlobalScope::origin() const {
  return GetSecurityOrigin()->ToString();
}

void WorkerGlobalScope::importScripts(const Vector<String>& urls) {
  ImportScriptsInternal(urls);
}

namespace {

String NetworkErrorMessageAtImportScript(const KURL& url) {
  return "The script at '" + url.ElidedString() + "' failed to load.";
}

}  // namespace

// Implementation of the "import scripts into worker global scope" algorithm:
// https://html.spec.whatwg.org/C/#import-scripts-into-worker-global-scope
void WorkerGlobalScope::ImportScriptsInternal(const Vector<String>& urls) {
  DCHECK(GetContentSecurityPolicy());
  DCHECK(GetExecutionContext());
  v8::Isolate* isolate = GetThread()->GetIsolate();

  // Step 1: "If worker global scope's type is "module", throw a TypeError
  // exception."
  if (script_type_ == mojom::blink::ScriptType::kModule) {
    V8ThrowException::ThrowTypeError(
        isolate, "Module scripts don't support importScripts().");
    return;
  }

  // Step 2: "Let settings object be the current settings object."
  // |this| roughly corresponds to the current settings object.

  // Step 3: "If urls is empty, return."
  if (urls.empty())
    return;

  // Step 4: "Parse each value in urls relative to settings object. If any fail,
  // throw a "SyntaxError" DOMException."
  Vector<KURL> completed_urls;
  for (const String& url_string : urls) {
    const KURL& url = CompleteURL(url_string);
    if (!url.IsValid()) {
      V8ThrowException::ThrowException(
          isolate, V8ThrowDOMException::CreateOrEmpty(
                       isolate, DOMExceptionCode::kSyntaxError,
                       "The URL '" + url_string + "' is invalid."));
      return;
    }
    if (!GetContentSecurityPolicy()->AllowScriptFromSource(
            url, AtomicString(), IntegrityMetadataSet(), kNotParserInserted,
            url, RedirectStatus::kNoRedirect)) {
      V8ThrowException::ThrowException(
          isolate, V8ThrowDOMException::CreateOrEmpty(
                       isolate, DOMExceptionCode::kNetworkError,
                       NetworkErrorMessageAtImportScript(url)));
      return;
    }
    completed_urls.push_back(url);
  }

  // Step 5: "For each url in the resulting URL records, run these substeps:"
  for (const KURL& complete_url : completed_urls) {
    KURL response_url;
    String source_code;
    std::unique_ptr<Vector<uint8_t>> cached_meta_data;
    const String error_message =
        NetworkErrorMessageAtImportScript(complete_url);

    // Step 5.1: "Fetch a classic worker-imported script given url and settings
    // object, passing along any custom perform the fetch steps provided. If
    // this succeeds, let script be the result. Otherwise, rethrow the
    // exception."
    if (!FetchClassicImportedScript(complete_url, &response_url, &source_code,
                                    &cached_meta_data)) {
      // TODO(vogelheim): In case of certain types of failure - e.g. 'nosniff'
      // block - this ought to be a DOMExceptionCode::kSecurityError, but that
      // information presently gets lost on the way.
      V8ThrowException::ThrowException(
          isolate,
          V8ThrowDOMException::CreateOrEmpty(
              isolate, DOMExceptionCode::kNetworkError, error_message));
      return;
    }

    // importScripts always uses "no-cors", so simply checking the origin is
    // enough.
    // TODO(yhirano): Remove this ad-hoc logic and use the response type.
    const SanitizeScriptErrors sanitize_script_errors =
        GetSecurityOrigin()->CanReadContent(response_url)
            ? SanitizeScriptErrors::kDoNotSanitize
            : SanitizeScriptErrors::kSanitize;

    // https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-classic-worker-imported-script
    // Step 7: Let script be the result of creating a classic script given
    // source text, settings object, response's url, the default classic script
    // fetch options, and muted errors.
    // TODO(crbug.com/1082086): Fix the base URL.
    CachedMetadataHandler* handler(CreateWorkerScriptCachedMetadataHandler(
        complete_url, std::move(cached_meta_data)));
    ClassicScript* script = ClassicScript::Create(
        source_code, ClassicScript::StripFragmentIdentifier(complete_url),
        response_url /* base_url */, ScriptFetchOptions(),
        ScriptSourceLocationType::kUnknown, sanitize_script_errors, handler);

    // Step 5.2: "Run the classic script script, with the rethrow errors
    // argument set to true."
    v8::HandleScope scope(isolate);
    ScriptEvaluationResult result =
        script->RunScriptOnScriptStateAndReturnValue(
            ScriptController()->GetScriptState(),
            ExecuteScriptPolicy::kDoNotExecuteScriptWhenScriptsDisabled,
            V8ScriptRunner::RethrowErrorsOption::Rethrow(error_message));

    // Step 5.2: "If an exception was thrown or if the script was prematurely
    // aborted, then abort all these steps, letting the exception or aborting
    // continue to be processed by the calling script."
    if (result.GetResultType() != ScriptEvaluationResult::ResultType::kSuccess)
      return;
  }
}

// Implementation of the "fetch a classic worker-imported script" algorithm.
// https://html.spec.whatwg.org/C/#fetch-a-classic-worker-imported-script
bool WorkerGlobalScope::FetchClassicImportedScript(
    const KURL& script_url,
    KURL* out_response_url,
    String* out_source_code,
    std::unique_ptr<Vector<uint8_t>>* out_cached_meta_data) {
  ExecutionContext* execution_context = GetExecutionContext();
  WorkerClassicScriptLoader* classic_script_loader =
      MakeGarbageCollected<WorkerClassicScriptLoader>();
  classic_script_loader->LoadSynchronously(
      *execution_context, Fetcher(), script_url,
      mojom::blink::RequestContextType::SCRIPT,
      network::mojom::RequestDestination::kScript);
  if (classic_script_loader->Failed())
    return false;
  *out_response_url = classic_script_loader->ResponseURL();
  *out_source_code = classic_script_loader->SourceText();
  *out_cached_meta_data = classic_script_loader->ReleaseCachedMetadata();
  probe::ScriptImported(execution_context, classic_script_loader->Identifier(),
                        classic_script_loader->SourceText());
  return true;
}

bool WorkerGlobalScope::IsContextThread() const {
  return GetThread()->IsCurrentThread();
}

void WorkerGlobalScope::AddConsoleMessageImpl(ConsoleMessage* console_message,
                                              bool discard_duplicates) {
  DCHECK(IsContextThread());
  ReportingProxy().ReportConsoleMessage(
      console_message->GetSource(), console_message->GetLevel(),
      console_message->Message(), console_message->Location());
  GetThread()->GetConsoleMessageStorage()->AddConsoleMessage(
      this, console_message, discard_duplicates);
}

void WorkerGlobalScope::AddInspectorIssue(AuditsIssue issue) {
  GetThread()->GetInspectorIssueStorage()->AddInspectorIssue(this,
                                                             std::move(issue));
}

void WorkerGlobalScope::WillBeginLoading() {
  loading_virtual_time_pauser_ =
      GetScheduler()
          ->GetVirtualTimeController()
          ->CreateWebScopedVirtualTimePauser(
              "WorkerStart",
              WebScopedVirtualTimePauser::VirtualTaskDuration::kInstant);
  loading_virtual_time_pauser_.PauseVirtualTime();
}

CoreProbeSink* WorkerGlobalScope::GetProbeSink() {
  if (IsClosing())
    return nullptr;
  if (WorkerInspectorController* controller =
          GetThread()->GetWorkerInspectorController())
    return controller->GetProbeSink();
  return nullptr;
}

const BrowserInterfaceBrokerProxyImpl&
WorkerGlobalScope::GetBrowserInterfaceBroker() const {
  return browser_interface_broker_proxy_;
}

ExecutionContext* WorkerGlobalScope::GetExecutionContext() const {
  return const_cast<WorkerGlobalScope*>(this);
}

void WorkerGlobalScope::EvaluateClassicScript(
    const KURL& script_url,
    String source_code,
    std::unique_ptr<Vector<uint8_t>> cached_meta_data,
    const v8_inspector::V8StackTraceId& stack_id) {
  DCHECK(!IsContextPaused());

  CachedMetadataHandler* handler = CreateWorkerScriptCachedMetadataHandler(
      script_url, std::move(cached_meta_data));
  // Cross-origin workers are disallowed, so use
  // SanitizeScriptErrors::kDoNotSanitize.
  Script* worker_script = ClassicScript::Create(
      source_code, script_url, script_url /* base_url */, ScriptFetchOptions(),
      ScriptSourceLocationType::kUnknown, SanitizeScriptErrors::kDoNotSanitize,
      handler, TextPosition::MinimumPosition(),
      ScriptStreamer::NotStreamingReason::kWorkerTopLevelScript);
  WorkerScriptFetchFinished(*worker_script, stack_id);
}

void WorkerGlobalScope::WorkerScriptFetchFinished(
    Script& worker_script,
    std::optional<v8_inspector::V8StackTraceId> stack_id) {
  DCHECK(IsContextThread());
  TRACE_EVENT("blink.worker", "WorkerGlobalScope::WorkerScriptFetchFinished");

  DCHECK_NE(ScriptEvalState::kEvaluated, script_eval_state_);
  DCHECK(!worker_script_);
  worker_script_ = worker_script;
  stack_id_ = stack_id;

  // Proceed to RunWorkerScript() once WorkerScriptFetchFinished() is called and
  // |script_eval_state_| becomes kReadyToEvaluate.
  if (script_eval_state_ == ScriptEvalState::kReadyToEvaluate)
    RunWorkerScript();
}

void WorkerGlobalScope::ReadyToRunWorkerScript() {
  DCHECK(IsContextThread());

  DCHECK_EQ(ScriptEvalState::kPauseAfterFetch, script_eval_state_);
  script_eval_state_ = ScriptEvalState::kReadyToEvaluate;

  // Proceed to RunWorkerScript() once WorkerScriptFetchFinished() is called and
  // |script_eval_state_| becomes kReadyToEvaluate.
  if (worker_script_)
    RunWorkerScript();
}

// https://html.spec.whatwg.org/C/#run-a-worker
void WorkerGlobalScope::RunWorkerScript() {
  DCHECK(IsContextThread());
  DCHECK(!IsContextPaused());
  CHECK(GetExecutionContext()) << "crbug.com/1045818: attempted to evaluate "
                                  "script but no execution context";
  // If the context has already been destroyed, it should be a orphan worker.
  // It should be fine to close the worker.
  if (GetExecutionContext()->IsContextDestroyed()) {
    close();
    return;
  }

  DCHECK(worker_script_);
  DCHECK_EQ(script_eval_state_, ScriptEvalState::kReadyToEvaluate);
  TRACE_EVENT("blink.worker", "WorkerGlobalScope::RunWorkerScript");

  WorkerThreadDebugger* debugger =
      WorkerThreadDebugger::From(GetThread()->GetIsolate());
  if (debugger && stack_id_)
    debugger->ExternalAsyncTaskStarted(*stack_id_);

  ReportingProxy().WillEvaluateScript();
  loading_virtual_time_pauser_.UnpauseVirtualTime();

  // Step 24. If script is a classic script, then run the classic script script.
  // Otherwise, it is a module script; run the module script script. [spec text]
  bool is_success = false;
  if (ScriptState* script_state = ScriptController()->GetScriptState()) {
    v8::HandleScope handle_scope(script_state->GetIsolate());
    ScriptEvaluationResult result =
        std::move(worker_script_)
            ->RunScriptOnScriptStateAndReturnValue(script_state);
    switch (worker_script_->GetScriptType()) {
      case mojom::blink::ScriptType::kClassic:
        is_success = result.GetResultType() ==
                     ScriptEvaluationResult::ResultType::kSuccess;
        break;
      case mojom::blink::ScriptType::kModule:
        // Service workers prohibit async module graphs (those with top-level
        // await), so the promise result from executing a service worker module
        // is always settled. To maintain compatibility with synchronous module
        // graphs, rejected promises are considered synchronous failures in
        // service workers.
        //
        // https://w3c.github.io/ServiceWorker/#run-service-worker
        // Step 14.2-14.4 https://github.com/w3c/ServiceWorker/pull/1444
        if (IsServiceWorkerGlobalScope() &&
            result.GetResultType() ==
                ScriptEvaluationResult::ResultType::kSuccess) {
          v8::Local<v8::Promise> promise =
              result.GetSuccessValue().As<v8::Promise>();
          switch (promise->State()) {
            case v8::Promise::kFulfilled:
              is_success = true;
              break;
            case v8::Promise::kRejected:
              is_success = false;
              break;
            case v8::Promise::kPending:
              NOTREACHED_IN_MIGRATION();
              is_success = false;
              break;
          }
        } else {
          is_success = result.GetResultType() ==
                       ScriptEvaluationResult::ResultType::kSuccess;
        }
        break;
    }
  }
  ReportingProxy().DidEvaluateTopLevelScript(is_success);

  if (debugger && stack_id_)
    debugger->ExternalAsyncTaskFinished(*stack_id_);

  script_eval_state_ = ScriptEvalState::kEvaluated;
  TRACE_EVENT_NESTABLE_ASYNC_END0("blink.worker", "WorkerGlobalScope setup",
                                  TRACE_ID_LOCAL(this));
}

void WorkerGlobalScope::ReceiveMessage(BlinkTransferableMessage message) {
  DCHECK(!IsContextPaused());
  MessagePortArray* ports =
      MessagePort::EntanglePorts(*this, std::move(message.ports));
  WorkerThreadDebugger* debugger =
      WorkerThreadDebugger::From(GetThread()->GetIsolate());
  if (debugger) {
    debugger->ExternalAsyncTaskStarted(message.sender_stack_trace_id);
  }
  if (message.message->CanDeserializeIn(this)) {
    UserActivation* user_activation = nullptr;
    if (message.user_activation) {
      user_activation = MakeGarbageCollected<UserActivation>(
          message.user_activation->has_been_active,
          message.user_activation->was_active);
    }
    MessageEvent* message_event = MessageEvent::Create(
        ports, std::move(message.message), user_activation);
    message_event->SetTraceId(message.trace_id);
    TRACE_EVENT(
        "devtools.timeline", "HandlePostMessage", "data",
        [&](perfetto::TracedValue context) {
          inspector_handle_post_message_event::Data(
              std::move(context), GetExecutionContext(), *message_event);
        },
        perfetto::Flow::Global(message_event->GetTraceId()));
    DispatchEvent(*message_event);
  } else {
    DispatchEvent(*MessageEvent::CreateError());
  }

  if (debugger)
    debugger->ExternalAsyncTaskFinished(message.sender_stack_trace_id);
}

Event* WorkerGlobalScope::ReceiveCustomEventInternal(
    CrossThreadFunction<Event*(ScriptState*, CustomEventMessage)>
        event_factory_callback,
    CrossThreadFunction<Event*(ScriptState* script_state)>
        event_factory_error_callback,
    CustomEventMessage message) {
  if (!message.message || message.message->CanDeserializeIn(this)) {
    return event_factory_callback.Run(ScriptController()->GetScriptState(),
                                      std::move(message));
  } else if (event_factory_error_callback) {
    return event_factory_error_callback.Run(
        ScriptController()->GetScriptState());
  }
  return nullptr;
}

void WorkerGlobalScope::ReceiveCustomEvent(
    CrossThreadFunction<Event*(ScriptState*, CustomEventMessage)>
        event_factory_callback,
    CrossThreadFunction<Event*(ScriptState* script_state)>
        event_factory_error_callback,
    CustomEventMessage message) {
  CHECK(!IsContextPaused());
  auto* debugger = WorkerThreadDebugger::From(GetThread()->GetIsolate());
  if (debugger) {
    debugger->ExternalAsyncTaskStarted(message.sender_stack_trace_id);
  }
  Event* event = ReceiveCustomEventInternal(
      std::move(event_factory_callback),
      std::move(event_factory_error_callback), std::move(message));
  if (event) {
    DispatchEvent(*event);
  }
  if (debugger) {
    debugger->ExternalAsyncTaskFinished(message.sender_stack_trace_id);
  }
}

WorkerGlobalScope::WorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerThread* thread,
    base::TimeTicks time_origin,
    bool is_service_worker_global_scope)
    : WorkerOrWorkletGlobalScope(
          thread->GetIsolate(),
          CreateSecurityOrigin(creation_params.get(),
                               is_service_worker_global_scope),
          creation_params->starter_secure_context,
          MakeGarbageCollected<Agent>(
              thread->GetIsolate(),
              (creation_params->agent_cluster_id.is_empty()
                   ? base::UnguessableToken::Create()
                   : creation_params->agent_cluster_id),
              v8::MicrotaskQueue::New(thread->GetIsolate(),
                                      v8::MicrotasksPolicy::kScoped)),
          creation_params->global_scope_name,
          creation_params->parent_devtools_token,
          creation_params->v8_cache_options,
          creation_params->worker_clients,
          std::move(creation_params->content_settings_client),
          std::move(creation_params->web_worker_fetch_context),
          thread->GetWorkerReportingProxy(),
          creation_params->script_url.ProtocolIsData(),
          /*is_default_world_of_isolate=*/
          creation_params->is_default_world_of_isolate),
      ActiveScriptWrappable<WorkerGlobalScope>({}),
      script_type_(creation_params->script_type),
      user_agent_(creation_params->user_agent),
      ua_metadata_(creation_params->ua_metadata),
      thread_(thread),
      agent_group_scheduler_compositor_task_runner_(std::move(
          creation_params->agent_group_scheduler_compositor_task_runner)),
      time_origin_(time_origin),
      font_selector_(MakeGarbageCollected<OffscreenFontSelector>(this)),
      browser_interface_broker_proxy_(this),
      script_eval_state_(ScriptEvalState::kPauseAfterFetch),
      ukm_source_id_(creation_params->ukm_source_id),
      top_level_frame_security_origin_(
          std::move(creation_params->top_level_frame_security_origin)) {
  // Workers should always maintain the default world of an isolate.
  CHECK(creation_params->is_default_world_of_isolate);
  TRACE_EVENT("blink.worker", "WorkerGlobalScope::WorkerGlobalScope");
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("blink.worker", "WorkerGlobalScope setup",
                                    TRACE_ID_LOCAL(this));

  InstanceCounters::IncrementCounter(
      InstanceCounters::kWorkerGlobalScopeCounter);

  // https://html.spec.whatwg.org/C/#run-a-worker
  // 4. Set worker global scope's HTTPS state to response's HTTPS state. [spec
  // text]
  https_state_ = CalculateHttpsState(GetSecurityOrigin(),
                                     creation_params->starter_https_state);

  SetOutsideContentSecurityPolicies(
      std::move(creation_params->outside_content_security_policies));
  SetWorkerSettings(std::move(creation_params->worker_settings));

  // TODO(sammc): Require a valid |creation_params->browser_interface_broker|
  // once all worker types provide a valid
  // |creation_params->browser_interface_broker|.
  if (creation_params->browser_interface_broker.is_valid()) {
    browser_interface_broker_proxy_.Bind(
        ToCrossVariantMojoType(
            std::move(creation_params->browser_interface_broker)),
        GetTaskRunner(TaskType::kInternalDefault));
  }

  // A PermissionsPolicy is created by
  // PermissionsPolicy::CreateFromParentPolicy, even if the parent policy is
  // null.
  DCHECK(creation_params->worker_permissions_policy);
  GetSecurityContext().SetPermissionsPolicy(
      std::move(creation_params->worker_permissions_policy));

  // UKM recorder is needed in the Dispose() method but sometimes it is not
  // initialized by then because of a race problem.
  // If the Identifiability Study is enabled, we need the UKM recorder in any
  // case so it should not affect anything if we initialize it here.
  // TODO(crbug.com/1370978): Check if there is another fix instead of
  // initializing UKM Recorder here.
  if (blink::IdentifiabilityStudySettings::Get()->IsActive())
    UkmRecorder();
}

void WorkerGlobalScope::ExceptionThrown(ErrorEvent* event) {
  int next_id = ++last_pending_error_event_id_;
  pending_error_events_.Set(next_id, event);
  ReportingProxy().ReportException(event->MessageForConsole(),
                                   event->Location()->Clone(), next_id);
}

void WorkerGlobalScope::RemoveURLFromMemoryCache(const KURL& url) {
  // MemoryCache can be accessed only from the main thread.
  PostCrossThreadTask(
      *Thread::MainThread()->GetTaskRunner(MainThreadTaskRunnerRestricted()),
      FROM_HERE, CrossThreadBindOnce(&RemoveURLFromMemoryCacheInternal, url));
}

NOINLINE void WorkerGlobalScope::InitializeURL(const KURL& url) {
  CHECK(url_.IsNull());
  DCHECK(url.IsValid());
  if (GetSecurityOrigin()->IsOpaque()) {
    DCHECK(SecurityOrigin::Create(url)->IsOpaque());
  } else if (GetSecurityOrigin()->IsLocal()) {
    // SecurityOrigin::CanRequest called from CanReadContent has a special logic
    // for local origins, and the logic doesn't work here, so we have this
    // DCHECK instead.
    auto origin = SecurityOrigin::Create(url);
    DCHECK(origin->IsOpaque() || origin->IsLocal());
  } else {
    DCHECK(GetSecurityOrigin()->CanReadContent(url));
  }
  url_ = url;
}

void WorkerGlobalScope::SetWorkerMainScriptLoadingParametersForModules(
    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params_for_modules) {
  DCHECK(worker_main_script_load_params_for_modules);
  DCHECK(!worker_main_script_load_params_for_modules_);
  worker_main_script_load_params_for_modules_ =
      std::move(worker_main_script_load_params_for_modules);
}

void WorkerGlobalScope::queueMicrotask(V8VoidFunction* callback) {
  GetAgent()->event_loop()->EnqueueMicrotask(
      WTF::BindOnce(&V8VoidFunction::InvokeAndReportException,
                    WrapPersistent(callback), nullptr));
}

void WorkerGlobalScope::SetWorkerSettings(
    std::unique_ptr<WorkerSettings> worker_settings) {
  worker_settings_ = std::move(worker_settings);
  font_selector_->UpdateGenericFontFamilySettings(
      worker_settings_->GetGenericFontFamilySettings());
}

TrustedTypePolicyFactory* WorkerGlobalScope::GetTrustedTypes() const {
  if (!trusted_types_) {
    trusted_types_ =
        MakeGarbageCollected<TrustedTypePolicyFactory>(GetExecutionContext());
  }
  return trusted_types_.Get();
}

ukm::UkmRecorder* WorkerGlobalScope::UkmRecorder() {
  if (ukm_recorder_)
    return ukm_recorder_.get();

  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
  GetBrowserInterfaceBroker().GetInterface(
      factory.BindNewPipeAndPassReceiver());
  ukm_recorder_ = ukm::MojoUkmRecorder::Create(*factory);

  return ukm_recorder_.get();
}

std::unique_ptr<WorkerMainScriptLoadParameters>
WorkerGlobalScope::TakeWorkerMainScriptLoadingParametersForModules() {
  return std::move(worker_main_script_load_params_for_modules_);
}

void WorkerGlobalScope::Trace(Visitor* visitor) const {
  visitor->Trace(location_);
  visitor->Trace(navigator_);
  visitor->Trace(pending_error_events_);
  visitor->Trace(font_selector_);
  visitor->Trace(trusted_types_);
  visitor->Trace(worker_script_);
  visitor->Trace(browser_interface_broker_proxy_);
  WorkerOrWorkletGlobalScope::Trace(visitor);
  Supplementable<WorkerGlobalScope>::Trace(visitor);
}

bool WorkerGlobalScope::HasPendingActivity() const {
  return !ExecutionContext::IsContextDestroyed();
}

FontMatchingMetrics* WorkerGlobalScope::GetFontMatchingMetrics() {
  if (!font_matching_metrics_) {
    font_matching_metrics_ = std::make_unique<FontMatchingMetrics>(
        this, GetTaskRunner(TaskType::kInternalDefault));
  }
  return font_matching_metrics_.get();
}

CodeCacheHost* WorkerGlobalScope::GetCodeCacheHost() {
  if (!code_cache_host_) {
    // We may not have a valid browser interface in tests. For ex:
    // FakeWorkerGlobalScope doesn't provide a valid interface. These tests
    // don't rely on code caching so it's safe to return nullptr here.
    if (!GetBrowserInterfaceBroker().is_bound())
      return nullptr;
    mojo::Remote<mojom::blink::CodeCacheHost> remote;
    GetBrowserInterfaceBroker().GetInterface(
        remote.BindNewPipeAndPassReceiver());
    code_cache_host_ = std::make_unique<CodeCacheHost>(std::move(remote));
  }
  return code_cache_host_.get();
}

}  // namespace blink
