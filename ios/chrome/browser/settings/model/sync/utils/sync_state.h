// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_SYNC_STATE_H_
#define IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_SYNC_STATE_H_

// State of Sync-the-feature.
enum class SyncState {
  kSyncDisabledByAdministrator,
  kSyncConsentOff,
  kSyncOff,
  kSyncEnabledWithNoSelectedTypes,
  kSyncEnabledWithError,
  kSyncEnabled,
};

#endif  // IOS_CHROME_BROWSER_SETTINGS_MODEL_SYNC_UTILS_SYNC_STATE_H_
