// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "base/task/single_thread_task_runner.h"
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
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/scheduler/public/page_scheduler.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {
namespace virtual_time_test {

class ScriptExecutionCallbackHelper final {
 public:
  const String Result() const { return result_; }
  void Completed(std::optional<base::Value> value, base::TimeTicks start_time) {
    if (!value)
      return;
    if (std::string* str = value->GetIfString())
      result_ = String(*str);
  }

 private:
  String result_;
};

class VirtualTimeTest : public SimTest {
 protected:
  VirtualTimeController* GetVirtualTimeController() {
    return WebView().Scheduler()->GetVirtualTimeController();
  }
  void SetUp() override {
    ThreadState::Current()->CollectAllGarbageForTesting();
    SimTest::SetUp();
    GetVirtualTimeController()->EnableVirtualTime(base::Time());
  }

  String ExecuteJavaScript(String script_source) {
    ScriptExecutionCallbackHelper callback_helper;
    WebScriptSource source(script_source);
    WebView().MainFrame()->ToWebLocalFrame()->RequestExecuteScript(
        DOMWrapperWorld::kMainWorldId, base::span_from_ref(source),
        mojom::blink::UserActivationOption::kDoNotActivate,
        mojom::blink::EvaluationTiming::kSynchronous,
        mojom::blink::LoadEventBlockingOption::kDoNotBlock,
        WTF::BindOnce(&ScriptExecutionCallbackHelper::Completed,
                      base::Unretained(&callback_helper)),
        BackForwardCacheAware::kAllow,
        mojom::blink::WantResultOption::kWantResult,
        mojom::blink::PromiseResultOption::kDoNotWait);

    return callback_helper.Result();
  }

  void TearDown() override {
    // SimTest::TearDown() calls RunPendingTasks. This is a problem because
    // if there are any repeating tasks, advancing virtual time will cause the
    // runloop to busy loop. Disabling virtual time here fixes that.
    GetVirtualTimeController()->DisableVirtualTimeForTesting();
    SimTest::TearDown();
  }

  void StopVirtualTimeAndExitRunLoop(base::OnceClosure quit_closure) {
    GetVirtualTimeController()->SetVirtualTimePolicy(
        VirtualTimeController::VirtualTimePolicy::kPause);
    std::move(quit_closure).Run();
  }

  // Some task queues may have repeating v8 tasks that run forever so we impose
  // a hard (virtual) time limit.
  void RunTasksForPeriod(double delay_ms) {
    base::RunLoop loop;
    scheduler::GetSingleThreadTaskRunnerForTesting()->PostDelayedTask(
        FROM_HERE,
        WTF::BindOnce(&VirtualTimeTest::StopVirtualTimeAndExitRunLoop,
                      WTF::Unretained(this), loop.QuitClosure()),
        base::Milliseconds(delay_ms));

    loop.Run();
  }

  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
};

// http://crbug.com/633321
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
#define MAYBE_SetInterval DISABLED_SetInterval
#else
#define MAYBE_SetInterval SetInterval
#endif
TEST_F(VirtualTimeTest, MAYBE_SetInterval) {
  GetVirtualTimeController()->EnableVirtualTime(base::Time());
  GetVirtualTimeController()->SetVirtualTimePolicy(
      VirtualTimeController::VirtualTimePolicy::kAdvance);

  ExecuteJavaScript(
      "var run_order = [];"
      "var count = 10;"
      "var interval_handle = setInterval(function() {"
      "  if (--window.count == 0) {"
      "     clearInterval(interval_handle);"
      "  }"
      "  run_order.push(count);"
      "}, 900);"
      "setTimeout(function() { run_order.push('timer'); }, 1500);");

  RunTasksForPeriod(9001);

  EXPECT_EQ("9, timer, 8, 7, 6, 5, 4, 3, 2, 1, 0",
            ExecuteJavaScript("run_order.join(', ')"));
}

// http://crbug.com/633321
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
#define MAYBE_AllowVirtualTimeToAdvance DISABLED_AllowVirtualTimeToAdvance
#else
#define MAYBE_AllowVirtualTimeToAdvance AllowVirtualTimeToAdvance
#endif
TEST_F(VirtualTimeTest, MAYBE_AllowVirtualTimeToAdvance) {
  GetVirtualTimeController()->SetVirtualTimePolicy(
      VirtualTimeController::VirtualTimePolicy::kPause);

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

  GetVirtualTimeController()->SetVirtualTimePolicy(
      VirtualTimeController::VirtualTimePolicy::kAdvance);
  RunTasksForPeriod(1000);

  EXPECT_EQ("c, b, a", ExecuteJavaScript("run_order.join(', ')"));
}

// http://crbug.com/633321
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
#define MAYBE_VirtualTimeNotAllowedToAdvanceWhileResourcesLoading \
  DISABLED_VirtualTimeNotAllowedToAdvanceWhileResourcesLoading
#else
#define MAYBE_VirtualTimeNotAllowedToAdvanceWhileResourcesLoading \
  VirtualTimeNotAllowedToAdvanceWhileResourcesLoading
#endif
TEST_F(VirtualTimeTest,
       MAYBE_VirtualTimeNotAllowedToAdvanceWhileResourcesLoading) {
  GetVirtualTimeController()->EnableVirtualTime(base::Time());
  GetVirtualTimeController()->SetVirtualTimePolicy(
      VirtualTimeController::VirtualTimePolicy::kDeterministicLoading);

  EXPECT_TRUE(GetVirtualTimeController()->VirtualTimeAllowedToAdvance());

  SimRequest main_resource("https://example.com/test.html", "text/html");
  SimSubresourceRequest css_resource("https://example.com/test.css",
                                     "text/css");

  // Loading, virtual time should not advance.
  LoadURL("https://example.com/test.html");
  EXPECT_FALSE(GetVirtualTimeController()->VirtualTimeAllowedToAdvance());

  // Still Loading, virtual time should not advance.
  main_resource.Write("<!DOCTYPE html><link rel=stylesheet href=test.css>");
  EXPECT_FALSE(GetVirtualTimeController()->VirtualTimeAllowedToAdvance());

  // Still Loading, virtual time should not advance.
  css_resource.Start();
  css_resource.Write("a { color: red; }");
  EXPECT_FALSE(GetVirtualTimeController()->VirtualTimeAllowedToAdvance());

  // Still Loading, virtual time should not advance.
  css_resource.Finish();
  EXPECT_FALSE(GetVirtualTimeController()->VirtualTimeAllowedToAdvance());

  // Still Loading, virtual time should not advance.
  main_resource.Write("<body>");
  EXPECT_FALSE(GetVirtualTimeController()->VirtualTimeAllowedToAdvance());

  // Finished loading, virtual time should be able to advance.
  main_resource.Finish();
  EXPECT_TRUE(GetVirtualTimeController()->VirtualTimeAllowedToAdvance());

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
