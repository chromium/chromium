// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/text_field_configuration.h"

#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using TextFieldConfigurationTest = PlatformTest;

// Tests that invoking start and stop on the coordinator presents and dismisses
// the activity overlay view, respectively.
TEST_F(TextFieldConfigurationTest, Init) {
  TextFieldConfiguration* configuration =
      [[TextFieldConfiguration alloc] initWithText:@"Text"
                                       placeholder:@"Placehorder"
                           accessibilityIdentifier:@"A11y"
                                   secureTextEntry:YES];
  EXPECT_TRUE([configuration.text isEqualToString:@"Text"]);
  EXPECT_TRUE([configuration.placeholder isEqualToString:@"Placehorder"]);
  EXPECT_TRUE([configuration.accessibilityIdentifier isEqualToString:@"A11y"]);
  EXPECT_TRUE(configuration.secureTextEntry);
}
