// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/btm/btm_short_visit_observer.h"

#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/run_loop.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/simple_test_clock.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/btm/btm_service_impl.h"
#include "content/browser/btm/btm_test_utils.h"
#include "content/browser/btm/btm_utils.h"
#include "content/public/browser/btm_redirect_info.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/switches.h"
#include "ui/base/page_transition_types.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace content {
namespace {

class BtmShortVisitObserverBrowserTest : public ContentBrowserTest {
 public:
  void SetUp() override {
    clock_.Advance(base::Days(1));
    BtmShortVisitObserver::SetDefaultClockForTesting(&clock_);

    ContentBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Prevents flakiness by handling clicks even before content is drawn.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_https_test_server().SetCertHostnames(
        {"a.test", "*.a.test", "b.test", "*.b.test", "c.test", "*.c.test",
         "d.test", "*.d.test", "e.test", "*.e.test"});
    embedded_https_test_server().AddDefaultHandlers(GetTestDataFilePath());
    ASSERT_TRUE(embedded_https_test_server().Start());
  }

  void PreRunTestOnMainThread() override {
    ContentBrowserTest::PreRunTestOnMainThread();
    ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  }

  WebContents* web_contents() { return shell()->web_contents(); }

  base::SimpleTestClock clock_;
};

// Extracts the source URLs from the return value of
// TestUkmRecorder::GetEntriesByName() (which has a weird vector<raw_ptr<>>
// type that looks likely to change).
template <class EntryPtr>
std::vector<GURL> EntryURLs(ukm::TestUkmRecorder& ukm_recorder,
                            const std::vector<EntryPtr>& entries) {
  std::vector<GURL> urls;

  for (const EntryPtr& entry : entries) {
    const ukm::UkmSource* src =
        ukm_recorder.GetSourceForSourceId(entry->source_id);
    urls.push_back(CHECK_DEREF(src).url());
  }

  return urls;
}

// The return type of TestUkmRecorder::GetEntriesByName().
using UkmEntryVector =
    std::vector<raw_ptr<const ukm::mojom::UkmEntry, VectorExperimental>>;

// Waits for at least `n` BTM.ShortVisit events to be recorded, and returns
// their entries.
UkmEntryVector GetBtmShortVisits(size_t n, ukm::TestUkmRecorder& ukm_recorder) {
  static constexpr std::string_view kEntryName = "BTM.ShortVisit";
  UkmEntryVector entries = ukm_recorder.GetEntriesByName(kEntryName);
  if (entries.size() >= n) {
    return entries;
  }
  const size_t still_need = n - entries.size();
  base::RunLoop run_loop;
  ukm_recorder.SetOnAddEntryCallback(
      kEntryName, base::BarrierClosure(still_need, run_loop.QuitClosure()));
  run_loop.Run();
  return ukm_recorder.GetEntriesByName(kEntryName);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest, VisitDuration) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  clock_.Advance(base::Milliseconds(2499));
  ASSERT_TRUE(NavigateToURL(web_contents(), url2));
  clock_.Advance(base::Milliseconds(3500));
  ASSERT_TRUE(NavigateToURL(web_contents(), url3));

  UkmEntryVector entries = GetBtmShortVisits(2, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1, url2));
  ukm_recorder.ExpectEntryMetric(entries[0], "VisitDuration", 2);
  ukm_recorder.ExpectEntryMetric(entries[1], "VisitDuration", 4);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest, IgnoreLongVisits) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  clock_.Advance(base::Seconds(11));
  ASSERT_TRUE(NavigateToURL(web_contents(), url2));
  clock_.Advance(base::Seconds(9));
  ASSERT_TRUE(NavigateToURL(web_contents(), url3));

  // The 11-second visit is not reported.
  UkmEntryVector entries = GetBtmShortVisits(1, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries), testing::ElementsAre(url2));
  ukm_recorder.ExpectEntryMetric(entries[0], "VisitDuration", 9);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest,
                       ExitWasRendererInitiated) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  ASSERT_TRUE(NavigateToURL(web_contents(), url2));
  ASSERT_TRUE(NavigateToURLFromRenderer(web_contents(), url3));

  UkmEntryVector entries = GetBtmShortVisits(2, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1, url2));
  ukm_recorder.ExpectEntryMetric(entries[0], "ExitWasRendererInitiated", 0);
  ukm_recorder.ExpectEntryMetric(entries[1], "ExitWasRendererInitiated", 1);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest, ExitHadUserGesture) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  ASSERT_TRUE(NavigateToURLFromRenderer(web_contents(), url2));
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents(), url3));

  UkmEntryVector entries = GetBtmShortVisits(2, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1, url2));
  ukm_recorder.ExpectEntryMetric(entries[0], "ExitHadUserGesture", 1);
  ukm_recorder.ExpectEntryMetric(entries[1], "ExitHadUserGesture", 0);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest, ExitPageTransition) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("b.test", "/title1.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  ASSERT_TRUE(NavigateToURL(web_contents(), url2));
  // Click a link to navigate to url3.
  ASSERT_TRUE(ExecJs(web_contents(), R"(
      const anchor = document.createElement("a");
      anchor.href = "/title1.html";
      anchor.click(); )",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));

  UkmEntryVector entries = GetBtmShortVisits(2, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1, url2));
  ukm_recorder.ExpectEntryMetric(
      entries[0], "ExitPageTransition",
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  ukm_recorder.ExpectEntryMetric(entries[1], "ExitPageTransition",
                                 ui::PAGE_TRANSITION_LINK);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest, PreviousSiteSame) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2a =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url2b =
      embedded_https_test_server().GetURL("sub.b.test", "/title1.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  ASSERT_TRUE(NavigateToURL(web_contents(), url2a));
  ASSERT_TRUE(NavigateToURL(web_contents(), url2b));
  ASSERT_TRUE(NavigateToURL(web_contents(), url3));

  UkmEntryVector entries = GetBtmShortVisits(3, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1, url2a, url2b));
  // Previous site unknown.
  ukm_recorder.ExpectEntryMetric(entries[0], "PreviousSiteSame", 0);
  // Previous site not same.
  ukm_recorder.ExpectEntryMetric(entries[1], "PreviousSiteSame", 0);
  // Previous site same.
  ukm_recorder.ExpectEntryMetric(entries[2], "PreviousSiteSame", 1);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest, NextSiteSame) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2a =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url2b =
      embedded_https_test_server().GetURL("sub.b.test", "/title1.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  ASSERT_TRUE(NavigateToURL(web_contents(), url2a));
  ASSERT_TRUE(NavigateToURL(web_contents(), url2b));
  ASSERT_TRUE(NavigateToURL(web_contents(), url3));

  UkmEntryVector entries = GetBtmShortVisits(3, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1, url2a, url2b));
  ukm_recorder.ExpectEntryMetric(entries[0], "NextSiteSame", 0);
  ukm_recorder.ExpectEntryMetric(entries[1], "NextSiteSame", 1);
  ukm_recorder.ExpectEntryMetric(entries[2], "NextSiteSame", 0);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest,
                       PreviousAndNextSiteSame) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  const GURL url4 =
      embedded_https_test_server().GetURL("sub.b.test", "/title1.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  ASSERT_TRUE(NavigateToURL(web_contents(), url2));
  ASSERT_TRUE(NavigateToURL(web_contents(), url3));
  ASSERT_TRUE(NavigateToURL(web_contents(), url4));

  UkmEntryVector entries = GetBtmShortVisits(3, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1, url2, url3));
  ukm_recorder.ExpectEntryMetric(entries[0], "PreviousAndNextSiteSame", 0);
  ukm_recorder.ExpectEntryMetric(entries[1], "PreviousAndNextSiteSame", 0);
  ukm_recorder.ExpectEntryMetric(entries[2], "PreviousAndNextSiteSame", 1);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest,
                       IgnoreUncommittedNavigations) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  // A navigation to `url2` won't commit since it returns an HTTP 204 response.
  const GURL url2 = embedded_https_test_server().GetURL("b.test", "/nocontent");
  const GURL url3 =
      embedded_https_test_server().GetURL("a.test", "/title1.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  clock_.Advance(base::Seconds(2));
  // Note: Since it doesn't commit, after the navigation to `url2` finishes, the
  // user will still be on `url1`.
  ASSERT_TRUE(NavigateToURL(web_contents(), url2, url1));
  clock_.Advance(base::Seconds(3));
  ASSERT_TRUE(NavigateToURLFromRenderer(web_contents(), url3));

  UkmEntryVector entries = GetBtmShortVisits(1, ukm_recorder);
  // Only one UKM event is reported -- for `url1`.
  ASSERT_THAT(EntryURLs(ukm_recorder, entries), testing::ElementsAre(url1));
  // The duration is the sum of the time before and after the `url2` navigation.
  ukm_recorder.ExpectEntryMetric(entries[0], "VisitDuration", 5);
  // `url1` and `url3` are same-site.
  ukm_recorder.ExpectEntryMetric(entries[0], "NextSiteSame", 1);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest, IgnoreAllSameSite) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1a =
      embedded_https_test_server().GetURL("a.test", "/empty.html?a");
  const GURL url1b =
      embedded_https_test_server().GetURL("sub1.a.test", "/empty.html?b");
  const GURL url1c =
      embedded_https_test_server().GetURL("sub2.a.test", "/empty.html?c");
  const GURL url2a =
      embedded_https_test_server().GetURL("b.test", "/empty.html?a");
  const GURL url2b =
      embedded_https_test_server().GetURL("sub1.b.test", "/empty.html?b");
  const GURL url2c =
      embedded_https_test_server().GetURL("sub2.b.test", "/empty.html?c");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");

  // Visit three pages on a.test.
  ASSERT_TRUE(NavigateToURL(web_contents(), url1a));
  ASSERT_TRUE(NavigateToURL(web_contents(), url1b));
  ASSERT_TRUE(NavigateToURL(web_contents(), url1c));
  // Visit three pages on b.test.
  ASSERT_TRUE(NavigateToURL(web_contents(), url2a));
  ASSERT_TRUE(NavigateToURL(web_contents(), url2b));
  ASSERT_TRUE(NavigateToURL(web_contents(), url2c));
  // Visit c.test.
  ASSERT_TRUE(NavigateToURL(web_contents(), url3));

  UkmEntryVector entries = GetBtmShortVisits(4, ukm_recorder);
  // No visits reported for url1b or url2b, because each is same-site to both of
  // the pages before/after it.
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1a, url1c, url2a, url2c));
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest,
                       TimeSinceLastInteraction) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  const GURL url4 =
      embedded_https_test_server().GetURL("d.test", "/empty.html");

  auto* btm_service = BtmServiceImpl::Get(web_contents()->GetBrowserContext());
  btm_service->SetStorageClockForTesting(&clock_);
  // Record an interaction an hour ago.
  btm_service->storage()
      ->AsyncCall(&BtmStorage::RecordUserActivation)
      .WithArgs(url2, clock_.Now(), BtmCookieMode::kBlock3PC);
  clock_.Advance(base::Hours(1));

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents(), url2));
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents(), url3));
  ASSERT_TRUE(NavigateToURLFromRenderer(web_contents(), url4));

  UkmEntryVector entries = GetBtmShortVisits(3, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1, url2, url3));
  // No previous interaction.
  ukm_recorder.ExpectEntryMetric(entries[0], "TimeSinceLastInteraction", -1);
  // A typical interaction.
  ukm_recorder.ExpectEntryMetric(entries[1], "TimeSinceLastInteraction",
                                 ukm::GetSemanticBucketMinForDurationTiming(
                                     base::Hours(1).InMilliseconds()));
  // We navigated away with a user gesture, but that shouldn't affect the
  // metric -- still no previous interaction.
  ukm_recorder.ExpectEntryMetric(entries[2], "TimeSinceLastInteraction", -1);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest,
                       TimeSinceLastInteraction_FutureInteraction) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");

  auto* btm_service = BtmServiceImpl::Get(web_contents()->GetBrowserContext());
  btm_service->SetStorageClockForTesting(&clock_);
  // Record a "future" interaction by setting the clock backwards.
  btm_service->storage()
      ->AsyncCall(&BtmStorage::RecordUserActivation)
      .WithArgs(url2, clock_.Now() + base::Minutes(1),
                BtmCookieMode::kBlock3PC);
  clock_.Advance(-base::Hours(1));

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents(), url2));
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents(), url3));

  UkmEntryVector entries = GetBtmShortVisits(2, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1, url2));
  // No previous interaction.
  ukm_recorder.ExpectEntryMetric(entries[0], "TimeSinceLastInteraction", -1);
  // "Future" interaction.
  ukm_recorder.ExpectEntryMetric(entries[1], "TimeSinceLastInteraction", -3);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest,
                       NewTabUsesOpenerAsPreviousUrl) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html?a");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("a.test", "/empty.html?b");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  ASSERT_OK_AND_ASSIGN(WebContents * new_tab,
                       OpenInNewTab(web_contents(), url2));
  ASSERT_TRUE(NavigateToURLFromRenderer(new_tab, url3));

  UkmEntryVector entries = GetBtmShortVisits(1, ukm_recorder);
  // Only one UKM event is reported -- for `url2`.
  ASSERT_THAT(EntryURLs(ukm_recorder, entries), testing::ElementsAre(url2));
  // `url1` is considered the previous url for the visit.
  ukm_recorder.ExpectEntryMetric(entries[0], "PreviousSiteSame", 0);
  ukm_recorder.ExpectEntryMetric(entries[0], "NextSiteSame", 0);
  ukm_recorder.ExpectEntryMetric(entries[0], "PreviousAndNextSiteSame", 1);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest, ShortVisitNeighbor) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  const GURL url4 =
      embedded_https_test_server().GetURL("d.test", "/empty.html");
  const GURL url5 =
      embedded_https_test_server().GetURL("e.test", "/empty.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  ASSERT_TRUE(NavigateToURL(web_contents(), url2));
  ASSERT_TRUE(NavigateToURL(web_contents(), url3));
  // Go back to url2.
  ASSERT_TRUE(HistoryGoBack(web_contents()));
  ASSERT_TRUE(NavigateToURL(web_contents(), url4));
  ASSERT_TRUE(NavigateToURL(web_contents(), url5));

  UkmEntryVector entries = GetBtmShortVisits(5, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1, url2, url3, url2, url4));
  auto get_visit_id = [&](const ukm::mojom::UkmEntry* entry) -> int64_t {
    return CHECK_DEREF(ukm_recorder.GetEntryMetric(entry, "ShortVisitId"));
  };
  const int64_t visit_id1 = get_visit_id(entries[0]),
                visit_id2 = get_visit_id(entries[1]),
                visit_id3 = get_visit_id(entries[2]),
                visit_id4 = get_visit_id(entries[3]),
                visit_id5 = get_visit_id(entries[4]);
  ASSERT_THAT((std::set<int64_t>{visit_id1, visit_id2}), testing::SizeIs(2))
      << "Visit IDs are not unique";

  UkmEntryVector neighbor_entries =
      ukm_recorder.GetEntriesByName("BTM.ShortVisitNeighbor");

  // 5 visits => 9 neighbors (not 10, because the first visit had no previous
  // neighbor).
  ASSERT_THAT(neighbor_entries, testing::SizeIs(9));
  // url1 had one neighbor, url2.
  ukm_recorder.ExpectEntrySourceHasUrl(neighbor_entries[0], url2);
  // The neighbor followed url1.
  ukm_recorder.ExpectEntryMetric(neighbor_entries[0], "IsPreceding", false);
  // The visit ids match.
  ukm_recorder.ExpectEntryMetric(neighbor_entries[0], "ShortVisitId",
                                 visit_id1);
  // url2 had two neighbors, url1 and url3. Both visit ids match.
  ukm_recorder.ExpectEntrySourceHasUrl(neighbor_entries[1], url1);
  ukm_recorder.ExpectEntryMetric(neighbor_entries[1], "IsPreceding", true);
  ukm_recorder.ExpectEntryMetric(neighbor_entries[1], "ShortVisitId",
                                 visit_id2);
  ukm_recorder.ExpectEntrySourceHasUrl(neighbor_entries[2], url3);
  ukm_recorder.ExpectEntryMetric(neighbor_entries[2], "IsPreceding", false);
  ukm_recorder.ExpectEntryMetric(neighbor_entries[2], "ShortVisitId",
                                 visit_id2);
  // url3 that two neighbors, url2 and the second visit to url2.
  ukm_recorder.ExpectEntrySourceHasUrl(neighbor_entries[3], url2);
  ukm_recorder.ExpectEntryMetric(neighbor_entries[3], "IsPreceding", true);
  ukm_recorder.ExpectEntryMetric(neighbor_entries[3], "ShortVisitId",
                                 visit_id3);
  ukm_recorder.ExpectEntrySourceHasUrl(neighbor_entries[4], url2);
  ukm_recorder.ExpectEntryMetric(neighbor_entries[4], "IsPreceding", false);
  ukm_recorder.ExpectEntryMetric(neighbor_entries[4], "ShortVisitId",
                                 visit_id3);
  // The second visit to url2 had two neighbors, url3 and url4.
  ukm_recorder.ExpectEntrySourceHasUrl(neighbor_entries[5], url3);
  ukm_recorder.ExpectEntryMetric(neighbor_entries[5], "IsPreceding", true);
  ukm_recorder.ExpectEntryMetric(neighbor_entries[5], "ShortVisitId",
                                 visit_id4);
  ukm_recorder.ExpectEntrySourceHasUrl(neighbor_entries[6], url4);
  ukm_recorder.ExpectEntryMetric(neighbor_entries[6], "IsPreceding", false);
  ukm_recorder.ExpectEntryMetric(neighbor_entries[6], "ShortVisitId",
                                 visit_id4);
  // url4 had two neighbors, url2 and url5.
  ukm_recorder.ExpectEntrySourceHasUrl(neighbor_entries[7], url2);
  ukm_recorder.ExpectEntryMetric(neighbor_entries[7], "IsPreceding", true);
  ukm_recorder.ExpectEntryMetric(neighbor_entries[7], "ShortVisitId",
                                 visit_id5);
  ukm_recorder.ExpectEntrySourceHasUrl(neighbor_entries[8], url5);
  ukm_recorder.ExpectEntryMetric(neighbor_entries[8], "IsPreceding", false);
  ukm_recorder.ExpectEntryMetric(neighbor_entries[8], "ShortVisitId",
                                 visit_id5);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest,
                       EntranceWasRendererInitiated) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  ASSERT_TRUE(NavigateToURLFromRenderer(web_contents(), url2));
  ASSERT_TRUE(NavigateToURL(web_contents(), url3));

  UkmEntryVector entries = GetBtmShortVisits(2, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1, url2));
  ukm_recorder.ExpectEntryMetric(entries[0], "EntranceWasRendererInitiated", 0);
  ukm_recorder.ExpectEntryMetric(entries[1], "EntranceWasRendererInitiated", 1);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest,
                       EntranceHadUserGesture) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  // FYI, the first navigation must be browser-based, because the WebContents
  // starts on the empty URL, which doesn't support JavaScript. So we need an
  // extra navigation in order to test renderer-initiated navigations with and
  // without user gestures.
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");
  const GURL url4 =
      embedded_https_test_server().GetURL("d.test", "/empty.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  ASSERT_TRUE(NavigateToURLFromRenderer(web_contents(), url2));
  ASSERT_TRUE(
      NavigateToURLFromRendererWithoutUserGesture(web_contents(), url3));
  ASSERT_TRUE(NavigateToURL(web_contents(), url4));

  UkmEntryVector entries = GetBtmShortVisits(3, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1, url2, url3));
  ukm_recorder.ExpectEntryMetric(entries[0], "EntranceHadUserGesture", 0);
  ukm_recorder.ExpectEntryMetric(entries[1], "EntranceHadUserGesture", 1);
  ukm_recorder.ExpectEntryMetric(entries[2], "EntranceHadUserGesture", 0);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest,
                       EntrancePageTransition) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("a.test", "/title1.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  // Click a link to navigate to url2.
  TestFrameNavigationObserver nav_observer(web_contents());
  ASSERT_TRUE(ExecJs(web_contents(), R"(
      const anchor = document.createElement("a");
      anchor.href = "/title1.html";
      anchor.click(); )",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  nav_observer.Wait();
  ASSERT_EQ(web_contents()->GetLastCommittedURL(), url2);
  ASSERT_TRUE(NavigateToURL(web_contents(), url3));

  UkmEntryVector entries = GetBtmShortVisits(2, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1, url2));
  ukm_recorder.ExpectEntryMetric(
      entries[0], "EntrancePageTransition",
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  ukm_recorder.ExpectEntryMetric(entries[1], "EntrancePageTransition",
                                 ui::PAGE_TRANSITION_LINK);
}

void SimulateKeyPress(WebContents* web_contents, std::string_view code_string) {
  // Taken (simplified) from content/browser/keyboard_lock_browsertest.cc.
  ui::DomKey dom_key = ui::KeycodeConverter::KeyStringToDomKey(code_string);
  ui::DomCode dom_code = ui::KeycodeConverter::CodeStringToDomCode(code_string);
  SimulateKeyPress(web_contents, dom_key, dom_code,
                   ui::DomCodeToUsLayoutKeyboardCode(dom_code), false, false,
                   false, false);
}

class UserInteractionObserver : public WebContentsObserver {
 public:
  explicit UserInteractionObserver(WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  [[nodiscard]] bool Wait() { return future_.Wait(); }

  void DidGetUserInteraction(const blink::WebInputEvent& event) override {
    future_.SetValue();
  }

 private:
  base::test::TestFuture<void> future_;
};

class StorageObserver : public WebContentsObserver {
 public:
  explicit StorageObserver(WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  [[nodiscard]] bool Wait() { return future_.Wait(); }

  void NotifyStorageAccessed(RenderFrameHost* render_frame_host,
                             blink::mojom::StorageTypeAccessed storage_type,
                             bool blocked) override {
    future_.SetValue();
  }

 private:
  base::test::TestFuture<void> future_;
};

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest, HadKeydownEvent) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  ASSERT_TRUE(NavigateToURL(web_contents(), url2));
  UserInteractionObserver observer(web_contents());
  SimulateKeyPress(web_contents(), "KeyK");
  ASSERT_TRUE(observer.Wait());
  ASSERT_TRUE(NavigateToURL(web_contents(), url3));

  UkmEntryVector entries = GetBtmShortVisits(2, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1, url2));
  ukm_recorder.ExpectEntryMetric(entries[0], "HadKeyDownEvent", 0);
  ukm_recorder.ExpectEntryMetric(entries[1], "HadKeyDownEvent", 1);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest,
                       HadKeydownEvent_IgnoresSyntheticEvents) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  ASSERT_TRUE(NavigateToURL(web_contents(), url2));

  TestFrameNavigationObserver nav_observer(web_contents());
  ASSERT_TRUE(ExecJs(web_contents(),
                     R"(
    const promise = new Promise((resolve, reject) => {
      document.body.addEventListener('keydown', resolve);
    });

    document.body.dispatchEvent(new Event('keydown'));

    // Return a promise that will resolve when the event is handled.
    promise;
    )",

                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_TRUE(NavigateToURL(web_contents(), url3));
  // nav_observer.Wait();
  // ASSERT_EQ(web_contents()->GetLastCommittedURL(), url3);

  UkmEntryVector entries = GetBtmShortVisits(2, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1, url2));
  ukm_recorder.ExpectEntryMetric(entries[0], "HadKeyDownEvent", 0);
  ukm_recorder.ExpectEntryMetric(entries[1], "HadKeyDownEvent", 0);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest,
                       HadKeydownEvent_IgnoresClicks) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  ASSERT_TRUE(NavigateToURL(web_contents(), url2));
  UserActivationObserver observer(web_contents(),
                                  web_contents()->GetPrimaryMainFrame());
  SimulateMouseClick(web_contents(), 0, blink::WebMouseEvent::Button::kLeft);
  observer.Wait();
  ASSERT_TRUE(NavigateToURL(web_contents(), url3));

  UkmEntryVector entries = GetBtmShortVisits(2, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1, url2));
  ukm_recorder.ExpectEntryMetric(entries[0], "HadKeyDownEvent", 0);
  ukm_recorder.ExpectEntryMetric(entries[1], "HadKeyDownEvent", 0);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest,
                       AccessedStorage_Cookies) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  ASSERT_TRUE(NavigateToURL(web_contents(), url2));
  URLCookieAccessObserver observer(web_contents(), url2,
                                   CookieOperation::kChange);
  ASSERT_TRUE(ExecJs(web_contents(), "document.cookie = 'foo=bar'",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  observer.Wait();
  ASSERT_TRUE(NavigateToURL(web_contents(), url3));

  UkmEntryVector entries = GetBtmShortVisits(2, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1, url2));
  ukm_recorder.ExpectEntryMetric(entries[0], "AccessedStorage", 0);
  ukm_recorder.ExpectEntryMetric(entries[1], "AccessedStorage", 1);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest,
                       AccessedStorage_IgnoreNavigationRead) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/set-cookie?foo=bar");
  const GURL url2 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  clock_.Advance(base::Minutes(1));
  URLCookieAccessObserver observer(web_contents(), url2,
                                   CookieOperation::kRead);
  ASSERT_TRUE(NavigateToURL(web_contents(), url2));
  observer.Wait();
  ASSERT_TRUE(NavigateToURL(web_contents(), url3));

  // The visit to url2 triggered a cookie read, but we don't include it in the
  // metric.
  UkmEntryVector entries = GetBtmShortVisits(1, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries), testing::ElementsAre(url2));
  ukm_recorder.ExpectEntryMetric(entries[0], "AccessedStorage", 0);
}

IN_PROC_BROWSER_TEST_F(BtmShortVisitObserverBrowserTest,
                       AccessedStorage_LocalStorage) {
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  const GURL url1 =
      embedded_https_test_server().GetURL("a.test", "/empty.html");
  const GURL url2 =
      embedded_https_test_server().GetURL("b.test", "/empty.html");
  const GURL url3 =
      embedded_https_test_server().GetURL("c.test", "/empty.html");

  ASSERT_TRUE(NavigateToURL(web_contents(), url1));
  ASSERT_TRUE(NavigateToURL(web_contents(), url2));
  StorageObserver observer(web_contents());
  ASSERT_TRUE(ExecJs(web_contents(), "localStorage.setItem('foo', 'bar')",
                     EXECUTE_SCRIPT_NO_USER_GESTURE));
  ASSERT_TRUE(observer.Wait());
  ASSERT_TRUE(NavigateToURL(web_contents(), url3));

  UkmEntryVector entries = GetBtmShortVisits(2, ukm_recorder);
  ASSERT_THAT(EntryURLs(ukm_recorder, entries),
              testing::ElementsAre(url1, url2));
  ukm_recorder.ExpectEntryMetric(entries[0], "AccessedStorage", 0);
  ukm_recorder.ExpectEntryMetric(entries[1], "AccessedStorage", 1);
}

}  // namespace
}  // namespace content
