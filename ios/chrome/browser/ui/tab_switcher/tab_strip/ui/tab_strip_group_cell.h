// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_GROUP_CELL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_GROUP_CELL_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/ui/tab_strip_cell.h"

// TabStripCell that contains a group title.
@interface TabStripGroupCell : TabStripCell

// Background color of the title container.
@property(nonatomic, strong) UIColor* titleContainerBackgroundColor;

// Whether the cell is that of a collapsed group. Default value is NO.
@property(nonatomic, assign) BOOL collapsed;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_STRIP_UI_TAB_STRIP_GROUP_CELL_H_
