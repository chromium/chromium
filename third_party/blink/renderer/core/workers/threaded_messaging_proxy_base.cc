// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/workers/threaded_messaging_proxy_base.h"

#include "base/synchronization/waitable_event.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/devtools_agent.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/workers/global_scope_creation_params.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

namespace blink {

namespace {

static int g_live_messaging_proxy_count = 0;

}  // namespace

ThreadedMessagingProxyBase::ThreadedMessagingProxyBase(
    ExecutionContext* execution_context)
    : execution_context_(execution_context),
      parent_execution_context_task_runners_(
          ParentExecutionContextTaskRunners::Create(execution_context_.Get())),
      terminate_sync_load_event_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED),
      feature_handle_for_scheduler_(
          execution_context->GetScheduler()->RegisterFeature(
              SchedulingPolicy::Feature::kDedicatedWorkerOrWorklet,
              {SchedulingPolicy::RecordMetricsForBackForwardCache()})),
      keep_alive_(PERSISTENT_FROM_HERE, this) {
  DCHECK(IsParentContextThread());
  g_live_messaging_proxy_count++;
}

ThreadedMessagingProxyBase::~ThreadedMessagingProxyBase() {
  g_live_messaging_proxy_count--;
}

int ThreadedMessagingProxyBase::ProxyCount() {
  DCHECK(IsMainThread());
  return g_live_messaging_proxy_count;
}

void ThreadedMessagingProxyBase::Trace(blink::Visitor* visitor) {
  visitor->Trace(execution_context_);
}

void ThreadedMessagingProxyBase::InitializeWorkerThread(
    std::unique_ptr<GlobalScopeCreationParams> global_scope_creation_params,
    const base::Optional<WorkerBackingThreadStartupData>& thread_startup_data) {
  DCHECK(IsParentContextThread());

  KURL script_url = global_scope_creation_params->script_url.Copy();

  if (global_scope_creation_params->web_worker_fetch_context) {
    global_scope_creation_params->web_worker_fetch_context
        ->SetTerminateSyncLoadEvent(&terminate_sync_load_event_);
  }

  worker_thread_ = CreateWorkerThread();

  auto devtools_params = DevToolsAgent::WorkerThreadCreated(
      execution_context_.Get(), worker_thread_.get(), script_url,
      global_scope_creation_params->global_scope_name.IsolatedCopy());

  worker_thread_->Start(std::move(global_scope_creation_params),
                        thread_startup_data, std::move(devtools_params));

  if (auto* scope = DynamicTo<WorkerGlobalScope>(*execution_context_)) {
    scope->GetThread()->ChildThreadStartedOnWorkerThread(worker_thread_.get());
  }
}

void ThreadedMessagingProxyBase::CountFeature(WebFeature feature) {
  DCHECK(IsParentContextThread());
  UseCounter::Count(execution_context_, feature);
}

void ThreadedMessagingProxyBase::CountDeprecation(WebFeature feature) {
  DCHECK(IsParentContextThread());
  Deprecation::CountDeprecation(execution_context_, feature);
}

void ThreadedMessagingProxyBase::ReportConsoleMessage(
    mojom::ConsoleMessageSource source,
    mojom::ConsoleMessageLevel level,
    const String& message,
    std::unique_ptr<SourceLocation> location) {
  DCHECK(IsParentContextThread());
  if (asked_to_terminate_)
    return;
  execution_context_->AddConsoleMessage(ConsoleMessage::CreateFromWorker(
      level, message, std::move(location), worker_thread_.get()));
}

void ThreadedMessagingProxyBase::ParentObjectDestroyed() {
  DCHECK(IsParentContextThread());
  if (worker_thread_) {
    // Request to terminate the global scope. This will eventually call
    // WorkerThreadTerminated().
    TerminateGlobalScope();
  } else {
    WorkerThreadTerminated();
  }
}

void ThreadedMessagingProxyBase::WorkerThreadTerminated() {
  DCHECK(IsParentContextThread());

  // This method is always the last to be performed, so the proxy is not
  // needed for communication in either side any more. However, the parent
  // Worker/Worklet object may still exist, and it assumes that the proxy
  // exists, too.
  asked_to_terminate_ = true;
  WorkerThread* parent_thread = nullptr;
  if (auto* scope = DynamicTo<WorkerGlobalScope>(*execution_context_))
    parent_thread = scope->GetThread();
  std::unique_ptr<WorkerThread> child_thread = std::move(worker_thread_);
  if (child_thread) {
    DevToolsAgent::WorkerThreadTerminated(execution_context_.Get(),
                                          child_thread.get());
  }

  // If the parent Worker/Worklet object was already destroyed, this will
  // destroy |this|.
  keep_alive_.Clear();

  if (parent_thread && child_thread)
    parent_thread->ChildThreadTerminatedOnWorkerThread(child_thread.get());
}

void ThreadedMessagingProxyBase::TerminateGlobalScope() {
  DCHECK(IsParentContextThread());

  if (asked_to_terminate_)
    return;
  asked_to_terminate_ = true;

  feature_handle_for_scheduler_.reset();

  terminate_sync_load_event_.Signal();

  if (!worker_thread_) {
    // Worker has been terminated before any backing thread was attached to the
    // messaging proxy.
    keep_alive_.Clear();
    return;
  }
  worker_thread_->Terminate();
  DevToolsAgent::WorkerThreadTerminated(execution_context_.Get(),
                                        worker_thread_.get());
}

ExecutionContext* ThreadedMessagingProxyBase::GetExecutionContext() const {
  DCHECK(IsParentContextThread());
  return execution_context_;
}

ParentExecutionContextTaskRunners*
ThreadedMessagingProxyBase::GetParentExecutionContextTaskRunners() const {
  DCHECK(IsParentContextThread());
  return parent_execution_context_task_runners_;
}

WorkerThread* ThreadedMessagingProxyBase::GetWorkerThread() const {
  DCHECK(IsParentContextThread());
  return worker_thread_.get();
}

bool ThreadedMessagingProxyBase::IsParentContextThread() const {
  return execution_context_->IsContextThread();
}

}  // namespace blink
