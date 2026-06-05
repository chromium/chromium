// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_TOOLBAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_TOOLBAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/toolbar/legacy/ui_bundled/banner_promo_view.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_mutator.h"

@class BrowserActionFactory;
@class DefaultBrowserBannerPromoAppAgent;
@protocol FullscreenCommands;
class FullscreenController;
@protocol SettingsCommands;
@protocol SceneCommands;
@class UIViewController;
@protocol ToolbarConsumer;
@protocol ToolbarHeightDelegate;
class PrefService;
class WebNavigationBrowserAgent;
namespace web {
class WebState;
}  // namespace web
class WebStateList;
class TabBasedIPHBrowserAgent;

class AuthenticationService;
@protocol BWGCommands;
class GeminiBrowserAgent;
class GeminiService;

// Mediator for the toolbar.
@interface ToolbarMediator : NSObject <BannerPromoViewDelegate, ToolbarMutator>

// The consumer for this mediator.
@property(nonatomic, weak) id<ToolbarConsumer> consumer;

// Helper for web navigation.
@property(nonatomic, assign) WebNavigationBrowserAgent* navigationBrowserAgent;

// Helper for tab-based IPH.
@property(nonatomic, assign) TabBasedIPHBrowserAgent* tabBasedIPHAgent;

// Delegate that handles the toolbars height.
@property(nonatomic, weak) id<ToolbarHeightDelegate> toolbarHeightDelegate;

// Commands handler for fullscreen.
@property(nonatomic, weak) id<FullscreenCommands> fullscreenCommands;

// Handler for settings commands.
@property(nonatomic, weak) id<SettingsCommands> settingsHandler;

// Dispatcher for Gemini commands.
@property(nonatomic, weak) id<BWGCommands> geminiHandler;

// Base view controller for presenting UI sheets.
@property(nonatomic, weak) UIViewController* baseViewController;

// Handler for scene commands.
@property(nonatomic, weak) id<SceneCommands> sceneHandler;

// Initializer.
- (instancetype)initWithIncognito:(BOOL)incognito
                     webStateList:(WebStateList*)webStateList
                    actionFactory:(BrowserActionFactory*)actionFactory
                      prefService:(PrefService*)prefService
             fullscreenController:(FullscreenController*)fullscreenController
                      topPosition:(BOOL)topPosition
     defaultBrowserBannerAppAgent:
         (DefaultBrowserBannerPromoAppAgent*)defaultBrowserBannerAppAgent
            authenticationService:(AuthenticationService*)authenticationService
                    geminiService:(GeminiService*)geminiService
               geminiBrowserAgent:(GeminiBrowserAgent*)geminiBrowserAgent
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Sets whether the UI currently supports showing the promo.
- (void)setUICurrentlySupportsPromo:(BOOL)supports;

// Updates the consumer with the current state of the web state.
- (void)updateConsumerWithWebState:(web::WebState*)webState
                          animated:(BOOL)animated;

// Disconnects observations.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_TOOLBAR_MEDIATOR_H_
