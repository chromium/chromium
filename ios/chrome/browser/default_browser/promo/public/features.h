// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_PUBLIC_FEATURES_H_
#define IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_PUBLIC_FEATURES_H_

#import "base/feature_list.h"

// Variations of Default Browser Promo Picture in Picture.
extern const char kDefaultBrowserPictureInPictureParam[];
extern const char kDefaultBrowserPictureInPictureParamEnabled[];
extern const char kDefaultBrowserPictureInPictureParamDisabledDefaultApps[];
extern const char kDefaultBrowserPictureInPictureParamEnabledDefaultApps[];

// Feature flag to enable default browser iPad specific instructions.
BASE_DECLARE_FEATURE(kDefaultBrowserPromoIpadInstructions);

// Feature flag to enable the default browser promo Picture in Picture.
BASE_DECLARE_FEATURE(kDefaultBrowserPictureInPicture);

// Returns true if the default browser iPad specific instructions are enabled.
bool IsDefaultBrowserPromoIpadInstructions();

// Returns true if the default browser promo Picture in Picture is enabled.
bool IsDefaultBrowserPictureInPictureEnabled();

// Returns the default browser promo Picture in Picture param.
std::string DefaultBrowserPictureInPictureParam();

// Returns true if the default browser promo destination for Picture in
// Picture flow is default apps.
bool IsDefaultAppsPictureInPictureVariant();

#endif  // IOS_CHROME_BROWSER_DEFAULT_BROWSER_PROMO_PUBLIC_FEATURES_H_
