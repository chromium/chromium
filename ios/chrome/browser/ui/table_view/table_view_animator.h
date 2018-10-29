// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_ANIMATOR_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_ANIMATOR_H_

#import <UIKit/UIKit.h>

namespace {

// Identifies from which edge of the screen the transition should start.
typedef NS_ENUM(NSInteger, TableAnimatorDirection) {
  // The animation will be from the Trailing edge to the Leading one.
  TableAnimatorDirectionFromTrailing = 0,
  // The animation will be from the Leading edge to the Trailing one.
  TableAnimatorDirectionFromLeading
};

}  // namespace

// TableViewAnimator implements an animation that slides the presented view in
// from the trailing edge of the screen.
@interface TableViewAnimator : NSObject<UIViewControllerAnimatedTransitioning>

// YES if this animator is presenting a view controller, NO if it is dismissing
// one.
@property(nonatomic, assign) BOOL presenting;

// Direction the animation. Default is TableAnimatorDirectionFromTrailing.
@property(nonatomic, assign) TableAnimatorDirection direction;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_VIEW_ANIMATOR_H_
