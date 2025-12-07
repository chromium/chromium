// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_HISTORY_SYNC_PUBLIC_HISTORY_SYNC_CONSTANTS_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_HISTORY_SYNC_PUBLIC_HISTORY_SYNC_CONSTANTS_H_

// Result from HistorySyncCoordinator or HistorySyncPopupCoordinator.
enum class HistorySyncResult {
  // The dialog is enabled either because the user accepted or because history
  // sync was accepted by a previous sign-in.
  kSuccess,
  // The dialog ended because there is no primary identity. Mostly likely
  // the primary account was removed while the dialog was opened.
  kPrimaryIdentityRemoved,
  // The user canceled the dialog.
  kUserCanceled,
  // The dialog was skipped, see `history_sync::HistorySyncSkipReason`.
  kSkipped,
};

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_HISTORY_SYNC_PUBLIC_HISTORY_SYNC_CONSTANTS_H_
