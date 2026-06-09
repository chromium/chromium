// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_NAVIGATION_PHASE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_NAVIGATION_PHASE_H_

#include <stdint.h>

namespace blink {

// https://drafts.csswg.org/css-navigation-1/#typedef-navigation-phase-keyword
enum class NavigationPhase : uint8_t {
  kLoading,
  kReady,
  kCommitted,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_NAVIGATION_PHASE_H_
