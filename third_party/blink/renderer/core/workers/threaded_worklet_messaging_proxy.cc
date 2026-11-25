// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/threaded_worklet_messaging_proxy.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/security_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/inspector/thread_debugger_common_impl.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/outside_settings_csp_violation_reporter.h"
#include "third_party/blink/renderer/core/workers/threaded_worklet_object_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_clients.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_module_responses_map.h"
#include "third_party/blink/renderer/core/workers/worklet_pending_tasks.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/worker_resource_timing_notifier.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

ThreadedWorkletMessagingProxy::ThreadedWorkletMessagingProxy(
    ExecutionContext* execution_context)
    : ThreadedMessagingProxyBase(execution_context) {}

void ThreadedWorkletMessagingProxy::Initialize(
    WorkerClients* worker_clients,
    WorkletModuleResponsesMap* module_responses_map,
    const std::optional<WorkerBackingThreadStartupData>& thread_startup_data) {
  DCHECK(IsMainThread());
  if (AskedToTerminate()) {
    return;
  }

  worklet_object_proxy_ =
      CreateObjectProxy(this, GetParentExecutionContextTaskRunners());

  // For now we don't use global scope name for threaded worklets.
  // TODO(nhiroki): Threaded worklets may want to have the global scope name to
  // distinguish multiple worklets created from the same script URL like
  // LayoutWorklet and PaintWorklet.
  const String global_scope_name = g_empty_string;

  LocalDOMWindow* window = To<LocalDOMWindow>(GetExecutionContext());
  ContentSecurityPolicy* csp = window->GetContentSecurityPolicy();
  DCHECK(csp);

  LocalFrameClient* frame_client = window->GetFrame()->Client();
  auto global_scope_creation_params =
      std::make_unique<GlobalScopeCreationParams>(
          window->Url(), mojom::blink::ScriptType::kModule, global_scope_name,
          frame_client->UserAgent(), frame_client->UserAgentMetadata(),
          frame_client->CreateWorkletFetchContext(),
          mojo::Clone(csp->GetParsedPolicies()),
          OutsideSettingsCSPViolationReporter::BindNewRemote(*window),
          window->GetReferrerPolicy(), window->GetSecurityOrigin(),
          window->IsSecureContext(), window->GetHttpsState(), worker_clients,
          frame_client->CreateWorkerContentSettingsClient(),
          OriginTrialContext::GetInheritedTrialFeatures(window).get(),
          base::UnguessableToken::Create(),
          std::make_unique<WorkerSettings>(window->GetFrame()->GetSettings()),
          mojom::blink::V8CacheOptions::kDefault, module_responses_map,
          mojo::NullRemote() /* browser_interface_broker */,
          window->GetFrame()->Loader().CreateWorkerCodeCacheHost(),
          window->GetFrame()->GetBlobUrlStorePendingRemote(),
          BeginFrameProviderParams(), nullptr /* parent_permissions_policy */,
          window->GetAgentClusterID(), ukm::kInvalidSourceId,
          window->GetExecutionContextToken(),
          window->CrossOriginIsolatedCapability(), window->IsIsolatedContext(),
          // Match window.performance's time origin directly; after
          // LocalDOMWindow reuse, DocumentLoader's reference time can advance
          // ahead of it.
          DOMWindowPerformance::performance(*window)->GetTimeOriginInternal());

  // Worklets share the pre-initialized backing thread so that we don't have to
  // specify the backing thread startup data.
  InitializeWorkerThread(std::move(global_scope_creation_params),
                         thread_startup_data, std::nullopt);
  if (pending_freeze_is_in_back_forward_cache_.has_value()) {
    Freeze(*pending_freeze_is_in_back_forward_cache_);
    pending_freeze_is_in_back_forward_cache_.reset();
  }
}

void ThreadedWorkletMessagingProxy::Trace(Visitor* visitor) const {
  ThreadedMessagingProxyBase::Trace(visitor);
}

void ThreadedWorkletMessagingProxy::FetchAndInvokeScript(
    const KURL& module_url_record,
    network::mojom::CredentialsMode credentials_mode,
    const FetchClientSettingsObjectSnapshot& outside_settings_object,
    WorkerResourceTimingNotifier& outside_resource_timing_notifier,
    scoped_refptr<base::SingleThreadTaskRunner> outside_settings_task_runner,
    WorkletPendingTasks* pending_tasks) {
  DCHECK(IsMainThread());
  WorkerThread* worker_thread = GetWorkerThread();
  if (!worker_thread) {
    // Worker thread has been terminated or not initialized.
    pending_tasks->Abort(nullptr);
    return;
  }
  PostCrossThreadTask(
      *worker_thread->GetTaskRunner(TaskType::kInternalLoading), FROM_HERE,
      CrossThreadBindOnce(
          &ThreadedWorkletObjectProxy::FetchAndInvokeScript,
          CrossThreadUnretained(worklet_object_proxy_.get()), module_url_record,
          credentials_mode, outside_settings_object.CopyData(),
          WrapCrossThreadPersistent(&outside_resource_timing_notifier),
          std::move(outside_settings_task_runner),
          WrapCrossThreadPersistent(pending_tasks),
          CrossThreadUnretained(worker_thread)));
}

void ThreadedWorkletMessagingProxy::WorkletObjectDestroyed() {
  DCHECK(IsMainThread());
  ParentObjectDestroyed();
}

void ThreadedWorkletMessagingProxy::TerminateWorkletGlobalScope() {
  DCHECK(IsMainThread());
  TerminateGlobalScope();
}

void ThreadedWorkletMessagingProxy::Freeze(bool is_in_back_forward_cache) {
  DCHECK(IsMainThread());
  if (AskedToTerminate()) {
    return;
  }
  WorkerThread* worker_thread = GetWorkerThread();
  if (!worker_thread) {
    pending_freeze_is_in_back_forward_cache_ = is_in_back_forward_cache;
    return;
  }
  // Post to the backing thread's own task runner rather than to one of the
  // worklet scheduler's task queues: Resume() must still be delivered while
  // that scheduler is paused. `AskedToTerminate()` is checked above before
  // posting, and `worker_thread_` is only destroyed in
  // `WorkerThreadTerminated()` after `PerformShutdownOnWorkerThread()` runs on
  // this same FIFO task runner, so `worker_thread` outlives this task.
  PostCrossThreadTask(
      *worker_thread->GetWorkerBackingThread().BackingThread().GetTaskRunner(),
      FROM_HERE,
      CrossThreadBindOnce(&ThreadedWorkletObjectProxy::FreezeOnWorkletThread,
                          CrossThreadUnretained(worker_thread),
                          is_in_back_forward_cache));
}

void ThreadedWorkletMessagingProxy::Resume() {
  DCHECK(IsMainThread());
  if (AskedToTerminate()) {
    return;
  }
  WorkerThread* worker_thread = GetWorkerThread();
  if (!worker_thread) {
    pending_freeze_is_in_back_forward_cache_.reset();
    return;
  }
  // See `Freeze()` for why `CrossThreadUnretained(worker_thread)` is safe.
  PostCrossThreadTask(
      *worker_thread->GetWorkerBackingThread().BackingThread().GetTaskRunner(),
      FROM_HERE,
      CrossThreadBindOnce(&ThreadedWorkletObjectProxy::ResumeOnWorkletThread,
                          CrossThreadUnretained(worker_thread)));
}

std::unique_ptr<ThreadedWorkletObjectProxy>
ThreadedWorkletMessagingProxy::CreateObjectProxy(
    ThreadedWorkletMessagingProxy* messaging_proxy,
    ParentExecutionContextTaskRunners* parent_execution_context_task_runners) {
  return ThreadedWorkletObjectProxy::Create(
      messaging_proxy, parent_execution_context_task_runners);
}

ThreadedWorkletObjectProxy&
ThreadedWorkletMessagingProxy::WorkletObjectProxy() {
  DCHECK(worklet_object_proxy_);
  return *worklet_object_proxy_;
}

}  // namespace blink
