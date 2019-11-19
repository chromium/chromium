// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_interface.h"

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/platform/bindings/scoped_persistent.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

namespace {

class ReadableStreamDefaultControllerNative final
    : public ReadableStreamDefaultControllerInterface {
 public:
  explicit ReadableStreamDefaultControllerNative(ScriptState* script_state,
                                                 ScriptValue controller)
      : ReadableStreamDefaultControllerInterface(script_state) {
    v8::Local<v8::Object> controller_object =
        controller.V8Value().As<v8::Object>();
    controller_ = V8ReadableStreamDefaultController::ToImpl(controller_object);

    DCHECK(controller_);
  }

  void NoteHasBeenCanceled() override { controller_ = nullptr; }

  void Close() override {
    if (!controller_)
      return;

    ScriptState::Scope scope(script_state_);

    ReadableStreamDefaultController::Close(script_state_, controller_);
    controller_ = nullptr;
  }

  double DesiredSize() const override {
    if (!controller_)
      return 0.0;

    base::Optional<double> desired_size = controller_->GetDesiredSize();
    DCHECK(desired_size.has_value());
    return desired_size.value();
  }

  void Enqueue(v8::Local<v8::Value> js_chunk) const override {
    if (!controller_)
      return;

    ScriptState::Scope scope(script_state_);

    ExceptionState exception_state(script_state_->GetIsolate(),
                                   ExceptionState::kUnknownContext, "", "");
    ReadableStreamDefaultController::Enqueue(script_state_, controller_,
                                             js_chunk, exception_state);
    if (exception_state.HadException()) {
      DLOG(WARNING) << "Ignoring exception from Enqueue()";
      exception_state.ClearException();
    }
  }

  void Error(v8::Local<v8::Value> js_error) override {
    if (!controller_)
      return;

    ScriptState::Scope scope(script_state_);

    ReadableStreamDefaultController::Error(script_state_, controller_,
                                           js_error);
    controller_ = nullptr;
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(controller_);
    ReadableStreamDefaultControllerInterface::Trace(visitor);
  }

 private:
  Member<ReadableStreamDefaultController> controller_;
};

}  // namespace

ReadableStreamDefaultControllerInterface*
ReadableStreamDefaultControllerInterface::Create(ScriptState* script_state,
                                                 ScriptValue controller) {
  return MakeGarbageCollected<ReadableStreamDefaultControllerNative>(
      script_state, controller);
}

ReadableStreamDefaultControllerInterface::
    ReadableStreamDefaultControllerInterface(ScriptState* script_state)
    : script_state_(script_state) {}

ReadableStreamDefaultControllerInterface::
    ~ReadableStreamDefaultControllerInterface() = default;

void ReadableStreamDefaultControllerInterface::Trace(Visitor* visitor) {
  visitor->Trace(script_state_);
}

}  // namespace blink
