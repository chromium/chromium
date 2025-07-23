// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/navigation_id_generator.h"

#include <cstdint>
#include <limits>

#include "base/check.h"
#include "base/rand_util.h"

namespace blink {
namespace {
// The minimum navigation id is 100, to avoid treatment as a counter.
constexpr uint32_t kMinNavigationId = 100;
// The max navigation id is 2^31 - 1, to avoid awkwardness with JSON
// and Javascript APIs. This is implementation dependent but compatible -
// according to spec it's an unsigned long long, which would be a uint64_t.
constexpr uint32_t kMaxNavigationId = std::numeric_limits<int32_t>::max();
// When a navigationId is randomly assigned, it's in the range between
// kMinNavigationId and kMaxNavigationIdForReset, inclusive.
constexpr uint32_t kMaxNavigationIdForReset = 10 * 1000;
// Soft navigations are assigned navigation ids by incrementing from
// the previous navigation id, it is a small increment to avoid treatment as
// a counter while preserving order. This intends to conform to the spec
// ("a small increment").
constexpr uint32_t kNavigationIdIncrement = 7;
}  // namespace

void NavigationIdGenerator::ResetNavigationId() {
  navigation_id_ = base::RandInt(kMinNavigationId, kMaxNavigationIdForReset);
}

void NavigationIdGenerator::IncrementNavigationId() {
  CHECK_NE(navigation_id_, kNavigationIdAbsentValue);
  // Check for overflow, and reset the navigation id if it happens.
  if (navigation_id_ > kMaxNavigationId - kNavigationIdIncrement) {
    ResetNavigationId();
    return;
  }
  navigation_id_ += kNavigationIdIncrement;
}
}  // namespace blink
