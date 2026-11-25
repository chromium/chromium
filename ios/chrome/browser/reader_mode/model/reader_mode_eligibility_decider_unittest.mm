// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reader_mode/model/reader_mode_eligibility_decider.h"

#import <memory>
#import <optional>

#import "base/functional/bind.h"
#import "base/location.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/test_future.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/web_distiller/core/eligibility/reader_mode_eligibility.h"
#import "components/web_distiller/core/extraction_utils.h"
#import "ios/chrome/browser/reader_mode/model/constants.h"
#import "ios/chrome/browser/reader_mode/model/reader_mode_metrics_helper.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/web_distiller/model/distiller_service_factory.h"
#import "ios/web/public/js_messaging/content_world.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace {

using web_distiller::ReaderModeEligibility;

constexpr char kArticleUrl[] = "https://example.com/article";

}  // namespace

// Tests the iOS adapter around the shared web_distiller::EligibilityDecider.
class ReaderModeEligibilityDeciderTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    // TestProfileIOS omits OptimizationGuideService by default
    // (TestingCreation::kNoServiceForTests), so the script result alone
    // decides.
    profile_ = TestProfileIOS::Builder().Build();
    web_state_.SetBrowserState(profile_.get());
    web_state_.SetContentIsHTML(true);
    metrics_helper_ = std::make_unique<ReaderModeMetricsHelper>(
        &web_state_, DistillerServiceFactory::GetForProfile(profile_.get())
                         ->GetDistilledPagePrefs());
    decider_ = std::make_unique<ReaderModeEligibilityDecider>(
        &web_state_, metrics_helper_.get());
  }

  void TearDown() override {
    decider_.reset();
    metrics_helper_.reset();
    PlatformTest::TearDown();
  }

  // Commits `url` and adds a main frame whose triggering script returns
  // `script_result`.
  void CommitUrlWithScriptResult(const GURL& url,
                                 std::unique_ptr<base::Value> script_result) {
    CommitUrl(url);
    script_result_ = std::move(script_result);
    auto main_frame = web::FakeWebFrame::CreateMainWebFrame(url);
    main_frame->AddResultForExecutedJs(
        script_result_.get(),
        base::UTF8ToUTF16(web_distiller::GetDistillerTriggeringScript()));
    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    frames_manager->AddWebFrame(std::move(main_frame));
    web_state_.SetWebFramesManager(web::ContentWorld::kIsolatedWorld,
                                   std::move(frames_manager));
  }

  void CommitUrl(const GURL& url) {
    web_state_.SetCurrentURL(url);
    decider_->SetLastCommittedUrl(url);
  }

  void WaitForDecision() {
    task_environment_.FastForwardBy(kReaderModeHeuristicPageLoadDelay);
  }

  web::WebTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
  std::unique_ptr<ReaderModeMetricsHelper> metrics_helper_;
  std::unique_ptr<ReaderModeEligibilityDecider> decider_;
  std::unique_ptr<base::Value> script_result_;
};

// Tests that restarting a pending decision records the cancelation.
TEST_F(ReaderModeEligibilityDeciderTest, RestartRecordsCanceled) {
  CommitUrl(GURL(kArticleUrl));
  decider_->StartDecision(GURL(kArticleUrl));
  histogram_tester_.ExpectTotalCount(kReaderModeStateHistogram, 0);

  decider_->StartDecision(GURL(kArticleUrl));

  histogram_tester_.ExpectUniqueSample(kReaderModeStateHistogram,
                                       ReaderModeState::kHeuristicCanceled, 1);
}

// Tests that a page failing the URL check is not distillable and records no
// heuristic result.
TEST_F(ReaderModeEligibilityDeciderTest, IneligibleUrlIsNotDistillable) {
  GURL search_url("https://www.google.com/search?q=a");
  CommitUrl(search_url);
  base::test::TestFuture<std::optional<bool>> distillable;
  decider_->FetchLastCommittedUrlDistillabilityResult(
      distillable.GetCallback());

  decider_->StartDecision(search_url);
  WaitForDecision();

  EXPECT_EQ(std::optional<bool>(false), distillable.Get());
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
}

// Tests that a not-eligible script result is recorded and makes the page not
// distillable.
TEST_F(ReaderModeEligibilityDeciderTest, ScriptNotEligible) {
  CommitUrlWithScriptResult(GURL(kArticleUrl),
                            std::make_unique<base::Value>(false));
  base::test::TestFuture<std::optional<bool>> distillable;
  decider_->FetchLastCommittedUrlDistillabilityResult(
      distillable.GetCallback());

  decider_->StartDecision(GURL(kArticleUrl));
  WaitForDecision();

  EXPECT_EQ(std::optional<bool>(false), distillable.Get());
  EXPECT_FALSE(decider_->CurrentPageIsDistillable());
  histogram_tester_.ExpectUniqueSample(
      kReaderModeHeuristicResultHistogram,
      ReaderModeEligibility::kReaderModeNotEligibleContentAndLength, 1);
}

// Tests that an eligible script result makes the page distillable.
TEST_F(ReaderModeEligibilityDeciderTest, ScriptEligibleNoOptimizationGuide) {
  CommitUrlWithScriptResult(GURL(kArticleUrl),
                            std::make_unique<base::Value>(true));
  base::test::TestFuture<std::optional<bool>> distillable;
  decider_->FetchLastCommittedUrlDistillabilityResult(
      distillable.GetCallback());

  decider_->StartDecision(GURL(kArticleUrl));
  WaitForDecision();

  EXPECT_EQ(std::optional<bool>(true), distillable.Get());
  EXPECT_TRUE(decider_->CurrentPageIsDistillable());
  histogram_tester_.ExpectUniqueSample(
      kReaderModeHeuristicResultHistogram,
      ReaderModeEligibility::kReaderModeEligible, 1);
}

// Tests that a result for a URL the web state has navigated away from while
// the script is in flight is recorded but not applied.
TEST_F(ReaderModeEligibilityDeciderTest, StaleResultIsRecordedButNotApplied) {
  CommitUrlWithScriptResult(GURL(kArticleUrl),
                            std::make_unique<base::Value>(true));
  bool callback_ran = false;
  decider_->FetchLastCommittedUrlDistillabilityResult(base::BindOnce(
      [](bool* ran, std::optional<bool>) { *ran = true; }, &callback_ran));
  decider_->StartDecision(GURL(kArticleUrl));

  // FakeWebFrame::ExecuteJavaScript posts the script result to the UI thread.
  // Queue a URL change at the same delay so it runs after `ExecuteScript`
  // posts the result, before that posted result is delivered.
  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&web::FakeWebState::SetCurrentURL,
                     base::Unretained(&web_state_),
                     GURL("https://example.com/other")),
      kReaderModeHeuristicPageLoadDelay);
  WaitForDecision();

  EXPECT_FALSE(callback_ran);
  EXPECT_FALSE(decider_->CurrentPageIsDistillable());
  histogram_tester_.ExpectUniqueSample(
      kReaderModeHeuristicResultHistogram,
      ReaderModeEligibility::kReaderModeEligible, 1);
}

// Tests that navigating away before the page-load delay elapses skips running
// the script and records nothing.
TEST_F(ReaderModeEligibilityDeciderTest, UrlChangeBeforeDelayRecordsNothing) {
  CommitUrlWithScriptResult(GURL(kArticleUrl),
                            std::make_unique<base::Value>(true));
  bool callback_ran = false;
  decider_->FetchLastCommittedUrlDistillabilityResult(base::BindOnce(
      [](bool* ran, std::optional<bool>) { *ran = true; }, &callback_ran));
  decider_->StartDecision(GURL(kArticleUrl));
  task_environment_.FastForwardBy(kReaderModeHeuristicPageLoadDelay -
                                  base::Milliseconds(1));

  web_state_.SetCurrentURL(GURL("https://example.com/other"));
  task_environment_.FastForwardBy(base::Milliseconds(1));

  EXPECT_FALSE(callback_ran);
  EXPECT_FALSE(decider_->CurrentPageIsDistillable());
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
}

// Tests that a decision without a main frame ends without a result.
TEST_F(ReaderModeEligibilityDeciderTest, NoMainFrameRecordsNothing) {
  CommitUrl(GURL(kArticleUrl));
  bool callback_ran = false;
  decider_->FetchLastCommittedUrlDistillabilityResult(base::BindOnce(
      [](bool* ran, std::optional<bool>) { *ran = true; }, &callback_ran));

  decider_->StartDecision(GURL(kArticleUrl));
  WaitForDecision();

  EXPECT_FALSE(callback_ran);
  histogram_tester_.ExpectTotalCount(kReaderModeHeuristicResultHistogram, 0);
}

// Tests that a result from the DOM features message is recorded but doesn't
// change the eligibility of the page.
TEST_F(ReaderModeEligibilityDeciderTest,
       HandleReaderModeHeuristicResultOnlyRecords) {
  CommitUrl(GURL(kArticleUrl));

  decider_->HandleReaderModeHeuristicResult(
      ReaderModeEligibility::kReaderModeEligible);

  EXPECT_FALSE(decider_->CurrentPageIsDistillable());
  histogram_tester_.ExpectUniqueSample(
      kReaderModeHeuristicResultHistogram,
      ReaderModeEligibility::kReaderModeEligible, 1);
}
