// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webui/ui_bundled/interstitials/interstitial_ui_constants.h"

const char kChromeInterstitialSslPath[] = "/ssl";
const char kChromeInterstitialCaptivePortalPath[] = "/captiveportal";
const char kChromeInterstitialSafeBrowsingPath[] = "/safebrowsing";

const char kChromeInterstitialSslUrlQueryKey[] = "url";
const char kChromeInterstitialSslOverridableQueryKey[] = "overridable";
const char kChromeInterstitialSslStrictEnforcementQueryKey[] =
    "strict_enforcement";
const char kChromeInterstitialSslTypeQueryKey[] = "type";
const char kChromeInterstitialSslTypeHpkpFailureQueryValue[] = "hpkp_failure";
const char kChromeInterstitialSslTypeCtFailureQueryValue[] = "ct_failure";

const char kChromeInterstitialSafeBrowsingUrlQueryKey[] = "url";
const char kChromeInterstitialSafeBrowsingTypeQueryKey[] = "type";
const char kChromeInterstitialSafeBrowsingTypeMalwareValue[] = "malware";
const char kChromeInterstitialSafeBrowsingTypePhishingValue[] = "phishing";
const char kChromeInterstitialSafeBrowsingTypeUnwantedValue[] = "unwanted";
const char kChromeInterstitialSafeBrowsingTypeClientsidePhishingValue[] =
    "clientside_phishing";
const char kChromeInterstitialSafeBrowsingTypeBillingValue[] = "billing";
