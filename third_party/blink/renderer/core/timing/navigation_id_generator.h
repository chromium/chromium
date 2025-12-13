// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_NAVIGATION_ID_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_NAVIGATION_ID_GENERATOR_H_

#include "base/gtest_prod_util.h"
#include "third_party/blink/public/web/web_performance_metrics_for_reporting.h"
#include "third_party/blink/renderer/core/core_export.h"

namespace blink {
// The value 0 indicates the absence of a navigation id.
// It's used for when there's no navigation id, e.g. in service workers.
inline constexpr uint32_t kNavigationIdAbsentValue = 0;

// Implements the navigationId as specified in
// https://w3c.github.io/performance-timeline/:
//
// * The constructor (and the ResetNavigationId() method) will assign a randomly
//   generated navigationId between 10 and 10000. This is used for hard
//   navigations.
// * The IncrementNavigationId() method will increment the navigationId by a
//   small integer. This is used for soft navigations and back-forward cache
//   restorations.
//
// TODO(https://github.com/w3c/performance-timeline/pull/219): Spec updates are
// in progress and this comment may need revision.
class CORE_EXPORT NavigationIdGenerator {
 public:
  NavigationIdGenerator() { ResetNavigationId(); }
  NavigationIdGenerator(const NavigationIdGenerator&) = delete;
  NavigationIdGenerator& operator=(const NavigationIdGenerator&) = delete;

  // Increments the navigation id by a small integer.
  void IncrementNavigationId();

  // Returns the current navigation id; see
  // https://w3c.github.io/performance-timeline/ for the spec.
  uint32_t NavigationId() const { return navigation_id_; }

 private:
  // Resets the navigation id to a randomly generated value.
  void ResetNavigationId();

  FRIEND_TEST_ALL_PREFIXES(NavigationIdGeneratorTest, SoftNavigationsOverflow);
  uint32_t navigation_id_ = kNavigationIdAbsentValue;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TIMING_NAVIGATION_ID_GENERATOR_H_
