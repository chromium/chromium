// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/resource_broker/observation_table.h"

#include <utility>

#include "base/check_op.h"

namespace content {

ObservationTable::ObservationTable(size_t max_entries)
    : max_entries_(max_entries) {
  CHECK_GT(max_entries_, 0u);
}

ObservationTable::~ObservationTable() = default;

void ObservationTable::AddRecord(ObservationRecord record) {
  if (records_.size() == max_entries_) {
    records_.pop_back();
    eviction_count_++;
  }
  records_.push_front(std::move(record));
}

}  // namespace content
