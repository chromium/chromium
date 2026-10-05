// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_UI_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_UI_TEST_UTIL_H_

#import <UIKit/UIKit.h>

// Returns the first view with `identifier` in the hierarchy of `view`, or `nil`
// if none is found.
UIView* FindViewWithIdentifier(UIView* view, NSString* identifier);

// Returns whether `view` has a `UILargeContentViewerInteraction`.
BOOL HasLargeContentViewerInteraction(UIView* view);

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_COMPOSEBOX_UI_TEST_UTIL_H_
