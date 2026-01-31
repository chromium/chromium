// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_PUBLIC_FEATURES_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_PUBLIC_FEATURES_H_

#import "base/feature_list.h"

// Variations of Default Browser Promo Refresh.
extern const char kDefaultBrowserPromoRefreshParam[];
extern const char kDefaultBrowserPromoRefreshParamNoInstructions[];
extern const char kDefaultBrowserPromoRefreshParamSystemAlertInstructions[];
extern const char
    kDefaultBrowserPromoRefreshParamPictureInPictureInstructions[];
extern const char kDefaultBrowserPromoRefreshParamCarouselInstructions[];

// Feature flag to enable default browser iPad specific instructions.
BASE_DECLARE_FEATURE(kDefaultBrowserPromoIpadInstructions);

// Feature flag to enable the default browser promo refresh.
BASE_DECLARE_FEATURE(kDefaultBrowserPromoRefresh);

// Returns true if the default browser iPad specific instructions are enabled.
bool IsDefaultBrowserPromoIpadInstructions();

// Returns true if the default browser promo refresh is enabled.
bool IsDefaultBrowserPromoRefreshEnabled();

// Returns the default browser promo refresh param.
std::string DefaultBrowserPromoRefreshParam();

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_PUBLIC_FEATURES_H_
