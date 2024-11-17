// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_TAB_GROUP_STATE_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_TAB_GROUP_STATE_H_

// Possible states for the toolbar UI whether the current tab is in a tab group.
enum class ToolbarTabGroupState {
  // The current tab is not in a group.
  kNormal,
  // The current tab is in a group.
  kTabGroup,
};

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_BUTTONS_TOOLBAR_TAB_GROUP_STATE_H_
