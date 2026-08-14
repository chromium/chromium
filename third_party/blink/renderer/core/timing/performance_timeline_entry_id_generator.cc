// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/performance_timeline_entry_id_generator.h"

#include <cstdint>

#include "base/check.h"
#include "base/rand_util.h"

namespace blink {

void PerformanceTimelineEntryIdGenerator::ResetWebExposedId() {
  current_value_.web_exposed_id =
      base::RandIntInclusive(PerformanceTimelineEntryIdInfo::kMinId,
                             PerformanceTimelineEntryIdInfo::kMaxIdForReset);
}

PerformanceTimelineEntryIdInfo
PerformanceTimelineEntryIdGenerator::IncrementId() {
  current_value_.non_web_exposed_id++;
  current_value_.web_exposed_id += PerformanceTimelineEntryIdInfo::kIdIncrement;

  // Check for overflow, and reset if it happens.
  // Note: Its fine to temporarily overflow here, because kMaxId is within
  // uint64.
  if (current_value_.web_exposed_id > PerformanceTimelineEntryIdInfo::kMaxId) {
    ResetWebExposedId();
  }
  return current_value_;
}
}  // namespace blink
