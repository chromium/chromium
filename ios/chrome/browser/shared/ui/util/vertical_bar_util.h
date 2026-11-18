// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_VERTICAL_BAR_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_VERTICAL_BAR_UTIL_H_

#import <UIKit/UIKit.h>

// Helpers for the system vertical bar (iOS 27.1+, iPhone Duo folded posture).
// Builds against an older SDK compile the vertical bar out, so callers that
// use the iOS 27.1 types must guard with
// `#if defined(__IPHONE_27_1) && __IPHONE_OS_VERSION_MAX_ALLOWED >=
// __IPHONE_27_1`.

#if defined(__IPHONE_27_1) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_27_1
// Returns the vertical bar behavior a window root prefers for `traits`. On
// iPhone Duo the bar is kept for horizontally compact layouts (folded) and
// disabled for regular widths (unfolded, where Chrome uses horizontal
// toolbars). Other devices keep the system default.
UIVerticalBarBehavior PreferredVerticalBarBehavior(UITraitCollection* traits)
    API_AVAILABLE(ios(27.1));
#endif

// Returns whether the system vertical bar column is active for `traits`.
bool IsVerticalBarActive(UITraitCollection* traits);

// Asks UIKit to re-query `view_controller`'s vertical bar preference after a
// change that affects it. No-op when the vertical bar API is unavailable.
void UpdateVerticalBarConfiguration(UIViewController* view_controller);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_VERTICAL_BAR_UTIL_H_
