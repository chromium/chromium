// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/load_timing_tab_helper.h"

#include <memory>
#include <vector>

#include "base/test/metrics/histogram_tester.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
