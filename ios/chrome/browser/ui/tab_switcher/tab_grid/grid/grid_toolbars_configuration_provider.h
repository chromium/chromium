// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_TOOLBARS_CONFIGURATION_PROVIDER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_TOOLBARS_CONFIGURATION_PROVIDER_H_

@class TabGridToolbarsConfiguration;

// Provider to get toolbars configuration from grid.
@protocol GridToolbarsConfigurationProvider <NSObject>

// Gets the toolbars configuration.
- (TabGridToolbarsConfiguration*)toolbarsConfiguration;

// YES if some tabs can be restored.
- (BOOL)didSavedClosedTabs;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_TOOLBARS_CONFIGURATION_PROVIDER_H_
