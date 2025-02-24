// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_mutator.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_navigation_delegate.h"
#import "ios/chrome/browser/side_swipe/ui_bundled/side_swipe_tab_delegate.h"

@protocol HelpCommands;
@protocol SideSwipeConsumer;

class WebStateList;

namespace feature_engagement {
class Tracker;
}  // namespace feature_engagement

// Controls how an edge gesture is processed, either as tab change or a page
// change.  For tab changes two full screen CardSideSwipeView views are dragged
// across the screen. For page changes the SideSwipeMediatorDelegate
// `contentView` is moved across the screen and a SideSwipeNavigationView is
// shown in the remaining space.
@interface SideSwipeMediator : NSObject <SideSwipeMutator,
                                         SideSwipeNavigationDelegate,
                                         SideSwipeTabDelegate>

@property(nonatomic) feature_engagement::Tracker* engagementTracker;

// Handler for in-product help tips.
@property(nonatomic, weak) id<HelpCommands> helpHandler;

// The side swipe consumer. It will mainly receive webstate updates.
@property(nonatomic, weak) id<SideSwipeConsumer> consumer;

// Initializer.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SIDE_SWIPE_UI_BUNDLED_SIDE_SWIPE_MEDIATOR_H_
