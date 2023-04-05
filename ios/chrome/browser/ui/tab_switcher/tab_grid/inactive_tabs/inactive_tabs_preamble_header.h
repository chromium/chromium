// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_PREAMBLE_HEADER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_PREAMBLE_HEADER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

// A collection view header displaying a preamble explanation of the Inactive
// Tabs feature and a link to its settings.
@interface InactiveTabsPreambleHeader : UICollectionReusableView

// The number of days after which tabs are considered inactive.
@property(nonatomic, assign) NSInteger daysThreshold;

// The callback when the Inactive Tabs settings link is pressed.
@property(nonatomic, copy) ProceduralBlock settingsLinkAction;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_INACTIVE_TABS_INACTIVE_TABS_PREAMBLE_HEADER_H_
