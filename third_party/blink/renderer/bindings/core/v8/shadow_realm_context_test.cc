// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/shadow_realm_context.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

class ShadowRealmContextTest : public BindingTestSupportingGC {};

TEST_F(ShadowRealmContextTest, DisposesPerContextDataAfterGarbageCollection) {
  V8TestingScope scope;
  SetIsolate(scope.GetIsolate());
  v8::Local<v8::Context> realm =
      OnCreateShadowRealmV8Context(scope.GetContext()).ToLocalChecked();
  Persistent<ScriptState> script_state =
      ScriptState::From(scope.GetIsolate(), realm);
  Persistent<ExecutionContext> execution_context =
      ExecutionContext::From(realm);
  ASSERT_NE(nullptr, script_state->PerContextData());

  // The weak lifecycle observer list must not be the controller's only owner.
  RunV8FullGC();
  scope.GetExecutionContext()->NotifyContextDestroyed();

  EXPECT_EQ(nullptr, script_state->PerContextData());
  EXPECT_TRUE(execution_context->IsContextDestroyed());
}

TEST_F(ShadowRealmContextTest, DestroysNestedRealmsAfterGarbageCollection) {
  V8TestingScope scope;
  SetIsolate(scope.GetIsolate());
  v8::Local<v8::Context> realm =
      OnCreateShadowRealmV8Context(scope.GetContext()).ToLocalChecked();
  v8::Local<v8::Context> nested_realm =
      OnCreateShadowRealmV8Context(realm).ToLocalChecked();
  Persistent<ScriptState> script_state =
      ScriptState::From(scope.GetIsolate(), realm);
  Persistent<ScriptState> nested_script_state =
      ScriptState::From(scope.GetIsolate(), nested_realm);
  Persistent<ExecutionContext> nested_execution_context =
      ExecutionContext::From(nested_realm);

  RunV8FullGC();
  scope.GetExecutionContext()->NotifyContextDestroyed();

  EXPECT_EQ(nullptr, script_state->PerContextData());
  EXPECT_EQ(nullptr, nested_script_state->PerContextData());
  EXPECT_TRUE(nested_execution_context->IsContextDestroyed());
}

}  // namespace blink
