// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_DEFAULT_CONTROLLER_INTERFACE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_DEFAULT_CONTROLLER_INTERFACE_H_

#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "v8/include/v8.h"

namespace blink {

class Visitor;

class CORE_EXPORT WritableStreamDefaultControllerInterface
    : public GarbageCollected<WritableStreamDefaultControllerInterface> {
 public:
  WritableStreamDefaultControllerInterface();
  virtual ~WritableStreamDefaultControllerInterface();

  static WritableStreamDefaultControllerInterface* Create(
      ScriptValue controller);

  // Unlike the corresponding method in
  // ReadableStreamDefaultControllerInterface, a caller needs to enter a
  // ScriptState::Scope.
  virtual void Error(ScriptState* script_state, v8::Local<v8::Value>) = 0;

  // Helper method
  template <typename ErrorType>
  void Error(ScriptState* script_state, ErrorType error) {
    Error(script_state, ToV8(error, script_state));
  }

  virtual void Trace(Visitor*) {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_WRITABLE_STREAM_DEFAULT_CONTROLLER_INTERFACE_H_
