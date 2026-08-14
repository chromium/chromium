// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_TIMELINE_ENTRY_ID_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_TIMELINE_ENTRY_ID_GENERATOR_H_

#include <cstdint>
#include <ostream>

#include "base/gtest_prod_util.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {

// Represents a unique ID and its associated offset.
struct PerformanceTimelineEntryIdInfo {
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
  // initial_value + (non_web_exposed_id * increment), and increment is
  // always 7. The value 0 is used to indicate that no ID is assigned.
  uint64_t web_exposed_id;
  // non_web_exposed_id is the monotonic ordinal counter for this entry,
  // without id obfuscation. Used for internal metric tracking and Mojo IPC.
  uint64_t non_web_exposed_id;

  bool operator==(const PerformanceTimelineEntryIdInfo& other) const = default;
  bool operator!=(const PerformanceTimelineEntryIdInfo& other) const = default;

  // Sentinel constant representing the absence of an ID (e.g. for events that
  // are not user interactions or before an ID has been assigned). Generators
  // only ever produce valid, non-zero IDs (with non_web_exposed_id >= 1), so
  // kNone serves as an explicit, safe sentinel instead of using magic 0
  // numbers.
  static const PerformanceTimelineEntryIdInfo kNone;
};

// This is needed to support EXPECT_EQ(id1, id2), since it prints when they
// dont match.
inline std::ostream& operator<<(std::ostream& os,
                                const PerformanceTimelineEntryIdInfo& info) {
  return os << "{web_exposed_id: " << info.web_exposed_id
            << ", non_web_exposed_id: " << info.non_web_exposed_id << "}";
}

// Defined out-of-line as `inline constexpr` because C++ does not allow
// in-class `static constexpr` initialization of an incomplete type.
inline constexpr PerformanceTimelineEntryIdInfo
    PerformanceTimelineEntryIdInfo::kNone = {
        .web_exposed_id = 0,
        .non_web_exposed_id = 0,
};

// Implements ID generation for Performance Timeline entries as specified in
// Event Timing and Soft Navigations specs.  See:
// https://w3c.github.io/event-timing/#user-interaction-value
//
// * The default constructor will assign a randomly generated web-exposed ID
//   between 100 and 10000, and a non_web_exposed_id of 1.
//
// This logic is designed to discourage developers from using the ID to 'count'
// the number of entries, while still providing unique and ordered values.
class CORE_EXPORT PerformanceTimelineEntryIdGenerator {
 public:
  PerformanceTimelineEntryIdGenerator() {
    current_value_.non_web_exposed_id = 1;
    ResetWebExposedId();
  }
  PerformanceTimelineEntryIdGenerator(
      const PerformanceTimelineEntryIdGenerator&) = delete;
  PerformanceTimelineEntryIdGenerator& operator=(
      const PerformanceTimelineEntryIdGenerator&) = delete;

  // Advances the generator to the next ID values: increments the internal
  // monotonic non_web_exposed_id ordinal counter, advances web_exposed_id by
  // a small increment (kIdIncrement), and resets web_exposed_id to a new
  // pseudo-random value if it exceeds kMaxId. Returns the updated ID struct.
  PerformanceTimelineEntryIdInfo IncrementId();

  PerformanceTimelineEntryIdInfo GetValue() const { return current_value_; }

 private:
  // Resets the web-exposed ID to a randomly generated value between
  // kMinId and kMaxIdForReset.
  void ResetWebExposedId();

  FRIEND_TEST_ALL_PREFIXES(PerformanceTimelineEntryIdGeneratorTest, IdOverflow);

  PerformanceTimelineEntryIdInfo current_value_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_PERFORMANCE_TIMELINE_ENTRY_ID_GENERATOR_H_
