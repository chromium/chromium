// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_TOOLBAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_TOOLBAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/toolbar/legacy/ui_bundled/banner_promo_view.h"
#import "ios/chrome/browser/toolbar/ui/toolbar_mutator.h"

class FullscreenBrowserAgent;
@class BrowserLayoutState;
@class DefaultBrowserBannerPromoAppAgent;
@protocol FullscreenCommands;
class FullscreenController;
@protocol SettingsCommands;
@protocol ToolbarConsumer;
@protocol ToolbarHeightDelegate;
@class ToolbarMediator;

/// Delegate for events in `ToolbarMediator`.
@protocol ToolbarMediatorDelegate <NSObject>

/// Notifies the delegate that `mediator`'s toolbar has the omnibox or not.
/// Only the toolbar with the omnibox shows the state of its control buttons.
- (void)toolbarMediator:(ToolbarMediator*)mediator
    didUpdateHasOmnibox:(BOOL)hasOmnibox;

@end

namespace web {
class WebState;
}  // namespace web
class WebStateList;

// Mediator for the toolbar.
@interface ToolbarMediator : NSObject <BannerPromoViewDelegate, ToolbarMutator>

// Browser layout state.
@property(nonatomic, weak) BrowserLayoutState* browserLayoutState;

// The consumer for this mediator.
@property(nonatomic, weak) id<ToolbarConsumer> consumer;

// Delegate for events in `ToolbarMediator`.
@property(nonatomic, weak) id<ToolbarMediatorDelegate> delegate;

// Delegate that handles the toolbars height.
@property(nonatomic, weak) id<ToolbarHeightDelegate> toolbarHeightDelegate;

// Commands handler for fullscreen.
@property(nonatomic, weak) id<FullscreenCommands> fullscreenCommands;

// Handler for settings commands.
@property(nonatomic, weak) id<SettingsCommands> settingsHandler;

// Initializer.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                fullscreenController:(FullscreenController*)fullscreenController
              fullscreenBrowserAgent:
                  (FullscreenBrowserAgent*)fullscreenBrowserAgent
                         topPosition:(BOOL)topPosition
        defaultBrowserBannerAppAgent:
            (DefaultBrowserBannerPromoAppAgent*)defaultBrowserBannerAppAgent
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Sets whether the UI currently supports showing the promo.
- (void)setUICurrentlySupportsPromo:(BOOL)supports;

// Updates the consumer with the state of `webState` that doesn't come from
// `ToolbarButtonsMediator`.
- (void)updateConsumerWithWebState:(web::WebState*)webState;

// Disconnects observations.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_TOOLBAR_MEDIATOR_H_
