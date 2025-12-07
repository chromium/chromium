// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DEVICE_BOUND_SESSIONS_DELETION_REASON_H_
#define NET_DEVICE_BOUND_SESSIONS_DELETION_REASON_H_

namespace net::device_bound_sessions {

// Reasons for session termination.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(DeletionReason)
enum class DeletionReason {
  kExpired = 0,                  // Session was not used for too long.
  kFailedToRestoreKey = 1,       // Could not restore key from disk.
  kFailedToUnwrapKey = 2,        // Could not unwrap a key loaded from disk.
  kStoragePartitionCleared = 3,  // Site data is being cleared due to
                                 // removal in the StoragePartitionImpl.
  kClearBrowsingData = 4,        // Site data is being cleared by the
                                 // user. For example, through
                                 // chrome://settings/clearBrowsingData.
  kServerRequested = 5,          // Server explicitly requested termination.
  kInvalidSessionParams = 6,     // Refresh provided invalid params.
  kRefreshFatalError = 7,        // Fatal error during refresh.
  kMaxValue = kRefreshFatalError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/net/enums.xml:DeviceBoundSessionDeletionReason)

}  // namespace net::device_bound_sessions

#endif  // NET_DEVICE_BOUND_SESSIONS_DELETION_REASON_H_
