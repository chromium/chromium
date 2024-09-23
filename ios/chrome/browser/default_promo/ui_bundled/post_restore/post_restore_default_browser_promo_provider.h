// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_POST_RESTORE_POST_RESTORE_DEFAULT_BROWSER_PROMO_PROVIDER_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_POST_RESTORE_POST_RESTORE_DEFAULT_BROWSER_PROMO_PROVIDER_H_

#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_alert_provider.h"

// Provider for displaying the Post Restore Default Browser Promo.
//
// The Post Restore Default Browser promo comes in multiple variations. This
// handler provides the necessary data and functionality to power two variations
// of this promo: (1) A native iOS alert promo, and (2) a half-sheet view
// controller style promo.
@interface PostRestoreDefaultBrowserPromoProvider
    : NSObject <StandardPromoAlertProvider>

// Delegate callback to tell the provider that the promo was displayed.
- (void)promoWasDisplayed;

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_POST_RESTORE_POST_RESTORE_DEFAULT_BROWSER_PROMO_PROVIDER_H_
