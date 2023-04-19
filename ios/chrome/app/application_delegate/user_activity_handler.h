// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_USER_ACTIVITY_HANDLER_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_USER_ACTIVITY_HANDLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/app/application_delegate/app_state_observer.h"

@protocol ConnectionInformation;
class ChromeBrowserState;
class PrefService;
@protocol StartupInformation;
@protocol TabOpening;

// TODO(crbug.com/619598): When the refactoring is over, check if it can be
// merged with StartupInformation.
// Handles all events based on user activity, as defined in
// UIApplicationDelegate.
@interface UserActivityHandler : NSObject

// If the userActivity is a Handoff or an opening from Spotlight, opens a new
// tab or setup startupParameters to open it later. If a new tab must be
// opened immediately (e.g. if a Siri Shortcut was triggered by the user while
// Chrome was already in the foreground), it will be done with the provided
// `browserState`. Returns wether it could continue userActivity.
+ (BOOL)continueUserActivity:(NSUserActivity*)userActivity
         applicationIsActive:(BOOL)applicationIsActive
                   tabOpener:(id<TabOpening>)tabOpener
       connectionInformation:(id<ConnectionInformation>)connectionInformation
          startupInformation:(id<StartupInformation>)startupInformation
                browserState:(ChromeBrowserState*)browserState
                   initStage:(InitStage)initStage;

// Handles the 3D touch application static items. If the First Run UI is active,
// `completionHandler` will be called with NO.
+ (void)performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
                   completionHandler:(void (^)(BOOL succeeded))completionHandler
                           tabOpener:(id<TabOpening>)tabOpener
               connectionInformation:
                   (id<ConnectionInformation>)connectionInformation
                  startupInformation:(id<StartupInformation>)startupInformation
                        browserState:(ChromeBrowserState*)browserState
                           initStage:(InitStage)initStage;

// Returns YES if Chrome is passing a Handoff to itself or if it is an opening
// from Spotlight.
+ (BOOL)willContinueUserActivityWithType:(NSString*)userActivityType;

// Opens a new Tab or routes to correct Tab.
+ (void)handleStartupParametersWithTabOpener:(id<TabOpening>)tabOpener
                       connectionInformation:
                           (id<ConnectionInformation>)connectionInformation
                          startupInformation:
                              (id<StartupInformation>)startupInformation
                                browserState:(ChromeBrowserState*)browserState
                                   initStage:(InitStage)initStage;

// Return YES if the user intends to open links in a certain mode and the
// browser will proceed the request.
+ (BOOL)canProceedWithUserActivity:(NSUserActivity*)userActivity
                       prefService:(PrefService*)prefService;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_USER_ACTIVITY_HANDLER_H_
