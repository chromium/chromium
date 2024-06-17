// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_DEFAULT_BROWSER_PROMO_NON_MODAL_COORDINATOR_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_DEFAULT_BROWSER_PROMO_NON_MODAL_COORDINATOR_H_

#import "ios/chrome/browser/ui/infobars/coordinators/infobar_coordinator.h"

// Coordinator that creates and manages a non-modal default browser promo.
@interface DefaultBrowserPromoNonModalCoordinator : InfobarCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithInfoBarDelegate:
                    (infobars::InfoBarDelegate*)infoBarDelegate
                           badgeSupport:(BOOL)badgeSupport
                                   type:(InfobarType)infobarType NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_DEFAULT_BROWSER_PROMO_NON_MODAL_COORDINATOR_H_
