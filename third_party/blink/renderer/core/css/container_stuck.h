// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_STUCK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_STUCK_H_

// Enum classes that represents whether a sticky positioned element is stuck to
// a scroll container edge for a given axis. Used for evaluating stuck state
// container queries.
enum class ContainerStuckLogical {
  // Not stuck
  kNo,
  // Stuck to inset-inline-start, or inset-block-start
  kStart,
  // Stuck to inset-inline-end, or inset-block-end
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

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CONTAINER_STUCK_H_
