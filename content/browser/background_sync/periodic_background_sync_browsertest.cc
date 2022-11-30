// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "content/browser/background_sync/background_sync_base_browsertest.h"
#include "content/browser/background_sync/background_sync_manager.h"
#include "content/public/common/content_features.h"
#include "content/public/test/background_sync_test_util.h"
#include "content/public/test/browser_test.h"

namespace {

constexpr base::TimeDelta kMinGapBetweenPeriodicSyncEvents = base::Seconds(5);

}  // namespace

namespace content {

class PeriodicBackgroundSyncBrowserTest : public BackgroundSyncBaseBrowserTest {
 public:
  PeriodicBackgroundSyncBrowserTest() {}

  PeriodicBackgroundSyncBrowserTest(const PeriodicBackgroundSyncBrowserTest&) =
      delete;
  PeriodicBackgroundSyncBrowserTest& operator=(
      const PeriodicBackgroundSyncBrowserTest&) = delete;

  ~PeriodicBackgroundSyncBrowserTest() override {}

  bool Register(const std::string& tag, int min_interval_ms);
  bool RegisterFromIFrame(const std::string& tag, int min_interval_ms);
  bool RegisterNoMinInterval(const std::string& tag);
  bool RegisterFromServiceWorker(const std::string& tag, int min_interval_ms);
  std::string RegisterFromCrossOriginFrame(const std::string& frame_url);
  bool RegisterFromServiceWorkerNoMinInterval(const std::string& tag);
  bool HasTag(const std::string& tag);
  bool HasTagFromServiceWorker(const std::string& tag);
  bool Unregister(const std::string& tag);
  bool UnregisterFromServiceWorker(const std::string& tag);
  int GetNumPeriodicSyncEvents();

 protected:
  base::SimpleTestClock clock_;
};

bool PeriodicBackgroundSyncBrowserTest::Register(const std::string& tag,
                                                 int min_interval_ms) {
  std::string script_result = RunScript(base::StringPrintf(
      "%s('%s', %d);", "registerPeriodicSync", tag.c_str(), min_interval_ms));
  return script_result == BuildExpectedResult(tag, "registered");
}

bool PeriodicBackgroundSyncBrowserTest::RegisterFromIFrame(
    const std::string& tag,
    int min_interval_ms) {
  std::string script_result = RunScript(
      base::StringPrintf("%s('%s', %d);", "registerPeriodicSyncFromIFrame",
                         tag.c_str(), min_interval_ms));
  return script_result == BuildExpectedResult(tag, "registered");
}

std::string PeriodicBackgroundSyncBrowserTest::RegisterFromCrossOriginFrame(
    const std::string& frame_url) {
  // Start a second https server to use as a second origin.
  net::EmbeddedTestServer alt_server(net::EmbeddedTestServer::TYPE_HTTPS);
  alt_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  EXPECT_TRUE(alt_server.Start());

  GURL url = alt_server.GetURL(frame_url);
  return RunScript(BuildScriptString("registerPeriodicSyncFromCrossOriginFrame",
                                     url.spec()));
}

bool PeriodicBackgroundSyncBrowserTest::RegisterNoMinInterval(
    const std::string& tag) {
  std::string script_result = RunScript(
      base::StringPrintf("%s('%s');", "registerPeriodicSync", tag.c_str()));
  return script_result == BuildExpectedResult(tag, "registered");
}

bool PeriodicBackgroundSyncBrowserTest::RegisterFromServiceWorker(
    const std::string& tag,
    int min_interval_ms) {
  std::string script_result = RunScript(base::StringPrintf(
      "%s('%s', %d);", "registerPeriodicSyncFromServiceWorker", tag.c_str(),
      min_interval_ms));
  return script_result == BuildExpectedResult(tag, "register sent to SW");
}

bool PeriodicBackgroundSyncBrowserTest::RegisterFromServiceWorkerNoMinInterval(
    const std::string& tag) {
  std::string script_result = RunScript(
      BuildScriptString("registerPeriodicSyncFromServiceWorker", tag));
  return script_result == BuildExpectedResult(tag, "register sent to SW");
}

bool PeriodicBackgroundSyncBrowserTest::HasTag(const std::string& tag) {
  std::string script_result =
      RunScript(BuildScriptString("hasPeriodicSyncTag", tag));
  return script_result == BuildExpectedResult(tag, "found");
}

bool PeriodicBackgroundSyncBrowserTest::HasTagFromServiceWorker(
    const std::string& tag) {
  std::string script_result =
      RunScript(BuildScriptString("hasPeriodicSyncTagFromServiceWorker", tag));
  return (script_result == "ok - hasTag sent to SW");
}

bool PeriodicBackgroundSyncBrowserTest::Unregister(const std::string& tag) {
  std::string script_result = RunScript(BuildScriptString("unregister", tag));
  return script_result == BuildExpectedResult(tag, "unregistered");
}

bool PeriodicBackgroundSyncBrowserTest::UnregisterFromServiceWorker(
    const std::string& tag) {
  std::string script_result =
      RunScript(BuildScriptString("unregisterFromServiceWorker", tag));
  return script_result == BuildExpectedResult(tag, "unregister sent to SW");
}

int PeriodicBackgroundSyncBrowserTest::GetNumPeriodicSyncEvents() {
  std::string script_result = RunScript("getNumPeriodicSyncEvents()");
  int num_periodic_sync_events = -1;
  bool converted =
      base::StringToInt(PopConsoleString(), &num_periodic_sync_events);
  DCHECK(converted);
  return num_periodic_sync_events;
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       RegisterFromControlledDocument) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  EXPECT_TRUE(Register("foo", /* min_interval_ms= */ 1000));
  EXPECT_TRUE(Unregister("foo"));
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       RegisterNoMinInterval) {
  EXPECT_TRUE(RegisterServiceWorker());

  EXPECT_TRUE(RegisterNoMinInterval("foo"));
  EXPECT_TRUE(Unregister("foo"));
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       RegisterFromIFrameWithTopLevelFrameForOrigin) {
  GURL url = https_server()->GetURL(kEmptyURL);
  std::string script_result = RunScript(
      BuildScriptString("registerPeriodicSyncFromLocalFrame", url.spec()));

  // This succeeds because there's a top level frame for the origin.
  EXPECT_EQ(BuildExpectedResult("iframe", "registered periodicSync"),
            script_result);
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       RegisterFromIFrameWithoutTopLevelFrameForOrigin) {
  std::string script_result =
      RegisterFromCrossOriginFrame(kRegisterPeriodicSyncFromIFrameURL);

  // This fails because there's no top level frame open for the origin.
  EXPECT_EQ(BuildExpectedResult("frame", "failed to register periodicSync"),
            script_result);
}

// Verify that Register works in a service worker
IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       RegisterFromServiceWorker) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));

  EXPECT_TRUE(RegisterFromServiceWorker("foo_sw", /* min_interval_ms= */ 10));
  EXPECT_TRUE(PopConsole("ok - foo_sw registered in SW"));
  EXPECT_TRUE(Unregister("foo"));
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       RegisterFromServiceWorkerNoMinInterval) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));

  EXPECT_TRUE(RegisterFromServiceWorkerNoMinInterval("foo_sw"));
  EXPECT_TRUE(PopConsole("ok - foo_sw registered in SW"));
  EXPECT_TRUE(Unregister("foo"));
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest, FindATag) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  EXPECT_TRUE(Register("foo", /* min_interval_ms= */ 1000));
  EXPECT_TRUE(HasTag("foo"));
  EXPECT_TRUE(Unregister("foo"));
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       FindATagFromServiceWorker) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));  // Control the page.

  EXPECT_TRUE(Register("foo", /* min_interval_ms= */ 1000));
  EXPECT_TRUE(HasTagFromServiceWorker("foo"));
  EXPECT_TRUE(PopConsole("ok - foo found in SW"));
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       UnregisterFromServiceWorker) {
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));

  EXPECT_TRUE(RegisterNoMinInterval("foo"));
  EXPECT_TRUE(HasTag("foo"));
  EXPECT_TRUE(UnregisterFromServiceWorker("foo"));
  EXPECT_TRUE(PopConsole("ok - foo unregistered in SW"));
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       FirePeriodicSyncOnConnectivity) {
  SetTestClock(&clock_);
  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));

  // Prevent firing by going offline.
  background_sync_test_util::SetOnline(web_contents(), false);
  EXPECT_TRUE(Register("foo", /* min_interval_ms= */ 10));
  EXPECT_TRUE(HasTag("foo"));

  int initial_periodic_sync_events = GetNumPeriodicSyncEvents();
  ASSERT_EQ(initial_periodic_sync_events, 0);

  clock_.Advance(kMinGapBetweenPeriodicSyncEvents);

  EXPECT_EQ(GetNumPeriodicSyncEvents(), initial_periodic_sync_events);

  // Resume firing by going online.
  background_sync_test_util::SetOnline(web_contents(), true);
  EXPECT_TRUE(PopConsole("foo fired"));
  EXPECT_EQ(GetNumPeriodicSyncEvents(), initial_periodic_sync_events + 1);
  EXPECT_TRUE(HasTag("foo"));
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest, MultipleEventsFired) {
  SetTestClock(&clock_);

  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));

  EXPECT_TRUE(Register("foo", /* min_interval_ms= */ 10));

  clock_.Advance(kMinGapBetweenPeriodicSyncEvents);
  EXPECT_TRUE(PopConsole("foo fired"));
  EXPECT_TRUE(HasTag("foo"));

  clock_.Advance(kMinGapBetweenPeriodicSyncEvents);
  EXPECT_TRUE(PopConsole("foo fired"));
  EXPECT_TRUE(HasTag("foo"));
  EXPECT_TRUE(Unregister("foo"));
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       MultipleMinIntervalsAndTags) {
  SetTestClock(&clock_);

  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));

  EXPECT_TRUE(Register("foo", /* min_interval_ms= */ 10));
  EXPECT_TRUE(Register("foo", /* min_interval_ms= */ 200));
  EXPECT_TRUE(HasTag("foo"));

  EXPECT_TRUE(Register("bar", /* min_interval_ms= */ 50));
  EXPECT_TRUE(HasTag("bar"));

  clock_.Advance(kMinGapBetweenPeriodicSyncEvents);

  // Ordering is important here.
  EXPECT_TRUE(PopConsole("bar fired"));
  EXPECT_TRUE(PopConsole("foo fired"));

  EXPECT_TRUE(Unregister("foo"));
  EXPECT_FALSE(HasTag("foo"));
  EXPECT_TRUE(HasTag("bar"));

  clock_.Advance(kMinGapBetweenPeriodicSyncEvents);
  EXPECT_TRUE(PopConsole("bar fired"));
  EXPECT_TRUE(Unregister("bar"));
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest, WaitUntil) {
  SetTestClock(&clock_);

  EXPECT_TRUE(RegisterServiceWorker());
  EXPECT_TRUE(LoadTestPage(kDefaultTestURL));

  background_sync_test_util::SetOnline(web_contents(), false);

  EXPECT_TRUE(Register("delay", /* min_interval_ms= */ 10));
  ASSERT_TRUE(HasTag("delay"));

  clock_.Advance(kMinGapBetweenPeriodicSyncEvents);
  background_sync_test_util::SetOnline(web_contents(), true);
  base::RunLoop().RunUntilIdle();

  int num_periodicsync_events_fired = GetNumPeriodicSyncEvents();

  // Complete the task.
  EXPECT_TRUE(CompleteDelayedSyncEvent());
  EXPECT_TRUE(PopConsole("ok - delay completed"));
  EXPECT_EQ(GetNumPeriodicSyncEvents(), num_periodicsync_events_fired + 1);

  EXPECT_TRUE(HasTag("delay"));
  EXPECT_TRUE(Unregister("delay"));
}

}  // namespace content
