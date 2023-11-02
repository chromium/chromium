// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_CHROME_XCUI_ACTIONS_H_
#define IOS_CHROME_TEST_EARL_GREY_CHROME_XCUI_ACTIONS_H_

#import <Foundation/Foundation.h>

#import "ios/testing/earl_grey/earl_grey_test.h"

namespace chrome_test_util {

// Action (XCUI, hence local) to long press a cell item with
// `accessibility_identifier` in `window_number` and drag it to the given `edge`
// of the app screen (can trigger a new window) before dropping it. Returns YES
// on success (finding the item).
BOOL LongPressCellAndDragToEdge(NSString* accessibility_identifier,
                                GREYContentEdge edge,
                                int window_number);

// Action (XCUI, hence local) to long press a cell item  with
// `src_accessibility_identifier` in `src_window_number` and drag it to the
// given normalized offset of the cell or window with
// `dst_accessibility_identifier` in `dst_window_number` before dropping it. To
// target a window, pass nil as `dst_accessibility_identifier`. Returns YES on
// success (finding both items).
BOOL LongPressCellAndDragToOffsetOf(NSString* src_accessibility_identifier,
                                    int src_window_number,
                                    NSString* dst_accessibility_identifier,
                                    int dst_window_number,
                                    CGVector dst_normalized_offset);

// Action (XCUI, hence local) to long press a link element in a webview with
// `src_accessibility_identifier` in `src_window_number` and drag it to the
// given normalized offset of the view with  `dst_accessibility_identifier` in
// `dst_window_number` before dropping it. To target a window, pass nil as
// `dst_accessibility_identifier`. Returns YES on success (finding both items).
BOOL LongPressLinkAndDragToView(NSString* src_accessibility_identifier,
                                int src_window_number,
                                NSString* dst_accessibility_identifier,
                                int dst_window_number,
                                CGVector dst_normalized_offset);

// Convenience helper for the above function, using the default window for both
// elements, and an offset of (0.5, 0.5).
BOOL LongPressLinkAndDragToView(NSString* src_accessibility_identifier,
                                NSString* dst_accessibility_identifier);

// Action (XCUI, hence local) to resize split windows by dragging the splitter.
// This action requires two windows (`first_window_number` and
// `second_window_number`, in any order) to find where the splitter is located.
// A given `first_window_normalized_screen_size` defines the normalized size
// [0.0 - 1.0] wanted for the `first_window_number`. Returns NO if any window
// is not found or if one of them is a floating window.
// Notes: The size requested
// will be matched by the OS to the closest available multiwindow layout. This
// function works with any device oreintation and with either LTR or RTL
// languages. Example of use:
//   [ChromeEarlGrey openNewWindow];
//   [ChromeEarlGrey waitForForegroundWindowCount:2];
//   chrome_test_util::DragWindowSplitterToSize(0, 1, 0.25);
// Starting with window sizes 438 and 320 pts, this will resize
// them to 320pts and 438 pts respectively.
BOOL DragWindowSplitterToSize(int first_window_number,
                              int second_window_number,
                              CGFloat first_window_normalized_screen_size);

// Action (XCUI, hence local) to tap item with `accessibility_identifier` in
// `window_number`. Should only be used in second or third window, until a
// (already requested) fix is made to EarlGrey to allow using grey_tap()
// correctly on extra windows (right now it fails visibility check).
BOOL TapAtOffsetOf(NSString* accessibility_identifier,
                   int window_number,
                   CGVector normalized_offset);

// Action (XCUI, hence local) to type text in text field with
// `accessibility_identifier` in `window_number`. Use to replace grey_typeText
// call that are flaky. grey_typeText fails sometime during a layout change of
// the keyboard.
BOOL TypeText(NSString* accessibility_identifier,
              int window_number,
              NSString* text);

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_EARL_GREY_CHROME_XCUI_ACTIONS_H_
