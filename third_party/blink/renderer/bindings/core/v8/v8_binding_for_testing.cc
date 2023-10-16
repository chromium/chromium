// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

V8TestingScope::V8TestingScope(const KURL& url)
    : V8TestingScope(DummyPageHolder::CreateAndCommitNavigation(url)) {}

V8TestingScope::V8TestingScope(std::unique_ptr<DummyPageHolder> holder)
    : holder_(std::move(holder)),
      handle_scope_(GetIsolate()),
      context_(GetScriptState()->GetContext()),
      context_scope_(GetContext()),
      try_catch_(GetIsolate()),
      microtasks_scope_(GetIsolate(),
                        ToMicrotaskQueue(GetScriptState()),
                        v8::MicrotasksScope::kDoNotRunMicrotasks) {
  GetFrame().GetSettings()->SetScriptEnabled(true);
}

ScriptState* V8TestingScope::GetScriptState() const {
  return ToScriptStateForMainWorld(holder_->GetDocument().GetFrame());
}

ExecutionContext* V8TestingScope::GetExecutionContext() const {
  return ExecutionContext::From(GetScriptState());
}

v8::Isolate* V8TestingScope::GetIsolate() const {
  return GetScriptState()->GetIsolate();
}

v8::Local<v8::Context> V8TestingScope::GetContext() const {
  return context_;
}

ExceptionState& V8TestingScope::GetExceptionState() {
  return exception_state_;
}

Page& V8TestingScope::GetPage() {
  return holder_->GetPage();
}

LocalFrame& V8TestingScope::GetFrame() {
  return holder_->GetFrame();
}

LocalDOMWindow& V8TestingScope::GetWindow() {
  return *GetFrame().DomWindow();
}

Document& V8TestingScope::GetDocument() {
  return holder_->GetDocument();
}

V8TestingScope::~V8TestingScope() {
  // Execute all pending microtasks.
  PerformMicrotaskCheckpoint();

  // TODO(yukishiino): We put this statement here to clear an exception from
  // the isolate.  Otherwise, the leak detector complains.  Really mysterious
  // hack.
  v8::Function::New(GetContext(), nullptr);
}

void V8TestingScope::PerformMicrotaskCheckpoint() {
  GetContext()->GetMicrotaskQueue()->PerformCheckpoint(
      GetContext()->GetIsolate());
}

}  // namespace blink
