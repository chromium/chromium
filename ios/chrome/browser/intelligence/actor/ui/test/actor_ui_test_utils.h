// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_TEST_ACTOR_UI_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_TEST_ACTOR_UI_TEST_UTILS_H_

#import <UIKit/UIKit.h>

namespace intelligence::actor {

// Performs a recursive depth-first search in `root_view` and its subview
// hierarchy, returning the first view encountered matching
// `accessibility_identifier`, or `nil` if no matching view is found.
UIView* FindViewByAccessibilityIdentifier(UIView* root_view,
                                          NSString* accessibility_identifier);

}  // namespace intelligence::actor

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_TEST_ACTOR_UI_TEST_UTILS_H_
