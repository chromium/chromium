// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_UKM_RECORDER_IMPL_UTILS_H_
#define SERVICES_METRICS_PUBLIC_CPP_UKM_RECORDER_IMPL_UTILS_H_

#include <stdint.h>

#include "services/metrics/public/cpp/metrics_export.h"

namespace ukm {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Update tools/metrics/histograms/enums.xml when new entries are added.
enum class DroppedDataReason {
  NOT_DROPPED = 0,
  RECORDING_DISABLED = 1,
  MAX_HIT = 2,
  DEPRECATED_NOT_WHITELISTED = 3,
  UNSUPPORTED_URL_SCHEME = 4,
  SAMPLED_OUT = 5,
  EXTENSION_URLS_DISABLED = 6,
  EXTENSION_NOT_SYNCED = 7,
  NOT_MATCHED = 8,
  EMPTY_URL = 9,
  REJECTED_BY_FILTER = 10,
  SAMPLING_UNCONFIGURED = 11,
  MSBB_CONSENT_DISABLED = 12,
  APPS_CONSENT_DISABLED = 13,
  EXTENSION_URL_INVALID = 14,
  // Captures dropped entries due to UkmReduceAddEntryIPC feature.
  RECORDING_DISABLED_REDUCE_ADDENTRYIPC = 15,
  NUM_DROPPED_DATA_REASONS
};

void METRICS_EXPORT RecordDroppedEntry(uint64_t event_hash,
                                       DroppedDataReason reason);

void METRICS_EXPORT RecordDroppedWebDXFeaturesSet(DroppedDataReason reason);

}  // namespace ukm

#endif  // SERVICES_METRICS_PUBLIC_CPP_UKM_RECORDER_IMPL_UTILS_H_
