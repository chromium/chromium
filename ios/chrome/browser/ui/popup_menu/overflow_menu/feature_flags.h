// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_FEATURE_FLAGS_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_FEATURE_FLAGS_H_

#include "base/feature_list.h"

// Feature to enable the new overflow menu.
extern const base::Feature kNewOverflowMenu;

// Feature to add a Clear Browsing Data action to the new overflow menu.
extern const base::Feature kNewOverflowMenuCBDAction;

// Feature to add a Settings action to the new overflow menu.
extern const base::Feature kNewOverflowMenuSettingsAction;

// Feature to enable smart sorting the new overflow menu.
extern const base::Feature kSmartSortingNewOverflowMenu;

// Whether the NewOverflowMenu feature is enabled.
bool IsNewOverflowMenuEnabled();

// Whether or not the NewOverflowMenuCBDAction feature is enabled.
bool IsNewOverflowMenuCBDActionEnabled();

// Whether or not the NewOverflowMenuSettingsAction feature is enabled.
bool IsNewOverflowMenuSettingsActionEnabled();

// Whether or not the NewOverflowMenuReorderSettings feature is enabled.
bool IsNewOverflowMenuReorderSettingsEnabled();

// Whether the new Google Password Manager branding is enabled.
bool IsPasswordManagerBrandingUpdateEnabled();

// Whether smart sorting the new overflow menu is enabled.
bool IsSmartSortingNewOverflowMenuEnabled();

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_FEATURE_FLAGS_H_
