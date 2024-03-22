// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_GROUP_STROKE_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_GROUP_STROKE_VIEW_H_

#import <UIKit/UIKit.h>

// A view which represents the group stroke indicator on a tab strip cell.
// It has three components:
// 1. A straight line which extends from the left to the right anchor of the
// view.
// 2. A left path which starts at the left end.
// 3. A right path which starts at the right end.
// The view is hidden by default.
@interface TabStripGroupStrokeView : UIView

// Sets the left path of the group stroke.
- (void)setLeftPath:(CGPathRef)path;

// Sets the right path of the group stroke.
- (void)setRightPath:(CGPathRef)path;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_GROUP_STROKE_VIEW_H_
