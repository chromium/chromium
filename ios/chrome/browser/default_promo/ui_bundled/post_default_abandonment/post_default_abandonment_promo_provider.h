// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_POST_DEFAULT_ABANDONMENT_POST_DEFAULT_ABANDONMENT_PROMO_PROVIDER_H_
#define IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_POST_DEFAULT_ABANDONMENT_POST_DEFAULT_ABANDONMENT_PROMO_PROVIDER_H_

#import "ios/chrome/browser/shared/public/commands/promos_manager_commands.h"
#import "ios/chrome/browser/ui/promos_manager/standard_promo_alert_provider.h"

// Provider for displaying the post-default browser abandonment alert.
@interface PostDefaultBrowserAbandonmentPromoProvider
    : NSObject <StandardPromoAlertProvider>

@end

#endif  // IOS_CHROME_BROWSER_DEFAULT_PROMO_UI_BUNDLED_POST_DEFAULT_ABANDONMENT_POST_DEFAULT_ABANDONMENT_PROMO_PROVIDER_H_
