// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/ad_tracker/monkey_patchable_api.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class MonkeyPatchableApiTest : public SimTest {};

TEST_F(MonkeyPatchableApiTest, GetMonkeyPatchableApiPropertyPath) {
  {
    auto path = GetMonkeyPatchableApiPropertyPath(
        MonkeyPatchableApi::kHistoryPushState);
    ASSERT_EQ(path.size(), 2u);
    EXPECT_STREQ(path[0], "history");
    EXPECT_STREQ(path[1], "pushState");
  }
  {
    auto path = GetMonkeyPatchableApiPropertyPath(
        MonkeyPatchableApi::kHistoryReplaceState);
    ASSERT_EQ(path.size(), 2u);
    EXPECT_STREQ(path[0], "history");
    EXPECT_STREQ(path[1], "replaceState");
  }
  {
    auto path =
        GetMonkeyPatchableApiPropertyPath(MonkeyPatchableApi::kNodeAppendChild);
    ASSERT_EQ(path.size(), 3u);
    EXPECT_STREQ(path[0], "Node");
    EXPECT_STREQ(path[1], "prototype");
    EXPECT_STREQ(path[2], "appendChild");
  }
}

TEST_F(MonkeyPatchableApiTest, NativeApiIsNotMonkeyPatched) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<html><body></body></html>");

  v8::Isolate* isolate = Window().GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(MainFrame().MainWorldScriptContext());

  {
    MonkeyPatchableApiFunctionInfo info = GetMonkeyPatchableApiFunctionInfo(
        isolate, MonkeyPatchableApi::kHistoryPushState);
    EXPECT_FALSE(info.is_monkey_patched);
  }
  {
    MonkeyPatchableApiFunctionInfo info = GetMonkeyPatchableApiFunctionInfo(
        isolate, MonkeyPatchableApi::kNodeAppendChild);
    EXPECT_FALSE(info.is_monkey_patched);
  }
}

TEST_F(MonkeyPatchableApiTest, MonkeyPatchedApiIsDetected) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest script("https://example.com/patch.js",
                               "text/javascript");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<html><body><script src='patch.js'></script></body></html>");

  script.Complete("window.history.pushState = function(...args) {};");
  test::RunPendingTasks();

  v8::Isolate* isolate = Window().GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(MainFrame().MainWorldScriptContext());

  MonkeyPatchableApiFunctionInfo info = GetMonkeyPatchableApiFunctionInfo(
      isolate, MonkeyPatchableApi::kHistoryPushState);
  EXPECT_TRUE(info.is_monkey_patched);

  v8::Local<v8::Function> func;
  ASSERT_TRUE(info.function.ToLocal(&func));
  EXPECT_TRUE(IsFunctionAMonkeyPatch(isolate, func,
                                     MonkeyPatchableApi::kHistoryPushState));
}

TEST_F(MonkeyPatchableApiTest, ProxyWithApplyTrapMonkeyPatchedApiIsDetected) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest script("https://example.com/patch.js",
                               "text/javascript");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<html><body><script src='patch.js'></script></body></html>");

  script.Complete(
      "window.proxyApplyTrap = function(target, thisArg, argumentsList) {"
      "  return target.apply(thisArg, argumentsList);"
      "};"
      "window.history.pushState = new Proxy(window.history.pushState, {"
      "  apply: window.proxyApplyTrap"
      "});");
  test::RunPendingTasks();

  v8::Isolate* isolate = Window().GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(MainFrame().MainWorldScriptContext());

  MonkeyPatchableApiFunctionInfo info = GetMonkeyPatchableApiFunctionInfo(
      isolate, MonkeyPatchableApi::kHistoryPushState);
  EXPECT_TRUE(info.is_monkey_patched);

  v8::Local<v8::Value> apply_trap_val = MainFrame().ExecuteScriptAndReturnValue(
      WebScriptSource(WebString::FromUtf8("window.proxyApplyTrap")));
  v8::Local<v8::Function> apply_trap = apply_trap_val.As<v8::Function>();

  v8::Local<v8::Function> api_function;
  ASSERT_TRUE(info.function.ToLocal(&api_function));

  EXPECT_TRUE(IsFunctionAMonkeyPatch(isolate, apply_trap, api_function));
}

TEST_F(MonkeyPatchableApiTest,
       ProxyWithoutApplyTrapMonkeyPatchedApiIsDetected) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest script("https://example.com/patch.js",
                               "text/javascript");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<html><body><script src='patch.js'></script></body></html>");

  script.Complete(
      "window.proxyTarget = function(...args) {};"
      "window.history.pushState = new Proxy(window.proxyTarget, {});");
  test::RunPendingTasks();

  v8::Isolate* isolate = Window().GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(MainFrame().MainWorldScriptContext());

  MonkeyPatchableApiFunctionInfo info = GetMonkeyPatchableApiFunctionInfo(
      isolate, MonkeyPatchableApi::kHistoryPushState);
  EXPECT_TRUE(info.is_monkey_patched);

  v8::Local<v8::Value> target_val = MainFrame().ExecuteScriptAndReturnValue(
      WebScriptSource(WebString::FromUtf8("window.proxyTarget")));
  v8::Local<v8::Function> target_func = target_val.As<v8::Function>();

  v8::Local<v8::Function> api_function;
  ASSERT_TRUE(info.function.ToLocal(&api_function));

  EXPECT_TRUE(IsFunctionAMonkeyPatch(isolate, target_func, api_function));

  v8::Local<v8::Value> unrelated_val = MainFrame().ExecuteScriptAndReturnValue(
      WebScriptSource(WebString::FromUtf8("(function() {})")));
  v8::Local<v8::Function> unrelated_func = unrelated_val.As<v8::Function>();

  EXPECT_FALSE(IsFunctionAMonkeyPatch(isolate, unrelated_func, api_function));
}

TEST_F(MonkeyPatchableApiTest, NonMatchingMonkeyPatchReturnsFalse) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimSubresourceRequest script("https://example.com/patch.js",
                               "text/javascript");
  LoadURL("https://example.com/");
  main_resource.Complete(
      "<html><body><script src='patch.js'></script></body></html>");

  script.Complete("window.history.pushState = function(...args) {};");
  test::RunPendingTasks();

  v8::Isolate* isolate = Window().GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(MainFrame().MainWorldScriptContext());

  MonkeyPatchableApiFunctionInfo info = GetMonkeyPatchableApiFunctionInfo(
      isolate, MonkeyPatchableApi::kHistoryPushState);
  v8::Local<v8::Function> func;
  ASSERT_TRUE(info.function.ToLocal(&func));

  // Checking against kHistoryReplaceState should return false since only
  // pushState was patched.
  EXPECT_FALSE(IsFunctionAMonkeyPatch(
      isolate, func, MonkeyPatchableApi::kHistoryReplaceState));
}

TEST_F(MonkeyPatchableApiTest, BrokenPropertyPathIntermediateNonObject) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<html><body></body></html>");

  MainFrame().ExecuteScript(WebScriptSource(
      WebString::FromUtf8("Object.defineProperty(window, 'history', {"
                          "  value: 'not_an_object', configurable: true"
                          "});")));

  v8::Isolate* isolate = Window().GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(MainFrame().MainWorldScriptContext());

  MonkeyPatchableApiFunctionInfo info = GetMonkeyPatchableApiFunctionInfo(
      isolate, MonkeyPatchableApi::kHistoryPushState);
  EXPECT_FALSE(info.is_monkey_patched);
  v8::Local<v8::Function> func;
  EXPECT_FALSE(info.function.ToLocal(&func));
}

TEST_F(MonkeyPatchableApiTest, BrokenPropertyPathEndNotFunction) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<html><body></body></html>");

  MainFrame().ExecuteScript(WebScriptSource(
      WebString::FromUtf8("window.history.pushState = 12345;")));

  v8::Isolate* isolate = Window().GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(MainFrame().MainWorldScriptContext());

  MonkeyPatchableApiFunctionInfo info = GetMonkeyPatchableApiFunctionInfo(
      isolate, MonkeyPatchableApi::kHistoryPushState);
  EXPECT_FALSE(info.is_monkey_patched);
  v8::Local<v8::Function> func;
  EXPECT_FALSE(info.function.ToLocal(&func));
}

TEST_F(MonkeyPatchableApiTest, NoScriptExecutionDuringCheck) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete("<html><body></body></html>");

  MainFrame().ExecuteScript(WebScriptSource(WebString::FromUtf8(
      "Object.defineProperty(window, 'history', {"
      "  get: function() { throw new Error('getter executed'); }"
      "});")));

  v8::Isolate* isolate = Window().GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(MainFrame().MainWorldScriptContext());

  // Property traversal uses DisallowJavascriptExecutionScope so getter must not
  // fire.
  MonkeyPatchableApiFunctionInfo info = GetMonkeyPatchableApiFunctionInfo(
      isolate, MonkeyPatchableApi::kHistoryPushState);
  EXPECT_FALSE(info.is_monkey_patched);
}

}  // namespace blink
