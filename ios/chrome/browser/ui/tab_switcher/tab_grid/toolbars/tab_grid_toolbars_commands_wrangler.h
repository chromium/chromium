// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_COMMANDS_WRANGLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_COMMANDS_WRANGLER_H_

#import <Foundation/Foundation.h>

// Temporary protocol to send commands to the toolbar. This should be removed in
// a future refactoring and implemented as TabGridToolbarDelegate for the Grid.
// TODO(crbug.com/1456659): Remove this class.
@protocol TabGridToolbarsCommandsWrangler

- (void)updateToolbarButtons;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_COMMANDS_WRANGLER_H_
