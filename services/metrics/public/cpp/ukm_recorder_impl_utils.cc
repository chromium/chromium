// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/metrics/public/cpp/ukm_recorder_impl_utils.h"

#include "base/logging.h"
#include "base/metrics/histogram_macros.h"

namespace ukm {

void RecordDroppedEntry(uint64_t event_hash, DroppedDataReason reason) {
  DVLOG(3) << "RecordDroppedEntry [event_hash=" << event_hash
           << " reason=" << static_cast<int>(reason) << "]";
  // Truncate the unsigned 64-bit hash to 31 bits, to
  // make it a suitable histogram sample.
  uint32_t value = event_hash & 0x7fffffff;
  // The enum for these histograms gets populated by the
  // PopulateEnumWithUkmEvents
  // function in populate_enums.py when producing the merged XML.

  UMA_HISTOGRAM_SPARSE("UKM.Entries.Dropped.ByEntryHash", value);

  // Because the "UKM.Entries.Dropped.ByEntryHash" histogram will be emitted
  // to every single time an entry is dropped, it will be dominated by the
  // RECORDING_DISABLED reason (which is not very insightful). We also emit
  // histograms split by selected reasons that are deemed interesting or
  // helpful for data quality investigations.
  switch (reason) {
    case DroppedDataReason::MAX_HIT:
      UMA_HISTOGRAM_SPARSE("UKM.Entries.Dropped.MaxHit.ByEntryHash", value);
      break;
    case DroppedDataReason::SAMPLED_OUT:
      UMA_HISTOGRAM_SPARSE("UKM.Entries.Dropped.SampledOut.ByEntryHash", value);
      break;
    case DroppedDataReason::REJECTED_BY_FILTER:
      UMA_HISTOGRAM_SPARSE("UKM.Entries.Dropped.RejectedByFilter.ByEntryHash",
                           value);
      break;
    default:
      break;
  }

  UMA_HISTOGRAM_ENUMERATION(
      "UKM.Entries.Dropped", static_cast<int>(reason),
      static_cast<int>(DroppedDataReason::NUM_DROPPED_DATA_REASONS));
}

void RecordDroppedWebDXFeaturesSet(DroppedDataReason reason) {
  UMA_HISTOGRAM_ENUMERATION(
      "UKM.WebDXFeatureSets.Dropped", static_cast<int>(reason),
      static_cast<int>(DroppedDataReason::NUM_DROPPED_DATA_REASONS));
}

}  // namespace ukm
