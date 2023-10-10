// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EARL_GREY_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EARL_GREY_H_

#import <Foundation/Foundation.h>

#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"

#define OmniboxEarlGrey \
  [OmniboxEarlGreyImpl invokedFromFile:@"" __FILE__ lineNumber:__LINE__]

/// Methods used for the EarlGrey tests.
@interface OmniboxEarlGreyImpl : BaseEGTestHelperImpl

/// Wait for the shortcuts backend to initialize.
- (void)waitForShortcutsBackendInitialization;

/// Wait for the number of shortcuts in the database to be equal to
/// `numberOfShortcuts`.
- (void)waitForNumberOfShortcutsInDatabase:(NSInteger)numberOfShortcuts;

@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EARL_GREY_H_
