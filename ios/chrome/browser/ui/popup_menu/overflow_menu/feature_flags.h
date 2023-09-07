// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_FEATURE_FLAGS_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_FEATURE_FLAGS_H_

#include "base/feature_list.h"

// Feature to enable the new overflow menu.
BASE_DECLARE_FEATURE(kNewOverflowMenu);

// Feature to add the Price Tracking destination (with Smart Sorting) to the new
// overflow menu.
BASE_DECLARE_FEATURE(kSmartSortingPriceTrackingDestination);

// Feature to enable overflow menu customization.
BASE_DECLARE_FEATURE(kOverflowMenuCustomization);

// Whether the NewOverflowMenu feature is enabled.
bool IsNewOverflowMenuEnabled();

// Whether or not overflow menu customization is enabled.
bool IsOverflowMenuCustomizationEnabled();

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_FEATURE_FLAGS_H_
