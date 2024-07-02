// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SWIPE_VIEW_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SWIPE_VIEW_H_

#import <UIKit/UIKit.h>

@interface SwipeView : UIView

// Space reserved at the top for the toolbar.
@property(nonatomic, assign) CGFloat topMargin;

- (instancetype)initWithFrame:(CGRect)frame topMargin:(CGFloat)topMargin;

- (void)setTopToolbarImage:(UIImage*)image;
- (void)setImage:(UIImage*)image;
- (void)setBottomToolbarImage:(UIImage*)image;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SWIPE_VIEW_H_
