// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_METRICS_PUBLIC_CPP_UKM_ENTRY_BUILDER_BASE_H_
#define SERVICES_METRICS_PUBLIC_CPP_UKM_ENTRY_BUILDER_BASE_H_

#include "services/metrics/public/cpp/metrics_export.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"

namespace ukm::internal {

// A base class for generated UkmEntry builder objects.
// This class should not be used directly.
class METRICS_EXPORT UkmEntryBuilderBase {
 public:
  UkmEntryBuilderBase(const UkmEntryBuilderBase&) = delete;
  UkmEntryBuilderBase(UkmEntryBuilderBase&&);
  UkmEntryBuilderBase& operator=(const UkmEntryBuilderBase&) = delete;
  UkmEntryBuilderBase& operator=(UkmEntryBuilderBase&&);

  virtual ~UkmEntryBuilderBase();

  // Records the complete entry into the recorder. If recorder is null, the
  // entry is simply discarded. The |entry_| is used up by this call so
  // further calls to this or TakeEntry() will do nothing.
  void Record(UkmRecorder* recorder);

  // Return a copy of created UkmEntryPtr for testing.
  mojom::UkmEntryPtr GetEntryForTesting();

  // Transfers ownership of |entry_| externally.
  mojom::UkmEntryPtr TakeEntry() { return std::move(entry_); }

 protected:
  UkmEntryBuilderBase(ukm::SourceIdObj source_id, uint64_t event_hash);
  // TODO(crbug.com/40589246): Remove this version once callers are migrated.
  UkmEntryBuilderBase(SourceId source_id, uint64_t event_hash);

  // Add metric to the entry. A metric contains a metric hash and value.
  void SetMetricInternal(uint64_t metric_hash, int64_t value);

 private:
  mojom::UkmEntryPtr entry_;
};

}  // namespace ukm::internal

#endif  // SERVICES_METRICS_PUBLIC_CPP_UKM_ENTRY_BUILDER_BASE_H_
