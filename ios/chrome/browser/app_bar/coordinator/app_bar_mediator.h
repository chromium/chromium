// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/app_bar/ui/app_bar_mutator.h"

@protocol AppBarConsumer;
class AuthenticationService;
class BwgService;
class ChromeAccountManagerService;
@class BrowserActionFactory;
class FullscreenController;
@protocol FullscreenUIElement;
@class IncognitoState;
namespace signin {
class IdentityManager;
}  // namespace signin
class PrefService;
@protocol SceneCommands;
@protocol TabGridCommands;
@class TabGridState;
@protocol TabGroupsCommands;
class TemplateURLService;
class UrlLoadingBrowserAgent;
class WebStateList;

// Mediator for the App Bar coordinator.
@interface AppBarMediator : NSObject <AppBarMutator>

// Handler for the scene commands.
@property(nonatomic, weak) id<SceneCommands> sceneHandler;

// Handler for the scene commands.
@property(nonatomic, weak) id<TabGridCommands> tabGridHandler;

// The regular Tab Groups command handler.
@property(nonatomic, weak) id<TabGroupsCommands> regularTabGroupsCommands;

// The consumer of this mediator.
@property(nonatomic, weak) id<AppBarConsumer, FullscreenUIElement> consumer;

// The regular actions factory.
@property(nonatomic, strong) BrowserActionFactory* regularActionFactory;

// The incognito actions factory.
@property(nonatomic, strong) BrowserActionFactory* incognitoActionFactory;

// Initializes the mediator with the two web state lists.
- (instancetype)
      initWithRegularWebStateList:(WebStateList*)regularWebStateList
            incognitoWebStateList:(WebStateList*)incognitoWebStateList
      regularFullscreenController:
          (FullscreenController*)regularFullscreenController
    incognitoFullscreenController:
        (FullscreenController*)incognitoFullscreenController
                      prefService:(PrefService*)prefService
               templateURLService:(TemplateURLService*)templateURLService
            authenticationService:(AuthenticationService*)authenticationService
                    geminiService:(BwgService*)geminiService
            accountManagerService:
                (ChromeAccountManagerService*)accountManagerService
                  identityManager:(signin::IdentityManager*)identityManager
                        URLLoader:(UrlLoadingBrowserAgent*)URLLoader
                     tabGridState:(TabGridState*)tabGridState
                   incognitoState:(IncognitoState*)incognitoState;

- (instancetype)init NS_UNAVAILABLE;

// Resets the incognito web state list.
- (void)setIncognitoWebStateList:(WebStateList*)incognitoWebStateList;

// Resets the incognito fullscreen controller.
- (void)setIncognitoFullscreenController:
    (FullscreenController*)fullscreenController;

// Disconnects the mediator from the coordinator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_MEDIATOR_H_
