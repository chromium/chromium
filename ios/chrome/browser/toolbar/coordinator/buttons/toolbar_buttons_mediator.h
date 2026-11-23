// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_BUTTONS_TOOLBAR_BUTTONS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_BUTTONS_TOOLBAR_BUTTONS_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/toolbar/ui/toolbar_buttons_mutator.h"

class AuthenticationService;
@class BrowserActionFactory;
class GeminiBrowserAgent;
@protocol GeminiCommands;
class PrefService;
class ProfileIOS;
class TabBasedIPHBrowserAgent;
@protocol ToolbarButtonsConsumer;
@class ToolbarButtonsMediator;
class WebNavigationBrowserAgent;
namespace web {
class WebState;
}  // namespace web
class WebStateList;

// Delegate for events in `ToolbarButtonsMediator`.
@protocol ToolbarButtonsMediatorDelegate <NSObject>

// Notifies the delegate that the assistant button was tapped in Incognito
// mode.
- (void)toolbarButtonsMediatorDidTapAssistantInIncognito:
    (ToolbarButtonsMediator*)mediator;

@end

// Computes the state of the toolbar's control buttons once for the Browser, and
// pushes it to the active consumers. Inactive consumers get nothing, which
// avoids building menus and updating the loading progress of hidden toolbars.
// A consumer is synced with the current state when it becomes active.
@interface ToolbarButtonsMediator : NSObject <ToolbarButtonsMutator>

// Delegate for events in `ToolbarButtonsMediator`.
@property(nonatomic, weak) id<ToolbarButtonsMediatorDelegate> delegate;

// Helper for web navigation.
@property(nonatomic, assign) WebNavigationBrowserAgent* navigationBrowserAgent;

// Helper for tab-based IPH.
@property(nonatomic, assign) TabBasedIPHBrowserAgent* tabBasedIPHAgent;

// Dispatcher for Gemini commands.
@property(nonatomic, weak) id<GeminiCommands> geminiHandler;

// Base view controller for presenting UI sheets.
@property(nonatomic, weak) UIViewController* baseViewController;

// TODO(crbug.com/588026569): Remove the `ProfileIOS` dependency by injecting a
// Gemini availability provider.
- (instancetype)initWithIncognito:(BOOL)incognito
                     webStateList:(WebStateList*)webStateList
                    actionFactory:(BrowserActionFactory*)actionFactory
                          profile:(ProfileIOS*)profile
                      prefService:(PrefService*)prefService
            authenticationService:(AuthenticationService*)authenticationService
               geminiBrowserAgent:(GeminiBrowserAgent*)geminiBrowserAgent
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Adds `consumer` to the active consumers and syncs it with the current state.
- (void)addConsumer:(id<ToolbarButtonsConsumer>)consumer;

// Removes `consumer` from the active consumers, so that it gets no more
// updates.
- (void)removeConsumer:(id<ToolbarButtonsConsumer>)consumer;

// Updates only `consumer` with the state of `webState`, if it's an active
// consumer. Used to snapshot a toolbar showing another WebState, without
// updating the other active consumers.
- (void)updateConsumer:(id<ToolbarButtonsConsumer>)consumer
          withWebState:(web::WebState*)webState
              animated:(BOOL)animated;

// Disconnects observations.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_TOOLBAR_COORDINATOR_BUTTONS_TOOLBAR_BUTTONS_MEDIATOR_H_
