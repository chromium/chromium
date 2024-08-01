// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_MENU_PROVIDER_H_
#define IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_MENU_PROVIDER_H_

#import "ios/chrome/browser/history/ui_bundled/history_entry_item.h"

// Protocol for instances that will provide menus to History components.
@protocol HistoryMenuProvider

// Creates a context menu configuration instance for the given `item`, which is
// represented on the UI by `view`.
- (UIContextMenuConfiguration*)contextMenuConfigurationForItem:
                                   (HistoryEntryItem*)item
                                                      withView:(UIView*)view;

@end

#endif  // IOS_CHROME_BROWSER_HISTORY_UI_BUNDLED_HISTORY_MENU_PROVIDER_H_
