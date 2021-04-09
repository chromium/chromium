// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_FEATURES_H_

#include "base/feature_list.h"

// Feature to enable Close All Tabs confirmation.
extern const base::Feature kEnableCloseAllTabsConfirmation;

// Feature flag to enable TabGrid context menu.
extern const base::Feature kTabGridContextMenu;

// Feature flag to enable Bulk Actions.
extern const base::Feature kTabsBulkActions;

// Whether the kCloseAllTabsConfirmation flag is enabled.
bool IsCloseAllTabsConfirmationEnabled();

// Whether the kTabGridContextMenu flag is enabled.
bool IsTabGridContextMenuEnabled();

// Whether the kTabsBulkActions flag is enabled.
bool IsTabsBulkActionsEnabled();

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_FEATURES_H_
