// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GROUP_ACTION_TYPE_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GROUP_ACTION_TYPE_H_

// Enum to represent an action that a tab group is going to take.
enum class TabGroupActionType {
  kUngroupTabGroup,
  kDeleteTabGroup,
  kLeaveSharedTabGroup,
  kDeleteSharedTabGroup,
  kDeleteOrKeepSharedTabGroup,
  kLeaveOrKeepSharedTabGroup,
  kCloseLastTabUnknownRole,
};

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GROUP_ACTION_TYPE_H_
