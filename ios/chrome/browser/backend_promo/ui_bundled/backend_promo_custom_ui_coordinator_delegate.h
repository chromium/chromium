// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BACKEND_PROMO_UI_BUNDLED_BACKEND_PROMO_CUSTOM_UI_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_BACKEND_PROMO_UI_BUNDLED_BACKEND_PROMO_CUSTOM_UI_COORDINATOR_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/backend_promo/ui_bundled/backend_promo_user_action.h"

@class BackendPromoCustomUICoordinator;

// Delegate protocol for handling custom UI promo completion/dismissal.
@protocol BackendPromoCustomUICoordinatorDelegate <NSObject>

// Called when the custom UI promo is dismissed with the user's action.
// The delegate is responsible for stopping `coordinator`.
- (void)backendPromoCustomUICoordinator:
            (BackendPromoCustomUICoordinator*)coordinator
                   didDismissWithAction:(BackendPromoUserAction)action;

@end

#endif  // IOS_CHROME_BROWSER_BACKEND_PROMO_UI_BUNDLED_BACKEND_PROMO_CUSTOM_UI_COORDINATOR_DELEGATE_H_
