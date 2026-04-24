// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BANNER_PROMO_MODEL_FAKE_DEFAULT_BROWSER_BANNER_PROMO_APP_AGENT_H_
#define IOS_CHROME_BROWSER_BANNER_PROMO_MODEL_FAKE_DEFAULT_BROWSER_BANNER_PROMO_APP_AGENT_H_

#import "ios/chrome/browser/banner_promo/model/default_browser_banner_promo_app_agent.h"

// A fake version of DefaultBrowserBannerPromoAppAgent that allows manual
// triggering of observer methods for testing.
@interface FakeDefaultBrowserBannerPromoAppAgent
    : DefaultBrowserBannerPromoAppAgent

// Manually trigger displayPromoFromAppAgent: on observers.
- (void)forceDisplayPromo;

// Manually trigger hidePromoFromAppAgent: on observers.
- (void)forceHidePromo;

@end

#endif  // IOS_CHROME_BROWSER_BANNER_PROMO_MODEL_FAKE_DEFAULT_BROWSER_BANNER_PROMO_APP_AGENT_H_
