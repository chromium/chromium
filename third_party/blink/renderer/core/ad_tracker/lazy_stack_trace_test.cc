// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/lazy_stack_trace.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

v8::Local<v8::Script> CompileWithUrl(V8TestingScope& scope,
                                     const char* source,
                                     const char* url) {
  v8::Isolate* isolate = scope.GetIsolate();
  v8::Local<v8::Context> context = scope.GetContext();
  v8::ScriptOrigin origin(
      V8String(isolate, url), /*resource_line_offset=*/0,
      /*resource_column_offset=*/0, /*resource_is_shared_cross_origin=*/false,
      /*script_id=*/-1,
      /*source_map_url=*/v8::Local<v8::Value>(), /*resource_is_opaque=*/false,
      /*is_wasm=*/false, /*is_module=*/false);
  return v8::Script::Compile(context, V8String(isolate, source), &origin)
      .ToLocalChecked();
}

void LazyStackTraceTestCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  LazyStackTrace lazy_stack(isolate);

  // 1. Walk the stack up to 5 frames
  auto stack1 = lazy_stack.GetStack(5);
  // The stack should contain at least 2 frames (f2 and f1).
  ASSERT_GE(stack1.size(), 2u);

  // Verify the top frames.
  // Note: V8 script IDs are 1-based, we just want to ensure they are valid.
  EXPECT_NE(stack1[0].id, 0);
  EXPECT_NE(stack1[1].id, 0);

  // 2. Fetch stack with smaller limit -> should return subspan of cache (same
  // address)
  auto stack2 = lazy_stack.GetStack(2);
  EXPECT_EQ(stack2.size(), 2u);
  EXPECT_EQ(stack2.data(), stack1.data());

  // 3. Fetch stack with larger limit -> should return cached stack (same
  // address) since the stack was fully captured
  auto stack3 = lazy_stack.GetStack(10);
  ASSERT_GE(stack3.size(), stack1.size());
  EXPECT_EQ(stack3.data(), stack1.data());
}

void LazyStackTraceGrowthTestCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  LazyStackTrace lazy_stack(isolate);

  // 1. Walk the stack up to 3 frames.
  auto stack1 = lazy_stack.GetStack(3);
  EXPECT_EQ(stack1.size(), 3u);

  // 2. Fetch stack with larger limit (8 frames) -> should grow and fetch more.
  auto stack2 = lazy_stack.GetStack(8);
  EXPECT_GE(stack2.size(), 5u);
  EXPECT_GT(stack2.size(), stack1.size());
}

}  // namespace

TEST(LazyStackTraceTest, NullIsolate) {
  LazyStackTrace lazy_stack(nullptr);
  EXPECT_TRUE(lazy_stack.GetStack(5).empty());
}

TEST(LazyStackTraceTest, EmptyStack) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  LazyStackTrace lazy_stack(scope.GetIsolate());

  // Since we are not running any JS right now, the stack should be empty.
  EXPECT_TRUE(lazy_stack.GetStack(5).empty());
}

TEST(LazyStackTraceTest, StackWalkAndCache) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  v8::Local<v8::Context> context = scope.GetContext();
  v8::Local<v8::Object> global = context->Global();
  v8::Local<v8::Function> function =
      v8::Function::New(context, LazyStackTraceTestCallback).ToLocalChecked();
  global->Set(context, V8String(scope.GetIsolate(), "testCallback"), function)
      .ToChecked();

  // Define scripts and execute them.
  // Using CompileWithUrl to ensure they are treated as separate scripts.
  auto script_f1 = CompileWithUrl(scope, "function f1() { f2(); }",
                                  "https://example.com/f1.js");
  script_f1->Run(context).ToLocalChecked();

  auto script_f2 = CompileWithUrl(scope, "function f2() { testCallback(); }",
                                  "https://example.com/f2.js");
  script_f2->Run(context).ToLocalChecked();

  // Run the entry point
  auto script_main =
      CompileWithUrl(scope, "f1();", "https://example.com/main.js");
  script_main->Run(context).ToLocalChecked();
}

TEST(LazyStackTraceTest, CacheGrowth) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  v8::Local<v8::Context> context = scope.GetContext();
  v8::Local<v8::Object> global = context->Global();
  v8::Local<v8::Function> function =
      v8::Function::New(context, LazyStackTraceGrowthTestCallback)
          .ToLocalChecked();
  global->Set(context, V8String(scope.GetIsolate(), "testCallback"), function)
      .ToChecked();

  // Create a deep stack of 5 frames.
  auto script = CompileWithUrl(scope,
                               "function f4() { testCallback(); }\n"
                               "function f3() { f4(); }\n"
                               "function f2() { f3(); }\n"
                               "function f1() { f2(); }\n"
                               "f1();",
                               "https://example.com/stack.js");
  script->Run(context).ToLocalChecked();
}

}  // namespace blink
