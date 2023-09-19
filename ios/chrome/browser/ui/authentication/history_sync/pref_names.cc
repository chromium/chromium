// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/authentication/history_sync/pref_names.h"

namespace history_sync_prefs {

// Number of times the user declined successively History Sync (in the opt-in
// screen or in the settings).
// This value is reset to zero when the user accepts History Sync (opt-in screen
// or settings).
const char kHistorySyncSuccessiveDeclineCount[] =
    "ios.history_sync.successive_decline_count";

// The timestamp when History Sync was last declined (in the opt-in screen or
// in the settings).
// Is reset when the user accepts History Sync (opt-in screen or settings).
const char kHistorySyncLastDeclinedTimestamp[] =
    "ios.history_sync.last_declined_timestamp";

}  // namespace history_sync_prefs
