// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_DEFAULT_BROWSER_PROMO_BASE_DEFAULT_BROWSER_PROMO_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_DEFAULT_BROWSER_PROMO_BASE_DEFAULT_BROWSER_PROMO_SCENE_AGENT_H_

#import <UIKit/UIKit.h>

#import "base/time/time.h"
#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"

typedef NS_ENUM(NSUInteger, PromoReason) {
  PromoReasonNone,
  PromoReasonOmniboxPaste,
  PromoReasonExternalLink,
  PromoReasonShare
};

// An abstract base scene-agent scheduler that determines when to show the
// default browser promo based on many sources of data.
@interface BaseDefaultBrowserPromoSchedulerSceneAgent : ObservingSceneAgent

- (instancetype)init NS_DESIGNATED_INITIALIZER;

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

// Handles timer and promo cancelation when app is entring background.
- (void)cancelTimerAndPromoOnBackground;

// Returns whether specific default browser promo can be displayed. Should be
// implemented in subclassses.
- (bool)promoCanBeDisplayed;

// Resets the subclass specific promo handler. Should be implemented in
// subclassses.
- (void)resetPromoHandler;

// Initializes the subclass specific promo handler. Should be implemented in
// subclassses.
- (void)initPromoHandler:(Browser*)browser;

// Notifies the promo handler that promo should be displayed. Should be
// implemented in subclassses.
- (void)notifyHandlerShowPromo;

// Notifies the promo handler that promo should be dismissed. Should be
// implemented in subclassses.
- (void)notifyHandlerDismissPromo:(bool)animated;

// Performs actions needed on app entering background. Should be implemented in
// subclassses.
- (void)onEnteringBackground:(PromoReason)currentPromoReason
              promoIsShowing:(bool)promoIsShowing;

// Performs actions needed on app entering foreground. Should be implemented in
// subclassses.
- (void)onEnteringForeground;

// Performs actions needed on promo being displayed. Should be implemented in
// subclassses.
- (void)logPromoAppear:(PromoReason)currentPromoReason;

// Performs actions needed on promo primary action. Should be implemented in
// subclassses.
- (void)logPromoAction:(PromoReason)currentPromoReason
        promoShownTime:(base::TimeTicks)promoShownTime;

// Performs actions needed on promo dismiss by user. Should be implemented in
// subclassses.
- (void)logPromoUserDismiss:(PromoReason)currentPromoReason
             promoShownTime:(base::TimeTicks)promoShownTime;

// Performs actions needed on promo timeout. Should be implemented in
// subclassses.
- (void)logPromoTimeout:(PromoReason)currentPromoReason
         promoShownTime:(base::TimeTicks)promoShownTime;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_DEFAULT_BROWSER_PROMO_BASE_DEFAULT_BROWSER_PROMO_SCENE_AGENT_H_
