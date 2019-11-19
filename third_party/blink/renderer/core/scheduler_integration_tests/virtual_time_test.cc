// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_execution_callback.h"
#include "third_party/blink/public/web/web_script_source.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {
namespace virtual_time_test {

class ScriptExecutionCallbackHelper : public WebScriptExecutionCallback {
 public:
  const String Result() const { return result_; }

 private:
  void Completed(const WebVector<v8::Local<v8::Value>>& values) override {
    if (!values.empty() && !values[0].IsEmpty() && values[0]->IsString()) {
      result_ = ToCoreString(v8::Local<v8::String>::Cast(values[0]));
    }
  }

  String result_;
};

class VirtualTimeTest : public SimTest {
 protected:
  void SetUp() override {
    SimTest::SetUp();
    WebView().Scheduler()->EnableVirtualTime();
  }

  String ExecuteJavaScript(String script_source) {
    ScriptExecutionCallbackHelper callback_helper;
    WebView()
        .MainFrame()
        ->ToWebLocalFrame()
        ->RequestExecuteScriptAndReturnValue(
            WebScriptSource(WebString(script_source)), false, &callback_helper);
    return callback_helper.Result();
  }

  void TearDown() override {
    // SimTest::TearDown() calls RunPendingTasks. This is a problem because
    // if there are any repeating tasks, advancing virtual time will cause the
    // runloop to busy loop. Disabling virtual time here fixes that.
    WebView().Scheduler()->DisableVirtualTimeForTesting();
    SimTest::TearDown();
  }

  void StopVirtualTimeAndExitRunLoop() {
    WebView().Scheduler()->SetVirtualTimePolicy(
        PageScheduler::VirtualTimePolicy::kPause);
    test::ExitRunLoop();
  }

  // Some task queues may have repeating v8 tasks that run forever so we impose
  // a hard (virtual) time limit.
  void RunTasksForPeriod(double delay_ms) {
    scheduler::GetSingleThreadTaskRunnerForTesting()->PostDelayedTask(
        FROM_HERE,
        WTF::Bind(&VirtualTimeTest::StopVirtualTimeAndExitRunLoop,
                  WTF::Unretained(this)),
        base::TimeDelta::FromMillisecondsD(delay_ms));
    test::EnterRunLoop();
  }
};

// http://crbug.com/633321
#if defined(OS_ANDROID)
#define MAYBE_SetInterval DISABLED_SetInterval
#else
#define MAYBE_SetInterval SetInterval
#endif
TEST_F(VirtualTimeTest, MAYBE_SetInterval) {
  WebView().Scheduler()->EnableVirtualTime();
  WebView().Scheduler()->SetVirtualTimePolicy(
      PageScheduler::VirtualTimePolicy::kAdvance);

  ExecuteJavaScript(
      "var run_order = [];"
      "var count = 10;"
      "var interval_handle = setInterval(function() {"
      "  if (--window.count == 0) {"
      "     clearInterval(interval_handle);"
      "  }"
      "  run_order.push(count);"
      "}, 1000);"
      "setTimeout(function() { run_order.push('timer'); }, 1500);");

  RunTasksForPeriod(10001);

  EXPECT_EQ("9, timer, 8, 7, 6, 5, 4, 3, 2, 1, 0",
            ExecuteJavaScript("run_order.join(', ')"));
}

// http://crbug.com/633321
#if defined(OS_ANDROID)
#define MAYBE_AllowVirtualTimeToAdvance DISABLED_AllowVirtualTimeToAdvance
#else
#define MAYBE_AllowVirtualTimeToAdvance AllowVirtualTimeToAdvance
#endif
TEST_F(VirtualTimeTest, MAYBE_AllowVirtualTimeToAdvance) {
  WebView().Scheduler()->SetVirtualTimePolicy(
      PageScheduler::VirtualTimePolicy::kPause);

  ExecuteJavaScript(
      "var run_order = [];"
      "timerFn = function(delay, value) {"
      "  setTimeout(function() { run_order.push(value); }, delay);"
      "};"
      "timerFn(100, 'a');"
      "timerFn(10, 'b');"
      "timerFn(1, 'c');");

  test::RunPendingTasks();
  EXPECT_EQ("", ExecuteJavaScript("run_order.join(', ')"));

  WebView().Scheduler()->SetVirtualTimePolicy(
      PageScheduler::VirtualTimePolicy::kAdvance);
  RunTasksForPeriod(1000);

  EXPECT_EQ("c, b, a", ExecuteJavaScript("run_order.join(', ')"));
}

// http://crbug.com/633321
#if defined(OS_ANDROID)
#define MAYBE_VirtualTimeNotAllowedToAdvanceWhileResourcesLoading \
  DISABLED_VirtualTimeNotAllowedToAdvanceWhileResourcesLoading
#else
#define MAYBE_VirtualTimeNotAllowedToAdvanceWhileResourcesLoading \
  VirtualTimeNotAllowedToAdvanceWhileResourcesLoading
#endif
TEST_F(VirtualTimeTest,
       MAYBE_VirtualTimeNotAllowedToAdvanceWhileResourcesLoading) {
  WebView().Scheduler()->EnableVirtualTime();
  WebView().Scheduler()->SetVirtualTimePolicy(
      PageScheduler::VirtualTimePolicy::kDeterministicLoading);

  EXPECT_TRUE(WebView().Scheduler()->VirtualTimeAllowedToAdvance());

  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest css_resource("https://example.com/test.css",
                                     "text/css");

  // Loading, virtual time should not advance.
  LoadURL("https://example.com/test.html");
  EXPECT_FALSE(WebView().Scheduler()->VirtualTimeAllowedToAdvance());

  // Still Loading, virtual time should not advance.
  main_resource.Write("<!DOCTYPE html><link rel=stylesheet href=test.css>");
  EXPECT_FALSE(WebView().Scheduler()->VirtualTimeAllowedToAdvance());

  // Still Loading, virtual time should not advance.
  css_resource.Start();
  css_resource.Write("a { color: red; }");
  EXPECT_FALSE(WebView().Scheduler()->VirtualTimeAllowedToAdvance());

  // Still Loading, virtual time should not advance.
  css_resource.Finish();
  EXPECT_FALSE(WebView().Scheduler()->VirtualTimeAllowedToAdvance());

  // Still Loading, virtual time should not advance.
  main_resource.Write("<body>");
  EXPECT_FALSE(WebView().Scheduler()->VirtualTimeAllowedToAdvance());

  // Finished loading, virtual time should be able to advance.
  main_resource.Finish();
  EXPECT_TRUE(WebView().Scheduler()->VirtualTimeAllowedToAdvance());

  // The loading events are delayed for 10 virtual ms after they have run, we
  // let tasks run for a little while to ensure we don't get any asserts on
  // teardown as a result.
  RunTasksForPeriod(10);
}

#undef MAYBE_SetInterval
#undef MAYBE_AllowVirtualTimeToAdvance
#undef MAYBE_VirtualTimeNotAllowedToAdvanceWhileResourcesLoading
}  // namespace virtual_time_test
}  // namespace blink
