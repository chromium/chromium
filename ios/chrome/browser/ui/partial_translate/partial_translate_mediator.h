// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PARTIAL_TRANSLATE_PARTIAL_TRANSLATE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PARTIAL_TRANSLATE_PARTIAL_TRANSLATE_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/partial_translate/partial_translate_delegate.h"

@protocol BrowserCoordinatorCommands;
@protocol EditMenuAlertDelegate;
class FullscreenController;
class PrefService;
class WebStateList;

// Mediator that mediates between the browser container views and the
// partial translate tab helpers.
@interface PartialTranslateMediator : NSObject <PartialTranslateDelegate>

// Initializer for a mediator. `webStateList` is the WebStateList for the
// Browser whose content is shown within the BrowserContainerConsumer. It must
// be non-null.
- (instancetype)initWithWebStateList:(WebStateList*)webStateList
              withBaseViewController:(UIViewController*)baseViewController
                         prefService:(PrefService*)prefs
                fullscreenController:(FullscreenController*)fullscreenController
                           incognito:(BOOL)incognito NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator.
- (void)shutdown;

// The handler for BrowserCoordinator commands (to trigger full page translate).
@property(nonatomic, weak) id<BrowserCoordinatorCommands> browserHandler;

// The delegate to present error message alerts.
@property(nonatomic, weak) id<EditMenuAlertDelegate> alertDelegate;

// Handles the link to text menu item selection.
- (void)handlePartialTranslateSelection;

// Returns whether a partial translate can be handled.
- (BOOL)canHandlePartialTranslateSelection;

// Whether partial translate action should be proposed (independently of the
// current selection).
- (BOOL)shouldInstallPartialTranslate;

@end

#endif  // IOS_CHROME_BROWSER_UI_PARTIAL_TRANSLATE_PARTIAL_TRANSLATE_MEDIATOR_H_
