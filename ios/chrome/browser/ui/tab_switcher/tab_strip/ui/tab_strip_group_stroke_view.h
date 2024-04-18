// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_GROUP_STROKE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_GROUP_STROKE_VIEW_H_

#import <UIKit/UIKit.h>

// A view which represents the group stroke indicator on a tab strip cell.
// It has three components:
// 1. A straight line which extends from the leading to the trailing anchor of
// the view.
// 2. A leading path which starts at the leading end.
// 3. A trailing path which starts at the trailing end.
// The view is hidden by default.
@interface TabStripGroupStrokeView : UIView

// Sets the leading path of the group stroke.
- (void)setLeadingPath:(CGPathRef)path;

// Sets the trailing path of the group stroke.
- (void)setTrailingPath:(CGPathRef)path;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_GROUP_STROKE_VIEW_H_
