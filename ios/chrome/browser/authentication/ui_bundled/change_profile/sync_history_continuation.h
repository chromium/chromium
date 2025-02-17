// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CHANGE_PROFILE_SYNC_HISTORY_CONTINUATION_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CHANGE_PROFILE_SYNC_HISTORY_CONTINUATION_H_

#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/app/change_profile_continuation.h"

// Returns a ChangeProfileSyncHistoryContinuation that opens the history sync
// opt-in view. Accepts the 'accessPoint' and `optionalHistorySync` (even if it
// is NO, history sync ui might still be skipped if the user previously approved
// it).
ChangeProfileContinuation CreateChangeProfileSyncHistoryContinuation(
    signin_metrics::AccessPoint accessPoint,
    BOOL optionalHistorySync);

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CHANGE_PROFILE_SYNC_HISTORY_CONTINUATION_H_
