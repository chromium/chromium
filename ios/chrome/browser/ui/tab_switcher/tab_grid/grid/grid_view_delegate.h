// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_VIEW_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_VIEW_DELEGATE_H_

// Delegate protocol to relay user interactions from a grid UI.
@protocol GridViewDelegate <NSObject>

// Notify if the header is visible or not.
- (void)gridViewHeaderHidden:(BOOL)hidden;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_VIEW_DELEGATE_H_
