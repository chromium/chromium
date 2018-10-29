// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code if governed by a BSD-style license that can be
// found in LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

using testing::ElementsAre;
using testing::AnyOf;

namespace blink {

class DisableBackgroundThrottlingIsRespectedTest
    : public SimTest,
      private ScopedTimerThrottlingForBackgroundTabsForTest {
 public:
  DisableBackgroundThrottlingIsRespectedTest()
      : ScopedTimerThrottlingForBackgroundTabsForTest(false) {}
  void SetUp() override {
    SimTest::SetUp();
  }
};

TEST_F(DisableBackgroundThrottlingIsRespectedTest,
       DisableBackgroundThrottlingIsRespected) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(
      "(<script>"
      "  function f(repetitions) {"
      "     if (repetitions == 0) return;"
      "     console.log('called f');"
      "     setTimeout(f, 10, repetitions - 1);"
      "  }"
      "  f(5);"
      "</script>)");

  Platform::Current()
      ->CurrentThread()
      ->Scheduler()
      ->GetWebMainThreadSchedulerForTest()
      ->SetRendererBackgrounded(true);

  // Run delayed tasks for 1 second. All tasks should be completed
  // with throttling disabled.
  test::RunDelayedTasks(TimeDelta::FromSeconds(1));

  EXPECT_THAT(ConsoleMessages(), ElementsAre("called f", "called f", "called f",
                                             "called f", "called f"));
}

class BackgroundRendererThrottlingTest : public SimTest {};

TEST_F(BackgroundRendererThrottlingTest, BackgroundRenderersAreThrottled) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  main_resource.Complete(
      "(<script>"
      "  function f(repetitions) {"
      "     if (repetitions == 0) return;"
      "     console.log('called f');"
      "     setTimeout(f, 10, repetitions - 1);"
      "  }"
      "  setTimeout(f, 10, 50);"
      "</script>)");

  Platform::Current()
      ->CurrentThread()
      ->Scheduler()
      ->GetWebMainThreadSchedulerForTest()
      ->SetRendererBackgrounded(true);

  // Make sure that we run no more than one task a second.
  test::RunDelayedTasks(TimeDelta::FromMilliseconds(3000));
  EXPECT_THAT(
      ConsoleMessages(),
      AnyOf(ElementsAre("called f", "called f", "called f"),
            ElementsAre("called f", "called f", "called f", "called f")));
}

}  // namespace blink
