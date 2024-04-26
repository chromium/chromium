// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_RECENT_TABS_SESSIONS_SYNC_USER_STATE_H_
#define IOS_CHROME_BROWSER_UI_RECENT_TABS_SESSIONS_SYNC_USER_STATE_H_

// States listing the user's signed-in and sync status.
// TODO(crbug.com/40276546): Update this enum to reflect the more modern product
// state machine.
enum class SessionsSyncUserState {
  USER_SIGNED_OUT,
  USER_SIGNED_IN_SYNC_OFF,
  USER_SIGNED_IN_SYNC_IN_PROGRESS,
  USER_SIGNED_IN_SYNC_ON_NO_SESSIONS,
  USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS,
};

#endif  // IOS_CHROME_BROWSER_UI_RECENT_TABS_SESSIONS_SYNC_USER_STATE_H_
