// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/ocmock_extensions.h"
#import "ui/base/l10n/l10n_util_mac.h"

@protocol MockTarget
- (void)doSomething;
@end

namespace {

using ChromeIconTest = PlatformTest;

TEST_F(ChromeIconTest, NonNilIcons) {
  EXPECT_TRUE([ChromeIcon backIcon]);
  EXPECT_TRUE([ChromeIcon closeIcon]);
  EXPECT_TRUE([ChromeIcon infoIcon]);
  EXPECT_TRUE([ChromeIcon searchIcon]);
}

TEST_F(ChromeIconTest, Accessibility) {
  EXPECT_TRUE([ChromeIcon backIcon].accessibilityIdentifier);
  EXPECT_TRUE([ChromeIcon backIcon].accessibilityLabel);

  EXPECT_TRUE([ChromeIcon closeIcon].accessibilityIdentifier);
  EXPECT_TRUE([ChromeIcon closeIcon].accessibilityLabel);

  EXPECT_TRUE([ChromeIcon infoIcon].accessibilityIdentifier);
  EXPECT_TRUE([ChromeIcon infoIcon].accessibilityLabel);

  EXPECT_TRUE([ChromeIcon searchIcon].accessibilityIdentifier);
  EXPECT_TRUE([ChromeIcon searchIcon].accessibilityLabel);
}

TEST_F(ChromeIconTest, RTL) {
  EXPECT_TRUE([ChromeIcon backIcon].flipsForRightToLeftLayoutDirection);
  EXPECT_FALSE([ChromeIcon searchIcon].flipsForRightToLeftLayoutDirection);
}

TEST_F(ChromeIconTest, TemplateBarButtonItem) {
  UIImage* image = [UIImage imageNamed:@"ic_close"];
  image.accessibilityIdentifier = @"identifier";
  image.accessibilityLabel = @"label";
  id mockTarget = [OCMockObject mockForProtocol:@protocol(MockTarget)];
  [[mockTarget expect] doSomething];

  UIBarButtonItem* barButtonItem =
      [ChromeIcon templateBarButtonItemWithImage:image
                                          target:mockTarget
                                          action:@selector(doSomething)];

  EXPECT_NSEQ(@"identifier", barButtonItem.accessibilityIdentifier);
  EXPECT_NSEQ(@"label", barButtonItem.accessibilityLabel);
  EXPECT_EQ(image.size.width, barButtonItem.image.size.width);
  EXPECT_EQ(image.size.height, barButtonItem.image.size.height);
  EXPECT_EQ(image.scale, barButtonItem.image.scale);
  EXPECT_EQ(image.capInsets.top, barButtonItem.image.capInsets.top);
  EXPECT_EQ(image.capInsets.left, barButtonItem.image.capInsets.left);
  EXPECT_EQ(image.capInsets.bottom, barButtonItem.image.capInsets.bottom);
  EXPECT_EQ(image.capInsets.right, barButtonItem.image.capInsets.right);
  EXPECT_EQ(image.flipsForRightToLeftLayoutDirection,
            barButtonItem.image.flipsForRightToLeftLayoutDirection);
  EXPECT_EQ(UIImageRenderingModeAlwaysTemplate,
            barButtonItem.image.renderingMode);
}

}  // namespace
