// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/components/enterprise/data_controls/features.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

@interface DataControlsTestCase : ChromeTestCase
@end

@implementation DataControlsTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(
      data_controls::kEnableClipboardDataControlsIOS);
  return config;
}

- (void)setUp {
  [super setUp];
}

- (void)tearDownHelper {
  [super tearDownHelper];
}

// Tests that copy is blocked when a "BLOCK" rule matches the page URL.
- (void)DISABLED_testCopyBlocked {
  // TODO(crbug.com/457472925): This is a placeholder, update the tests.
}

@end
