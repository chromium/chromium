// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_PROMO_NON_MODAL_SCHEDULER_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_PROMO_NON_MODAL_SCHEDULER_H_

#import <UIKit/UIKit.h>

@class CommandDispatcher;
class OverlayPresenter;
class WebStateList;

// A scheduler that determines when to show the non-modal default browser
// promo based on many sources of data.
@interface DefaultBrowserPromoNonModalScheduler : NSObject

@property(nonatomic, weak) CommandDispatcher* dispatcher;

// The web state list that this scheduler uses to listen to page load and
// WebState change events.
@property(nonatomic, assign) WebStateList* webStateList;

// The overlay presenter that this scheduler listens to when preventing the
// promo from showing over an overlay.
@property(nonatomic, assign) OverlayPresenter* overlayPresenter;

// Handles the user pasting in the omnibox and schedules a promo if necessary.
- (void)logUserPastedInOmnibox;

// Handles the user finishing a share and schedules a promo if necessary.
- (void)logUserFinishedActivityFlow;

// Handles the promo being dismissed, either through user action or timeout.
- (void)logPromoWasDismissed;

@end

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_PROMO_NON_MODAL_SCHEDULER_H_
