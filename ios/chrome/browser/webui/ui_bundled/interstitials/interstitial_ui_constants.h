// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_INTERSTITIALS_INTERSTITIAL_UI_CONSTANTS_H_
#define IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_INTERSTITIALS_INTERSTITIAL_UI_CONSTANTS_H_

// Paths used by chrome://interstitials.
extern const char kChromeInterstitialSslPath[];
extern const char kChromeInterstitialCaptivePortalPath[];
extern const char kChromeInterstitialSafeBrowsingPath[];

// Query keys and values for chrome://interstitials/ssl
extern const char kChromeInterstitialSslUrlQueryKey[];
extern const char kChromeInterstitialSslOverridableQueryKey[];
extern const char kChromeInterstitialSslStrictEnforcementQueryKey[];
extern const char kChromeInterstitialSslTypeQueryKey[];
extern const char kChromeInterstitialSslTypeHpkpFailureQueryValue[];
extern const char kChromeInterstitialSslTypeCtFailureQueryValue[];

// Query keys and values for chrome://interstitials/safe_browsing
extern const char kChromeInterstitialSafeBrowsingUrlQueryKey[];
extern const char kChromeInterstitialSafeBrowsingTypeQueryKey[];
extern const char kChromeInterstitialSafeBrowsingTypeMalwareValue[];
extern const char kChromeInterstitialSafeBrowsingTypePhishingValue[];
extern const char kChromeInterstitialSafeBrowsingTypeUnwantedValue[];
extern const char kChromeInterstitialSafeBrowsingTypeClientsidePhishingValue[];
extern const char kChromeInterstitialSafeBrowsingTypeBillingValue[];

#endif  // IOS_CHROME_BROWSER_WEBUI_UI_BUNDLED_INTERSTITIALS_INTERSTITIAL_UI_CONSTANTS_H_
