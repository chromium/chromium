// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POST_RESTORE_SIGNIN_UI_BUNDLED_POST_RESTORE_SIGNIN_PROVIDER_H_
#define IOS_CHROME_BROWSER_POST_RESTORE_SIGNIN_UI_BUNDLED_POST_RESTORE_SIGNIN_PROVIDER_H_

#import "ios/chrome/browser/promos_manager/coordinator/bannered_promo_view_provider.h"
#import "ios/chrome/browser/promos_manager/coordinator/standard_promo_alert_provider.h"

namespace signin {
class IdentityManager;
}  // namespace signin

namespace syncer {
class SyncService;
}  // namespace syncer

class AuthenticationService;
class PrefService;
@protocol SceneCommands;

// Provider for displaying the Post Restore Sign-in Promo.
//
// The Post Restore Sign-in promo comes in two variations: (1) A fullscreen,
// FRE-like promo, and (2) a native iOS alert promo. This handler provides the
// necessary data and functionality to power both variations of this promo.
@interface PostRestoreSignInProvider : NSObject <StandardPromoAlertProvider>

- (instancetype)initWithSyncService:(syncer::SyncService*)syncService
              authenticationService:(AuthenticationService*)authService
                    identityManager:(signin::IdentityManager*)identityManager
                        prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Delegate callback to tell the provider that the promo was displayed.
- (void)promoWasDisplayed;

// The handler is used to start the sign-in flow.
@property(nonatomic, weak) id<SceneCommands> sceneHandler;

@end

#endif  // IOS_CHROME_BROWSER_POST_RESTORE_SIGNIN_UI_BUNDLED_POST_RESTORE_SIGNIN_PROVIDER_H_
