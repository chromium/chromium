// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/app_bar/ui/app_bar_mutator.h"

@protocol AppBarConsumer;
class WebStateList;

// Mediator for the app bar coordinator.
@interface AppBarMediator : NSObject <AppBarMutator>

// The web state list observed by this mediator.
@property(nonatomic, assign) WebStateList* webStateList;

// The consumer of this mediator.
@property(nonatomic, weak) id<AppBarConsumer> consumer;

// Disconnects the mediator from the coordinator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_MEDIATOR_H_
