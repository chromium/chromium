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

#include "third_party/blink/renderer/core/inspector/main_thread_debugger.h"

#include <memory>

#include "third_party/blink/renderer/bindings/core/v8/binding_security.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_node.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_window.h"
#include "third_party/blink/renderer/bindings/core/v8/worker_or_worklet_script_controller.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/static_node_list.h"
#include "third_party/blink/renderer/core/events/error_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/deprecation.h"
#include "third_party/blink/renderer/core/frame/frame_console.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/console_message_storage.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspected_frames.h"
#include "third_party/blink/renderer/core/inspector/v8_inspector_string.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/timing/memory_info.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/core/xml/xpath_evaluator.h"
#include "third_party/blink/renderer/core/xml/xpath_result.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

namespace blink {

namespace {

Mutex& CreationMutex() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, mutex, ());
  return mutex;
}

LocalFrame* ToFrame(ExecutionContext* context) {
  if (!context)
    return nullptr;
  if (auto* document = DynamicTo<Document>(context))
    return document->GetFrame();
  if (context->IsMainThreadWorkletGlobalScope())
    return To<WorkletGlobalScope>(context)->GetFrame();
  return nullptr;
}
}

MainThreadDebugger* MainThreadDebugger::instance_ = nullptr;

MainThreadDebugger::MainThreadDebugger(v8::Isolate* isolate)
    : ThreadDebugger(isolate),
      paused_(false) {
  MutexLocker locker(CreationMutex());
  DCHECK(!instance_);
  instance_ = this;
}

MainThreadDebugger::~MainThreadDebugger() {
  MutexLocker locker(CreationMutex());
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

void MainThreadDebugger::ReportConsoleMessage(
    ExecutionContext* context,
    mojom::ConsoleMessageSource source,
    mojom::ConsoleMessageLevel level,
    const String& message,
    SourceLocation* location) {
  if (LocalFrame* frame = ToFrame(context))
    frame->Console().ReportMessageToClient(source, level, message, location);
}

int MainThreadDebugger::ContextGroupId(ExecutionContext* context) {
  LocalFrame* frame = ToFrame(context);
  return frame ? ContextGroupId(frame) : 0;
}

void MainThreadDebugger::SetClientMessageLoop(
    std::unique_ptr<ClientMessageLoop> client_message_loop) {
  DCHECK(!client_message_loop_);
  DCHECK(client_message_loop);
  client_message_loop_ = std::move(client_message_loop);
}

void MainThreadDebugger::DidClearContextsForFrame(LocalFrame* frame) {
  DCHECK(IsMainThread());
  if (frame->LocalFrameRoot() == frame)
    GetV8Inspector()->resetContextGroup(ContextGroupId(frame));
}

void MainThreadDebugger::ContextCreated(ScriptState* script_state,
                                        LocalFrame* frame,
                                        const SecurityOrigin* origin) {
  DCHECK(IsMainThread());
  v8::HandleScope handles(script_state->GetIsolate());
  DOMWrapperWorld& world = script_state->World();
  StringBuilder aux_data_builder;
  aux_data_builder.Append("{\"isDefault\":");
  aux_data_builder.Append(world.IsMainWorld() ? "true" : "false");
  if (world.IsMainWorld())
    aux_data_builder.Append(",\"type\":\"default\"");
  else if (world.IsIsolatedWorld())
    aux_data_builder.Append(",\"type\":\"isolated\"");
  else if (world.IsWorkerWorld())
    aux_data_builder.Append(",\"type\":\"worker\"");
  aux_data_builder.Append(",\"frameId\":\"");
  aux_data_builder.Append(IdentifiersFactory::FrameId(frame));
  aux_data_builder.Append("\"}");
  String aux_data = aux_data_builder.ToString();
  String human_readable_name =
      !world.IsMainWorld() ? world.NonMainWorldHumanReadableName() : String();
  String origin_string = origin ? origin->ToRawString() : String();
  v8_inspector::V8ContextInfo context_info(
      script_state->GetContext(), ContextGroupId(frame),
      ToV8InspectorStringView(human_readable_name));
  context_info.origin = ToV8InspectorStringView(origin_string);
  context_info.auxData = ToV8InspectorStringView(aux_data);
  context_info.hasMemoryOnConsole =
      ExecutionContext::From(script_state) &&
      ExecutionContext::From(script_state)->IsDocument();
  GetV8Inspector()->contextCreated(context_info);
}

void MainThreadDebugger::ContextWillBeDestroyed(ScriptState* script_state) {
  v8::HandleScope handles(script_state->GetIsolate());
  GetV8Inspector()->contextDestroyed(script_state->GetContext());
}

void MainThreadDebugger::ExceptionThrown(ExecutionContext* context,
                                         ErrorEvent* event) {
  LocalFrame* frame = nullptr;
  ScriptState* script_state = nullptr;
  if (auto* document = DynamicTo<Document>(context)) {
    frame = document->GetFrame();
    if (!frame)
      return;
    script_state =
        event->World() ? ToScriptState(frame, *event->World()) : nullptr;
  } else if (context->IsMainThreadWorkletGlobalScope()) {
    auto* scope = To<WorkletGlobalScope>(context);
    frame = scope->GetFrame();
    if (!frame)
      return;
    script_state = scope->ScriptController()->GetScriptState();
  } else {
    NOTREACHED();
  }

  frame->Console().ReportMessageToClient(
      mojom::ConsoleMessageSource::kJavaScript,
      mojom::ConsoleMessageLevel::kError, event->MessageForConsole(),
      event->Location());

  const String default_message = "Uncaught";
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

int MainThreadDebugger::ContextGroupId(LocalFrame* frame) {
  LocalFrame& local_frame_root = frame->LocalFrameRoot();
  return WeakIdentifierMap<LocalFrame>::Identifier(&local_frame_root);
}

MainThreadDebugger* MainThreadDebugger::Instance() {
  DCHECK(IsMainThread());
  ThreadDebugger* debugger =
      ThreadDebugger::From(V8PerIsolateData::MainThreadIsolate());
  DCHECK(debugger && !debugger->IsWorker());
  return static_cast<MainThreadDebugger*>(debugger);
}

void MainThreadDebugger::runMessageLoopOnPause(int context_group_id) {
  LocalFrame* paused_frame =
      WeakIdentifierMap<LocalFrame>::Lookup(context_group_id);
  // Do not pause in Context of detached frame.
  if (!paused_frame)
    return;
  // If we hit a break point in the paint() function for CSS paint, then we are
  // in the middle of document life cycle. In this case, we should not allow
  // any style update or layout, which could be triggered by resizing the
  // browser window, or clicking at the element panel on devtool.
  if (paused_frame->GetDocument() &&
      !paused_frame->GetDocument()->Lifecycle().StateAllowsTreeMutations()) {
    postponed_transition_scope_ =
        std::make_unique<DocumentLifecycle::PostponeTransitionScope>(
            paused_frame->GetDocument()->Lifecycle());
  }
  DCHECK(paused_frame == paused_frame->LocalFrameRoot());
  paused_ = true;

  // Wait for continue or step command.
  if (client_message_loop_)
    client_message_loop_->Run(paused_frame);
}

void MainThreadDebugger::quitMessageLoopOnPause() {
  paused_ = false;
  postponed_transition_scope_.reset();
  if (client_message_loop_)
    client_message_loop_->QuitNow();
}

void MainThreadDebugger::muteMetrics(int context_group_id) {
  LocalFrame* frame = WeakIdentifierMap<LocalFrame>::Lookup(context_group_id);
  if (!frame)
    return;
  if (frame->GetDocument() && frame->GetDocument()->Loader())
    frame->GetDocument()->Loader()->GetUseCounterHelper().MuteForInspector();
  if (frame->GetPage())
    frame->GetPage()->GetDeprecation().MuteForInspector();
}

void MainThreadDebugger::unmuteMetrics(int context_group_id) {
  LocalFrame* frame = WeakIdentifierMap<LocalFrame>::Lookup(context_group_id);
  if (!frame)
    return;
  if (frame->GetDocument() && frame->GetDocument()->Loader())
    frame->GetDocument()->Loader()->GetUseCounterHelper().UnmuteForInspector();
  if (frame->GetPage())
    frame->GetPage()->GetDeprecation().UnmuteForInspector();
}

v8::Local<v8::Context> MainThreadDebugger::ensureDefaultContextInGroup(
    int context_group_id) {
  LocalFrame* frame = WeakIdentifierMap<LocalFrame>::Lookup(context_group_id);
  if (!frame)
    return v8::Local<v8::Context>();

  // This is a workaround code with a bailout to avoid crashing in
  // LocalWindowProxy::Initialize().
  // We cannot request a ScriptState on a provisional frame as it would lead
  // to a context creation on it, which is not allowed. Remove this extra check
  // when provisional frames concept gets eliminated. See crbug.com/897816
  // The DCHECK is kept to catch additional regressions earlier.
  DCHECK(!frame->IsProvisional());
  if (frame->IsProvisional())
    return v8::Local<v8::Context>();

  ScriptState* script_state = ToScriptStateForMainWorld(frame);
  return script_state ? script_state->GetContext() : v8::Local<v8::Context>();
}

void MainThreadDebugger::beginEnsureAllContextsInGroup(int context_group_id) {
  LocalFrame* frame = WeakIdentifierMap<LocalFrame>::Lookup(context_group_id);
  frame->GetSettings()->SetForceMainWorldInitialization(true);
}

void MainThreadDebugger::endEnsureAllContextsInGroup(int context_group_id) {
  LocalFrame* frame = WeakIdentifierMap<LocalFrame>::Lookup(context_group_id);
  frame->GetSettings()->SetForceMainWorldInitialization(false);
}

bool MainThreadDebugger::canExecuteScripts(int context_group_id) {
  LocalFrame* frame = WeakIdentifierMap<LocalFrame>::Lookup(context_group_id);
  return frame->GetDocument()->CanExecuteScripts(kNotAboutToExecuteScript);
}

void MainThreadDebugger::runIfWaitingForDebugger(int context_group_id) {
  LocalFrame* frame = WeakIdentifierMap<LocalFrame>::Lookup(context_group_id);
  if (client_message_loop_)
    client_message_loop_->RunIfWaitingForDebugger(frame);
}

void MainThreadDebugger::consoleAPIMessage(
    int context_group_id,
    v8::Isolate::MessageErrorLevel level,
    const v8_inspector::StringView& message,
    const v8_inspector::StringView& url,
    unsigned line_number,
    unsigned column_number,
    v8_inspector::V8StackTrace* stack_trace) {
  LocalFrame* frame = WeakIdentifierMap<LocalFrame>::Lookup(context_group_id);
  if (!frame)
    return;
  // TODO(dgozman): we can save a copy of message and url here by making
  // FrameConsole work with StringView.
  std::unique_ptr<SourceLocation> location = std::make_unique<SourceLocation>(
      ToCoreString(url), line_number, column_number,
      stack_trace ? stack_trace->clone() : nullptr, 0);
  frame->Console().ReportMessageToClient(
      mojom::ConsoleMessageSource::kConsoleApi,
      V8MessageLevelToMessageLevel(level), ToCoreString(message),
      location.get());
}

void MainThreadDebugger::consoleClear(int context_group_id) {
  LocalFrame* frame = WeakIdentifierMap<LocalFrame>::Lookup(context_group_id);
  if (!frame)
    return;
  if (frame->GetPage())
    frame->GetPage()->GetConsoleMessageStorage().Clear();
}

v8::MaybeLocal<v8::Value> MainThreadDebugger::memoryInfo(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context) {
  ExecutionContext* execution_context = ToExecutionContext(context);
  DCHECK(execution_context);
  DCHECK(execution_context->IsDocument());
  return ToV8(
      MakeGarbageCollected<MemoryInfo>(MemoryInfo::Precision::Bucketized),
      context->Global(), isolate);
}

void MainThreadDebugger::installAdditionalCommandLineAPI(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> object) {
  ThreadDebugger::installAdditionalCommandLineAPI(context, object);
  CreateFunctionProperty(
      context, object, "$", MainThreadDebugger::QuerySelectorCallback,
      "function $(selector, [startNode]) { [Command Line API] }",
      v8::SideEffectType::kHasNoSideEffect);
  CreateFunctionProperty(
      context, object, "$$", MainThreadDebugger::QuerySelectorAllCallback,
      "function $$(selector, [startNode]) { [Command Line API] }",
      v8::SideEffectType::kHasNoSideEffect);
  CreateFunctionProperty(
      context, object, "$x", MainThreadDebugger::XpathSelectorCallback,
      "function $x(xpath, [startNode]) { [Command Line API] }",
      v8::SideEffectType::kHasNoSideEffect);
}

static Node* SecondArgumentAsNode(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() > 1) {
    if (Node* node = V8Node::ToImplWithTypeCheck(info.GetIsolate(), info[1]))
      return node;
  }
  ExecutionContext* execution_context =
      ToExecutionContext(info.GetIsolate()->GetCurrentContext());
  return DynamicTo<Document>(execution_context);
}

void MainThreadDebugger::QuerySelectorCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 1)
    return;
  String selector = ToCoreStringWithUndefinedOrNullCheck(info[0]);
  if (selector.IsEmpty())
    return;
  auto* container_node = DynamicTo<ContainerNode>(SecondArgumentAsNode(info));
  if (!container_node)
    return;
  ExceptionState exception_state(info.GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 "CommandLineAPI", "$");
  Element* element =
      container_node->QuerySelector(AtomicString(selector), exception_state);
  if (exception_state.HadException())
    return;
  if (element)
    info.GetReturnValue().Set(ToV8(element, info.Holder(), info.GetIsolate()));
  else
    info.GetReturnValue().Set(v8::Null(info.GetIsolate()));
}

void MainThreadDebugger::QuerySelectorAllCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 1)
    return;
  String selector = ToCoreStringWithUndefinedOrNullCheck(info[0]);
  if (selector.IsEmpty())
    return;
  auto* container_node = DynamicTo<ContainerNode>(SecondArgumentAsNode(info));
  if (!container_node)
    return;
  ExceptionState exception_state(info.GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 "CommandLineAPI", "$$");
  // ToV8(elementList) doesn't work here, since we need a proper Array instance,
  // not NodeList.
  StaticElementList* element_list =
      container_node->QuerySelectorAll(AtomicString(selector), exception_state);
  if (exception_state.HadException() || !element_list)
    return;
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Array> nodes = v8::Array::New(isolate, element_list->length());
  for (wtf_size_t i = 0; i < element_list->length(); ++i) {
    Element* element = element_list->item(i);
    if (!CreateDataPropertyInArray(
             context, nodes, i, ToV8(element, info.Holder(), info.GetIsolate()))
             .FromMaybe(false))
      return;
  }
  info.GetReturnValue().Set(nodes);
}

void MainThreadDebugger::XpathSelectorCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  if (info.Length() < 1)
    return;
  String selector = ToCoreStringWithUndefinedOrNullCheck(info[0]);
  if (selector.IsEmpty())
    return;
  Node* node = SecondArgumentAsNode(info);
  if (!node || !node->IsContainerNode())
    return;

  ExceptionState exception_state(info.GetIsolate(),
                                 ExceptionState::kExecutionContext,
                                 "CommandLineAPI", "$x");
  XPathResult* result = XPathEvaluator::Create()->evaluate(
      selector, node, nullptr, XPathResult::kAnyType, ScriptValue(),
      exception_state);
  if (exception_state.HadException() || !result)
    return;
  if (result->resultType() == XPathResult::kNumberType) {
    info.GetReturnValue().Set(ToV8(result->numberValue(exception_state),
                                   info.Holder(), info.GetIsolate()));
  } else if (result->resultType() == XPathResult::kStringType) {
    info.GetReturnValue().Set(ToV8(result->stringValue(exception_state),
                                   info.Holder(), info.GetIsolate()));
  } else if (result->resultType() == XPathResult::kBooleanType) {
    info.GetReturnValue().Set(ToV8(result->booleanValue(exception_state),
                                   info.Holder(), info.GetIsolate()));
  } else {
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Array> nodes = v8::Array::New(isolate);
    wtf_size_t index = 0;
    while (Node* node = result->iterateNext(exception_state)) {
      if (exception_state.HadException())
        return;
      if (!CreateDataPropertyInArray(
               context, nodes, index++,
               ToV8(node, info.Holder(), info.GetIsolate()))
               .FromMaybe(false))
        return;
    }
    info.GetReturnValue().Set(nodes);
  }
}

}  // namespace blink
