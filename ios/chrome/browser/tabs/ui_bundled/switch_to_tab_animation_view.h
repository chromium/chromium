// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_UI_BUNDLED_SWITCH_TO_TAB_ANIMATION_VIEW_H_
#define IOS_CHROME_BROWSER_TABS_UI_BUNDLED_SWITCH_TO_TAB_ANIMATION_VIEW_H_

#import <UIKit/UIKit.h>

// Position of the tab the SwitchToTabAnimationView will switch to, relatively
// to the currently active tab.
typedef NS_ENUM(NSInteger, SwitchToTabAnimationPosition) {
  SwitchToTabAnimationPositionBefore,
  SwitchToTabAnimationPositionAfter,
};

// View to manage the animation when switching tabs.
@interface SwitchToTabAnimationView : UIView

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Starts the animation between the `currentView`, to the `newView` which has a
// `position` relatively to the `currentView`. At the end of the animation, this
// view is removing itself from its parent.
- (void)animateFromCurrentView:(UIView*)currentView
                     toNewView:(UIView*)newView
                    inPosition:(SwitchToTabAnimationPosition)position;

@end

#endif  // IOS_CHROME_BROWSER_TABS_UI_BUNDLED_SWITCH_TO_TAB_ANIMATION_VIEW_H_
