// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/elements/text_field_configuration.h"

#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using TextFieldConfigurationTest = PlatformTest;

// Tests that invoking start and stop on the coordinator presents and dismisses
// the activity overlay view, respectively.
TEST_F(TextFieldConfigurationTest, Init) {
  TextFieldConfiguration* configuration = [[TextFieldConfiguration alloc]
                 initWithText:@"Text"
                  placeholder:@"Placehorder"
      accessibilityIdentifier:@"A11y"
       autocapitalizationType:UITextAutocapitalizationTypeNone
              secureTextEntry:YES];
  EXPECT_TRUE([configuration.text isEqualToString:@"Text"]);
  EXPECT_TRUE([configuration.placeholder isEqualToString:@"Placehorder"]);
  EXPECT_TRUE([configuration.accessibilityIdentifier isEqualToString:@"A11y"]);
  EXPECT_EQ(UITextAutocapitalizationTypeNone,
            configuration.autocapitalizationType);
  EXPECT_TRUE(configuration.secureTextEntry);
}
