// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/parcel_tracking/parcel_tracking_opt_in_coordinator.h"

#import <Foundation/Foundation.h>

#import "base/test/metrics/histogram_tester.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/parcel_tracking/metrics.h"
#import "ios/chrome/browser/parcel_tracking/parcel_tracking_util.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Tests the ParcelTrackingOptInCoordinator.
class ParcelTrackingOptInCoordinatorTest : public PlatformTest {
 protected:
  void SetUp() override {
    browser_state_ = TestChromeBrowserState::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(browser_state_.get());
    web_state_ = std::make_unique<web::FakeWebState>();
    NSArray<CustomTextCheckingResult*>* array = [[NSArray alloc] init];
    coordinator_ = [[ParcelTrackingOptInCoordinator alloc]
        initWithBaseViewController:[[UIViewController alloc] init]
                           browser:browser_.get()
                          webState:web_state_.get()
                           parcels:array];
  }

 protected:
  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<web::FakeWebState> web_state_;
  ParcelTrackingOptInCoordinator* coordinator_;
};

// Test that the kIosParcelTrackingOptInStatus pref is properly set when the
// "always track" button is tapped.
TEST_F(ParcelTrackingOptInCoordinatorTest, AlwaysTrack) {
  base::HistogramTester histogram_tester;
  [coordinator_ alwaysTrackTapped];
  EXPECT_EQ(static_cast<int>(IOSParcelTrackingOptInStatus::kAlwaysTrack),
            browser_state_->GetPrefs()->GetInteger(
                prefs::kIosParcelTrackingOptInStatus));
  histogram_tester.ExpectUniqueSample(
      parcel_tracking::kOptInPromptActionHistogramName,
      parcel_tracking::OptInPromptActionType::kAlwaysTrack, 1);
}

// Test that the kIosParcelTrackingOptInStatus pref is properly set when the
// "no thanks" button is tapped.
TEST_F(ParcelTrackingOptInCoordinatorTest, NeverTrack) {
  base::HistogramTester histogram_tester;
  [coordinator_ noThanksTapped];
  EXPECT_EQ(static_cast<int>(IOSParcelTrackingOptInStatus::kNeverTrack),
            browser_state_->GetPrefs()->GetInteger(
                prefs::kIosParcelTrackingOptInStatus));
  histogram_tester.ExpectUniqueSample(
      parcel_tracking::kOptInPromptActionHistogramName,
      parcel_tracking::OptInPromptActionType::kNoThanks, 1);
}

// Test that the kIosParcelTrackingOptInStatus pref is properly set when the
// "ask to track" button is tapped.
TEST_F(ParcelTrackingOptInCoordinatorTest, AskToTrack) {
  base::HistogramTester histogram_tester;
  [coordinator_ askToTrackTapped];
  EXPECT_EQ(static_cast<int>(IOSParcelTrackingOptInStatus::kAskToTrack),
            browser_state_->GetPrefs()->GetInteger(
                prefs::kIosParcelTrackingOptInStatus));
  histogram_tester.ExpectUniqueSample(
      parcel_tracking::kOptInPromptActionHistogramName,
      parcel_tracking::OptInPromptActionType::kAskEveryTime, 1);
}
