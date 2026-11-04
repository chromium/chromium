// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/ttc_page_context_monitor.h"

#import <memory>
#import <string>
#import <utility>
#import <vector>

#import "base/types/expected.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

// Fake `TtcPageContextMonitor::Delegate` recording fetched `PageContext`
// responses.
class FakeTtcPageContextMonitorDelegate
    : public TtcPageContextMonitor::Delegate {
 public:
  FakeTtcPageContextMonitorDelegate() = default;
  ~FakeTtcPageContextMonitorDelegate() override = default;

  const std::vector<optimization_guide::proto::PageContext>& fetched_contexts()
      const {
    return fetched_contexts_;
  }

  const std::vector<PageContextWrapperError>& fetched_errors() const {
    return fetched_errors_;
  }

  // `TtcPageContextMonitor::Delegate` implementation:
  void OnPageContextFetched(
      PageContextWrapperCallbackResponse response) override {
    if (response.has_value() && response.value()) {
      fetched_contexts_.push_back(*response.value());
    } else if (!response.has_value()) {
      fetched_errors_.push_back(response.error());
    }
  }

 private:
  std::vector<optimization_guide::proto::PageContext> fetched_contexts_;
  std::vector<PageContextWrapperError> fetched_errors_;
};

// Builds a synthetic `PageContext` proto for `url` and `title`.
PageContextWrapperCallbackResponse MakePageContextResponse(
    const GURL& url,
    const std::string& title) {
  auto context = std::make_unique<optimization_guide::proto::PageContext>();
  context->set_url(url.spec());
  context->set_title(title);
  auto* apc = context->mutable_annotated_page_content();
  apc->mutable_main_frame_data()->set_url(url.spec());
  apc->mutable_main_frame_data()->set_title(title);
  return base::ok(std::move(context));
}

}  // namespace

// Test fixture for `TtcPageContextMonitor`.
class TtcPageContextMonitorTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    profile_ = TestProfileIOS::Builder().Build();
    ConfigureEligibleWebState(web_state_, GURL("https://example.com/initial"));
  }

  // Configures `web_state` as a realized, finished-loading HTML page at `url`.
  void ConfigureEligibleWebState(web::FakeWebState& web_state,
                                 const GURL& url) {
    web_state.SetBrowserState(profile_.get());
    web_state.SetIsRealized(true);
    web_state.SetCurrentURL(url);
    web_state.SetContentsMimeType("text/html");
    web_state.SetLoading(false);
    for (web::ContentWorld world : {web::ContentWorld::kIsolatedWorld,
                                    web::ContentWorld::kPageContentWorld}) {
      web_state.SetWebFramesManager(
          world, std::make_unique<web::FakeWebFramesManager>());
    }
  }

  // Returns `true` if `monitor` currently holds an in-flight
  // `PageContextWrapper`.
  bool HasPendingWrapper(const TtcPageContextMonitor& monitor) const {
    return monitor.page_context_wrapper_ != nil;
  }

  // Simulates completion of `monitor`'s pending extraction for `expected_url`
  // with `response`.
  void SimulateExtractionCompleted(
      TtcPageContextMonitor& monitor,
      const GURL& expected_url,
      PageContextWrapperCallbackResponse response) {
    monitor.OnPageContextExtracted(expected_url, std::move(response));
  }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
  FakeTtcPageContextMonitorDelegate delegate_;
};

// Tests that constructing `TtcPageContextMonitor` on a realized, non-loading,
// eligible `WebState` starts an initial extraction immediately and forwards the
// result to the delegate.
TEST_F(TtcPageContextMonitorTest, ExtractsOnConstructionWhenLoaded) {
  TtcPageContextMonitor monitor(web_state_, &delegate_);
  EXPECT_EQ(monitor.web_state(), &web_state_);
  EXPECT_TRUE(HasPendingWrapper(monitor));

  const GURL url("https://example.com/initial");
  SimulateExtractionCompleted(monitor, url,
                              MakePageContextResponse(url, "Initial Page"));
  EXPECT_FALSE(HasPendingWrapper(monitor));
  ASSERT_EQ(delegate_.fetched_contexts().size(), 1u);
  EXPECT_EQ(delegate_.fetched_contexts()[0].url(), url.spec());
  EXPECT_EQ(delegate_.fetched_contexts()[0].title(), "Initial Page");
}

// Tests that constructing `TtcPageContextMonitor` while the `WebState` is
// loading waits until `PageLoaded(SUCCESS)` before starting extraction.
TEST_F(TtcPageContextMonitorTest, WaitsForPageLoadedWhenInitiallyLoading) {
  web_state_.SetLoading(true);
  TtcPageContextMonitor monitor(web_state_, &delegate_);
  EXPECT_FALSE(HasPendingWrapper(monitor));

  // A failed page load must not trigger extraction and must notify the delegate
  // of the failure.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  EXPECT_FALSE(HasPendingWrapper(monitor));
  ASSERT_EQ(delegate_.fetched_errors().size(), 1u);
  EXPECT_EQ(delegate_.fetched_errors()[0],
            PageContextWrapperError::kGenericError);

  web_state_.SetLoading(false);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_TRUE(HasPendingWrapper(monitor));

  // A subsequent failed page load cancels any in-flight extraction and reports
  // `kGenericError`.
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  EXPECT_FALSE(HasPendingWrapper(monitor));
  ASSERT_EQ(delegate_.fetched_errors().size(), 2u);
  EXPECT_EQ(delegate_.fetched_errors()[1],
            PageContextWrapperError::kGenericError);
}

// Tests that a committed cross-document navigation cancels any in-flight
// extraction and discards stale responses.
TEST_F(TtcPageContextMonitorTest, CommittedNavigationCancelsPendingExtraction) {
  TtcPageContextMonitor monitor(web_state_, &delegate_);
  ASSERT_TRUE(HasPendingWrapper(monitor));

  // Uncommitted navigation must be ignored.
  web::FakeNavigationContext uncommitted_nav;
  uncommitted_nav.SetHasCommitted(false);
  web_state_.OnNavigationFinished(&uncommitted_nav);
  EXPECT_TRUE(HasPendingWrapper(monitor));

  // Committed cross-document navigation clears the wrapper.
  const GURL next_url("https://example.com/next");
  web_state_.SetCurrentURL(next_url);
  web::FakeNavigationContext committed_nav;
  committed_nav.SetHasCommitted(true);
  committed_nav.SetIsSameDocument(false);
  web_state_.OnNavigationFinished(&committed_nav);

  EXPECT_FALSE(HasPendingWrapper(monitor));

  // Stale extraction completion for the previous URL must be discarded.
  const GURL old_url("https://example.com/initial");
  SimulateExtractionCompleted(monitor, old_url,
                              MakePageContextResponse(old_url, "Stale"));
  EXPECT_TRUE(delegate_.fetched_contexts().empty());
}

// Tests that a committed same-document navigation immediately starts a new
// extraction without waiting for `PageLoaded`.
TEST_F(TtcPageContextMonitorTest, SameDocumentNavigationExtractsImmediately) {
  TtcPageContextMonitor monitor(web_state_, &delegate_);
  const GURL initial_url("https://example.com/initial");
  SimulateExtractionCompleted(monitor, initial_url,
                              MakePageContextResponse(initial_url, "Initial"));
  ASSERT_FALSE(HasPendingWrapper(monitor));
  ASSERT_EQ(delegate_.fetched_contexts().size(), 1u);

  const GURL spa_url("https://example.com/initial#section2");
  web_state_.SetCurrentURL(spa_url);

  web::FakeNavigationContext same_doc_nav;
  same_doc_nav.SetHasCommitted(true);
  same_doc_nav.SetIsSameDocument(true);
  web_state_.OnNavigationFinished(&same_doc_nav);

  EXPECT_TRUE(HasPendingWrapper(monitor));

  SimulateExtractionCompleted(monitor, spa_url,
                              MakePageContextResponse(spa_url, "SPA Section"));
  ASSERT_EQ(delegate_.fetched_contexts().size(), 2u);
  EXPECT_EQ(delegate_.fetched_contexts()[1].url(), spa_url.spec());

  // A failed same-document navigation must not trigger a new extraction.
  web::FakeNavigationContext failed_same_doc_nav;
  failed_same_doc_nav.SetHasCommitted(true);
  failed_same_doc_nav.SetIsSameDocument(true);
  failed_same_doc_nav.SetError([NSError errorWithDomain:@"test"
                                                   code:-1
                                               userInfo:nil]);
  web_state_.OnNavigationFinished(&failed_same_doc_nav);
  EXPECT_FALSE(HasPendingWrapper(monitor));
}

// Tests that ineligible pages (such as `chrome://newtab`) do not trigger
// extraction and instead report `kPageNotExtractableError` to the delegate.
TEST_F(TtcPageContextMonitorTest, SkipsIneligiblePages) {
  web_state_.SetCurrentURL(GURL("chrome://newtab"));
  TtcPageContextMonitor monitor(web_state_, &delegate_);
  EXPECT_FALSE(HasPendingWrapper(monitor));
  ASSERT_EQ(delegate_.fetched_errors().size(), 1u);
  EXPECT_EQ(delegate_.fetched_errors()[0],
            PageContextWrapperError::kPageNotExtractableError);

  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  EXPECT_FALSE(HasPendingWrapper(monitor));
  ASSERT_EQ(delegate_.fetched_errors().size(), 2u);
  EXPECT_EQ(delegate_.fetched_errors()[1],
            PageContextWrapperError::kPageNotExtractableError);
}

// Tests that `Disconnect()` stops observing the `WebState` and suppresses
// further delegate callbacks.
TEST_F(TtcPageContextMonitorTest, DisconnectStopsObservation) {
  TtcPageContextMonitor monitor(web_state_, &delegate_);
  EXPECT_TRUE(HasPendingWrapper(monitor));

  monitor.Disconnect();
  EXPECT_EQ(monitor.web_state(), nullptr);
  EXPECT_FALSE(HasPendingWrapper(monitor));

  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  EXPECT_FALSE(HasPendingWrapper(monitor));
  EXPECT_TRUE(delegate_.fetched_errors().empty());
}

// Tests that constructing `TtcPageContextMonitor` on an unrealized `WebState`
// does not start extraction.
TEST_F(TtcPageContextMonitorTest, SkipsUnrealizedWebStateOnConstruction) {
  web_state_.SetIsRealized(false);
  TtcPageContextMonitor monitor(web_state_, &delegate_);
  EXPECT_FALSE(HasPendingWrapper(monitor));
}

// Tests that a renderer termination cancels any pending extraction and reports
// `kPageNotExtractableError` to the delegate, and is ignored after
// `Disconnect()`.
TEST_F(TtcPageContextMonitorTest, RenderProcessGoneCancelsPendingExtraction) {
  TtcPageContextMonitor monitor(web_state_, &delegate_);
  EXPECT_TRUE(HasPendingWrapper(monitor));

  web_state_.OnRenderProcessGone();
  EXPECT_FALSE(HasPendingWrapper(monitor));
  ASSERT_EQ(delegate_.fetched_errors().size(), 1u);
  EXPECT_EQ(delegate_.fetched_errors()[0],
            PageContextWrapperError::kPageNotExtractableError);

  monitor.Disconnect();
  web_state_.OnRenderProcessGone();
  EXPECT_EQ(delegate_.fetched_errors().size(), 1u);
}

// Tests that destroying the monitored `WebState` cancels any pending extraction
// and clears `web_state()`.
TEST_F(TtcPageContextMonitorTest, WebStateDestroyedCancelsPendingExtraction) {
  auto transient_web_state = std::make_unique<web::FakeWebState>();
  ConfigureEligibleWebState(*transient_web_state,
                            GURL("https://example.com/transient"));

  TtcPageContextMonitor monitor(*transient_web_state, &delegate_);
  EXPECT_EQ(monitor.web_state(), transient_web_state.get());
  EXPECT_TRUE(HasPendingWrapper(monitor));

  transient_web_state.reset();
  EXPECT_EQ(monitor.web_state(), nullptr);
  EXPECT_FALSE(HasPendingWrapper(monitor));
}

// Tests that `TtcPageContextMonitor` records `kBegin` and `kEnd` journal
// entries with `actionable=true`, `result=ok`, and `size_bytes` when extraction
// succeeds.
TEST_F(TtcPageContextMonitorTest,
       LogsBeginAndEndJournalEntriesOnSuccessfulExtraction) {
  TTCJournal journal;
  const GURL url("https://example.com/initial");
  TtcPageContextMonitor monitor(web_state_, &delegate_, &journal);
  ASSERT_EQ(journal.entries().size(), 1u);

  SimulateExtractionCompleted(monitor, url,
                              MakePageContextResponse(url, "Initial Page"));

  ASSERT_EQ(journal.entries().size(), 2u);
  EXPECT_EQ(journal.entries()[0].type, TTCJournalEntryType::kBegin);
  EXPECT_EQ(journal.entries()[0].event_name, "TtcPageContextExtraction");
  EXPECT_THAT(journal.entries()[0].details,
              testing::Contains(TTCJournalDetail{"actionable", "true"}));
  EXPECT_EQ(journal.entries()[1].type, TTCJournalEntryType::kEnd);
  EXPECT_EQ(journal.entries()[1].event_name, "TtcPageContextExtraction");
  EXPECT_EQ(journal.entries()[1].summary, "Extracted APC (45 B)");
  EXPECT_THAT(journal.entries()[1].details,
              testing::IsSupersetOf({
                  TTCJournalDetail{"result", "ok"},
                  TTCJournalDetail{"size_bytes", "45"},
              }));
}

// Tests that cancelling an in-flight extraction via `Disconnect()` records a
// `kEnd` journal entry with `result=cancelled`.
TEST_F(TtcPageContextMonitorTest,
       DisconnectLogsCancelledJournalEntryForPendingExtraction) {
  TTCJournal journal;
  TtcPageContextMonitor monitor(web_state_, &delegate_, &journal);
  ASSERT_EQ(journal.entries().size(), 1u);

  monitor.Disconnect();

  ASSERT_EQ(journal.entries().size(), 2u);
  EXPECT_EQ(journal.entries()[1].type, TTCJournalEntryType::kEnd);
  EXPECT_EQ(journal.entries()[1].event_name, "TtcPageContextExtraction");
  EXPECT_THAT(journal.entries()[1].details,
              testing::Contains(TTCJournalDetail{"result", "cancelled"}));
}

// Tests that `TtcPageContextMonitor` records a `kEnd` journal entry with
// `result=stale_url` when `WebState` navigated away before extraction finished.
TEST_F(TtcPageContextMonitorTest,
       LogsStaleUrlJournalEntryWhenExtractionCompletesAfterUrlChange) {
  TTCJournal journal;
  TtcPageContextMonitor monitor(web_state_, &delegate_, &journal);
  ASSERT_EQ(journal.entries().size(), 1u);
  web_state_.SetCurrentURL(GURL("https://example.com/other"));

  SimulateExtractionCompleted(
      monitor, GURL("https://example.com/initial"),
      MakePageContextResponse(GURL("https://example.com/initial"), "Stale"));

  ASSERT_EQ(journal.entries().size(), 2u);
  EXPECT_EQ(journal.entries()[1].type, TTCJournalEntryType::kEnd);
  EXPECT_THAT(journal.entries()[1].details,
              testing::Contains(TTCJournalDetail{"result", "stale_url"}));
}

// Tests that `TtcPageContextMonitor` records a `kEnd` journal entry with
// `result=error` and `error=generic_error` when extraction fails.
TEST_F(TtcPageContextMonitorTest, LogsErrorJournalEntryWhenExtractionFails) {
  TTCJournal journal;
  TtcPageContextMonitor monitor(web_state_, &delegate_, &journal);
  ASSERT_EQ(journal.entries().size(), 1u);

  SimulateExtractionCompleted(
      monitor, GURL("https://example.com/initial"),
      base::unexpected(PageContextWrapperError::kGenericError));

  ASSERT_EQ(journal.entries().size(), 2u);
  EXPECT_EQ(journal.entries()[1].type, TTCJournalEntryType::kEnd);
  EXPECT_THAT(journal.entries()[1].details,
              testing::IsSupersetOf({
                  TTCJournalDetail{"result", "error"},
                  TTCJournalDetail{"error", "generic_error"},
              }));
}
