// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_BUTTON_UI_SWIFT_BRIDGE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_BUTTON_UI_SWIFT_BRIDGE_H_

// Bridging header between Swift and Obj-C. These types/imports need to be pure
// Obj-C and have no C++ in them.

#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/inactive_tabs/inactive_tabs_constants.h"
#import "ios/chrome/grit/ios_strings.h"

// Explicitly import the bridging header of the Swift dependencies, as the
// implicit import of these bridging header is deprecated and will be removed in
// a later version of Swift.
#import "ios/chrome/common/ui/colors/swift_bridge.h"
#import "ui/base/l10n/l10n_util_mac_bridge.h"

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_BUTTON_UI_SWIFT_BRIDGE_H_
