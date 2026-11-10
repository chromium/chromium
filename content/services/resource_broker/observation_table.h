// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_RESOURCE_BROKER_OBSERVATION_TABLE_H_
#define CONTENT_SERVICES_RESOURCE_BROKER_OBSERVATION_TABLE_H_

#include <cstddef>
#include <cstdint>

#include "base/containers/circular_deque.h"
#include "content/services/resource_broker/observation_record.h"

namespace content {

// Bounded ring buffer storing the most recent `max_entries` observations,
// ordered newest first. Used for diagnostics and testing rather than metrics
// aggregation.
class ObservationTable {
 public:
  explicit ObservationTable(size_t max_entries);

  ObservationTable(const ObservationTable&) = delete;
  ObservationTable& operator=(const ObservationTable&) = delete;

  ~ObservationTable();

  // Inserts `record` at the front, evicting the oldest entry if at capacity.
  void AddRecord(ObservationRecord record);

  size_t size() const { return records_.size(); }
  uint64_t eviction_count() const { return eviction_count_; }
  const base::circular_deque<ObservationRecord>& records() const {
    return records_;
  }

 private:
  const size_t max_entries_;
  base::circular_deque<ObservationRecord> records_;
  uint64_t eviction_count_ = 0;
};

}  // namespace content

#endif  // CONTENT_SERVICES_RESOURCE_BROKER_OBSERVATION_TABLE_H_
