// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/load_timing_tab_helper.h"

#import <memory>
#import <vector>

#import "base/test/metrics/histogram_tester.h"
#import "base/threading/platform_thread.h"
#import "base/time/time.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

class LoadTimingTabHelperTest : public PlatformTest {
 protected:
  LoadTimingTabHelperTest() {
    LoadTimingTabHelper::CreateForWebState(&web_state_);
  }

  LoadTimingTabHelper* tab_helper() {
    return LoadTimingTabHelper::FromWebState(&web_state_);
  }

  void ExpectEmptyHistogram() {
    EXPECT_TRUE(histogram_tester_
                    .GetTotalCountsForPrefix(
                        LoadTimingTabHelper::kOmnibarToPageLoadedMetric)
                    .empty());
  }

  web::FakeWebState web_state_;
  base::HistogramTester histogram_tester_;
};

TEST_F(LoadTimingTabHelperTest, ReportMetricOnLoadSuccess) {
  tab_helper()->DidInitiatePageLoad();
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);

  histogram_tester_.ExpectTimeBucketCount(
      LoadTimingTabHelper::kOmnibarToPageLoadedMetric, base::TimeDelta(), 1);
}

TEST_F(LoadTimingTabHelperTest, MetricNotReportedOnLoadFailure) {
  tab_helper()->DidInitiatePageLoad();
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::FAILURE);
  ExpectEmptyHistogram();
}

TEST_F(LoadTimingTabHelperTest, MetricNotReportedIfTimerNotStarted) {
  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  ExpectEmptyHistogram();
}

TEST_F(LoadTimingTabHelperTest, ReportZeroIfPrerenderPromotedAfterPageLoaded) {
  ASSERT_FALSE(web_state_.IsLoading());
  tab_helper()->DidPromotePrerenderTab();
  histogram_tester_.ExpectTimeBucketCount(
      LoadTimingTabHelper::kOmnibarToPageLoadedMetric, base::TimeDelta(), 1);
}

// Tests that if a prerender is promoted before the load finishes, instead of
// immediately reporting 0, the timer is reset and reported later.
TEST_F(LoadTimingTabHelperTest,
       RestartTimerIfPrerenderPromotedBeforePageLoaded) {
  ASSERT_FALSE(web_state_.IsLoading());
  web_state_.SetLoading(true);

  tab_helper()->DidPromotePrerenderTab();
  ExpectEmptyHistogram();

  web_state_.OnPageLoaded(web::PageLoadCompletionStatus::SUCCESS);
  histogram_tester_.ExpectTimeBucketCount(
      LoadTimingTabHelper::kOmnibarToPageLoadedMetric, base::TimeDelta(), 1);
}
