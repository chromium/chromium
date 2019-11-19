// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/voice/voice_search_navigations_tab_helper.h"

#include "base/test/scoped_feature_list.h"
#include "ios/web/common/features.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// VoiceSearchNavigationsTest is parameterized on this enum to test both
// LegacyNavigationManager and WKBasedNavigationManager.
enum class NavigationManagerChoice {
  LEGACY,
  WK_BASED,
};

// Test fixture for VoiceSearchNavigations.
class VoiceSearchNavigationsTest
    : public web::WebTestWithWebState,
      public ::testing::WithParamInterface<NavigationManagerChoice> {
 public:
  void SetUp() override {
    if (GetParam() == NavigationManagerChoice::LEGACY) {
      scoped_feature_list_.InitAndDisableFeature(
          web::features::kSlimNavigationManager);
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          web::features::kSlimNavigationManager);
    }
    web::WebTestWithWebState::SetUp();
    VoiceSearchNavigationTabHelper::CreateForWebState(web_state());
  }

  VoiceSearchNavigationTabHelper* navigations() {
    return VoiceSearchNavigationTabHelper::FromWebState(web_state());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that a navigation commit reset the value of
// IsExpectingVoiceSearch().
TEST_P(VoiceSearchNavigationsTest, CommitResetVoiceSearchExpectation) {
  navigations()->WillLoadVoiceSearchResult();
  EXPECT_TRUE(navigations()->IsExpectingVoiceSearch());
  LoadHtml(@"<html></html>");
  EXPECT_FALSE(navigations()->IsExpectingVoiceSearch());
}

INSTANTIATE_TEST_SUITE_P(ProgrammaticVoiceSearchNavigationsTest,
                         VoiceSearchNavigationsTest,
                         ::testing::Values(NavigationManagerChoice::LEGACY,
                                           NavigationManagerChoice::WK_BASED));
