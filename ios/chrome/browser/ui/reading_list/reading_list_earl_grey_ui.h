// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_EARL_GREY_UI_H_
#define IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_EARL_GREY_UI_H_

#import <Foundation/Foundation.h>

#import "ios/testing/earl_grey/base_eg_test_helper_impl.h"

// Public macro to invoke helper methods in test methods (Test Process). Usage
// example:
//
// @interface PageLoadTestCase : XCTestCase
// @end
// @implementation PageLoadTestCase
// - (void)testPageload {
//   [ReadingListEarlGreyUI loadURL:GURL("https://chromium.org")];
// }
//
// In this example ReadingListEarlGreyUIImpl must implement -loadURL:.
//

#define ReadingListEarlGreyUI \
  [ReadingListEarlGreyUIImpl invokedFromFile:@"" __FILE__ lineNumber:__LINE__]

// Test methods that perform actions on Reading List. These methods only affect
// Chrome using the UI with Earl Grey.
@interface ReadingListEarlGreyUIImpl : BaseEGTestHelperImpl

// Opens the reading list.
- (void)openReadingList;

@end

#endif  // IOS_CHROME_BROWSER_UI_READING_LIST_READING_LIST_EARL_GREY_UI_H_
