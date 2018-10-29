// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worker_or_worklet_global_scope.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/events/event_queue.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/modulescript/module_script_fetch_request.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

WorkerOrWorkletGlobalScope::WorkerOrWorkletGlobalScope(
    v8::Isolate* isolate,
    WorkerClients* worker_clients,
    WorkerReportingProxy& reporting_proxy)
    : worker_clients_(worker_clients),
      script_controller_(
          WorkerOrWorkletScriptController::Create(this, isolate)),
      reporting_proxy_(reporting_proxy),
      used_features_(static_cast<int>(WebFeature::kNumberOfFeatures)) {
  if (worker_clients_)
    worker_clients_->ReattachThread();
}

WorkerOrWorkletGlobalScope::~WorkerOrWorkletGlobalScope() = default;

// EventTarget
const AtomicString& WorkerOrWorkletGlobalScope::InterfaceName() const {
  NOTREACHED() << "Each global scope that uses events should define its own "
                  "interface name.";
  return g_null_atom;
}

v8::Local<v8::Object> WorkerOrWorkletGlobalScope::Wrap(
    v8::Isolate*,
    v8::Local<v8::Object> creation_context) {
  LOG(FATAL) << "WorkerOrWorkletGlobalScope must never be wrapped with wrap "
                "method. The global object of ECMAScript environment is used "
                "as the wrapper.";
  return v8::Local<v8::Object>();
}

v8::Local<v8::Object> WorkerOrWorkletGlobalScope::AssociateWithWrapper(
    v8::Isolate*,
    const WrapperTypeInfo*,
    v8::Local<v8::Object> wrapper) {
  LOG(FATAL) << "WorkerOrWorkletGlobalScope must never be wrapped with wrap "
                "method. The global object of ECMAScript environment is used "
                "as the wrapper.";
  return v8::Local<v8::Object>();
}

bool WorkerOrWorkletGlobalScope::HasPendingActivity() const {
  // The global scope wrapper is kept alive as longs as its execution context is
  // active.
  return !ExecutionContext::IsContextDestroyed();
}

void WorkerOrWorkletGlobalScope::CountFeature(WebFeature feature) {
  DCHECK(IsContextThread());
  DCHECK_NE(WebFeature::kOBSOLETE_PageDestruction, feature);
  DCHECK_GT(WebFeature::kNumberOfFeatures, feature);
  if (used_features_.QuickGet(static_cast<int>(feature)))
    return;
  used_features_.QuickSet(static_cast<int>(feature));
  ReportingProxy().CountFeature(feature);
}

void WorkerOrWorkletGlobalScope::CountDeprecation(WebFeature feature) {
  DCHECK(IsContextThread());
  DCHECK_NE(WebFeature::kOBSOLETE_PageDestruction, feature);
  DCHECK_GT(WebFeature::kNumberOfFeatures, feature);
  if (used_features_.QuickGet(static_cast<int>(feature)))
    return;
  used_features_.QuickSet(static_cast<int>(feature));

  // Adds a deprecation message to the console.
  DCHECK(!Deprecation::DeprecationMessage(feature).IsEmpty());
  AddConsoleMessage(
      ConsoleMessage::Create(kDeprecationMessageSource, kWarningMessageLevel,
                             Deprecation::DeprecationMessage(feature)));
  ReportingProxy().CountDeprecation(feature);
}

ResourceFetcher* WorkerOrWorkletGlobalScope::EnsureFetcher() {
  DCHECK(IsContextThread());
  if (resource_fetcher_)
    return resource_fetcher_;
  WorkerFetchContext* fetch_context = WorkerFetchContext::Create(*this);
  resource_fetcher_ = ResourceFetcher::Create(fetch_context);
  if (IsContextPaused())
    resource_fetcher_->SetDefersLoading(true);
  DCHECK(resource_fetcher_);
  return resource_fetcher_;
}

ResourceFetcher* WorkerOrWorkletGlobalScope::Fetcher() const {
  DCHECK(IsContextThread());
  DCHECK(resource_fetcher_);
  return resource_fetcher_;
}

bool WorkerOrWorkletGlobalScope::IsJSExecutionForbidden() const {
  return script_controller_->IsExecutionForbidden();
}

void WorkerOrWorkletGlobalScope::DisableEval(const String& error_message) {
  script_controller_->DisableEval(error_message);
}

bool WorkerOrWorkletGlobalScope::CanExecuteScripts(
    ReasonForCallingCanExecuteScripts) {
  return !IsJSExecutionForbidden();
}

void WorkerOrWorkletGlobalScope::Dispose() {
  DCHECK(script_controller_);

  RemoveAllEventListeners();

  script_controller_->Dispose();
  script_controller_.Clear();

  if (resource_fetcher_) {
    resource_fetcher_->StopFetching();
    resource_fetcher_->ClearContext();
  }
}

void WorkerOrWorkletGlobalScope::SetModulator(Modulator* modulator) {
  modulator_ = modulator;
}

scheduler::WorkerScheduler* WorkerOrWorkletGlobalScope::GetScheduler() {
  DCHECK(IsContextThread());
  return GetThread()->GetScheduler();
}

scoped_refptr<base::SingleThreadTaskRunner>
WorkerOrWorkletGlobalScope::GetTaskRunner(TaskType type) {
  DCHECK(IsContextThread());
  return GetThread()->GetTaskRunner(type);
}

void WorkerOrWorkletGlobalScope::InitContentSecurityPolicyFromVector(
    const Vector<CSPHeaderAndType>& headers) {
  if (!GetContentSecurityPolicy()) {
    ContentSecurityPolicy* csp = ContentSecurityPolicy::Create();
    SetContentSecurityPolicy(csp);
  }
  for (const auto& policy_and_type : headers) {
    GetContentSecurityPolicy()->DidReceiveHeader(
        policy_and_type.first, policy_and_type.second,
        kContentSecurityPolicyHeaderSourceHTTP);
  }
}

void WorkerOrWorkletGlobalScope::BindContentSecurityPolicyToExecutionContext() {
  DCHECK(IsContextThread());
  GetContentSecurityPolicy()->BindToExecutionContext(GetExecutionContext());
}

// Implementation of the "fetch a module worker script graph" algorithm in the
// HTML spec:
// https://html.spec.whatwg.org/multipage/webappapis.html#fetch-a-module-worker-script-tree
void WorkerOrWorkletGlobalScope::FetchModuleScript(
    const KURL& module_url_record,
    FetchClientSettingsObjectSnapshot* fetch_client_settings_object,
    mojom::RequestContextType destination,
    network::mojom::FetchCredentialsMode credentials_mode,
    ModuleScriptCustomFetchType custom_fetch_type,
    ModuleTreeClient* client) {
  // Step 2: "Let options be a script fetch options whose cryptographic nonce is
  // the empty string,
  String nonce;
  // integrity metadata is the empty string,
  String integrity_attribute;
  // parser metadata is "not-parser-inserted,
  ParserDisposition parser_state = kNotParserInserted;
  // credentials mode is credentials mode, and referrer policy is the empty
  // string."
  ScriptFetchOptions options(nonce, IntegrityMetadataSet(), integrity_attribute,
                             parser_state, credentials_mode,
                             kReferrerPolicyDefault);

  Modulator* modulator = Modulator::From(ScriptController()->GetScriptState());
  // Step 3. "Perform the internal module script graph fetching procedure ..."
  modulator->FetchTree(module_url_record, fetch_client_settings_object,
                       destination, options, custom_fetch_type, client);
}

void WorkerOrWorkletGlobalScope::TasksWerePaused() {
  ExecutionContext::TasksWerePaused();
  if (resource_fetcher_)
    resource_fetcher_->SetDefersLoading(true);
}

void WorkerOrWorkletGlobalScope::TasksWereUnpaused() {
  ExecutionContext::TasksWereUnpaused();
  if (resource_fetcher_)
    resource_fetcher_->SetDefersLoading(false);
}

void WorkerOrWorkletGlobalScope::Trace(blink::Visitor* visitor) {
  visitor->Trace(resource_fetcher_);
  visitor->Trace(script_controller_);
  visitor->Trace(modulator_);
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContext::Trace(visitor);
  SecurityContext::Trace(visitor);
}

}  // namespace blink
