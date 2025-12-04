// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/ui/home_customization_background_cell.h"

#import "ios/chrome/browser/home_customization/coordinator/background_customization_configuration_item.h"
#import "ios/chrome/browser/ntp/search_engine_logo/mediator/search_engine_logo_mediator.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
#import "url/gurl.h"

class HomeCustomizationBackgroundCellTest : public PlatformTest {
 public:
  void SetUp() override {
    cell_ = [[HomeCustomizationBackgroundCell alloc] initWithFrame:CGRectZero];
  }
  HomeCustomizationBackgroundCell* cell_;
};

// Tests that the cell correctly omits the optional logo view from the hierarchy
// when the `SearchEngineLogoMediator` provides a nil view.
TEST_F(HomeCustomizationBackgroundCellTest, TestNoLogoInsertion) {
  BackgroundCustomizationConfigurationItem* configuration =
      [[BackgroundCustomizationConfigurationItem alloc] initWithNoBackground];

  id mockMediator = OCMClassMock([SearchEngineLogoMediator class]);

  // Capture the baseline count. This count includes all standard, persistent
  // subviews (e.g., spacer view, omniboxView, magicStackView, and feedsView).
  NSUInteger initialSubviewCount =
      cell_.innerContentView.arrangedSubviews.count;

  [cell_ configureWithBackgroundOption:configuration
              searchEngineLogoMediator:mockMediator];

  // Verify that the total number of arranged subviews has not changed.
  // This confirms that the conditional logic correctly prevented the
  // insertion of the nil logo view.
  ASSERT_EQ(cell_.innerContentView.arrangedSubviews.count, initialSubviewCount);
}

// Tests that the cell correctly inserts the logo view into the view hierarchy
// when the `SearchEngineLogoMediator` provides a non-nil view.
TEST_F(HomeCustomizationBackgroundCellTest, TestLogoInsertion) {
  BackgroundCustomizationConfigurationItem* configuration =
      [[BackgroundCustomizationConfigurationItem alloc] initWithNoBackground];

  id mockMediator = OCMClassMock([SearchEngineLogoMediator class]);
  UIView* mockLogoView = [[UIView alloc] initWithFrame:CGRectZero];

  OCMStub([mockMediator view]).andReturn(mockLogoView);
  id mockStackView = OCMPartialMock(cell_.innerContentView);

  // The specific logoView must be inserted at index 1.
  OCMExpect([mockStackView insertArrangedSubview:mockLogoView atIndex:1]);

  [cell_ configureWithBackgroundOption:configuration
              searchEngineLogoMediator:mockMediator];

  EXPECT_OCMOCK_VERIFY(mockStackView);
}
