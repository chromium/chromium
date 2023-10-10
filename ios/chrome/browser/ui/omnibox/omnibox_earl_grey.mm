// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_earl_grey.h"

#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_app_interface.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

using base::test::ios::WaitUntilConditionOrTimeout;

@implementation OmniboxEarlGreyImpl

- (void)waitForShortcutsBackendInitialization {
  NSString* errorString = [NSString
      stringWithFormat:@"Failed waiting for shortcuts backend initialization."];

  GREYCondition* waitCondition = [GREYCondition
      conditionWithName:errorString
                  block:^{
                    return [OmniboxAppInterface shortcutsBackendInitialized];
                  }];
  bool conditionMet = [waitCondition
      waitWithTimeout:base::test::ios::kWaitForFileOperationTimeout
                          .InSecondsF()];
  EG_TEST_HELPER_ASSERT_TRUE(conditionMet, errorString);
}

- (void)waitForNumberOfShortcutsInDatabase:(NSInteger)numberOfShortcuts {
  NSString* errorString =
      [NSString stringWithFormat:
                    @"Failed waiting for shortcut database size equal to %ld.",
                    numberOfShortcuts];

  GREYCondition* waitCondition = [GREYCondition
      conditionWithName:errorString
                  block:^{
                    return [OmniboxAppInterface numberOfShortcutsInDatabase] ==
                           numberOfShortcuts;
                  }];
  bool conditionMet = [waitCondition
      waitWithTimeout:base::test::ios::kWaitForFileOperationTimeout
                          .InSecondsF()];

  EG_TEST_HELPER_ASSERT_TRUE(conditionMet, errorString);
}

@end
