// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/ui/util/chrome_button.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

// Test fixture for ChromeButton.
class ChromeButtonTest : public PlatformTest {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();
    button_ = [[ChromeButton alloc] initWithStyle:ChromeButtonStylePrimary];
  }

  void TearDown() override {
    button_ = nil;
    PlatformTest::TearDown();
  }

  ChromeButton* button_;
};

// Verifies that the font getter returns nil when the
// titleTextAttributesTransformer is nil instead of crashing.
TEST_F(ChromeButtonTest, FontGetterReturnsNilWhenTransformerIsNil) {
  // Reset configuration to clear the transformer.
  button_.configuration = [UIButtonConfiguration plainButtonConfiguration];

  // Verify that it returns nil and does not crash.
  EXPECT_EQ(button_.font, nil);
}

// Verifies that the font setter correctly sets the font and creates a
// transformer when the titleTextAttributesTransformer is nil.
TEST_F(ChromeButtonTest, FontSetterSetsFontWhenTransformerIsNil) {
  // Reset configuration to clear the transformer.
  button_.configuration = [UIButtonConfiguration plainButtonConfiguration];

  UIFont* testFont = [UIFont systemFontOfSize:12];
  button_.font = testFont;

  // Verify that the font was set correctly and we can retrieve it.
  EXPECT_NE(button_.font, nil);
  EXPECT_EQ(button_.font.pointSize, testFont.pointSize);
  EXPECT_EQ(button_.font.familyName, testFont.familyName);
}

}  // namespace
