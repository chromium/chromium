// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_COORDINATOR_DEFAULT_BROWSER_PROMO_NON_MODAL_DEFAULT_BROWSER_PROMO_SCHEDULER_SCENE_AGENT_H_
#define IOS_CHROME_BROWSER_SHARED_COORDINATOR_DEFAULT_BROWSER_PROMO_NON_MODAL_DEFAULT_BROWSER_PROMO_SCHEDULER_SCENE_AGENT_H_

#import "ios/chrome/browser/shared/coordinator/scene/observing_scene_state_agent.h"

// A scene-agent scheduler that determines when to show the non-modal default
// browser promo based on many sources of data.
@interface NonModalDefaultBrowserPromoSchedulerSceneAgent : ObservingSceneAgent

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

#endif  // IOS_CHROME_BROWSER_SHARED_COORDINATOR_DEFAULT_BROWSER_PROMO_NON_MODAL_DEFAULT_BROWSER_PROMO_SCHEDULER_SCENE_AGENT_H_
