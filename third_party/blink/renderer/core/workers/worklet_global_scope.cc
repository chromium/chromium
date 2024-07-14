// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "services/metrics/public/cpp/mojo_ukm_recorder.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_storage.h"
#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"
#include "third_party/blink/renderer/core/inspector/worker_inspector_controller.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/modulator.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/core/workers/worklet_module_tree_client.h"
#include "third_party/blink/renderer/core/workers/worklet_pending_tasks.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"

namespace blink {

WorkletGlobalScope::WorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy,
    LocalFrame* frame)
    : WorkletGlobalScope(std::move(creation_params),
                         reporting_proxy,
                         ToIsolate(frame),
                         ThreadType::kMainThread,
                         frame,
                         nullptr /* worker_thread */) {}

WorkletGlobalScope::WorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy,
    WorkerThread* worker_thread)
    : WorkletGlobalScope(std::move(creation_params),
                         reporting_proxy,
                         worker_thread->GetIsolate(),
                         ThreadType::kOffMainThread,
                         nullptr /* frame */,
                         worker_thread) {}

// Partial implementation of the "set up a worklet environment settings object"
// algorithm:
// https://drafts.css-houdini.org/worklets/#script-settings-for-worklets
WorkletGlobalScope::WorkletGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    WorkerReportingProxy& reporting_proxy,
    v8::Isolate* isolate,
    ThreadType thread_type,
    LocalFrame* frame,
    WorkerThread* worker_thread)
    : WorkerOrWorkletGlobalScope(
          isolate,
          SecurityOrigin::CreateUniqueOpaque(),
          creation_params->starter_secure_context,
          MakeGarbageCollected<Agent>(
              isolate,
              creation_params->agent_cluster_id,
              v8::MicrotaskQueue::New(isolate, v8::MicrotasksPolicy::kScoped)),
          creation_params->global_scope_name,
          creation_params->parent_devtools_token,
          creation_params->v8_cache_options,
          creation_params->worker_clients,
          std::move(creation_params->content_settings_client),
          std::move(creation_params->web_worker_fetch_context),
          reporting_proxy,
          /*is_worker_loaded_from_data_url=*/false,
          /*is_default_world_of_isolate=*/
          creation_params->is_default_world_of_isolate),
      ActiveScriptWrappable<WorkletGlobalScope>({}),
      url_(creation_params->script_url),
      user_agent_(creation_params->user_agent),
      document_security_origin_(creation_params->starter_origin),
      module_responses_map_(creation_params->module_responses_map),
      // Step 4. "Let inheritedHTTPSState be outsideSettings's HTTPS state."
      https_state_(creation_params->starter_https_state),
      thread_type_(thread_type),
      frame_(frame),
      worker_thread_(worker_thread),
      // Worklets should often have a parent LocalFrameToken. Only shared
      // storage worklet does not have it.
      frame_token_(
          creation_params->parent_context_token
              ? creation_params->parent_context_token->GetAs<LocalFrameToken>()
              : blink::LocalFrameToken()),
      parent_cross_origin_isolated_capability_(
          creation_params->parent_cross_origin_isolated_capability),
      parent_is_isolated_context_(creation_params->parent_is_isolated_context) {
  DCHECK((thread_type_ == ThreadType::kMainThread && frame_) ||
         (thread_type_ == ThreadType::kOffMainThread && worker_thread_));

  // Default world implies that we are at least off main thread. Off main
  // thread may still have cases where threads are shared between multiple
  // worklets (and thus the Isolate may not be owned by this world)..
  CHECK(!creation_params->is_default_world_of_isolate ||
        thread_type == ThreadType::kOffMainThread);

  // Worklet should be in the owner's agent cluster.
  // https://html.spec.whatwg.org/C/#obtain-a-worklet-agent
  DCHECK(creation_params->agent_cluster_id ||
         !creation_params->parent_context_token);

  // Step 2: "Let inheritedAPIBaseURL be outsideSettings's API base URL."
  // |url_| is the inheritedAPIBaseURL passed from the parent Document.

  // Step 5: "Let inheritedReferrerPolicy be outsideSettings's referrer policy."
  SetReferrerPolicy(creation_params->referrer_policy);

  SetOutsideContentSecurityPolicies(
      mojo::Clone(creation_params->outside_content_security_policies));

  // https://drafts.css-houdini.org/worklets/#creating-a-workletglobalscope
  // Step 6: "Invoke the initialize a global object's CSP list algorithm given
  // workletGlobalScope."
  InitContentSecurityPolicyFromVector(
      std::move(creation_params->outside_content_security_policies));
  BindContentSecurityPolicyToExecutionContext();

  OriginTrialContext::ActivateWorkerInheritedFeatures(
      this, creation_params->inherited_trial_features.get());

  // WorkletGlobalScopes are not currently provided with UKM source IDs.
  DCHECK_EQ(creation_params->ukm_source_id, ukm::kInvalidSourceId);

  if (creation_params->code_cache_host_interface.is_valid()) {
    code_cache_host_ = std::make_unique<CodeCacheHost>(
        mojo::Remote<mojom::blink::CodeCacheHost>(
            std::move(creation_params->code_cache_host_interface)));
  }

  blob_url_store_pending_remote_ = std::move(creation_params->blob_url_store);
}

WorkletGlobalScope::~WorkletGlobalScope() = default;

const BrowserInterfaceBrokerProxy&
WorkletGlobalScope::GetBrowserInterfaceBroker() const {
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
      console_message->GetSource(), console_message->GetLevel(),
      console_message->Message(), console_message->Location());
  worker_thread_->GetConsoleMessageStorage()->AddConsoleMessage(
      worker_thread_->GlobalScope(), console_message, discard_duplicates);
}

void WorkletGlobalScope::AddInspectorIssue(AuditsIssue issue) {
  if (IsMainThreadWorkletGlobalScope()) {
    frame_->DomWindow()->AddInspectorIssue(std::move(issue));
  } else {
    worker_thread_->GetInspectorIssueStorage()->AddInspectorIssue(
        this, std::move(issue));
  }
}

void WorkletGlobalScope::ExceptionThrown(ErrorEvent* error_event) {
  if (IsMainThreadWorkletGlobalScope()) {
    MainThreadDebugger::Instance(GetIsolate())
        ->ExceptionThrown(this, error_event);
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

const base::UnguessableToken& WorkletGlobalScope::GetDevToolsToken() const {
  if (IsMainThreadWorkletGlobalScope()) {
    return frame_->GetDevToolsFrameToken();
  }
  return GetThread()->GetDevToolsWorkerToken();
}

CodeCacheHost* WorkletGlobalScope::GetCodeCacheHost() {
  if (IsMainThreadWorkletGlobalScope())
    return frame_->Loader().GetDocumentLoader()->GetCodeCacheHost();
  if (!code_cache_host_)
    return nullptr;
  return code_cache_host_.get();
}

CoreProbeSink* WorkletGlobalScope::GetProbeSink() {
  switch (thread_type_) {
    case ThreadType::kMainThread:
      DCHECK(frame_);
      return probe::ToCoreProbeSink(frame_);
    case ThreadType::kOffMainThread:
      DCHECK(worker_thread_);
      return worker_thread_->GetWorkerInspectorController()->GetProbeSink();
  }
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
  return frame_.Get();
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

  auto request_context_type = mojom::blink::RequestContextType::SCRIPT;
  FetchModuleScript(module_url_record, outside_settings_object,
                    outside_resource_timing_notifier, request_context_type,
                    GetDestination(), credentials_mode,
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

bool WorkletGlobalScope::CrossOriginIsolatedCapability() const {
  return parent_cross_origin_isolated_capability_;
}

bool WorkletGlobalScope::IsIsolatedContext() const {
  return parent_is_isolated_context_;
}

ukm::UkmRecorder* WorkletGlobalScope::UkmRecorder() {
  if (ukm_recorder_)
    return ukm_recorder_.get();

  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
  GetBrowserInterfaceBroker().GetInterface(
      factory.BindNewPipeAndPassReceiver());
  ukm_recorder_ = ukm::MojoUkmRecorder::Create(*factory);

  return ukm_recorder_.get();
}

ukm::SourceId WorkletGlobalScope::UkmSourceID() const {
  return ukm::kInvalidSourceId;
}

mojo::PendingRemote<mojom::blink::BlobURLStore>
WorkletGlobalScope::TakeBlobUrlStorePendingRemote() {
  DCHECK(blob_url_store_pending_remote_.is_valid());
  return std::move(blob_url_store_pending_remote_);
}

void WorkletGlobalScope::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  WorkerOrWorkletGlobalScope::Trace(visitor);
}

bool WorkletGlobalScope::HasPendingActivity() const {
  return !ExecutionContext::IsContextDestroyed();
}

}  // namespace blink
