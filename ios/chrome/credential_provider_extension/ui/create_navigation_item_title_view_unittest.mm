// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/create_navigation_item_title_view.h"

#import "ios/chrome/credential_provider_extension/font_provider.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace credential_provider_extension {

class CreateNavigationItemTitleViewTest : public PlatformTest {
 public:
  void SetUp() override;
};

void CreateNavigationItemTitleViewTest::SetUp() {
  if (@available(iOS 17.0, *)) {
  } else {
    GTEST_SKIP() << "Does not apply on iOS 16 and below";
  }
}

// Tests creating a navigation item title view works.
TEST_F(CreateNavigationItemTitleViewTest, Creation) {
  UIFont* font =
      ios::provider::GetBrandedProductRegularFont(UIFont.labelFontSize);
  ASSERT_NSNE(font, nil);

  UIView* view = CreateNavigationItemTitleView(font);
  ASSERT_NSNE(view, nil);
}

}  // namespace credential_provider_extension
