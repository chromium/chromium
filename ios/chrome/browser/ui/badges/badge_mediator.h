// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/badges/badge_delegate.h"

@protocol BadgeConsumer;
@protocol BadgeItem;
class Browser;
@protocol BrowserCoordinatorCommands;
@protocol InfobarCommands;

// A mediator object that updates the consumer when the state of badges changes.
@interface BadgeMediator : NSObject <BadgeDelegate>

- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Stops observing all objects.
- (void)disconnect;

// The dispatcher for badge related actions.
@property(nonatomic, weak) id<InfobarCommands, BrowserCoordinatorCommands>
    dispatcher;

// The consumer being set up by this mediator.  Setting to a new value updates
// the new consumer.
@property(nonatomic, weak) id<BadgeConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_MEDIATOR_H_
