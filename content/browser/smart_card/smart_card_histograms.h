// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMART_CARD_SMART_CARD_HISTOGRAMS_H_
#define CONTENT_BROWSER_SMART_CARD_SMART_CARD_HISTOGRAMS_H_

// Reasons why the smart card connection might have been closed.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SmartCardConnectionClosedReason)
enum SmartCardConnectionClosedReason {
  // Disconnect for PC/SC-related reasons (either explicit disconnect or
  // connection lost).
  kSmartCardConnectionClosedDisconnect = 0,
  // Connection severed forcibly because of the permission expiry.
  kSmartCardConnectionClosedPermissionRevoked,
  kMaxValue = kSmartCardConnectionClosedPermissionRevoked,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/smart_card/enums.xml:SmartCardConnectionClosedReason)

void RecordSmartCardConnectionClosedReason(
    SmartCardConnectionClosedReason reason);

#endif  // CONTENT_BROWSER_SMART_CARD_SMART_CARD_HISTOGRAMS_H_
