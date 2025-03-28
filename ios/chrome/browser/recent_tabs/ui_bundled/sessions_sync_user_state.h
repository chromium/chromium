// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_RECENT_TABS_UI_BUNDLED_SESSIONS_SYNC_USER_STATE_H_
#define IOS_CHROME_BROWSER_RECENT_TABS_UI_BUNDLED_SESSIONS_SYNC_USER_STATE_H_

// States listing the user's signed-in and tab sync (syncer::SESSIONS) status.
enum class SessionsSyncUserState {
  // The user signed-out and thus tab sync is off.
  USER_SIGNED_OUT,
  // The user is signed-in but tab sync is not working, either because the
  // user is not opted into it, or because there's an error that needs to be
  // resolved (e.g. passphrase error).
  USER_SIGNED_IN_SYNC_OFF,
  // Tab sync is on but tabs from other devices are still being downloaded.
  USER_SIGNED_IN_SYNC_IN_PROGRESS,
  // Tab sync is on but there are no tabs from other devices.
  USER_SIGNED_IN_SYNC_ON_NO_SESSIONS,
  // Tab sync is on and there are tabs from other devices.
  USER_SIGNED_IN_SYNC_ON_WITH_SESSIONS,
};

#endif  // IOS_CHROME_BROWSER_RECENT_TABS_UI_BUNDLED_SESSIONS_SYNC_USER_STATE_H_
