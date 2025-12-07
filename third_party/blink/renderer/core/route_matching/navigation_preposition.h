// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_NAVIGATION_PREPOSITION_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_NAVIGATION_PREPOSITION_H_

#include <stdint.h>

namespace blink {

enum class NavigationPreposition : uint8_t {
  kAt,
  kFrom,
  kTo,
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ROUTE_MATCHING_NAVIGATION_PREPOSITION_H_
