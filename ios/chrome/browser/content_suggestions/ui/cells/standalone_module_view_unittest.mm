// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui/cells/standalone_module_view.h"

#import "ios/chrome/browser/content_suggestions/ui/cells/standalone_module_view_config.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

// Text displayed on the `StandaloneModuleView`.
NSString* const kStandaloneModuleViewTitleText =
    @"StandaloneModuleView Title Text";
NSString* const kStandaloneModuleViewBodyText =
    @"StandaloneModuleView body text";
NSString* const kStandaloneModuleViewButtonText =
    @"StandaloneModuleView button text";

// Helper function to create and configure a `StandaloneModuleView`
// with a `StandaloneModuleViewConfiguraion` for testing.
StandaloneModuleView* GetConfiguredStandaloneModuleView() {
  StandaloneModuleView* view =
      [[StandaloneModuleView alloc] initWithFrame:CGRectZero];
  StandaloneModuleViewConfig* config =
      [[StandaloneModuleViewConfig alloc] init];
  config.titleText = kStandaloneModuleViewTitleText;
  config.bodyText = kStandaloneModuleViewBodyText;
  config.buttonText = kStandaloneModuleViewButtonText;
  config.fallbackSymbolImage = CustomSymbolWithPointSize(kDownTrendSymbol, 10);
  [view configureView:config];
  return view;
}

}  // namespace

// Testing category to expose `StandaloneModuleView` methods.
@interface StandaloneModuleView (ForTesting)

- (NSString*)titleLabelTextForTesting;
- (NSString*)descriptionLabelTextForTesting;
- (NSString*)allowLabelTextForTesting;
- (void)addConstraintsForProductImageForTesting;

@end

// Test fixture for `StandaloneModuleView`.
using StandaloneModuleViewTest = PlatformTest;

// Tests that the module view displays the correct title text.
TEST_F(StandaloneModuleViewTest, TestTitle) {
  StandaloneModuleView* view = GetConfiguredStandaloneModuleView();
  EXPECT_NSEQ(kStandaloneModuleViewTitleText, view.titleLabelTextForTesting);
}

// Tests that the module view displays the correct description text.
TEST_F(StandaloneModuleViewTest, TestDescription) {
  StandaloneModuleView* view = GetConfiguredStandaloneModuleView();
  EXPECT_NSEQ(kStandaloneModuleViewBodyText,
              view.descriptionLabelTextForTesting);
}

// Tests that the "Allow" text within the module view contains the expected
// string.
TEST_F(StandaloneModuleViewTest, TestAllow) {
  StandaloneModuleView* view = GetConfiguredStandaloneModuleView();
  EXPECT_TRUE([view.allowLabelTextForTesting
      containsString:kStandaloneModuleViewButtonText]);
}

// Tests the layout constraints related to the favicon when no product image is
// shown. This test verifies that calling
// `addConstraintsForProductImageForTesting` in this state does not cause
// issues.
TEST_F(StandaloneModuleViewTest, TestFaviconWhenNoProductImage) {
  StandaloneModuleView* view = GetConfiguredStandaloneModuleView();
  [view addConstraintsForProductImageForTesting];
}
