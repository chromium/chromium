// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_PUBLIC_FEATURES_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_PUBLIC_FEATURES_H_

#import "base/feature_list.h"

enum class BestFeaturesItemType;

// Whether `kWelcomeBackInFirstRun` is enabled. This experiment is disabled
// when `kBestFeaturesScreenInFirstRun` is enabled.
bool IsWelcomeBackInFirstRunEnabled();

// Erases an item from `kWelcomeBackEligibleItems`.
void MarkWelcomeBackFeatureUsed(BestFeaturesItemType item_type);

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_PUBLIC_FEATURES_H_
