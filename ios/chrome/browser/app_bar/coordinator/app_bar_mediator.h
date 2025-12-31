// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/app_bar/ui/app_bar_mutator.h"
#import "ios/chrome/browser/tab_switcher/tab_grid/base_grid/coordinator/tab_grid_observing.h"

@protocol AppBarConsumer;
class WebStateList;

// Mediator for the app bar coordinator.
@interface AppBarMediator : NSObject <AppBarMutator, TabGridObserving>

// The consumer of this mediator.
@property(nonatomic, weak) id<AppBarConsumer> consumer;

// Initializes the mediator with the two web state lists.
- (instancetype)initWithRegularWebStateList:(WebStateList*)regularWebStateList
                      incognitoWebStateList:(WebStateList*)incognitoWebStateList
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Resets the incognito web state list.
- (void)setIncognitoWebStateList:(WebStateList*)incognitoWebStateList;

// Disconnects the mediator from the coordinator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_MEDIATOR_H_
