// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TEST_UNDERLYING_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TEST_UNDERLYING_SOURCE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller_interface.h"
#include "third_party/blink/renderer/core/streams/underlying_source_base.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

// This class is for testing.
class TestUnderlyingSource final : public UnderlyingSourceBase {
 public:
  explicit TestUnderlyingSource(ScriptState* script_state)
      : UnderlyingSourceBase(script_state) {}

  // Just expose the controller methods for easy testing
  void Enqueue(ScriptValue value) { Controller()->Enqueue(value); }
  void Close() { Controller()->Close(); }
  void Error(ScriptValue value) { Controller()->Error(value); }
  double DesiredSize() { return Controller()->DesiredSize(); }

  ScriptPromise Start(ScriptState* script_state) override {
    DCHECK(!is_start_called_);
    is_start_called_ = true;
    return ScriptPromise::CastUndefined(script_state);
  }
  ScriptPromise Cancel(ScriptState* script_state, ScriptValue reason) override {
    DCHECK(!is_cancelled_);
    DCHECK(!is_cancelled_with_undefined_);
    DCHECK(!is_cancelled_with_null_);

    is_cancelled_ = true;
    is_cancelled_with_undefined_ = reason.V8Value()->IsUndefined();
    is_cancelled_with_null_ = reason.V8Value()->IsNull();
    return ScriptPromise::CastUndefined(script_state);
  }

  bool IsStartCalled() const { return is_start_called_; }
  bool IsCancelled() const { return is_cancelled_; }
  bool IsCancelledWithUndefined() const { return is_cancelled_with_undefined_; }
  bool IsCancelledWithNull() const { return is_cancelled_with_null_; }

 private:
  bool is_start_called_ = false;
  bool is_cancelled_ = false;
  bool is_cancelled_with_undefined_ = false;
  bool is_cancelled_with_null_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STREAMS_TEST_UNDERLYING_SOURCE_H_
