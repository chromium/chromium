// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tab_helper_util.h"

#import "ios/chrome/browser/infobars/model/infobar_badge_tab_helper.h"
#import "ios/chrome/browser/lens/model/lens_tab_helper.h"
#import "ios/chrome/browser/overlays/model/public/overlay_modality.h"
#import "ios/chrome/browser/overlays/model/public/overlay_request_queue.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/test/ios_chrome_scoped_testing_local_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/platform_test.h"

// Test fixture for AttachTabHelpers.
class TabHelperUtilTest : public PlatformTest,
                          public testing::WithParamInterface<TabHelperFilter> {
 public:
  // Returns the string version of the TabHelperFilter test parameter.
  static std::string TabHelperFilterToString(
      testing::TestParamInfo<TabHelperFilter> info) {
    switch (info.param) {
      case TabHelperFilter::kEmpty:
        return "Empty";
      case TabHelperFilter::kPrerender:
        return "Prerender";
      case TabHelperFilter::kReaderMode:
        return "ReaderMode";
      case TabHelperFilter::kLensOverlay:
        return "LensOverlay";
    }
  }

 protected:
  TabHelperUtilTest() {
    profile_ = TestProfileIOS::Builder().Build();

    web_state_.SetBrowserState(profile_.get());
    web_state_.SetWebFramesManager(
        web::ContentWorld::kPageContentWorld,
        std::make_unique<web::FakeWebFramesManager>());
    web_state_.SetWebFramesManager(
        web::ContentWorld::kIsolatedWorld,
        std::make_unique<web::FakeWebFramesManager>());
    web_state_.SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());
  }

  TabHelperFilter tab_helper_filter() { return GetParam(); }

  web::WebTaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  web::FakeWebState web_state_;
  IOSChromeScopedTestingLocalState scoped_testing_local_state_;
};

// Validate that OverlayRequestQueue is created for all web states.
// This test case demonstrates the default tab helper creation state,
// which is to create and attach itself to all web states.
TEST_P(TabHelperUtilTest, OverlayRequestQueue) {
  AttachTabHelpers(&web_state_, tab_helper_filter());

  EXPECT_TRUE(OverlayRequestQueue::FromWebState(
      &web_state_, OverlayModality::kWebContentArea));
}

// Validate that LensTabHelper is created when there is no filter.
// This test case demonstrates the creation of a tab helper when one
// or more filters result in the suppression of the tab helper.
TEST_P(TabHelperUtilTest, LensTabHelper) {
  AttachTabHelpers(&web_state_, tab_helper_filter());

  switch (tab_helper_filter()) {
    case TabHelperFilter::kEmpty:
      ASSERT_TRUE(LensTabHelper::FromWebState(&web_state_));
      break;
    case TabHelperFilter::kPrerender:
    case TabHelperFilter::kReaderMode:
    case TabHelperFilter::kLensOverlay:
      ASSERT_FALSE(LensTabHelper::FromWebState(&web_state_));
      break;
  }
}

// Validate that InfobarBadgeTabHelper is created for tab helper setup with
// standard navigation filter, which does not include special web states for
// Reader Mode or Lens features.
// This is a regression test for crbug.com/465908300.
TEST_P(TabHelperUtilTest, InfobarBadgeTabHelper) {
  AttachTabHelpers(&web_state_, tab_helper_filter());

  InfobarBadgeTabHelper* tab_helper = static_cast<InfobarBadgeTabHelper*>(
      web_state_.GetUserData(InfobarBadgeTabHelper::UserDataKey()));

  switch (tab_helper_filter()) {
    case TabHelperFilter::kEmpty:
    case TabHelperFilter::kPrerender: {
      ASSERT_TRUE(tab_helper);
    } break;
    case TabHelperFilter::kReaderMode:
    case TabHelperFilter::kLensOverlay: {
      ASSERT_FALSE(tab_helper);
    } break;
  }
}

INSTANTIATE_TEST_SUITE_P(,
                         TabHelperUtilTest,
                         testing::Values(TabHelperFilter::kEmpty,
                                         TabHelperFilter::kPrerender,
                                         TabHelperFilter::kLensOverlay,
                                         TabHelperFilter::kReaderMode),
                         TabHelperUtilTest::TabHelperFilterToString);
