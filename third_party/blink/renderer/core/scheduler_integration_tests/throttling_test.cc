// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in LICENSE file.

#include "base/numerics/safe_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/testing/sim/sim_request.h"
#include "third_party/blink/renderer/core/testing/sim/sim_test.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support_with_mock_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

using testing::AnyOf;
using testing::ElementsAre;

namespace blink {

// When a page is backgrounded this is the absolute smallest amount of time
// that can elapse between timer wake-ups.
constexpr auto kDefaultThrottledWakeUpInterval = base::Seconds(1);

// This test suite relies on messages being posted to the console. In order to
// be resilient against messages not posted by this specific test suite, a small
// prefix is used to allowed filtering.
constexpr char kTestConsoleMessagePrefix[] = "[ThrottlingTest]";

// A SimTest with mock time.
class ThrottlingTestBase : public SimTest {
 public:
  ThrottlingTestBase() {
    platform_->SetAutoAdvanceNowToPendingTasks(false);

    // Align the time on a 1-minute interval, to simplify expectations.
    platform_->AdvanceClock(platform_->NowTicks().SnappedToNextTick(
                                base::TimeTicks(), base::Minutes(1)) -
                            platform_->NowTicks());
  }

  String BuildTimerConsoleMessage(String suffix = String()) {
    String message(kTestConsoleMessagePrefix);

    message = message + " Timer called";

    if (!suffix.IsNull())
      message + " " + suffix;

    return message;
  }

  // Returns a filtered copy of console messages where items not prefixed with
  // |kTestConsoleMessagePrefix| are removed.
  Vector<String> FilteredConsoleMessages() {
    Vector<String> result = ConsoleMessages();

    result.erase(
        std::remove_if(result.begin(), result.end(),
                       [](const String& element) {
                         return !element.StartsWith(kTestConsoleMessagePrefix);
                       }),
        result.end());

    return result;
  }

  ScopedTestingPlatformSupport<TestingPlatformSupportWithMockScheduler>
      platform_;
};

class DisableBackgroundThrottlingIsRespectedTest
    : public ThrottlingTestBase,
      private ScopedTimerThrottlingForBackgroundTabsForTest {
 public:
  DisableBackgroundThrottlingIsRespectedTest()
      : ScopedTimerThrottlingForBackgroundTabsForTest(false) {}
};

TEST_F(DisableBackgroundThrottlingIsRespectedTest,
       DisableBackgroundThrottlingIsRespected) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  const String console_message = BuildTimerConsoleMessage();
  main_resource.Complete(
      String::Format("(<script>"
                     "  function f(repetitions) {"
                     "     if (repetitions == 0) return;"
                     "     console.log('%s');"
                     "     setTimeout(f, 10, repetitions - 1);"
                     "  }"
                     "  f(5);"
                     "</script>)",
                     console_message.Utf8().c_str()));

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  // Run delayed tasks for 1 second. All tasks should be completed
  // with throttling disabled.
  platform_->RunForPeriod(base::Seconds(1));

  EXPECT_THAT(FilteredConsoleMessages(),
              ElementsAre(console_message, console_message, console_message,
                          console_message, console_message));
}

class BackgroundPageThrottlingTest : public ThrottlingTestBase {};

TEST_F(BackgroundPageThrottlingTest, TimersThrottledInBackgroundPage) {
  SimRequest main_resource("https://example.com/", "text/html");

  LoadURL("https://example.com/");

  const String console_message = BuildTimerConsoleMessage();
  main_resource.Complete(
      String::Format("(<script>"
                     "  function f(repetitions) {"
                     "     if (repetitions == 0) return;"
                     "     console.log('%s');"
                     "     setTimeout(f, 10, repetitions - 1);"
                     "  }"
                     "  setTimeout(f, 10, 50);"
                     "</script>)",
                     console_message.Utf8().c_str()));

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  // Make sure that we run no more than one task a second.
  platform_->RunForPeriod(base::Seconds(3));
  EXPECT_THAT(FilteredConsoleMessages(),
              ElementsAre(console_message, console_message, console_message));
}

// Verify the execution time of non-nested timers on a hidden page.
// - setTimeout(..., 0) and setTimeout(..., -1) schedule their callback after
//   1ms. The 1 ms delay exists for historical reasons crbug.com/402694.
// - setTimeout(..., 5) schedules its callback at the next aligned time
TEST_F(BackgroundPageThrottlingTest, WithoutNesting) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  String timeout_0_message = BuildTimerConsoleMessage("0");
  String timeout_minus_1_message = BuildTimerConsoleMessage("-1");
  String timeout_5_message = BuildTimerConsoleMessage("5");
  main_resource.Complete(String::Format(
      "<script>"
      "  setTimeout(function() {"
      "    setTimeout(function() { console.log('%s'); }, 0);"
      "    setTimeout(function() { console.log('%s'); }, -1);"
      "    setTimeout(function() { console.log('%s'); }, 5);"
      "  }, 1000);"
      "</script>",
      timeout_0_message.Utf8().c_str(), timeout_minus_1_message.Utf8().c_str(),
      timeout_5_message.Utf8().c_str()));
  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  platform_->RunForPeriod(base::Milliseconds(1001));
  EXPECT_THAT(FilteredConsoleMessages(),
              ElementsAre(timeout_0_message, timeout_minus_1_message));

  platform_->RunForPeriod(base::Milliseconds(998));
  EXPECT_THAT(FilteredConsoleMessages(),
              ElementsAre(timeout_0_message, timeout_minus_1_message));

  platform_->RunForPeriod(base::Milliseconds(1));
  EXPECT_THAT(FilteredConsoleMessages(),
              ElementsAre(timeout_0_message, timeout_minus_1_message,
                          timeout_5_message));
}

// Verify that on a hidden page, a timer created with setTimeout(..., 0) is
// throttled after 5 nesting levels.
TEST_F(BackgroundPageThrottlingTest, NestedSetTimeoutZero) {
  // Disable this test when setTimeoutWithoutClamp feature is enabled.
  // TODO(crbug.com/1303275): Investigate the failure reason.
  if (blink::features::IsSetTimeoutWithoutClampEnabled())
    GTEST_SKIP() << "Skipping test for setTimeoutWithoutClamp feature enabled";

  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  const String console_message = BuildTimerConsoleMessage();
  main_resource.Complete(
      String::Format("<script>"
                     "  function f(repetitions) {"
                     "    if (repetitions == 0) return;"
                     "    console.log('%s');"
                     "    setTimeout(f, 0, repetitions - 1);"
                     "  }"
                     "  setTimeout(f, 0, 50);"
                     "</script>",
                     console_message.Utf8().c_str()));
  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  platform_->RunForPeriod(base::Milliseconds(1));
  EXPECT_THAT(FilteredConsoleMessages(), Vector<String>(1, console_message));
  platform_->RunForPeriod(base::Milliseconds(1));
  EXPECT_THAT(FilteredConsoleMessages(), Vector<String>(2, console_message));
  platform_->RunForPeriod(base::Milliseconds(1));
  EXPECT_THAT(FilteredConsoleMessages(), Vector<String>(3, console_message));
  platform_->RunForPeriod(base::Milliseconds(1));
  EXPECT_THAT(FilteredConsoleMessages(), Vector<String>(4, console_message));
  platform_->RunForPeriod(base::Milliseconds(995));
  EXPECT_THAT(FilteredConsoleMessages(), Vector<String>(4, console_message));
  platform_->RunForPeriod(base::Milliseconds(1));
  EXPECT_THAT(FilteredConsoleMessages(), Vector<String>(5, console_message));
}

// Verify that in a hidden page, a timer created with setInterval(..., 0) is
// throttled after 5 nesting levels.
TEST_F(BackgroundPageThrottlingTest, NestedSetIntervalZero) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  const String console_message = BuildTimerConsoleMessage();
  main_resource.Complete(
      String::Format("<script>"
                     "  function f() {"
                     "    if (repetitions == 0) clearInterval(interval_id);"
                     "    console.log('%s');"
                     "    repetitions = repetitions - 1;"
                     "  }"
                     "  var repetitions = 50;"
                     "  var interval_id = setInterval(f, 0);"
                     "</script>",
                     console_message.Utf8().c_str()));
  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  platform_->RunForPeriod(base::Milliseconds(1));
  EXPECT_THAT(FilteredConsoleMessages(), Vector<String>(1, console_message));
  platform_->RunForPeriod(base::Milliseconds(1));
  EXPECT_THAT(FilteredConsoleMessages(), Vector<String>(2, console_message));
  platform_->RunForPeriod(base::Milliseconds(1));
  EXPECT_THAT(FilteredConsoleMessages(), Vector<String>(3, console_message));
  platform_->RunForPeriod(base::Milliseconds(1));
  EXPECT_THAT(FilteredConsoleMessages(), Vector<String>(4, console_message));
  platform_->RunForPeriod(base::Milliseconds(995));
  EXPECT_THAT(FilteredConsoleMessages(), Vector<String>(4, console_message));
  platform_->RunForPeriod(base::Milliseconds(1));
  EXPECT_THAT(FilteredConsoleMessages(), Vector<String>(5, console_message));
}

class AbortSignalTimeoutThrottlingTest : public BackgroundPageThrottlingTest {
 public:
  AbortSignalTimeoutThrottlingTest()
      : console_message_(BuildTimerConsoleMessage()) {}

  String GetTestSource(wtf_size_t iterations, wtf_size_t timeout) {
    return String::Format(
        "(<script>"
        "  let count = 0;"
        "  function scheduleTimeout() {"
        "    const signal = AbortSignal.timeout('%d');"
        "    signal.onabort = () => {"
        "      console.log('%s');"
        "      if (++count < '%d') {"
        "        scheduleTimeout();"
        "      }"
        "    }"
        "  }"
        "  scheduleTimeout();"
        "</script>)",
        timeout, console_message_.Utf8().c_str(), iterations);
  }

  const String& console_message() { return console_message_; }

 protected:
  const String console_message_;
};

TEST_F(AbortSignalTimeoutThrottlingTest, TimeoutsThrottledInBackgroundPage) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(GetTestSource(/*iterations=*/20, /*timeout=*/10));

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  // Make sure that we run no more than one task a second.
  platform_->RunForPeriod(base::Seconds(3));
  EXPECT_THAT(FilteredConsoleMessages(), Vector<String>(3, console_message()));
}

TEST_F(AbortSignalTimeoutThrottlingTest, ZeroMsTimersNotThrottled) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  constexpr wtf_size_t kIterations = 20;
  main_resource.Complete(GetTestSource(kIterations, /*timeout=*/0));

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  // All tasks should run after 1 ms since time does not advance during the
  // test, the timeout was 0 ms, and the timeouts are not throttled.
  platform_->RunForPeriod(base::Milliseconds(1));
  EXPECT_THAT(FilteredConsoleMessages(),
              Vector<String>(kIterations, console_message()));
}

namespace {

class IntensiveWakeUpThrottlingTest : public ThrottlingTestBase {
 public:
  IntensiveWakeUpThrottlingTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kIntensiveWakeUpThrottling, {}},
         {features::kSetTimeoutWithoutClamp, {}}},
        // Disable freezing because it hides the effect of intensive throttling.
        {features::kStopInBackground});
  }

  // Expect a console message every second, for |num_1hz_messages| seconds.
  // Then, expect a console messages every minute.
  void ExpectRepeatingTimerConsoleMessages(int num_1hz_messages) {
    for (int i = 0; i < num_1hz_messages; ++i) {
      ConsoleMessages().clear();
      platform_->RunForPeriod(base::Seconds(1));
      EXPECT_EQ(FilteredConsoleMessages().size(), 1U);
    }

    constexpr int kNumIterations = 3;
    for (int i = 0; i < kNumIterations; ++i) {
      ConsoleMessages().clear();
      platform_->RunForPeriod(base::Seconds(30));
      // Task shouldn't execute earlier than expected.
      EXPECT_EQ(FilteredConsoleMessages().size(), 0U);
      platform_->RunForPeriod(base::Seconds(30));
      EXPECT_EQ(FilteredConsoleMessages().size(), 1U);
    }
  }

  void TestNoIntensiveThrottlingOnTitleOrFaviconUpdate(
      const String& console_message) {
    // The page does not attempt to run onTimer in the first 5 minutes.
    platform_->RunForPeriod(base::Minutes(5));
    EXPECT_THAT(FilteredConsoleMessages(), ElementsAre());

    // onTimer() communicates in background and re-posts itself. The background
    // communication inhibits intensive wake up throttling for 3 seconds, which
    // allows the re-posted task to run after |kDefaultThrottledWakeUpInterval|.
    constexpr int kNumIterations = 3;
    for (int i = 0; i < kNumIterations; ++i) {
      platform_->RunForPeriod(kDefaultThrottledWakeUpInterval);
      EXPECT_THAT(FilteredConsoleMessages(), ElementsAre(console_message));
      ConsoleMessages().clear();
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Use to install a function that does not actually communicate with the user.
constexpr char kCommunicationNop[] =
    "<script>"
    "  function maybeCommunicateInBackground() {"
    "    return;"
    "  }"
    "</script>";

// Use to install a function that will communicate with the user via title
// update.
constexpr char kCommunicateThroughTitleScript[] =
    "<script>"
    "  function maybeCommunicateInBackground() {"
    "    document.title += \"A\";"
    "  }"
    "</script>";

// Use to install a function that will communicate with the user via favicon
// update.
constexpr char kCommunicateThroughFavisonScript[] =
    "<script>"
    "  function maybeCommunicateInBackground() {"
    "  document.querySelector(\"link[rel*='icon']\").href = \"favicon.ico\";"
    "  }"
    "</script>";

// A script that schedules a timer task which logs to the console. The timer
// task has a high nesting level and its timeout is not aligned on the intensive
// wake up throttling interval.
constexpr char kLongUnalignedTimerScriptTemplate[] =
    "<script>"
    "  function onTimerWithHighNestingLevel() {"
    "     console.log('%s');"
    "  }"
    "  function onTimerWithLowNestingLevel(nesting_level) {"
    "    if (nesting_level == 4) {"
    "      setTimeout(onTimerWithHighNestingLevel, 338 * 1000);"
    "    } else {"
    "      setTimeout(onTimerWithLowNestingLevel, 1000, nesting_level + 1);"
    "    }"
    "  }"
    "  setTimeout(onTimerWithLowNestingLevel, 1000, 1);"
    "</script>";

// A time delta that matches the delay in the above script.
constexpr base::TimeDelta kLongUnalignedTimerDelay = base::Seconds(342);

// Builds a page that waits 5 minutes and then creates a timer that reschedules
// itself 50 times with 10 ms delay. The timer task logs |console_message| to
// the console and invokes maybeCommunicateInBackground(). The caller must
// provide the definition of maybeCommunicateInBackground() via
// |communicate_script|.
String BuildRepeatingTimerPage(const char* console_message,
                               const char* communicate_script) {
  constexpr char kRepeatingTimerPageTemplate[] =
      "<html>"
      "<head>"
      "  <link rel='icon' href='http://www.foobar.com/favicon.ico'>"
      "</head>"
      "<body>"
      "<script>"
      "  function onTimer(repetitions) {"
      "     if (repetitions == 0) return;"
      "     console.log('%s');"
      "     maybeCommunicateInBackground();"
      "     setTimeout(onTimer, 10, repetitions - 1);"
      "  }"
      "  function afterFiveMinutes() {"
      "    setTimeout(onTimer, 10, 50);"
      "  }"
      "  setTimeout(afterFiveMinutes, 5 * 60 * 1000);"
      "</script>"
      "%s"  // |communicate_script| inserted here
      "</body>"
      "</html>";

  return String::Format(kRepeatingTimerPageTemplate, console_message,
                        communicate_script);
}

}  // namespace

// Verify that a main frame timer that reposts itself with a 10 ms timeout runs
// once every minute.
TEST_F(IntensiveWakeUpThrottlingTest, MainFrameTimer_ShortTimeout) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  // Page does not communicate with the user. Normal intensive throttling
  // applies.
  main_resource.Complete(BuildRepeatingTimerPage(
      BuildTimerConsoleMessage().Utf8().c_str(), kCommunicationNop));

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  // No timer is scheduled in the 5 first minutes.
  platform_->RunForPeriod(base::Minutes(5));
  EXPECT_THAT(FilteredConsoleMessages(), ElementsAre());

  // Expected execution:
  //
  // t = 5min 0s : afterFiveMinutes    nesting=1 (low)
  // t = 5min 1s : onTimer             nesting=2 (low)     <
  // t = 5min 2s : onTimer             nesting=3 (low)     < 4 seconds at 1 Hz
  // t = 5min 3s : onTimer             nesting=4 (low)     <
  // t = 5min 4s : onTimer             nesting=5 (high) ** <
  // t = 6min    : onTimer             nesting=6 (high)
  // t = 7min    : onTimer             nesting=7 (high)
  // ...
  //
  // ** In a main frame, a task with high nesting level is 1-second aligned
  //    when no task with high nesting level ran in the last minute.
  ExpectRepeatingTimerConsoleMessages(4);
}

// Verify that a main frame timer that reposts itself with a 10 ms timeout runs
// once every |kDefaultThrottledWakeUpInterval| after the first confirmed page
// communication through title update.
TEST_F(IntensiveWakeUpThrottlingTest, MainFrameTimer_ShortTimeout_TitleUpdate) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  const String console_message = BuildTimerConsoleMessage();
  main_resource.Complete(BuildRepeatingTimerPage(
      console_message.Utf8().c_str(), kCommunicateThroughTitleScript));

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  TestNoIntensiveThrottlingOnTitleOrFaviconUpdate(console_message);
}

// Verify that a main frame timer that reposts itself with a 10 ms timeout runs
// once every |kDefaultThrottledWakeUpInterval| after the first confirmed page
// communication through favicon update.
TEST_F(IntensiveWakeUpThrottlingTest,
       MainFrameTimer_ShortTimeout_FaviconUpdate) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  const String console_message = BuildTimerConsoleMessage();
  main_resource.Complete(BuildRepeatingTimerPage(
      console_message.Utf8().c_str(), kCommunicateThroughFavisonScript));

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  TestNoIntensiveThrottlingOnTitleOrFaviconUpdate(console_message);
}

// Verify that a same-origin subframe timer that reposts itself with a 10 ms
// timeout runs once every minute.
TEST_F(IntensiveWakeUpThrottlingTest, SameOriginSubFrameTimer_ShortTimeout) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest subframe_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"(<iframe src="https://example.com/iframe.html" />)");
  // Run tasks to let the main frame request the iframe resource. It is not
  // possible to complete the iframe resource request before that.
  platform_->RunUntilIdle();

  subframe_resource.Complete(BuildRepeatingTimerPage(
      BuildTimerConsoleMessage().Utf8().c_str(), kCommunicationNop));

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  // No timer is scheduled in the 5 first minutes.
  platform_->RunForPeriod(base::Minutes(5));
  EXPECT_THAT(FilteredConsoleMessages(), ElementsAre());

  // Expected execution:
  //
  // t = 5min 0s : afterFiveMinutes    nesting=1 (low)
  // t = 5min 1s : onTimer             nesting=2 (low)     <
  // t = 5min 2s : onTimer             nesting=3 (low)     < 4 seconds at 1 Hz
  // t = 5min 3s : onTimer             nesting=4 (low)     <
  // t = 5min 4s : onTimer             nesting=5 (high) ** <
  // t = 6min    : onTimer             nesting=6 (high)
  // t = 7min    : onTimer             nesting=7 (high)
  // ...
  //
  // ** In a same-origin frame, a task with high nesting level is 1-second
  //    aligned when no task with high nesting level ran in the last minute.
  ExpectRepeatingTimerConsoleMessages(4);
}

// Verify that a cross-origin subframe timer that reposts itself with a 10 ms
// timeout runs once every minute.
TEST_F(IntensiveWakeUpThrottlingTest, CrossOriginSubFrameTimer_ShortTimeout) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest subframe_resource("https://cross-origin.example.com/iframe.html",
                               "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      R"(<iframe src="https://cross-origin.example.com/iframe.html" />)");
  // Run tasks to let the main frame request the iframe resource. It is not
  // possible to complete the iframe resource request before that.
  platform_->RunUntilIdle();

  subframe_resource.Complete(BuildRepeatingTimerPage(
      BuildTimerConsoleMessage().Utf8().c_str(), kCommunicationNop));

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  // No timer is scheduled in the 5 first minutes.
  platform_->RunForPeriod(base::Minutes(5));
  EXPECT_THAT(FilteredConsoleMessages(), ElementsAre());

  // Expected execution:
  //
  // t = 5min 0s : afterFiveMinutes    nesting=1 (low)
  // t = 5min 1s : onTimer             nesting=2 (low)  <
  // t = 5min 2s : onTimer             nesting=3 (low)  < 3 seconds at 1 Hz
  // t = 5min 3s : onTimer             nesting=4 (low)  <
  // t = 6min    : onTimer             nesting=5 (high)
  // t = 7min    : onTimer             nesting=6 (high)
  // t = 8min    : onTimer             nesting=7 (high)
  // ...
  ExpectRepeatingTimerConsoleMessages(3);
}

// Verify that a main frame timer with a long timeout runs at the desired run
// time when there is no other recent timer wake up.
TEST_F(IntensiveWakeUpThrottlingTest, MainFrameTimer_LongUnalignedTimeout) {
  SimRequest main_resource("https://example.com/", "text/html");
  LoadURL("https://example.com/");

  const String console_message = BuildTimerConsoleMessage();
  main_resource.Complete(String::Format(kLongUnalignedTimerScriptTemplate,
                                        console_message.Utf8().c_str()));

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  platform_->RunForPeriod(kLongUnalignedTimerDelay - base::Seconds(1));
  EXPECT_THAT(FilteredConsoleMessages(), ElementsAre());

  platform_->RunForPeriod(base::Seconds(1));
  EXPECT_THAT(FilteredConsoleMessages(), ElementsAre(console_message));
}

// Verify that a same-origin subframe timer with a long timeout runs at the
// desired run time when there is no other recent timer wake up.
TEST_F(IntensiveWakeUpThrottlingTest,
       SameOriginSubFrameTimer_LongUnalignedTimeout) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest subframe_resource("https://example.com/iframe.html", "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(R"(<iframe src="https://example.com/iframe.html" />)");
  // Run tasks to let the main frame request the iframe resource. It is not
  // possible to complete the iframe resource request before that.
  platform_->RunUntilIdle();

  const String console_message = BuildTimerConsoleMessage();
  subframe_resource.Complete(String::Format(kLongUnalignedTimerScriptTemplate,
                                            console_message.Utf8().c_str()));

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  platform_->RunForPeriod(kLongUnalignedTimerDelay - base::Seconds(1));
  EXPECT_THAT(FilteredConsoleMessages(), ElementsAre());

  platform_->RunForPeriod(base::Seconds(1));
  EXPECT_THAT(FilteredConsoleMessages(), ElementsAre(console_message));
}

// Verify that a cross-origin subframe timer with a long timeout runs at an
// aligned time, even when there is no other recent timer wake up (in a
// same-origin frame, it would have run at the desired time).
TEST_F(IntensiveWakeUpThrottlingTest,
       CrossOriginSubFrameTimer_LongUnalignedTimeout) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest subframe_resource("https://cross-origin.example.com/iframe.html",
                               "text/html");
  LoadURL("https://example.com/");
  main_resource.Complete(
      R"(<iframe src="https://cross-origin.example.com/iframe.html" />)");
  // Run tasks to let the main frame request the iframe resource. It is not
  // possible to complete the iframe resource request before that.
  platform_->RunUntilIdle();

  const String console_message = BuildTimerConsoleMessage();
  subframe_resource.Complete(String::Format(kLongUnalignedTimerScriptTemplate,
                                            console_message.Utf8().c_str()));

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  platform_->RunForPeriod(base::Seconds(342));
  EXPECT_THAT(FilteredConsoleMessages(), ElementsAre());

  // Fast-forward to the next aligned time.
  platform_->RunForPeriod(base::Seconds(18));
  EXPECT_THAT(FilteredConsoleMessages(), ElementsAre(console_message));
}

// Verify that if both the main frame and a cross-origin frame schedule a timer
// with a long unaligned delay, the main frame timer runs at the desired time
// (because there was no recent same-origin wake up) while the cross-origin
// timer runs at an aligned time.
TEST_F(IntensiveWakeUpThrottlingTest,
       MainFrameAndCrossOriginSubFrameTimer_LongUnalignedTimeout) {
  SimRequest main_resource("https://example.com/", "text/html");
  SimRequest subframe_resource("https://cross-origin.example.com/iframe.html",
                               "text/html");
  LoadURL("https://example.com/");

  const String console_message = BuildTimerConsoleMessage();
  const String script = String::Format(kLongUnalignedTimerScriptTemplate,
                                       console_message.Utf8().c_str());

  main_resource.Complete(
      script +
      "<iframe src=\"https://cross-origin.example.com/iframe.html\" />");
  // Run tasks to let the main frame request the iframe resource. It is not
  // possible to complete the iframe resource request before that.
  platform_->RunUntilIdle();
  subframe_resource.Complete(script);

  GetDocument().GetPage()->GetPageScheduler()->SetPageVisible(false);

  platform_->RunForPeriod(base::Seconds(342));
  EXPECT_THAT(FilteredConsoleMessages(), ElementsAre(console_message));

  // Fast-forward to the next aligned time.
  platform_->RunForPeriod(base::Seconds(18));
  EXPECT_THAT(FilteredConsoleMessages(),
              ElementsAre(console_message, console_message));
}

}  // namespace blink
