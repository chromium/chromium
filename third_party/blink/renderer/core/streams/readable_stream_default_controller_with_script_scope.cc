// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_with_script_scope.h"

#include <optional>

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/scoped_persistent.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

ReadableStreamDefaultControllerWithScriptScope::
    ReadableStreamDefaultControllerWithScriptScope(
        ScriptState* script_state,
        ReadableStreamDefaultController* controller)
    : script_state_(script_state), controller_(controller) {}

void ReadableStreamDefaultControllerWithScriptScope::Deactivate() {
  controller_ = nullptr;
}

void ReadableStreamDefaultControllerWithScriptScope::Close() {
  if (!controller_)
    return;

  if (ReadableStreamDefaultController::CanCloseOrEnqueue(controller_)) {
    if (script_state_->ContextIsValid()) {
      ScriptState::Scope scope(script_state_);
      ReadableStreamDefaultController::Close(script_state_, controller_);
    } else {
      // If the context is not valid then Close() will not try to resolve the
      // promises, and that is not a problem.
      ReadableStreamDefaultController::Close(script_state_, controller_);
    }
  }
  controller_ = nullptr;
}

double ReadableStreamDefaultControllerWithScriptScope::DesiredSize() const {
  if (!controller_)
    return 0.0;

  std::optional<double> desired_size = controller_->GetDesiredSize();
  DCHECK(desired_size.has_value());
  return desired_size.value();
}

void ReadableStreamDefaultControllerWithScriptScope::Enqueue(
    v8::Local<v8::Value> js_chunk) const {
  if (!controller_)
    return;

  if (!ReadableStreamDefaultController::CanCloseOrEnqueue(controller_)) {
    return;
  }

  ScriptState::Scope scope(script_state_);

  v8::Isolate* isolate = script_state_->GetIsolate();
  v8::MicrotasksScope microtasks_scope(
      isolate, ToMicrotaskQueue(script_state_),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  ReadableStreamDefaultController::Enqueue(script_state_, controller_, js_chunk,
                                           IGNORE_EXCEPTION);
}

void ReadableStreamDefaultControllerWithScriptScope::Error(
    v8::Local<v8::Value> js_error) {
  if (!controller_)
    return;

  ScriptState::Scope scope(script_state_);

  ReadableStreamDefaultController::Error(script_state_, controller_, js_error);
  controller_ = nullptr;
}

void ReadableStreamDefaultControllerWithScriptScope::Trace(
    Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(controller_);
}

}  // namespace blink
