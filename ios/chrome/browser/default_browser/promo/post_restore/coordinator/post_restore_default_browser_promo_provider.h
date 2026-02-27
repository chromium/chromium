// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_POST_RESTORE_COORDINATOR_POST_RESTORE_DEFAULT_BROWSER_PROMO_PROVIDER_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_POST_RESTORE_COORDINATOR_POST_RESTORE_DEFAULT_BROWSER_PROMO_PROVIDER_H_

@protocol PictureInPictureCommands;
@protocol PromosManagerCommands;

#import "ios/chrome/browser/promos_manager/coordinator/standard_promo_alert_provider.h"
#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"

// Provider for displaying the Post Restore Default Browser Promo.
//
// The Post Restore Default Browser promo comes in multiple variations. This
// handler provides the necessary data and functionality to power two variations
// of this promo: (1) A native iOS alert promo, and (2) a half-sheet view
// controller style promo.
@interface PostRestoreDefaultBrowserPromoProvider
    : NSObject <StandardPromoAlertProvider>

// The PictureInPictureCommands handler to use for Picture-in-Picture related
// functionality.
@property(nonatomic, weak) id<PictureInPictureCommands> PIPHandler;

// The PromosManagerCommands handler to use for promo related functionality.
@property(nonatomic, weak) id<PromosManagerCommands> promosManagerHandler;

// Delegate callback to tell the provider that the promo was displayed.
- (void)promoWasDisplayed;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_POST_RESTORE_COORDINATOR_POST_RESTORE_DEFAULT_BROWSER_PROMO_PROVIDER_H_
