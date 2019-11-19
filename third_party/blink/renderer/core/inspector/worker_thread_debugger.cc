/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/inspector/worker_thread_debugger.h"

#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/core/inspector/worker_inspector_controller.h"
#include "third_party/blink/renderer/core/workers/dedicated_worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worker_reporting_proxy.h"
#include "third_party/blink/renderer/core/workers/worker_thread.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

const int kInvalidContextGroupId = 0;

}  // namespace

WorkerThreadDebugger* WorkerThreadDebugger::From(v8::Isolate* isolate) {
  ThreadDebugger* debugger = ThreadDebugger::From(isolate);
  if (!debugger)
    return nullptr;
  DCHECK(debugger->IsWorker());
  return static_cast<WorkerThreadDebugger*>(debugger);
}

WorkerThreadDebugger::WorkerThreadDebugger(v8::Isolate* isolate)
    : ThreadDebugger(isolate),
      paused_context_group_id_(kInvalidContextGroupId) {}

WorkerThreadDebugger::~WorkerThreadDebugger() {
  DCHECK(worker_threads_.IsEmpty());
}

void WorkerThreadDebugger::ReportConsoleMessage(
    ExecutionContext* context,
    mojom::ConsoleMessageSource source,
    mojom::ConsoleMessageLevel level,
    const String& message,
    SourceLocation* location) {
  if (!context)
    return;
  To<WorkerOrWorkletGlobalScope>(context)
      ->GetThread()
      ->GetWorkerReportingProxy()
      .ReportConsoleMessage(source, level, message, location);
}

int WorkerThreadDebugger::ContextGroupId(WorkerThread* worker_thread) {
  return worker_thread->GetWorkerThreadId();
}

void WorkerThreadDebugger::WorkerThreadCreated(WorkerThread* worker_thread) {
  int worker_context_group_id = ContextGroupId(worker_thread);
  DCHECK(!worker_threads_.Contains(worker_context_group_id));
  worker_threads_.insert(worker_context_group_id, worker_thread);
}

void WorkerThreadDebugger::WorkerThreadDestroyed(WorkerThread* worker_thread) {
  int worker_context_group_id = ContextGroupId(worker_thread);
  DCHECK(worker_threads_.Contains(worker_context_group_id));
  worker_threads_.erase(worker_context_group_id);
  if (worker_context_group_id == paused_context_group_id_) {
    paused_context_group_id_ = kInvalidContextGroupId;
  }
}

void WorkerThreadDebugger::ContextCreated(WorkerThread* worker_thread,
                                          const KURL& url_for_debugger,
                                          v8::Local<v8::Context> context) {
  int worker_context_group_id = ContextGroupId(worker_thread);
  if (!worker_threads_.Contains(worker_context_group_id))
    return;
  String human_readable_name = "";
  WorkerOrWorkletGlobalScope* globalScope = worker_thread->GlobalScope();
  if (auto* scope = DynamicTo<DedicatedWorkerGlobalScope>(globalScope))
    human_readable_name = scope->name();
  v8_inspector::V8ContextInfo context_info(
      context, worker_context_group_id,
      ToV8InspectorStringView(human_readable_name));
  String origin = url_for_debugger;
  context_info.origin = ToV8InspectorStringView(origin);
  GetV8Inspector()->contextCreated(context_info);
}

void WorkerThreadDebugger::ContextWillBeDestroyed(
    WorkerThread* worker_thread,
    v8::Local<v8::Context> context) {
  // Note that we might have already got WorkerThreadDestroyed by this point.
  GetV8Inspector()->contextDestroyed(context);
}

void WorkerThreadDebugger::ExceptionThrown(WorkerThread* worker_thread,
                                           ErrorEvent* event) {
  worker_thread->GetWorkerReportingProxy().ReportConsoleMessage(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kError, event->MessageForConsole(),
      event->Location());

  const String default_message = "Uncaught";
  ScriptState* script_state =
      worker_thread->GlobalScope()->ScriptController()->GetScriptState();
  if (script_state && script_state->ContextIsValid()) {
    ScriptState::Scope scope(script_state);
    ScriptValue error = event->error(script_state);
    v8::Local<v8::Value> exception =
        error.IsEmpty()
            ? v8::Local<v8::Value>(v8::Null(script_state->GetIsolate()))
            : error.V8Value();
    SourceLocation* location = event->Location();
    String message = event->MessageForConsole();
    String url = location->Url();
    GetV8Inspector()->exceptionThrown(
        script_state->GetContext(), ToV8InspectorStringView(default_message),
        exception, ToV8InspectorStringView(message),
        ToV8InspectorStringView(url), location->LineNumber(),
        location->ColumnNumber(), location->TakeStackTrace(),
        location->ScriptId());
  }
}

int WorkerThreadDebugger::ContextGroupId(ExecutionContext* context) {
  return ContextGroupId(To<WorkerOrWorkletGlobalScope>(context)->GetThread());
}

void WorkerThreadDebugger::PauseWorkerOnStart(WorkerThread* worker_thread) {
  DCHECK(!worker_thread->GlobalScope()->IsClosing());
  if (paused_context_group_id_ == kInvalidContextGroupId)
    runMessageLoopOnPause(ContextGroupId(worker_thread));
}

void WorkerThreadDebugger::runMessageLoopOnPause(int context_group_id) {
  if (!worker_threads_.Contains(context_group_id))
    return;

  DCHECK_EQ(kInvalidContextGroupId, paused_context_group_id_);
  paused_context_group_id_ = context_group_id;

  WorkerThread* thread = worker_threads_.at(context_group_id);
  DCHECK(!thread->GlobalScope()->IsClosing());
  thread->GetWorkerInspectorController()->FlushProtocolNotifications();
  thread->Pause();
}

void WorkerThreadDebugger::quitMessageLoopOnPause() {
  DCHECK_NE(kInvalidContextGroupId, paused_context_group_id_);
  DCHECK(worker_threads_.Contains(paused_context_group_id_));

  WorkerThread* thread = worker_threads_.at(paused_context_group_id_);
  paused_context_group_id_ = kInvalidContextGroupId;
  DCHECK(!thread->GlobalScope()->IsClosing());
  thread->Resume();
}

void WorkerThreadDebugger::muteMetrics(int context_group_id) {
}

void WorkerThreadDebugger::unmuteMetrics(int context_group_id) {
}

v8::Local<v8::Context> WorkerThreadDebugger::ensureDefaultContextInGroup(
    int context_group_id) {
  if (!worker_threads_.Contains(context_group_id))
    return v8::Local<v8::Context>();
  ScriptState* script_state = worker_threads_.at(context_group_id)
                                  ->GlobalScope()
                                  ->ScriptController()
                                  ->GetScriptState();
  return script_state ? script_state->GetContext() : v8::Local<v8::Context>();
}

void WorkerThreadDebugger::beginEnsureAllContextsInGroup(int context_group_id) {
}

void WorkerThreadDebugger::endEnsureAllContextsInGroup(int context_group_id) {
}

bool WorkerThreadDebugger::canExecuteScripts(int context_group_id) {
  return true;
}

void WorkerThreadDebugger::runIfWaitingForDebugger(int context_group_id) {
  if (paused_context_group_id_ == context_group_id)
    quitMessageLoopOnPause();
}

void WorkerThreadDebugger::consoleAPIMessage(
    int context_group_id,
    v8::Isolate::MessageErrorLevel level,
    const v8_inspector::StringView& message,
    const v8_inspector::StringView& url,
    unsigned line_number,
    unsigned column_number,
    v8_inspector::V8StackTrace* stack_trace) {
  if (!worker_threads_.Contains(context_group_id))
    return;
  WorkerThread* worker_thread = worker_threads_.at(context_group_id);
  std::unique_ptr<SourceLocation> location = std::make_unique<SourceLocation>(
      ToCoreString(url), line_number, column_number,
      stack_trace ? stack_trace->clone() : nullptr, 0);
  worker_thread->GetWorkerReportingProxy().ReportConsoleMessage(
      mojom::ConsoleMessageSource::kConsoleApi,
      V8MessageLevelToMessageLevel(level), ToCoreString(message),
      location.get());
}

void WorkerThreadDebugger::consoleClear(int context_group_id) {
  if (!worker_threads_.Contains(context_group_id))
    return;
  WorkerThread* worker_thread = worker_threads_.at(context_group_id);
  worker_thread->GetConsoleMessageStorage()->Clear();
}

v8::MaybeLocal<v8::Value> WorkerThreadDebugger::memoryInfo(
    v8::Isolate*,
    v8::Local<v8::Context>) {
  NOTREACHED();
  return v8::MaybeLocal<v8::Value>();
}

}  // namespace blink
