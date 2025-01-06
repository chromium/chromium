// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BANNER_PROMO_MODEL_DEFAULT_BROWSER_BANNER_PROMO_APP_AGENT_H_
#define IOS_CHROME_BROWSER_BANNER_PROMO_MODEL_DEFAULT_BROWSER_BANNER_PROMO_APP_AGENT_H_

#import "ios/chrome/app/application_delegate/observing_app_state_agent.h"

// App agent to manage the Default Browser Banner Promo. It observes navigation
// events in all active web states to determine when to show and hide the promo.
@interface DefaultBrowserBannerPromoAppAgent : SceneObservingAppAgent

@end

#endif  // IOS_CHROME_BROWSER_BANNER_PROMO_MODEL_DEFAULT_BROWSER_BANNER_PROMO_APP_AGENT_H_
