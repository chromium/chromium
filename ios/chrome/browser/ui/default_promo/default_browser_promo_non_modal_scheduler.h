// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_PROMO_NON_MODAL_SCHEDULER_H_
#define IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_PROMO_NON_MODAL_SCHEDULER_H_

#import <UIKit/UIKit.h>

// A scheduler that determines when to show the non-modal default browser
// promo based on many sources of data.
@interface DefaultBrowserPromoNonModalScheduler : NSObject

// Handles the user pasting in the omnibox and schedules a promo if necessary.
- (void)logUserPastedInOmnibox;

// Handles the user finishing a share and schedules a promo if necessary.
- (void)logUserFinishedActivityFlow;

@end

#endif  // IOS_CHROME_BROWSER_UI_DEFAULT_PROMO_DEFAULT_BROWSER_PROMO_NON_MODAL_SCHEDULER_H_
