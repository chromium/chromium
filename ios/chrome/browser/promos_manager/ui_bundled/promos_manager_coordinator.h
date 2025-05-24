// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_UI_BUNDLED_PROMOS_MANAGER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_UI_BUNDLED_PROMOS_MANAGER_COORDINATOR_H_

#import "ios/chrome/browser/promos_manager/ui_bundled/promos_manager_ui_handler.h"
#import "ios/chrome/browser/shared/coordinator/chrome_coordinator/chrome_coordinator.h"

@protocol CredentialProviderPromoCommands;
@protocol DockingPromoCommands;

// Coordinator for displaying app-wide promos.
@interface PromosManagerCoordinator : ChromeCoordinator <PromosManagerUIHandler>

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
            credentialProviderPromoHandler:(id<CredentialProviderPromoCommands>)
                                               credentialProviderPromoHandler
                       dockingPromoHandler:
                           (id<DockingPromoCommands>)dockingPromoHandler
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser NS_UNAVAILABLE;

// Displays a promo if one is currently available based on impression history
// and any other restrictions.
- (void)displayPromoIfAvailable;

// Alerts the coordinator that the current promo was dismissed. Should be used
// when the presentation infrastructure can't listen for the dismissal itself.
// App store rating is one example.
- (void)promoWasDismissed;

@end

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_UI_BUNDLED_PROMOS_MANAGER_COORDINATOR_H_
