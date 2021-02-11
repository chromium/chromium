// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_XCUI_ACTIONS_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_XCUI_ACTIONS_H_

#import <Foundation/Foundation.h>

#import "ios/testing/earl_grey/earl_grey_test.h"

namespace chrome_test_util {

// Action (XCUI, hence local) to long press a cell item with
// |accessibility_identifier| in |window_number| and drag it to the given |edge|
// of the app screen (can trigger a new window) before dropping it. Returns YES
// on success (finding the item).
BOOL LongPressCellAndDragToEdge(NSString* accessibility_identifier,
                                GREYContentEdge edge,
                                int window_number);

// Action (XCUI, hence local) to long press a cell item  with
// |src_accessibility_identifier| in |src_window_number| and drag it to the
// given normalized offset of the cell or window with
// |dst_accessibility_identifier| in |dst_window_number| before dropping it. To
// target a window, pass nil as |dst_accessibility_identifier|. Returns YES on
// success (finding both items).
BOOL LongPressCellAndDragToOffsetOf(NSString* src_accessibility_identifier,
                                    int src_window_number,
                                    NSString* dst_accessibility_identifier,
                                    int dst_window_number,
                                    CGVector dst_normalized_offset);

// Action (XCUI, hence local) to tap item with |accessibility_identifier| in
// |window_number|. Should only be used in second or third window, until a
// (already requested) fix is made to EarlGrey to allow using grey_tap()
// correctly on extra windows (right now it fails visibility check).
BOOL TapAtOffsetOf(NSString* accessibility_identifier,
                   int window_number,
                   CGVector normalized_offset);

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_XCUI_ACTIONS_H_
