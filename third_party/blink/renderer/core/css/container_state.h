// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_STATE_H_

namespace blink {

// Enum classes that represents whether a sticky positioned element is stuck to
// a scroll container edge for a given axis. Used for evaluating stuck state
// container queries.
enum class ContainerStuckLogical {
  // Not stuck
  kNo,
  // Stuck to inline-start, or block-start
  kStart,
  // Stuck to inline-end, or block-end
  kEnd,
};

enum class ContainerStuckPhysical {
  kNo,
  kLeft,
  kRight,
  kTop,
  kBottom,
};

inline ContainerStuckLogical Flip(ContainerStuckLogical stuck) {
  switch (stuck) {
    case ContainerStuckLogical::kNo:
      return ContainerStuckLogical::kNo;
    case ContainerStuckLogical::kStart:
      return ContainerStuckLogical::kEnd;
    case ContainerStuckLogical::kEnd:
      return ContainerStuckLogical::kStart;
  }
}

// Flags that represent whether a scroll-snapped query container is snapped to
// its scroll container in a given direction.
enum class ContainerSnapped {
  kNone = 0,
  kX = 1 << 0,
  kY = 1 << 1,
};

using ContainerSnappedFlags = unsigned;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_STATE_H_
