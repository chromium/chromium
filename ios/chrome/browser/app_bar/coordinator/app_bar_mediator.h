// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/app_bar/ui/app_bar_mutator.h"

@protocol AppBarConsumer;
@class IncognitoState;
class PrefService;
@protocol SceneCommands;
@protocol TabGridCommands;
@protocol TabGroupsCommands;
@class TabGridState;
class UrlLoadingBrowserAgent;
class WebStateList;

// Mediator for the app bar coordinator.
@interface AppBarMediator : NSObject <AppBarMutator>

// Handler for the scene commands.
@property(nonatomic, weak) id<SceneCommands> sceneHandler;

// Handler for the scene commands.
@property(nonatomic, weak) id<TabGridCommands> tabGridHandler;

// The regular Tab Groups command handler.
@property(nonatomic, weak) id<TabGroupsCommands> regularTabGroupsCommands;

// The consumer of this mediator.
@property(nonatomic, weak) id<AppBarConsumer> consumer;

// Initializes the mediator with the two web state lists.
- (instancetype)initWithRegularWebStateList:(WebStateList*)regularWebStateList
                      incognitoWebStateList:(WebStateList*)incognitoWebStateList
                                prefService:(PrefService*)prefService
                                  URLLoader:(UrlLoadingBrowserAgent*)URLLoader
                               tabGridState:(TabGridState*)tabGridState
                             incognitoState:(IncognitoState*)incognitoState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Resets the incognito web state list.
- (void)setIncognitoWebStateList:(WebStateList*)incognitoWebStateList;

// Disconnects the mediator from the coordinator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_MEDIATOR_H_
