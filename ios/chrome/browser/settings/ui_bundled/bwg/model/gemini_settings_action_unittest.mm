// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/bwg/model/gemini_settings_action.h"

#import "ios/chrome/browser/settings/ui_bundled/bwg/model/gemini_settings_action_type.h"
#import "testing/platform_test.h"

namespace {
const char kTestURL[] = "http://www.example.com/";
}  // namespace

using GeminiSettingsActionTest = PlatformTest;

// Tests configuration validity for URL type with valid parameters.
TEST_F(GeminiSettingsActionTest, CheckValidURLConfigurationIsValid) {
  NSURL* test_url = [NSURL URLWithString:@(kTestURL)];

  GeminiSettingsAction* action =
      [[GeminiSettingsAction alloc] initWithType:GeminiSettingsActionTypeURL
                                             URL:test_url
                                  viewController:nil];

  EXPECT_NE(nil, action);
}

// Tests configuration validity for ViewController type with valid parameters.
TEST_F(GeminiSettingsActionTest, CheckValidViewControllerConfigurationIsValid) {
  UIViewController* view_controller = [[UIViewController alloc] init];

  GeminiSettingsAction* action = [[GeminiSettingsAction alloc]
        initWithType:GeminiSettingsActionTypeViewController
                 URL:nil
      viewController:view_controller];
  EXPECT_NE(nil, action);
}

// Tests configuration validity for Unknown type with valid parameters.
TEST_F(GeminiSettingsActionTest, CheckValidUnknownConfigurationIsValid) {
  GeminiSettingsAction* action =
      [[GeminiSettingsAction alloc] initWithType:GeminiSettingsActionTypeUnknown
                                             URL:nil
                                  viewController:nil];
  EXPECT_NE(nil, action);
}

// Tests configuration validity for ViewController type with missing
// ViewController parameter.
TEST_F(GeminiSettingsActionTest,
       CheckInvalidViewControllerConfigurationMissingViewController) {
  GeminiSettingsAction* action = [[GeminiSettingsAction alloc]
        initWithType:GeminiSettingsActionTypeViewController
                 URL:nil
      viewController:nil];
  EXPECT_EQ(nil, action);
}

// Tests configuration validity for ViewController type with redundant URL
// parameter.
TEST_F(GeminiSettingsActionTest,
       CheckInvalidViewControllerConfigurationRedundantURL) {
  NSURL* test_url = [NSURL URLWithString:@(kTestURL)];
  UIViewController* view_controller = [[UIViewController alloc] init];

  GeminiSettingsAction* action = [[GeminiSettingsAction alloc]
        initWithType:GeminiSettingsActionTypeViewController
                 URL:test_url
      viewController:view_controller];
  EXPECT_EQ(nil, action);
}

// Tests configuration validity for URL type with missing URL parameter.
TEST_F(GeminiSettingsActionTest, CheckInvalidURLConfigurationMissingURL) {
  GeminiSettingsAction* action =
      [[GeminiSettingsAction alloc] initWithType:GeminiSettingsActionTypeURL
                                             URL:nil
                                  viewController:nil];
  EXPECT_EQ(nil, action);
}

// Tests configuration validity for URL type with redundant ViewController
// parameter.
TEST_F(GeminiSettingsActionTest,
       CheckInvalidURLConfigurationRedundantViewController) {
  NSURL* test_url = [NSURL URLWithString:@(kTestURL)];
  UIViewController* view_controller = [[UIViewController alloc] init];

  GeminiSettingsAction* action =
      [[GeminiSettingsAction alloc] initWithType:GeminiSettingsActionTypeURL
                                             URL:test_url
                                  viewController:view_controller];
  EXPECT_EQ(nil, action);
}
