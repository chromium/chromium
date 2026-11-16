// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/javascript_call_stack_collector.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/scheme_registry.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {
namespace {

class JavaScriptCallStackCollectorTest : public testing::Test {
 protected:
  void SetUp() override {
    CommonSchemeRegistry::RegisterURLSchemeAsExtension("test-extension");
  }

  void TearDown() override {
    CommonSchemeRegistry::RemoveURLSchemeAsExtensionForTest("test-extension");
  }

  static void CollectStack(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(V8String(
        args.GetIsolate(),
        JavaScriptCallStackCollector::CollectJavaScriptCallStackForTesting(
            args.GetIsolate())));
  }

  test::TaskEnvironment task_environment_;
};

TEST_F(JavaScriptCallStackCollectorTest, FiltersScriptFrames) {
  const struct {
    const char* name;
    const char* script_url;
    bool is_shared_cross_origin;
    bool expect_redacted;
  } test_cases[] = {
      {"SameOrigin", "https://document.test/script.js", true, false},
      {"SharedCrossOrigin", "https://scripts.test/script.js", true, false},
      {"UnsharedCrossOrigin", "https://scripts.test/script.js", false, true},
      {"UnsharedSameOriginName", "https://document.test/script.js", false,
       true},
      {"UnsharedEmptyName", "", false, true},
      {"UnsharedInvalidName", "script.js", false, true},
      {"SharedEmptyName", "", true, false},
      {"SharedInvalidName", "script.js", true, false},
      {"Extension", "test-extension://id/script.js", true, true},
  };
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.name);
    [&] {
      V8TestingScope scope(KURL("https://document.test/"));
      v8::Isolate* isolate = scope.GetIsolate();
      v8::Local<v8::Context> context = scope.GetContext();
      ASSERT_TRUE(
          context->Global()
              ->Set(context, V8String(isolate, "collectStack"),
                    v8::Function::New(context, CollectStack).ToLocalChecked())
              .FromJust());

      v8::ScriptOrigin origin(V8String(isolate, test_case.script_url),
                              /*resource_line_offset=*/0,
                              /*resource_column_offset=*/0,
                              test_case.is_shared_cross_origin);
      for (bool through_builtin : {false, true}) {
        SCOPED_TRACE(through_builtin);
        const char* source = through_builtin
                                 ? "var stack;\n"
                                   "[0].forEach(function testFunction() {\n"
                                   "  stack = collectStack();\n"
                                   "});\n"
                                   "stack;"
                                 : "function testFunction() {\n"
                                   "  return collectStack();\n"
                                   "}\n"
                                   "testFunction();";
        v8::Local<v8::Script> script;
        ASSERT_TRUE(
            v8::Script::Compile(context, V8String(isolate, source), &origin)
                .ToLocal(&script));
        v8::Local<v8::Value> result;
        ASSERT_TRUE(script->Run(context).ToLocal(&result));
        ASSERT_TRUE(result->IsString());
        String stack = ToCoreString(isolate, result.As<v8::String>());
        if (test_case.expect_redacted) {
          EXPECT_EQ(
              through_builtin
                  ? "\n    at <redacted>\n    at <redacted>\n    at <redacted>"
                  : "\n    at <redacted>\n    at <redacted>",
              stack);
        } else {
          EXPECT_TRUE(stack.contains("testFunction"));
          EXPECT_FALSE(stack.contains("<redacted>"));
          EXPECT_EQ(through_builtin, stack.contains("forEach"));
          EXPECT_TRUE(stack.contains(test_case.script_url));
        }
      }
    }();
  }
}

TEST_F(JavaScriptCallStackCollectorTest, FiltersMixedScriptFrames) {
  V8TestingScope scope(KURL("https://document.test/"));
  v8::Isolate* isolate = scope.GetIsolate();
  v8::Local<v8::Context> context = scope.GetContext();
  ASSERT_TRUE(
      context->Global()
          ->Set(context, V8String(isolate, "collectStack"),
                v8::Function::New(context, CollectStack).ToLocalChecked())
          .FromJust());

  v8::ScriptOrigin shared_origin(
      V8String(isolate, "https://document.test/shared.js"),
      /*resource_line_offset=*/0, /*resource_column_offset=*/0,
      /*resource_is_shared_cross_origin=*/true);
  v8::ScriptOrigin unshared_origin(
      V8String(isolate, "https://scripts.test/unshared.js"),
      /*resource_line_offset=*/0, /*resource_column_offset=*/0,
      /*resource_is_shared_cross_origin=*/false);
  v8::Local<v8::Script> script;
  ASSERT_TRUE(
      v8::Script::Compile(
          context,
          V8String(isolate,
                   "function sharedCaller() { return hiddenCallee(); }"
                   "function sharedCallee() { return collectStack(); }"),
          &shared_origin)
          .ToLocal(&script));
  ASSERT_FALSE(script->Run(context).IsEmpty());
  ASSERT_TRUE(
      v8::Script::Compile(
          context,
          V8String(isolate,
                   "function hiddenCaller() { return sharedCallee(); }"
                   "function hiddenCallee() { return collectStack(); }"),
          &unshared_origin)
          .ToLocal(&script));
  ASSERT_FALSE(script->Run(context).IsEmpty());

  for (bool shared_on_top : {false, true}) {
    SCOPED_TRACE(shared_on_top);
    ASSERT_TRUE(v8::Script::Compile(
                    context,
                    V8String(isolate, shared_on_top ? "hiddenCaller();"
                                                    : "sharedCaller();"),
                    &shared_origin)
                    .ToLocal(&script));
    v8::Local<v8::Value> result;
    ASSERT_TRUE(script->Run(context).ToLocal(&result));
    ASSERT_TRUE(result->IsString());
    String stack = ToCoreString(isolate, result.As<v8::String>());
    EXPECT_TRUE(
        stack.contains(shared_on_top ? "sharedCallee" : "sharedCaller"));
    EXPECT_TRUE(stack.contains("https://document.test/shared.js"));
    EXPECT_TRUE(stack.contains("<redacted>"));
    EXPECT_FALSE(stack.contains("hiddenCaller"));
    EXPECT_FALSE(stack.contains("hiddenCallee"));
    EXPECT_FALSE(stack.contains("https://scripts.test/unshared.js"));
  }
}

}  // namespace
}  // namespace blink
