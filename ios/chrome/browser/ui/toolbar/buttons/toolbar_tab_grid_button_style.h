// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_TAB_GRID_BUTTON_STYLE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_TAB_GRID_BUTTON_STYLE_H_

// Possible styles for the toolbar tab grid button.
enum class ToolbarTabGridButtonStyle {
  // The Tab Grid button features the simple square with the tab count label
  // inside.
  kNormal,
  // The Tab Grid button features a filled square on a square with the tab count
  // label inside the filled square.
  kTabGroup,
};

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_TAB_GRID_BUTTON_STYLE_H_
