// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_UTIL_PAGE_ANIMATION_UTIL_H_
#define IOS_CHROME_BROWSER_UI_UTIL_PAGE_ANIMATION_UTIL_H_

#import <UIKit/UIKit.h>

// Utility for handling the animation of a page closing.
namespace page_animation_util {

// Animates |view| to its final position following |delay| seconds, then calls
// the given completion block when finished.
void AnimateOutWithCompletion(UIView* view,
                              NSTimeInterval delay,
                              BOOL clockwise,
                              BOOL isPortrait,
                              void (^completion)(void));

}  // namespace page_animation_util

#endif  // IOS_CHROME_BROWSER_UI_UTIL_PAGE_ANIMATION_UTIL_H_
