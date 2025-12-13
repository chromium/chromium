// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_container_view.h"

#import "base/test/scoped_feature_list.h"
#import "ios/chrome/browser/ntp/search_engine_logo/ui/search_engine_logo_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

@interface SearchEngineLogoContainerView (ExposedForTesting)
@property(nonatomic, readonly) UIImageView* doodleLogo;
@end

#pragma mark - TextToSpeechPlayerTest

class SearchEngineLogoContainerViewTest : public PlatformTest {
 protected:
  SearchEngineLogoContainerViewTest() {
    search_engine_logo_container_view_ = [[SearchEngineLogoContainerView alloc]
        initWithFrame:CGRectMake(0, 0, 300, 300)];
    shrunk_google_logo_ = search_engine_logo_container_view_.shrunkLogoView;
    shrunk_google_logo_.image = [UIImage imageNamed:@"google_logo"];
  }

  SearchEngineLogoContainerView* search_engine_logo_container_view() {
    return search_engine_logo_container_view_;
  }

  SearchEngineLogoContainerView* search_engine_logo_container_view_;
  UIImageView* shrunk_google_logo_;
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that the container's `shrunkLogoView` is added below `doodleLogo`.
TEST_F(SearchEngineLogoContainerViewTest, SetLogoViewTest) {
  EXPECT_EQ(shrunk_google_logo_.superview, search_engine_logo_container_view());
  UIView* doodle_logo = search_engine_logo_container_view().doodleLogo;
  EXPECT_EQ(doodle_logo.superview, search_engine_logo_container_view());
  NSArray* container_subviews = search_engine_logo_container_view().subviews;
  NSInteger logo_view_index =
      [container_subviews indexOfObject:shrunk_google_logo_];
  EXPECT_NE(logo_view_index, NSNotFound);
  NSInteger doodle_logo_index = [container_subviews indexOfObject:doodle_logo];
  EXPECT_NE(doodle_logo_index, NSNotFound);
  EXPECT_LT(logo_view_index, doodle_logo_index);
}

// Verifies that `logoState` setter correctly updates the views' opacity.
TEST_F(SearchEngineLogoContainerViewTest, ShowingDoodleTest) {
  EXPECT_EQ(search_engine_logo_container_view().shrunkLogoView.alpha, 1.0);
  EXPECT_EQ(search_engine_logo_container_view().doodleLogo.alpha, 0.0);
  [search_engine_logo_container_view()
      setLogoState:SearchEngineLogoState::kDoodle
          animated:NO];
  EXPECT_EQ(search_engine_logo_container_view().shrunkLogoView.alpha, 0.0);
  EXPECT_EQ(search_engine_logo_container_view().doodleLogo.alpha, 1.0);
}

// Tests that `-setDoodleImage:animated:` updates `doodleLogo`'s image.
TEST_F(SearchEngineLogoContainerViewTest, SetDoodleImageTest) {
  UIImage* image = [[UIImage alloc] init];
  [search_engine_logo_container_view() setDoodleImage:image
                                             animated:NO
                                           animations:nil];
  EXPECT_EQ(search_engine_logo_container_view().doodleLogo.image, image);
}

// Tests that `-setAnimatedDoodleImage:animated:` updates `doodleLogo`'s image.
TEST_F(SearchEngineLogoContainerViewTest, SetAnimatedDoodleImageTest) {
  UIImage* image = [[UIImage alloc] init];
  [search_engine_logo_container_view() setAnimatedDoodleImage:image
                                                     animated:NO];
  EXPECT_EQ(search_engine_logo_container_view().doodleLogo.image, image);
}

// Tests that setting `doodleAltText` updates `doodleLogo`'s a11y label.
TEST_F(SearchEngineLogoContainerViewTest, SetDoodleAltTextTest) {
  NSString* text = @"Doodle alt text";
  search_engine_logo_container_view().doodleAltText = text;
  EXPECT_TRUE(
      search_engine_logo_container_view().doodleLogo.isAccessibilityElement);
  EXPECT_NSEQ(search_engine_logo_container_view().doodleLogo.accessibilityLabel,
              text);
}
