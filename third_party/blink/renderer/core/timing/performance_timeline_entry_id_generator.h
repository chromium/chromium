// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_TIMELINE_ENTRY_ID_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_TIMELINE_ENTRY_ID_GENERATOR_H_

#include <cstdint>

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

// Represents a unique ID and its associated offset.
struct PerformanceTimelineEntryIdInfo {
  // The value 0 indicates the absence of an ID.
  static constexpr uint64_t kNoId = 0;

  // The minimum ID is 100, to avoid treatment as a counter.
  static constexpr uint64_t kMinId = 100;
  // The max ID is 2^53 - 1, which is Number.MAX_SAFE_INTEGER in JavaScript.
  static constexpr uint64_t kMaxId = (1ULL << 53) - 1;
  // When an ID is randomly assigned, it's in the range between
  // kMinId and kMaxIdForReset, inclusive.
  static constexpr uint64_t kMaxIdForReset = 10000;
  // IDs are incremented by a small amount to avoid treatment as a counter while
  // preserving order. This intends to conform to the spec ("a small
  // increment").
  static constexpr uint64_t kIdIncrement = 7;

  // This is a pseudo-random looking number, but in practice is just a simple:
  // initial_value + (offset * increment), and increment is always 7.
  // The value 0 (kNoId) is used to indicate that no ID is assigned.
  uint64_t id = kNoId;
  // Offset is the true "count" of this entry, without id obfuscation.
  uint64_t offset = 0;

  bool operator==(const PerformanceTimelineEntryIdInfo& other) const = default;
  bool operator!=(const PerformanceTimelineEntryIdInfo& other) const = default;

  static const PerformanceTimelineEntryIdInfo kNone;
};

inline constexpr PerformanceTimelineEntryIdInfo
    PerformanceTimelineEntryIdInfo::kNone = {
        PerformanceTimelineEntryIdInfo::kNoId, 0};

// Implements ID generation for Performance Timeline entries as specified in
// Event Timing and Soft Navigations specs.  See:
// https://w3c.github.io/event-timing/#user-interaction-value
//
// * The constructor (and the ResetId() method) will assign a randomly
//   generated ID between 100 and 10000. This is used for initial values (e.g.,
//   the first navigation or interaction).
// * The IncrementId() method will increment an internal offset.
// * The GetValue() method returns a struct containing:
//   (initial_id + offset * increment) and the offset.
//
// This logic is designed to discourage developers from using the ID to 'count'
// the number of entries, while still providing unique and ordered values.
class CORE_EXPORT PerformanceTimelineEntryIdGenerator {
 public:
  PerformanceTimelineEntryIdGenerator() { ResetId(); }
  PerformanceTimelineEntryIdGenerator(
      const PerformanceTimelineEntryIdGenerator&) = delete;
  PerformanceTimelineEntryIdGenerator& operator=(
      const PerformanceTimelineEntryIdGenerator&) = delete;

  // Increments the internal offset and returns the new ID and offset values.
  PerformanceTimelineEntryIdInfo IncrementId();

  // Returns the current ID and offset values.
  PerformanceTimelineEntryIdInfo GetValue() const { return current_value_; }

 private:
  // Resets the ID to a randomly generated value and resets offset to 0.
  void ResetId();

  FRIEND_TEST_ALL_PREFIXES(PerformanceTimelineEntryIdGeneratorTest, IdOverflow);

  PerformanceTimelineEntryIdInfo current_value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_TIMELINE_ENTRY_ID_GENERATOR_H_
