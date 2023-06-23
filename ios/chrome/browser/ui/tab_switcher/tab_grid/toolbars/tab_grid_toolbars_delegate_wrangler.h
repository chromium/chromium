// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_DELEGATE_WRANGLER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_DELEGATE_WRANGLER_H_

#import <Foundation/Foundation.h>

// Delegate for the toolbars. This is a temporary class that should be removed
// in future refactoring.
// TODO(crbug.com/1456659): Remove this class.
@protocol TabGridToolbarsDelegateWrangler

// Returns whether the current grid is empty or not (doesn't include inactive
// nor pinned tabs).
- (BOOL)isCurrentGridEmpty;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_TOOLBARS_TAB_GRID_TOOLBARS_DELEGATE_WRANGLER_H_
