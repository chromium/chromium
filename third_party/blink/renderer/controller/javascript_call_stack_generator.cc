// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/javascript_call_stack_generator.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/public/common/permissions_policy/document_policy_features.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace WTF {

template <>
struct CrossThreadCopier<std::optional<blink::LocalFrameToken>>
    : public CrossThreadCopierPassThrough<
          std::optional<blink::LocalFrameToken>> {};

}  // namespace WTF

namespace blink {

namespace {

// Format the callstack in a format that's
// consistent with Error.stack
void FormatStackTrace(v8::Isolate* isolate, StringBuilder& builder) {
  std::ostringstream oss;
  v8::Message::PrintCurrentStackTrace(isolate, oss);
  const std::string stack_trace = oss.str();
  std::istringstream iss(stack_trace);
  std::string line;
  while (std::getline(iss, line)) {
    builder.Append("\n    at ");
    builder.Append(line.data(), base::checked_cast<unsigned>(line.size()));
  }
}

void PostHandleCollectedCallStackTask(
    JavaScriptCallStackGenerator* generator,
    WTF::StringBuilder& builder,
    std::optional<LocalFrameToken> frame_token = std::nullopt) {
  DCHECK(Platform::Current());
  PostCrossThreadTask(
      *Platform::Current()->GetIOTaskRunner(), FROM_HERE,
      WTF::CrossThreadBindOnce(
          [](JavaScriptCallStackGenerator* generator, String call_stack,
             std::optional<LocalFrameToken> frame_token) {
            generator->HandleCallStackCollected(call_stack, frame_token);
          },
          CrossThreadUnretained(generator), builder.ReleaseString(),
          frame_token));
}

void GenerateJavaScriptCallStack(v8::Isolate* isolate, void* data) {
  CHECK(IsMainThread());

  auto* generator = static_cast<JavaScriptCallStackGenerator*>(data);
  v8::HandleScope handle_scope(isolate);
  WTF::StringBuilder builder;
  if (!isolate->InContext()) {
    PostHandleCollectedCallStackTask(generator, builder);
    return;
  }

  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  ScriptState* script_state = ScriptState::MaybeFrom(context);
  if (!script_state) {
    PostHandleCollectedCallStackTask(generator, builder);
    return;
  }
  ExecutionContext* execution_context = ToExecutionContext(script_state);
  DOMWrapperWorld& world = script_state->World();
  auto* execution_dom_window = DynamicTo<LocalDOMWindow>(execution_context);
  LocalFrame* frame =
      execution_dom_window ? execution_dom_window->GetFrame() : nullptr;

  std::optional<LocalFrameToken> frame_token;
  if (frame && world.IsMainWorld()) {
    frame_token = frame->GetLocalFrameToken();
    if (!execution_context->IsFeatureEnabled(
            mojom::blink::DocumentPolicyFeature::
                kIncludeJSCallStacksInCrashReports)) {
      builder.Append(
          "Website owner has not opted in for JS call stacks in crash "
          "reports.");
    } else {
      FormatStackTrace(isolate, builder);
    }
  }
  PostHandleCollectedCallStackTask(generator, builder, frame_token);
}

}  // namespace

// At any point in time, there is at most one execution context per isolate
// that is actually executing code.
void JavaScriptCallStackGenerator::InterruptIsolateAndCollectCallStack(
    v8::Isolate* isolate) {
  isolate->RequestInterrupt(&GenerateJavaScriptCallStack,
                            static_cast<void*>(this));
}

void JavaScriptCallStackGenerator::HandleCallStackCollected(
    const String& call_stack,
    const std::optional<LocalFrameToken> frame_token) {
  if (!call_stack_collected_) {
    call_stack_collected_ = true;
    DCHECK(callback_);
    std::move(callback_).Run(call_stack, frame_token);
  }
}

void JavaScriptCallStackGenerator::CollectJavaScriptCallStack(
    CollectJavaScriptCallStackCallback callback) {
  call_stack_collected_ = false;
  if (RuntimeEnabledFeatures::
          DocumentPolicyIncludeJSCallStacksInCrashReportsEnabled()) {
    callback_ = std::move(callback);
    Thread::MainThread()
        ->Scheduler()
        ->ToMainThreadScheduler()
        ->ForEachMainThreadIsolate(WTF::BindRepeating(
            &JavaScriptCallStackGenerator::InterruptIsolateAndCollectCallStack,
            WTF::Unretained(this)));
  }
}

JavaScriptCallStackGenerator& GetJavaScriptCallStackGenerator() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(JavaScriptCallStackGenerator,
                                  javascript_call_stack_generator, ());
  return javascript_call_stack_generator;
}

void JavaScriptCallStackGenerator::Bind(
    mojo::PendingReceiver<mojom::blink::CallStackGenerator> receiver) {
  DCHECK(!GetJavaScriptCallStackGenerator().receiver_.is_bound());
  GetJavaScriptCallStackGenerator().receiver_.Bind(std::move(receiver));
}

}  // namespace blink
