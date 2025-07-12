// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_PERFORMANCE_TIMELINE_CONSTANTS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_PERFORMANCE_TIMELINE_CONSTANTS_H_

#include "base/time/time.h"

namespace blink {

inline constexpr uint32_t kSoftNavigationCountDefaultValue = 0;

// The default value (0) indicates the absence of a navigation id.
// It's used for initialization and for cases when there's no navigation id
// e.g. in service workers. See also navigation_id_generator.h.
inline constexpr uint32_t kNavigationIdDefaultValue = 0;

struct SoftNavigationMetrics {
  uint32_t count = kSoftNavigationCountDefaultValue;
  base::TimeDelta start_time;
  base::TimeDelta first_contentful_paint;
  uint32_t navigation_id = kNavigationIdDefaultValue;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERFORMANCE_PERFORMANCE_TIMELINE_CONSTANTS_H_
