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
#include "content/public/test/browser_test_utils.h"

namespace {

constexpr base::TimeDelta kMinGapBetweenPeriodicSyncEvents = base::Seconds(5);

}  // namespace

namespace content {

class PeriodicBackgroundSyncBrowserTest : public BackgroundSyncBaseBrowserTest {
 public:
  PeriodicBackgroundSyncBrowserTest() = default;

  PeriodicBackgroundSyncBrowserTest(const PeriodicBackgroundSyncBrowserTest&) =
      delete;
  PeriodicBackgroundSyncBrowserTest& operator=(
      const PeriodicBackgroundSyncBrowserTest&) = delete;

  ~PeriodicBackgroundSyncBrowserTest() override = default;

  void Register(const std::string& tag, int min_interval_ms);
  void RegisterNoMinInterval(const std::string& tag);
  void RegisterFromServiceWorker(const std::string& tag, int min_interval_ms);
  EvalJsResult RegisterFromCrossOriginFrame(const std::string& frame_url);
  void RegisterFromServiceWorkerNoMinInterval(const std::string& tag);
  bool HasTag(const std::string& tag);
  bool HasTagFromServiceWorker(const std::string& tag);
  void Unregister(const std::string& tag);
  void UnregisterFromServiceWorker(const std::string& tag);
  int GetNumPeriodicSyncEvents();

 protected:
  base::SimpleTestClock clock_;
};

void PeriodicBackgroundSyncBrowserTest::Register(const std::string& tag,
                                                 int min_interval_ms) {
  ASSERT_EQ(BuildExpectedResult(tag, "registered"),
            EvalJs(web_contents(),
                   base::StringPrintf("%s('%s', %d);", "registerPeriodicSync",
                                      tag.c_str(), min_interval_ms)));
}

EvalJsResult PeriodicBackgroundSyncBrowserTest::RegisterFromCrossOriginFrame(
    const std::string& frame_url) {
  // Start a second https server to use as a second origin.
  net::EmbeddedTestServer alt_server(net::EmbeddedTestServer::TYPE_HTTPS);
  alt_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  EXPECT_TRUE(alt_server.Start());

  GURL url = alt_server.GetURL(frame_url);
  return EvalJs(web_contents(),
                BuildScriptString("registerPeriodicSyncFromCrossOriginFrame",
                                  url.spec()));
}

void PeriodicBackgroundSyncBrowserTest::RegisterNoMinInterval(
    const std::string& tag) {
  ASSERT_EQ(BuildExpectedResult(tag, "registered"),
            EvalJs(web_contents(),
                   base::StringPrintf("%s('%s');", "registerPeriodicSync",
                                      tag.c_str())));
}

void PeriodicBackgroundSyncBrowserTest::RegisterFromServiceWorker(
    const std::string& tag,
    int min_interval_ms) {
  ASSERT_EQ(BuildExpectedResult(tag, "register sent to SW"),
            EvalJs(web_contents(),
                   base::StringPrintf("%s('%s', %d);",
                                      "registerPeriodicSyncFromServiceWorker",
                                      tag.c_str(), min_interval_ms)));
}

void PeriodicBackgroundSyncBrowserTest::RegisterFromServiceWorkerNoMinInterval(
    const std::string& tag) {
  ASSERT_EQ(
      BuildExpectedResult(tag, "register sent to SW"),
      EvalJs(web_contents(),
             BuildScriptString("registerPeriodicSyncFromServiceWorker", tag)));
}

bool PeriodicBackgroundSyncBrowserTest::HasTag(const std::string& tag) {
  return EvalJs(web_contents(), BuildScriptString("hasPeriodicSyncTag", tag)) ==
         BuildExpectedResult(tag, "found");
}

bool PeriodicBackgroundSyncBrowserTest::HasTagFromServiceWorker(
    const std::string& tag) {
  return EvalJs(web_contents(),
                BuildScriptString("hasPeriodicSyncTagFromServiceWorker",
                                  tag)) == "ok - hasTag sent to SW";
}

void PeriodicBackgroundSyncBrowserTest::Unregister(const std::string& tag) {
  ASSERT_EQ(BuildExpectedResult(tag, "unregistered"),
            EvalJs(web_contents(), BuildScriptString("unregister", tag)));
}

void PeriodicBackgroundSyncBrowserTest::UnregisterFromServiceWorker(
    const std::string& tag) {
  ASSERT_EQ(BuildExpectedResult(tag, "unregister sent to SW"),
            EvalJs(web_contents(),
                   BuildScriptString("unregisterFromServiceWorker", tag)));
}

int PeriodicBackgroundSyncBrowserTest::GetNumPeriodicSyncEvents() {
  EXPECT_TRUE(ExecJs(web_contents(), "getNumPeriodicSyncEvents()"));
  int num_periodic_sync_events = -1;
  bool converted = base::StringToInt(PopConsoleString().ExtractString(),
                                     &num_periodic_sync_events);
  DCHECK(converted);
  return num_periodic_sync_events;
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       RegisterFromControlledDocument) {
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);  // Control the page.

  Register("foo", /* min_interval_ms= */ 1000);
  Unregister("foo");
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       RegisterNoMinInterval) {
  RegisterServiceWorker();

  RegisterNoMinInterval("foo");
  Unregister("foo");
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       RegisterFromIFrameWithTopLevelFrameForOrigin) {
  GURL url = https_server()->GetURL(kEmptyURL);

  // This succeeds because there's a top level frame for the origin.
  EXPECT_EQ(EvalJs(web_contents(),
                   BuildScriptString("registerPeriodicSyncFromLocalFrame",
                                     url.spec())),
            BuildExpectedResult("iframe", "registered periodicSync"));
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       RegisterFromIFrameWithoutTopLevelFrameForOrigin) {
  // This fails because there's no top level frame open for the origin.
  EXPECT_EQ(BuildExpectedResult("frame", "failed to register periodicSync"),
            RegisterFromCrossOriginFrame(kRegisterPeriodicSyncFromIFrameURL));
}

// Verify that Register works in a service worker
IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       RegisterFromServiceWorker) {
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);

  RegisterFromServiceWorker("foo_sw", /* min_interval_ms= */ 10);
  EXPECT_EQ("ok - foo_sw registered in SW", PopConsoleString());
  Unregister("foo");
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       RegisterFromServiceWorkerNoMinInterval) {
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);

  RegisterFromServiceWorkerNoMinInterval("foo_sw");
  EXPECT_EQ("ok - foo_sw registered in SW", PopConsoleString());
  Unregister("foo");
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest, FindATag) {
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);  // Control the page.

  Register("foo", /* min_interval_ms= */ 1000);
  EXPECT_TRUE(HasTag("foo"));
  Unregister("foo");
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       FindATagFromServiceWorker) {
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);  // Control the page.

  Register("foo", /* min_interval_ms= */ 1000);
  EXPECT_TRUE(HasTagFromServiceWorker("foo"));
  EXPECT_EQ("ok - foo found in SW", PopConsoleString());
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       UnregisterFromServiceWorker) {
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);

  RegisterNoMinInterval("foo");
  EXPECT_TRUE(HasTag("foo"));
  UnregisterFromServiceWorker("foo");
  EXPECT_EQ("ok - foo unregistered in SW", PopConsoleString());
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       FirePeriodicSyncOnConnectivity) {
  SetTestClock(&clock_);
  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);

  // Prevent firing by going offline.
  background_sync_test_util::SetOnline(web_contents(), false);
  Register("foo", /* min_interval_ms= */ 10);
  EXPECT_TRUE(HasTag("foo"));

  int initial_periodic_sync_events = GetNumPeriodicSyncEvents();
  ASSERT_EQ(initial_periodic_sync_events, 0);

  clock_.Advance(kMinGapBetweenPeriodicSyncEvents);

  EXPECT_EQ(GetNumPeriodicSyncEvents(), initial_periodic_sync_events);

  // Resume firing by going online.
  background_sync_test_util::SetOnline(web_contents(), true);
  EXPECT_EQ("foo fired", PopConsoleString());
  EXPECT_EQ(GetNumPeriodicSyncEvents(), initial_periodic_sync_events + 1);
  EXPECT_TRUE(HasTag("foo"));
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest, MultipleEventsFired) {
  SetTestClock(&clock_);

  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);

  Register("foo", /* min_interval_ms= */ 10);

  clock_.Advance(kMinGapBetweenPeriodicSyncEvents);
  EXPECT_EQ("foo fired", PopConsoleString());
  EXPECT_TRUE(HasTag("foo"));

  clock_.Advance(kMinGapBetweenPeriodicSyncEvents);
  EXPECT_EQ("foo fired", PopConsoleString());
  EXPECT_TRUE(HasTag("foo"));
  Unregister("foo");
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest,
                       MultipleMinIntervalsAndTags) {
  SetTestClock(&clock_);

  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);

  Register("foo", /* min_interval_ms= */ 10);
  Register("foo", /* min_interval_ms= */ 200);
  EXPECT_TRUE(HasTag("foo"));

  Register("bar", /* min_interval_ms= */ 50);
  EXPECT_TRUE(HasTag("bar"));

  clock_.Advance(kMinGapBetweenPeriodicSyncEvents);

  // Ordering is important here.
  EXPECT_EQ("bar fired", PopConsoleString());
  EXPECT_EQ("foo fired", PopConsoleString());

  Unregister("foo");
  EXPECT_FALSE(HasTag("foo"));
  EXPECT_TRUE(HasTag("bar"));

  clock_.Advance(kMinGapBetweenPeriodicSyncEvents);
  EXPECT_EQ("bar fired", PopConsoleString());
  Unregister("bar");
}

IN_PROC_BROWSER_TEST_F(PeriodicBackgroundSyncBrowserTest, WaitUntil) {
  SetTestClock(&clock_);

  RegisterServiceWorker();
  LoadTestPage(kDefaultTestURL);

  background_sync_test_util::SetOnline(web_contents(), false);

  Register("delay", /* min_interval_ms= */ 10);
  ASSERT_TRUE(HasTag("delay"));

  clock_.Advance(kMinGapBetweenPeriodicSyncEvents);
  background_sync_test_util::SetOnline(web_contents(), true);
  base::RunLoop().RunUntilIdle();

  int num_periodicsync_events_fired = GetNumPeriodicSyncEvents();

  // Complete the task.
  CompleteDelayedSyncEvent();
  EXPECT_EQ("ok - delay completed", PopConsoleString());
  EXPECT_EQ(GetNumPeriodicSyncEvents(), num_periodicsync_events_fired + 1);

  EXPECT_TRUE(HasTag("delay"));
  Unregister("delay");
}

}  // namespace content
