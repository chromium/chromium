/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/inspector/worker_inspector_controller.h"

#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/inspector/devtools_session.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_emulation_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_event_breakpoints_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_issue_reporter.h"
#include "third_party/blink/renderer/core/inspector/inspector_log_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_media_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_network_agent.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/inspector/worker_devtools_params.h"
#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"
#include "third_party/blink/renderer/core/loader/worker_fetch_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/workers/worker_backing_thread.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"

namespace blink {

// static
WorkerInspectorController* WorkerInspectorController::Create(
    WorkerThread* thread,
    const KURL& url,
    scoped_refptr<InspectorTaskRunner> inspector_task_runner,
    std::unique_ptr<WorkerDevToolsParams> devtools_params) {
  WorkerThreadDebugger* debugger =
      WorkerThreadDebugger::From(thread->GetIsolate());
  return debugger ? MakeGarbageCollected<WorkerInspectorController>(
                        thread, url, debugger, std::move(inspector_task_runner),
                        std::move(devtools_params))
                  : nullptr;
}

WorkerInspectorController::WorkerInspectorController(
    WorkerThread* thread,
    const KURL& url,
    WorkerThreadDebugger* debugger,
    scoped_refptr<InspectorTaskRunner> inspector_task_runner,
    std::unique_ptr<WorkerDevToolsParams> devtools_params)
    : debugger_(debugger),
      thread_(thread),
      inspected_frames_(nullptr),
      probe_sink_(MakeGarbageCollected<CoreProbeSink>()),
      worker_thread_id_(base::PlatformThread::CurrentId()) {
  // The constructor must run on the backing thread of |thread|. Otherwise, it
  // would be incorrect to initialize |worker_thread_id_| with the current
  // thread id.
  DCHECK(thread->IsCurrentThread());

  probe_sink_->AddInspectorIssueReporter(
      MakeGarbageCollected<InspectorIssueReporter>(
          thread->GetInspectorIssueStorage()));
  probe_sink_->AddInspectorTraceEvents(
      MakeGarbageCollected<InspectorTraceEvents>());
  worker_devtools_token_ = devtools_params->devtools_worker_token;
  parent_devtools_token_ = thread->GlobalScope()->GetParentDevToolsToken();
  url_ = url;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
      Platform::Current()->GetIOTaskRunner();
  if (!parent_devtools_token_.is_empty() && io_task_runner) {
    // There may be no io task runner in unit tests.
    wait_for_debugger_ = devtools_params->wait_for_debugger;
    agent_ = MakeGarbageCollected<DevToolsAgent>(
        this, inspected_frames_.Get(), probe_sink_.Get(),
        std::move(inspector_task_runner), std::move(io_task_runner));
    agent_->BindReceiverForWorker(
        std::move(devtools_params->agent_host_remote),
        std::move(devtools_params->agent_receiver),
        thread->GetTaskRunner(TaskType::kInternalInspector));
  }
  trace_event::AddEnabledStateObserver(this);
  EmitTraceEvent();
}

WorkerInspectorController::~WorkerInspectorController() {
  DCHECK(!thread_);
  trace_event::RemoveEnabledStateObserver(this);
}

void WorkerInspectorController::AttachSession(DevToolsSession* session,
                                              bool restore) {
  if (!session_count_)
    thread_->GetWorkerBackingThread().BackingThread().AddTaskObserver(this);
  session->ConnectToV8(debugger_->GetV8Inspector(),
                       debugger_->ContextGroupId(thread_));
  session->CreateAndAppend<InspectorLogAgent>(
      thread_->GetConsoleMessageStorage(), nullptr, session->V8Session());
  session->CreateAndAppend<InspectorEventBreakpointsAgent>(
      session->V8Session());

  auto* worker_or_worklet_global_scope =
      DynamicTo<WorkerOrWorkletGlobalScope>(thread_->GlobalScope());
  auto* worker_global_scope =
      DynamicTo<WorkerGlobalScope>(thread_->GlobalScope());

  if (worker_or_worklet_global_scope) {
    auto* network_agent = session->CreateAndAppend<InspectorNetworkAgent>(
        inspected_frames_.Get(), worker_or_worklet_global_scope,
        session->V8Session());
    session->CreateAndAppend<InspectorAuditsAgent>(
        network_agent, thread_->GetInspectorIssueStorage(),
        /*inspected_frames=*/nullptr, /*web_autofill_client=*/nullptr);
  }
  if (worker_global_scope) {
    auto* virtual_time_controller =
        thread_->GetScheduler()->GetVirtualTimeController();
    DCHECK(virtual_time_controller);
    session->CreateAndAppend<InspectorEmulationAgent>(nullptr,
                                                      *virtual_time_controller);
    session->CreateAndAppend<InspectorMediaAgent>(inspected_frames_.Get(),
                                                  worker_global_scope);
  }
  ++session_count_;
}

void WorkerInspectorController::DetachSession(DevToolsSession*) {
  --session_count_;
  if (!session_count_)
    thread_->GetWorkerBackingThread().BackingThread().RemoveTaskObserver(this);
}

void WorkerInspectorController::InspectElement(const gfx::Point&) {
  NOTREACHED_IN_MIGRATION();
}

void WorkerInspectorController::DebuggerTaskStarted() {
  thread_->DebuggerTaskStarted();
}

void WorkerInspectorController::DebuggerTaskFinished() {
  thread_->DebuggerTaskFinished();
}

void WorkerInspectorController::Dispose() {
  if (agent_)
    agent_->Dispose();
  thread_ = nullptr;
}

void WorkerInspectorController::FlushProtocolNotifications() {
  if (agent_)
    agent_->FlushProtocolNotifications();
}

void WorkerInspectorController::WaitForDebuggerIfNeeded() {
  if (!wait_for_debugger_)
    return;
  wait_for_debugger_ = false;
  debugger_->PauseWorkerOnStart(thread_);
}

void WorkerInspectorController::WillProcessTask(
    const base::PendingTask& pending_task,
    bool was_blocked_or_low_priority) {}

void WorkerInspectorController::DidProcessTask(
    const base::PendingTask& pending_task) {
  FlushProtocolNotifications();
}

void WorkerInspectorController::OnTraceLogEnabled() {
  EmitTraceEvent();
}

void WorkerInspectorController::OnTraceLogDisabled() {}

void WorkerInspectorController::EmitTraceEvent() {
  if (worker_devtools_token_.is_empty())
    return;
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT_WITH_CATEGORIES(
      TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
      "TracingSessionIdForWorker",
      inspector_tracing_session_id_for_worker_event::Data,
      worker_devtools_token_, parent_devtools_token_, url_, worker_thread_id_);
}

void WorkerInspectorController::Trace(Visitor* visitor) const {
  visitor->Trace(agent_);
  visitor->Trace(inspected_frames_);
  visitor->Trace(probe_sink_);
}

}  // namespace blink
