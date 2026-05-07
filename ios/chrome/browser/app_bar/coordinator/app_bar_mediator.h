// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/app_bar/ui/app_bar_mutator.h"

@protocol AppBarConsumer;
class AuthenticationService;
class GeminiService;
@class BrowserActionFactory;
@protocol FullscreenBrowserAgentObserving;
class FullscreenController;
@protocol FullscreenUIElement;
@class IncognitoState;
class FullscreenBrowserAgent;
class PrefService;
@protocol LensCommands;
@protocol SceneCommands;
@protocol TabGridCommands;
@protocol SettingsCommands;
@protocol BWGCommands;
@protocol FullscreenCommands;
@class TabGridState;
@protocol TabGroupsCommands;
class TemplateURLService;
class UrlLoadingBrowserAgent;
class WebStateList;

@protocol AppBarMediatorDelegate

// Indicates to the delegate to show the account menu anchored to `anchorView`.
- (void)showAccountMenu:(UIView*)anchorView;

@end

// Mediator for the App Bar coordinator.
@interface AppBarMediator : NSObject <AppBarMutator>

// The base view controller for presenting modals.
@property(nonatomic, weak) UIViewController* baseViewController;

// Handler for the scene commands.
@property(nonatomic, weak) id<SceneCommands> sceneHandler;

// Handler for the lens commands.
@property(nonatomic, weak) id<LensCommands> lensHandler;

// Handler for the tab grid commands.
@property(nonatomic, weak) id<TabGridCommands> tabGridHandler;

// The regular Tab Groups command handler.
@property(nonatomic, weak) id<TabGroupsCommands> regularTabGroupsCommands;

// The incognito Tab Groups command handler.
@property(nonatomic, weak) id<TabGroupsCommands> incognitoTabGroupsCommands;

// Handler for the settings commands.
@property(nonatomic, weak) id<SettingsCommands> settingsHandler;

// Handler for the BWG commands.
@property(nonatomic, weak) id<BWGCommands> geminiHandler;

// The regular FullscreenCommands handler.
@property(nonatomic, weak) id<FullscreenCommands> regularFullscreenHandler;

// The incognito FullscreenCommands handler.
@property(nonatomic, weak) id<FullscreenCommands> incognitoFullscreenHandler;

// The consumer of this mediator.
@property(nonatomic, weak)
    id<AppBarConsumer, FullscreenUIElement, FullscreenBrowserAgentObserving>
        consumer;

// Initializes the mediator with the two web state lists.
- (instancetype)
        initWithRegularWebStateList:(WebStateList*)regularWebStateList
              incognitoWebStateList:(WebStateList*)incognitoWebStateList
        regularFullscreenController:
            (FullscreenController*)regularFullscreenController
      incognitoFullscreenController:
          (FullscreenController*)incognitoFullscreenController
      regularFullscreenBrowserAgent:
          (FullscreenBrowserAgent*)regularFullscreenBrowserAgent
    incognitoFullscreenBrowserAgent:
        (FullscreenBrowserAgent*)incognitoFullscreenBrowserAgent
               regularActionFactory:(BrowserActionFactory*)regularActionFactory
             incognitoActionFactory:
                 (BrowserActionFactory*)incognitoActionFactory
                        prefService:(PrefService*)prefService
                 templateURLService:(TemplateURLService*)templateURLService
              authenticationService:
                  (AuthenticationService*)authenticationService
                      geminiService:(GeminiService*)geminiService
                          URLLoader:(UrlLoadingBrowserAgent*)URLLoader
                       tabGridState:(TabGridState*)tabGridState
                     incognitoState:(IncognitoState*)incognitoState;

- (instancetype)init NS_UNAVAILABLE;

// Resets the incognito web state list.
- (void)setIncognitoWebStateList:(WebStateList*)incognitoWebStateList;

// Resets the incognito fullscreen controller.
- (void)setIncognitoFullscreenController:
    (FullscreenController*)fullscreenController;

// Resets the incognito fullscreen browser agent.
- (void)setIncognitoFullscreenBrowserAgent:
    (FullscreenBrowserAgent*)fullscreenBrowserAgent;

// Resets the incognito action factory.
- (void)setIncognitoActionFactory:(BrowserActionFactory*)incognitoActionFactory;

// Disconnects the mediator from the coordinator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_APP_BAR_COORDINATOR_APP_BAR_MEDIATOR_H_
