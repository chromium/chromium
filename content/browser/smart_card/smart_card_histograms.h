// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMART_CARD_SMART_CARD_HISTOGRAMS_H_
#define CONTENT_BROWSER_SMART_CARD_SMART_CARD_HISTOGRAMS_H_

// Reasons why the smart card connection might have been closed.
enum SmartCardConnectionClosedReason {
  // Disconnect for PC/SC-related reasons (either explicit disconnect or
  // connection lost).
  kSmartCardConnectionClosedDisconnect = 0,
  // Connection severed forcibly because of the permission expiry.
  kSmartCardConnectionClosedPermissionRevoked,
  kSmartCardConnectionMax = kSmartCardConnectionClosedPermissionRevoked,
};

void RecordSmartCardConnectionClosedReason(
    SmartCardConnectionClosedReason reason);

#endif  // CONTENT_BROWSER_SMART_CARD_SMART_CARD_HISTOGRAMS_H_
