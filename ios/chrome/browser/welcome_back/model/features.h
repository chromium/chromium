// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WELCOME_BACK_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_WELCOME_BACK_MODEL_FEATURES_H_

#import "base/feature_list.h"

enum class BestFeaturesItemType;

// Enum to represent the arms of feature kWelcomeBackInFirstRun.
enum class WelcomeBackScreenVariationType {
  kDisabled,
  // Show the Search with Lens, Enhanced Safe Browsing, and Locked Incognito
  // items.
  kBasicsWithLockedIncognitoTabs,
  // Show the Enhanced Safe Browsing, Search with Lens, and Save & Autofill
  // Passwords items.
  kBasicsWithPasswords,
  // Show the Tab Groups, Locked Incognito, and Price Tracking & Insights items.
  kProductivityAndShopping,
  // Show the Search with Lens, Enhanced Safe Browsing, and Autofill Passwords
  // in Other Apps items. If Credential Provider Extension (CPE) is already
  // enabled, Autofill Passwords in Other Apps is replaced with Share Passwords.
  kSignInBenefits,
};

// Feature to enable the Welcome Back screen.
BASE_DECLARE_FEATURE(kWelcomeBackInFirstRun);

// Name of the param that indicates which variation of the
// kWelcomeBackInFirstRun is enabled.
extern const char kWelcomeBackInFirstRunParam[];

// Whether `kWelcomeBackInFirstRun` is enabled. This experiment is disabled
// when `kBestFeaturesScreenInFirstRun` is enabled.
bool IsWelcomeBackInFirstRunEnabled();

// Erases an item from `kWelcomeBackEligibleItems`.
void MarkWelcomeBackFeatureUsed(BestFeaturesItemType item_type);

// Returns which variation of the kWelcomeBackInFirstRun feature is enabled or
// `kDisabled` if the feature is disabled.
WelcomeBackScreenVariationType GetWelcomeBackScreenVariationType();

// Whether `kWelcomeBackInFirstRun` is enabled. This experiment is disabled when
// `kBestFeaturesScreenInFirstRun` is enabled.
bool IsWelcomeBackInFirstRunEnabled();

#endif  // IOS_CHROME_BROWSER_WELCOME_BACK_MODEL_FEATURES_H_
