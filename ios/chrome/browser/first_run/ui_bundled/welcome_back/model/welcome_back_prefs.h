// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_WELCOME_BACK_MODEL_WELCOME_BACK_PREFS_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_WELCOME_BACK_MODEL_WELCOME_BACK_PREFS_H_

#import <UIKit/UIKit.h>

#import "components/prefs/pref_service.h"

class PrefRegistrySimple;
enum class BestFeaturesItemType;

// Registers the prefs associated with Welcome Back.
void RegisterWelcomeBackLocalStatePrefs(PrefRegistrySimple* registry);

// Erases an item from `kWelcomeBackEligibleItems`.
void MarkWelcomeBackFeatureUsed(BestFeaturesItemType item_type);

// Returns a vector of `kWelcomeBackEligibleItems`. This is used to check the
// current eligible items.
std::vector<BestFeaturesItemType> GetWelcomeBackEligibleItems();

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_WELCOME_BACK_MODEL_WELCOME_BACK_PREFS_H_
