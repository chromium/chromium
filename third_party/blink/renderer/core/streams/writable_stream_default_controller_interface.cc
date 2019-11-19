// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/writable_stream_default_controller_interface.h"

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_writable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

namespace {

class WritableStreamDefaultControllerNative final
    : public WritableStreamDefaultControllerInterface {
 public:
  explicit WritableStreamDefaultControllerNative(ScriptValue controller) {
    DCHECK(controller.IsObject());
    controller_ = V8WritableStreamDefaultController::ToImpl(
        controller.V8Value().As<v8::Object>());
    DCHECK(controller_);
  }

  void Error(ScriptState* script_state, v8::Local<v8::Value> error) override {
    WritableStreamDefaultController::Error(script_state, controller_, error);
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(controller_);
    WritableStreamDefaultControllerInterface::Trace(visitor);
  }

 private:
  Member<WritableStreamDefaultController> controller_;
};

}  // namespace

WritableStreamDefaultControllerInterface::
    WritableStreamDefaultControllerInterface() = default;
WritableStreamDefaultControllerInterface::
    ~WritableStreamDefaultControllerInterface() = default;

WritableStreamDefaultControllerInterface*
WritableStreamDefaultControllerInterface::Create(ScriptValue controller) {
  return MakeGarbageCollected<WritableStreamDefaultControllerNative>(
      controller);
}

}  // namespace blink
