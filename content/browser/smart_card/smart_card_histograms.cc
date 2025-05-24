// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/smart_card/smart_card_histograms.h"

#include "base/metrics/histogram_macros.h"

void RecordSmartCardConnectionClosedReason(
    SmartCardConnectionClosedReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "SmartCard.ConnectionClosedReason", reason,
      SmartCardConnectionClosedReason::kSmartCardConnectionMax);
}
