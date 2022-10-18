// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/dedicated_worker_messaging_proxy.h"

#include <memory>
#include "base/feature_list.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host.mojom-blink-forward.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/sanitize_script_errors.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_worker_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/fetch/request.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/thread_debugger_common_impl.h"
#include "third_party/blink/renderer/core/loader/worker_resource_timing_notifier_impl.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"
#include "third_party/blink/renderer/core/script_type_names.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_object_proxy.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_thread.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/parent_execution_context_task_runners.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

DedicatedWorkerMessagingProxy::DedicatedWorkerMessagingProxy(
    ExecutionContext* execution_context,
    DedicatedWorker* worker_object)
    : DedicatedWorkerMessagingProxy(
          execution_context,
          worker_object,
          [](DedicatedWorkerMessagingProxy* messaging_proxy,
             DedicatedWorker* worker_object,
             ParentExecutionContextTaskRunners* runners) {
            return std::make_unique<DedicatedWorkerObjectProxy>(
                messaging_proxy, runners, worker_object->GetToken());
          }) {}

DedicatedWorkerMessagingProxy::DedicatedWorkerMessagingProxy(
    ExecutionContext* execution_context,
    DedicatedWorker* worker_object,
    base::FunctionRef<std::unique_ptr<DedicatedWorkerObjectProxy>(
        DedicatedWorkerMessagingProxy*,
        DedicatedWorker*,
        ParentExecutionContextTaskRunners*)> worker_object_proxy_factory)
    : ThreadedMessagingProxyBase(execution_context),
      worker_object_proxy_(
          worker_object_proxy_factory(this,
                                      worker_object,
                                      GetParentExecutionContextTaskRunners())),
      worker_object_(worker_object) {}

DedicatedWorkerMessagingProxy::~DedicatedWorkerMessagingProxy() = default;

void DedicatedWorkerMessagingProxy::StartWorkerGlobalScope(
    std::unique_ptr<GlobalScopeCreationParams> creation_params,
    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params,
    const WorkerOptions* options,
    const KURL& script_url,
    const FetchClientSettingsObjectSnapshot& outside_settings_object,
    const v8_inspector::V8StackTraceId& stack_id,
    const String& source_code,
    RejectCoepUnsafeNone reject_coep_unsafe_none,
    const blink::DedicatedWorkerToken& token,
    mojo::PendingRemote<mojom::blink::DedicatedWorkerHost>
        dedicated_worker_host,
    mojo::PendingRemote<mojom::blink::BackForwardCacheControllerHost>
        back_forward_cache_controller_host) {
  DCHECK(IsParentContextThread());
  if (AskedToTerminate()) {
    // Worker.terminate() could be called from JS before the thread was
    // created.
    return;
  }

  // These must be stored before InitializeWorkerThread.
  pending_dedicated_worker_host_ = std::move(dedicated_worker_host);
  pending_back_forward_cache_controller_host_ =
      std::move(back_forward_cache_controller_host);
  InitializeWorkerThread(
      std::move(creation_params),
      CreateBackingThreadStartupData(GetExecutionContext()->GetIsolate()),
      token);

  // Step 13: "Obtain script by switching on the value of options's type
  // member:"
  if (options->type() == script_type_names::kClassic) {
    // "classic: Fetch a classic worker script given url, outside settings,
    // destination, and inside settings."
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kClassicDedicatedWorker);
    if (base::FeatureList::IsEnabled(features::kPlzDedicatedWorker)) {
      auto* resource_timing_notifier =
          WorkerResourceTimingNotifierImpl::CreateForOutsideResourceFetcher(
              *GetExecutionContext());
      // TODO(crbug.com/1177199): pass a proper policy container
      GetWorkerThread()->FetchAndRunClassicScript(
          script_url, std::move(worker_main_script_load_params),
          /*policy_container=*/nullptr, outside_settings_object.CopyData(),
          resource_timing_notifier, stack_id);
    } else {
      // Legacy code path (to be deprecated, see https://crbug.com/835717):
      GetWorkerThread()->EvaluateClassicScript(
          script_url, source_code, nullptr /* cached_meta_data */, stack_id);
    }
  } else if (options->type() == script_type_names::kModule) {
    // "module: Fetch a module worker script graph given url, outside settings,
    // destination, the value of the credentials member of options, and inside
    // settings."
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kModuleDedicatedWorker);
    absl::optional<network::mojom::CredentialsMode> credentials_mode =
        Request::ParseCredentialsMode(options->credentials());
    DCHECK(credentials_mode);

    auto* resource_timing_notifier =
        WorkerResourceTimingNotifierImpl::CreateForOutsideResourceFetcher(
            *GetExecutionContext());
    // TODO(crbug.com/1177199): pass a proper policy container
    GetWorkerThread()->FetchAndRunModuleScript(
        script_url, std::move(worker_main_script_load_params),
        /*policy_container=*/nullptr, outside_settings_object.CopyData(),
        resource_timing_notifier, *credentials_mode, reject_coep_unsafe_none);
  } else {
    NOTREACHED();
  }
}

void DedicatedWorkerMessagingProxy::PostMessageToWorkerGlobalScope(
    BlinkTransferableMessage message) {
  DCHECK(IsParentContextThread());
  if (AskedToTerminate())
    return;
  if (!was_script_evaluated_) {
    queued_early_tasks_.push_back(std::move(message));
    return;
  }
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kPostedMessage), FROM_HERE,
      CrossThreadBindOnce(
          &DedicatedWorkerObjectProxy::ProcessMessageFromWorkerObject,
          CrossThreadUnretained(&WorkerObjectProxy()), std::move(message),
          CrossThreadUnretained(GetWorkerThread())));
}

bool DedicatedWorkerMessagingProxy::HasPendingActivity() const {
  DCHECK(IsParentContextThread());
  return !AskedToTerminate();
}

void DedicatedWorkerMessagingProxy::DidFailToFetchScript() {
  DCHECK(IsParentContextThread());
  if (!worker_object_ || AskedToTerminate())
    return;
  worker_object_->DispatchErrorEventForScriptFetchFailure();
}

void DedicatedWorkerMessagingProxy::Freeze(bool is_in_back_forward_cache) {
  DCHECK(IsParentContextThread());
  auto* worker_thread = GetWorkerThread();
  if (AskedToTerminate() || !worker_thread)
    return;
  worker_thread->Freeze(is_in_back_forward_cache);
}

void DedicatedWorkerMessagingProxy::Resume() {
  DCHECK(IsParentContextThread());
  auto* worker_thread = GetWorkerThread();
  if (AskedToTerminate() || !worker_thread)
    return;
  worker_thread->Resume();
}

void DedicatedWorkerMessagingProxy::DidEvaluateScript(bool success) {
  DCHECK(IsParentContextThread());
  was_script_evaluated_ = true;

  Vector<BlinkTransferableMessage> tasks;
  queued_early_tasks_.swap(tasks);

  // The worker thread can already be terminated.
  if (!GetWorkerThread()) {
    DCHECK(AskedToTerminate());
    return;
  }

  // Post all queued tasks to the worker.
  // TODO(nhiroki): Consider whether to post the queued tasks to the worker when
  // |success| is false.
  for (auto& task : tasks) {
    PostCrossThreadTask(
        *GetWorkerThread()->GetTaskRunner(TaskType::kPostedMessage), FROM_HERE,
        CrossThreadBindOnce(
            &DedicatedWorkerObjectProxy::ProcessMessageFromWorkerObject,
            CrossThreadUnretained(&WorkerObjectProxy()), std::move(task),
            CrossThreadUnretained(GetWorkerThread())));
  }
}

void DedicatedWorkerMessagingProxy::PostMessageToWorkerObject(
    BlinkTransferableMessage message) {
  DCHECK(IsParentContextThread());
  if (!worker_object_ || AskedToTerminate())
    return;

  ThreadDebugger* debugger =
      ThreadDebugger::From(GetExecutionContext()->GetIsolate());
  MessagePortArray* ports = MessagePort::EntanglePorts(
      *GetExecutionContext(), std::move(message.ports));
  debugger->ExternalAsyncTaskStarted(message.sender_stack_trace_id);
  if (message.message->CanDeserializeIn(GetExecutionContext())) {
    worker_object_->DispatchEvent(
        *MessageEvent::Create(ports, std::move(message.message)));
  } else {
    worker_object_->DispatchEvent(*MessageEvent::CreateError());
  }
  debugger->ExternalAsyncTaskFinished(message.sender_stack_trace_id);
}

void DedicatedWorkerMessagingProxy::DispatchErrorEvent(
    const String& error_message,
    std::unique_ptr<SourceLocation> location,
    int exception_id) {
  DCHECK(IsParentContextThread());
  if (!worker_object_)
    return;

  // We don't bother checking the AskedToTerminate() flag for dispatching the
  // event on the owner context, because exceptions should *always* be reported
  // even if the thread is terminated as the spec says:
  //
  // "Thus, error reports propagate up to the chain of dedicated workers up to
  // the original Document, even if some of the workers along this chain have
  // been terminated and garbage collected."
  // https://html.spec.whatwg.org/C/#runtime-script-errors-2
  ErrorEvent* event =
      ErrorEvent::Create(error_message, location->Clone(), nullptr);
  if (worker_object_->DispatchEvent(*event) !=
      DispatchEventResult::kNotCanceled)
    return;

  // The worker thread can already be terminated.
  if (!GetWorkerThread()) {
    DCHECK(AskedToTerminate());
    return;
  }

  // The HTML spec requires to queue an error event using the DOM manipulation
  // task source.
  // https://html.spec.whatwg.org/C/#runtime-script-errors-2
  PostCrossThreadTask(
      *GetWorkerThread()->GetTaskRunner(TaskType::kDOMManipulation), FROM_HERE,
      CrossThreadBindOnce(
          &DedicatedWorkerObjectProxy::ProcessUnhandledException,
          CrossThreadUnretained(worker_object_proxy_.get()), exception_id,
          CrossThreadUnretained(GetWorkerThread())));

  // Propagate an unhandled error to the parent context.
  const auto mute_script_errors = SanitizeScriptErrors::kDoNotSanitize;
  GetExecutionContext()->DispatchErrorEvent(event, mute_script_errors);
}

void DedicatedWorkerMessagingProxy::Trace(Visitor* visitor) const {
  visitor->Trace(worker_object_);
  ThreadedMessagingProxyBase::Trace(visitor);
}

absl::optional<WorkerBackingThreadStartupData>
DedicatedWorkerMessagingProxy::CreateBackingThreadStartupData(
    v8::Isolate* isolate) {
  using HeapLimitMode = WorkerBackingThreadStartupData::HeapLimitMode;
  using AtomicsWaitMode = WorkerBackingThreadStartupData::AtomicsWaitMode;
  return WorkerBackingThreadStartupData(
      isolate->IsHeapLimitIncreasedForDebugging()
          ? HeapLimitMode::kIncreasedForDebugging
          : HeapLimitMode::kDefault,
      AtomicsWaitMode::kAllow);
}

std::unique_ptr<WorkerThread>
DedicatedWorkerMessagingProxy::CreateWorkerThread() {
  DCHECK(pending_dedicated_worker_host_);
  return std::make_unique<DedicatedWorkerThread>(
      GetExecutionContext(), WorkerObjectProxy(),
      std::move(pending_dedicated_worker_host_),
      std::move(pending_back_forward_cache_controller_host_));
}

}  // namespace blink
