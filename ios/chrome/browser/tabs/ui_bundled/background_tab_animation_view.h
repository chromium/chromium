// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_UI_BUNDLED_BACKGROUND_TAB_ANIMATION_VIEW_H_
#define IOS_CHROME_BROWSER_TABS_UI_BUNDLED_BACKGROUND_TAB_ANIMATION_VIEW_H_

#import <UIKit/UIKit.h>

@class LayoutGuideCenter;

// View containing a link image and a shadow. Used to notify the user that a new
// tab has been opened in background.
@interface BackgroundTabAnimationView : UIView

// The layout guide center to use to refer to the tab grid button.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

- (instancetype)initWithFrame:(CGRect)frame
                    incognito:(BOOL)incognito NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Starts an Open In New Tab animation in the superview, from `originPoint` with
// a `completion` block. `originPoint` should be in window coordinates.
// Internally, kTabSwitcherGuide is used to determine the location of the tab
// grid button.
- (void)animateFrom:(CGPoint)originPoint
    toTabGridButtonWithCompletion:(void (^)())completion;

@end

#endif  // IOS_CHROME_BROWSER_TABS_UI_BUNDLED_BACKGROUND_TAB_ANIMATION_VIEW_H_
