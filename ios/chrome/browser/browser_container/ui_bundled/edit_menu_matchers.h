// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSER_CONTAINER_UI_BUNDLED_EDIT_MENU_MATCHERS_H_
#define IOS_CHROME_BROWSER_BROWSER_CONTAINER_UI_BUNDLED_EDIT_MENU_MATCHERS_H_

#import <Foundation/Foundation.h>

@protocol GREYMatcher;

// Returns a matcher to an edit menu entry.
// Edit menu must be visible when called.
// The method will return the matcher if it can be found, or nil if it cannot.
id<GREYMatcher> FindEditMenuActionWithAccessibilityLabel(
    NSString* accessibility_label);

#endif  // IOS_CHROME_BROWSER_BROWSER_CONTAINER_UI_BUNDLED_EDIT_MENU_MATCHERS_H_
