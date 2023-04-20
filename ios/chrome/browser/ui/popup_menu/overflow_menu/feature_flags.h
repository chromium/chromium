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

// Feature to add a "Share Chrome App" action to the overflow menu
BASE_DECLARE_FEATURE(kNewOverflowMenuShareChromeAction);

// Feature to use the alternate overflow IPH flow.
BASE_DECLARE_FEATURE(kNewOverflowMenuAlternateIPH);

// Whether the NewOverflowMenu feature is enabled.
bool IsNewOverflowMenuEnabled();

// Whether or not the NewOverflowMenuReorderSettings feature is enabled.
bool IsNewOverflowMenuReorderSettingsEnabled();

// Whether the Price Tracking destination (with Smart Sorting) is added to the
// new overflow menu.
bool IsSmartSortingPriceTrackingDestinationEnabled();

// Whether or not the NewOverflowMenuShareChromeAction is enabled.
bool IsNewOverflowMenuShareChromeActionEnabled();

// Whether or not the alternate overflow menu IPH flow is enabled.
bool IsNewOverflowMenuAlternateIPHEnabled();

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_FEATURE_FLAGS_H_
