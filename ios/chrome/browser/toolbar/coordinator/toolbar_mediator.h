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
@protocol ToolbarConsumer;
@protocol ToolbarHeightDelegate;
class WebNavigationBrowserAgent;
namespace web {
class WebState;
}  // namespace web
class WebStateList;

// Mediator for the toolbar.
@interface ToolbarMediator : NSObject <BannerPromoViewDelegate, ToolbarMutator>

// The consumer for this mediator.
@property(nonatomic, weak) id<ToolbarConsumer> consumer;

// Helper for web navigation.
@property(nonatomic, assign) WebNavigationBrowserAgent* navigationBrowserAgent;

// Delegate that handles the toolbars height.
@property(nonatomic, weak) id<ToolbarHeightDelegate> toolbarHeightDelegate;

// Whether the toolbar is being shown in incognito or not.
@property(nonatomic, assign, getter=isIncognito) BOOL incognito;

// Commands handler for fullscreen.
@property(nonatomic, weak) id<FullscreenCommands> fullscreenCommands;

// Handler for settings commands.
@property(nonatomic, weak) id<SettingsCommands> settingsHandler;

// Initializer.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                       actionFactory:(BrowserActionFactory*)actionFactory
                fullscreenController:(FullscreenController*)fullscreenController
                         topPosition:(BOOL)topPosition
        defaultBrowserBannerAppAgent:
            (DefaultBrowserBannerPromoAppAgent*)defaultBrowserBannerAppAgent
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Sets whether the UI currently supports showing the promo.
- (void)setUICurrentlySupportsPromo:(BOOL)supports;

// Updates the consumer with the current state of the web state.
- (void)updateConsumerWithWebState:(web::WebState*)webState;

// Disconnects observations.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_TOOLBAR_MEDIATOR_H_
