// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_FEATURE_FLAGS_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_FEATURE_FLAGS_H_

#include "base/feature_list.h"

// Feature to enable the new overflow menu.
extern const base::Feature kNewOverflowMenu;

// Feature to enable smart sorting the new overflow menu.
extern const base::Feature kSmartSortingNewOverflowMenu;

// Whether the NewOverflowMenu feature is enabled.
bool IsNewOverflowMenuEnabled();

// Whether the new Google Password Manager branding is enabled.
bool IsPasswordManagerBrandingUpdateEnabled();

// Whether smart sorting the new overflow menu is enabled.
bool IsSmartSortingNewOverflowMenuEnabled();

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_OVERFLOW_MENU_FEATURE_FLAGS_H_
