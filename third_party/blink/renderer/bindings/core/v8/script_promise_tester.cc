// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"

#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

void ScriptPromiseTester::WaitUntilSettled() {
  auto* isolate = script_state_->GetIsolate();
  while (state_ == State::kNotSettled) {
    if (exception_state_ && exception_state_->HadException()) {
      state_ = State::kRejected;
      break;
    }
    script_state_->GetContext()->GetMicrotaskQueue()->PerformCheckpoint(
        isolate);
    test::RunPendingTasks();
  }
}

ScriptValue ScriptPromiseTester::Value() const {
  return value_object_->Value();
}

String ScriptPromiseTester::ValueAsString() const {
  String result;
  Value().ToString(result);
  return result;
}

}  // namespace blink
