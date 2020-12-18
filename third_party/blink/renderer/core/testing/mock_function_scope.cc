// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/mock_function_scope.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"

namespace blink {

MockFunctionScope::MockFunctionScope(ScriptState* script_state)
    : script_state_(script_state) {}

MockFunctionScope::~MockFunctionScope() {
  v8::MicrotasksScope::PerformCheckpoint(script_state_->GetIsolate());
  for (MockFunction* mock_function : mock_functions_) {
    testing::Mock::VerifyAndClearExpectations(mock_function);
  }
}

v8::Local<v8::Function> MockFunctionScope::ExpectCall(String* captor) {
  mock_functions_.push_back(
      MakeGarbageCollected<MockFunction>(script_state_, captor));
  EXPECT_CALL(*mock_functions_.back(), Call(testing::_));
  return mock_functions_.back()->Bind();
}

v8::Local<v8::Function> MockFunctionScope::ExpectCall() {
  mock_functions_.push_back(MakeGarbageCollected<MockFunction>(script_state_));
  EXPECT_CALL(*mock_functions_.back(), Call(testing::_));
  return mock_functions_.back()->Bind();
}

v8::Local<v8::Function> MockFunctionScope::ExpectNoCall() {
  mock_functions_.push_back(MakeGarbageCollected<MockFunction>(script_state_));
  EXPECT_CALL(*mock_functions_.back(), Call(testing::_)).Times(0);
  return mock_functions_.back()->Bind();
}

ACTION_P2(SaveValueIn, script_state, captor) {
  *captor = ToCoreString(
      arg0.V8Value()->ToString(script_state->GetContext()).ToLocalChecked());
}

MockFunctionScope::MockFunction::MockFunction(ScriptState* script_state)
    : ScriptFunction(script_state) {
  ON_CALL(*this, Call(testing::_)).WillByDefault(testing::ReturnArg<0>());
}

MockFunctionScope::MockFunction::MockFunction(ScriptState* script_state,
                                              String* captor)
    : ScriptFunction(script_state) {
  ON_CALL(*this, Call(testing::_))
      .WillByDefault(
          testing::DoAll(SaveValueIn(WrapPersistent(script_state), captor),
                         testing::ReturnArg<0>()));
}

v8::Local<v8::Function> MockFunctionScope::MockFunction::Bind() {
  return BindToV8Function();
}

}  // namespace blink
