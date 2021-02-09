// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_XCUI_ACTIONS_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_XCUI_ACTIONS_H_

#import <Foundation/Foundation.h>

#import "ios/testing/earl_grey/earl_grey_test.h"

namespace chrome_test_util {

// Action (XCUI, hence local) to long press an item and drag it to the given
// |edge| (which probably will trigger a new window) before dropping it.
// Returns YES on success (finding the element with given
// |accessibilityIdentifier|).
BOOL LongPressAndDragToEdge(NSString* accessibilityIdentifier,
                            GREYContentEdge edge);

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_XCUI_ACTIONS_H_
