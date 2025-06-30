// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"
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

}  // namespace
