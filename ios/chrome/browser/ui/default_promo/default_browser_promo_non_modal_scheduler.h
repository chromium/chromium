// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_PROMO_NON_MODAL_SCHEDULER_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_PROMO_NON_MODAL_SCHEDULER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/shared/coordinator/scene/scene_state_observer.h"

class Browser;
@class CommandDispatcher;

// A scheduler that determines when to show the non-modal default browser
// promo based on many sources of data.
@interface DefaultBrowserPromoNonModalScheduler : NSObject <SceneStateObserver>

@property(nonatomic, weak) CommandDispatcher* dispatcher;

// The browser that this scheduler uses to listen to events, such as page loads
// and overlay events
@property(nonatomic, assign) Browser* browser;

// Handles the user pasting in the omnibox and schedules a promo if necessary.
- (void)logUserPastedInOmnibox;

// Handles the user finishing a share and schedules a promo if necessary.
- (void)logUserFinishedActivityFlow;

// Handles the user launching the app via a first party scheme and schedules a
// promo if necessary.
- (void)logUserEnteredAppViaFirstPartyScheme;

// Handles the promo being dismissed, either through user action or timeout.
- (void)logPromoWasDismissed;

// Handles entering the tab grid, dismissing the promo.
- (void)logTabGridEntered;

// Handles presenting the popup menu, dismissing the promo.
- (void)logPopupMenuEntered;

// Handles the user performing the promo action.
- (void)logUserPerformedPromoAction;

// Handles the user manually dismissing the promo.
- (void)logUserDismissedPromo;

@end

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_PROMO_NON_MODAL_SCHEDULER_H_
