// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"

#include <memory>
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_source_code.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/core/workers/worklet_module_tree_client.h"
#include "third_party/blink/renderer/core/workers/worklet_pending_tasks.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"

namespace blink {

WorkletGlobalScope::WorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy,
    LocalFrame* frame,
    Agent* agent)
    : WorkletGlobalScope(std::move(creation_params),
                         reporting_proxy,
                         ToIsolate(frame),
                         ThreadType::kMainThread,
                         frame,
                         nullptr /* worker_thread */,
                         agent) {}

WorkletGlobalScope::WorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy,
    WorkerThread* worker_thread)
    : WorkletGlobalScope(std::move(creation_params),
                         reporting_proxy,
                         worker_thread->GetIsolate(),
                         ThreadType::kOffMainThread,
                         nullptr /* frame */,
                         worker_thread,
                         nullptr /* agent */) {}

// Partial implementation of the "set up a worklet environment settings object"
// algorithm:
// https://drafts.css-houdini.org/worklets/#script-settings-for-worklets
WorkletGlobalScope::WorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy,
    v8::Isolate* isolate,
    ThreadType thread_type,
    LocalFrame* frame,
    WorkerThread* worker_thread,
    Agent* agent)
    : WorkerOrWorkletGlobalScope(
          isolate,
          SecurityOrigin::CreateUniqueOpaque(),
          // TODO(tzik): Assign an Agent for Worklets after
          // NonMainThreadScheduler gets ready to run microtasks.
          agent,
          creation_params->off_main_thread_fetch_option,
          creation_params->global_scope_name,
          creation_params->parent_devtools_token,
          creation_params->v8_cache_options,
          creation_params->worker_clients,
          std::move(creation_params->content_settings_client),
          std::move(creation_params->web_worker_fetch_context),
          reporting_proxy),
      url_(creation_params->script_url),
      user_agent_(creation_params->user_agent),
      document_security_origin_(creation_params->starter_origin),
      document_secure_context_(creation_params->starter_secure_context),
      module_responses_map_(creation_params->module_responses_map),
      // Step 4. "Let inheritedHTTPSState be outsideSettings's HTTPS state."
      https_state_(creation_params->starter_https_state),
      agent_cluster_id_(creation_params->agent_cluster_id.is_empty()
                            ? base::UnguessableToken::Create()
                            : creation_params->agent_cluster_id),
      thread_type_(thread_type),
      frame_(frame),
      worker_thread_(worker_thread) {
  DCHECK((thread_type_ == ThreadType::kMainThread && frame_) ||
         (thread_type_ == ThreadType::kOffMainThread && worker_thread_));

  // Step 2: "Let inheritedAPIBaseURL be outsideSettings's API base URL."
  // |url_| is the inheritedAPIBaseURL passed from the parent Document.

  // Step 5: "Let inheritedReferrerPolicy be outsideSettings's referrer policy."
  SetReferrerPolicy(creation_params->referrer_policy);

  SetOutsideContentSecurityPolicyHeaders(
      creation_params->outside_content_security_policy_headers);

  // https://drafts.css-houdini.org/worklets/#creating-a-workletglobalscope
  // Step 6: "Invoke the initialize a global object's CSP list algorithm given
  // workletGlobalScope."
  InitContentSecurityPolicyFromVector(
      creation_params->outside_content_security_policy_headers);
  BindContentSecurityPolicyToExecutionContext();

  OriginTrialContext::AddTokens(this,
                                creation_params->origin_trial_tokens.get());
}

WorkletGlobalScope::~WorkletGlobalScope() = default;

BrowserInterfaceBrokerProxy& WorkletGlobalScope::GetBrowserInterfaceBroker() {
  NOTIMPLEMENTED();
  return GetEmptyBrowserInterfaceBroker();
}

bool WorkletGlobalScope::IsMainThreadWorkletGlobalScope() const {
  return thread_type_ == ThreadType::kMainThread;
}

bool WorkletGlobalScope::IsThreadedWorkletGlobalScope() const {
  return thread_type_ == ThreadType::kOffMainThread;
}

ExecutionContext* WorkletGlobalScope::GetExecutionContext() const {
  return const_cast<WorkletGlobalScope*>(this);
}

bool WorkletGlobalScope::IsSecureContext(String& error_message) const {
  // Until there are APIs that are available in worklets and that
  // require a privileged context test that checks ancestors, just do
  // a simple check here.
  if (GetSecurityOrigin()->IsPotentiallyTrustworthy())
    return true;
  error_message = GetSecurityOrigin()->IsPotentiallyTrustworthyErrorMessage();
  return false;
}

bool WorkletGlobalScope::IsContextThread() const {
  if (IsMainThreadWorkletGlobalScope())
    return IsMainThread();
  return worker_thread_->IsCurrentThread();
}

void WorkletGlobalScope::AddConsoleMessageImpl(ConsoleMessage* console_message,
                                               bool discard_duplicates) {
  if (IsMainThreadWorkletGlobalScope()) {
    frame_->Console().AddMessage(console_message, discard_duplicates);
    return;
  }
  worker_thread_->GetWorkerReportingProxy().ReportConsoleMessage(
      console_message->Source(), console_message->Level(),
      console_message->Message(), console_message->Location());
  worker_thread_->GetConsoleMessageStorage()->AddConsoleMessage(
      worker_thread_->GlobalScope(), console_message, discard_duplicates);
}

void WorkletGlobalScope::ExceptionThrown(ErrorEvent* error_event) {
  if (IsMainThreadWorkletGlobalScope()) {
    MainThreadDebugger::Instance()->ExceptionThrown(this, error_event);
    return;
  }
  if (WorkerThreadDebugger* debugger =
          WorkerThreadDebugger::From(GetThread()->GetIsolate())) {
    debugger->ExceptionThrown(worker_thread_, error_event);
  }
}

void WorkletGlobalScope::Dispose() {
  frame_ = nullptr;
  worker_thread_ = nullptr;
  WorkerOrWorkletGlobalScope::Dispose();
}

WorkerThread* WorkletGlobalScope::GetThread() const {
  DCHECK(!IsMainThreadWorkletGlobalScope());
  return worker_thread_;
}

CoreProbeSink* WorkletGlobalScope::GetProbeSink() {
  if (IsMainThreadWorkletGlobalScope())
    return probe::ToCoreProbeSink(frame_);
  return nullptr;
}

scoped_refptr<base::SingleThreadTaskRunner> WorkletGlobalScope::GetTaskRunner(
    TaskType task_type) {
  if (IsMainThreadWorkletGlobalScope())
    return frame_->GetFrameScheduler()->GetTaskRunner(task_type);
  return worker_thread_->GetTaskRunner(task_type);
}

FrameOrWorkerScheduler* WorkletGlobalScope::GetScheduler() {
  DCHECK(IsContextThread());
  if (IsMainThreadWorkletGlobalScope())
    return frame_->GetFrameScheduler();
  return worker_thread_->GetScheduler();
}

LocalFrame* WorkletGlobalScope::GetFrame() const {
  DCHECK(IsMainThreadWorkletGlobalScope());
  return frame_;
}

// Implementation of the first half of the "fetch and invoke a worklet script"
// algorithm:
// https://drafts.css-houdini.org/worklets/#fetch-and-invoke-a-worklet-script
void WorkletGlobalScope::FetchAndInvokeScript(
    const KURL& module_url_record,
    network::mojom::CredentialsMode credentials_mode,
    const FetchClientSettingsObjectSnapshot& outside_settings_object,
    WorkerResourceTimingNotifier& outside_resource_timing_notifier,
    scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
    WorkletPendingTasks* pending_tasks) {
  DCHECK(IsContextThread());

  // Step 1: "Let insideSettings be the workletGlobalScope's associated
  // environment settings object."
  // Step 2: "Let script by the result of fetch a worklet script given
  // moduleURLRecord, moduleResponsesMap, credentialOptions, outsideSettings,
  // and insideSettings when it asynchronously completes."

  // Step 3 to 5 are implemented in
  // WorkletModuleTreeClient::NotifyModuleTreeLoadFinished.
  auto* client = MakeGarbageCollected<WorkletModuleTreeClient>(
      ScriptController()->GetScriptState(),
      std::move(outside_settings_task_runner), pending_tasks);

  // TODO(nhiroki): Pass an appropriate destination defined in each worklet
  // spec (e.g., "paint worklet", "audio worklet") (https://crbug.com/843980,
  // https://crbug.com/843982)
  auto destination = mojom::RequestContextType::SCRIPT;
  FetchModuleScript(module_url_record, outside_settings_object,
                    outside_resource_timing_notifier, destination,
                    credentials_mode,
                    ModuleScriptCustomFetchType::kWorkletAddModule, client);
}

KURL WorkletGlobalScope::CompleteURL(const String& url) const {
  // Always return a null URL when passed a null string.
  // TODO(ikilpatrick): Should we change the KURL constructor to have this
  // behavior?
  if (url.IsNull())
    return KURL();
  // Always use UTF-8 in Worklets.
  return KURL(BaseURL(), url);
}

void WorkletGlobalScope::BindContentSecurityPolicyToExecutionContext() {
  WorkerOrWorkletGlobalScope::BindContentSecurityPolicyToExecutionContext();

  // CSP checks should resolve self based on the 'fetch client settings object'
  // (i.e., the document's origin), not the 'module map settings object' (i.e.,
  // the opaque origin of this worklet global scope). The current implementation
  // doesn't have separate CSP objects for these two contexts. Therefore,
  // we initialize the worklet global scope's CSP object (which would naively
  // appear to be a CSP object for the 'module map settings object') entirely
  // based on state from the document (the origin and CSP headers it passed
  // here), and use the document's origin for 'self' CSP checks.
  GetContentSecurityPolicy()->SetupSelf(*document_security_origin_);
}

void WorkletGlobalScope::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  WorkerOrWorkletGlobalScope::Trace(visitor);
}

}  // namespace blink
