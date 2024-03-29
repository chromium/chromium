// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

MockFunctionScope::MockFunctionScope(ScriptState* script_state)
    : script_state_(script_state) {}

MockFunctionScope::~MockFunctionScope() {
  script_state_->GetContext()->GetMicrotaskQueue()->PerformCheckpoint(
      script_state_->GetIsolate());
  for (MockFunction* mock_function : mock_functions_) {
    testing::Mock::VerifyAndClearExpectations(mock_function);
  }
}

ScriptFunction* MockFunctionScope::ExpectCall(String* captor) {
  mock_functions_.push_back(
      MakeGarbageCollected<MockFunction>(script_state_, captor));
  EXPECT_CALL(*mock_functions_.back(), Call(script_state_, testing::_));
  return MakeGarbageCollected<ScriptFunction>(script_state_,
                                              mock_functions_.back());
}

ScriptFunction* MockFunctionScope::ExpectCall() {
  mock_functions_.push_back(MakeGarbageCollected<MockFunction>());
  EXPECT_CALL(*mock_functions_.back(), Call(script_state_, testing::_));
  return MakeGarbageCollected<ScriptFunction>(script_state_,
                                              mock_functions_.back());
}

ScriptFunction* MockFunctionScope::ExpectNoCall() {
  mock_functions_.push_back(MakeGarbageCollected<MockFunction>());
  EXPECT_CALL(*mock_functions_.back(), Call(script_state_, testing::_))
      .Times(0);
  return MakeGarbageCollected<ScriptFunction>(script_state_,
                                              mock_functions_.back());
}

ACTION_P2(SaveValueIn, script_state, captor) {
  *captor = ToCoreString(
      script_state->GetIsolate(),
      arg1.V8Value()->ToString(script_state->GetContext()).ToLocalChecked());
}

MockFunctionScope::MockFunction::MockFunction() {
  ON_CALL(*this, Call(testing::_, testing::_))
      .WillByDefault(testing::ReturnArg<1>());
}

MockFunctionScope::MockFunction::MockFunction(ScriptState* script_state,
                                              String* captor) {
  ON_CALL(*this, Call(script_state, testing::_))
      .WillByDefault(testing::DoAll(SaveValueIn(script_state, captor),
                                    testing::ReturnArg<1>()));
}

}  // namespace blink
