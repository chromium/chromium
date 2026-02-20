// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/geometry/axis.h"

#include <ostream>

#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

String ToString(LogicalAxes axes) {
  if (axes == kLogicalAxesNone) {
    return "kLogicalAxesNone";
  }
  if (axes == kLogicalAxesInline) {
    return "kLogicalAxesInline";
  }
  if (axes == kLogicalAxesBlock) {
    return "kLogicalAxesBlock";
  }
  if (axes == kLogicalAxesBoth) {
    return "kLogicalAxesBoth";
  }

  // Fallback: cast .value() to int so it prints a number, not an invisible char
  return String::Format("LogicalAxes(%d)", static_cast<int>(axes.value()));
}

String ToString(PhysicalAxes axes) {
  if (axes == kPhysicalAxesNone) {
    return "kPhysicalAxesNone";
  }
  if (axes == kPhysicalAxesHorizontal) {
    return "kPhysicalAxesHorizontal";
  }
  if (axes == kPhysicalAxesVertical) {
    return "kPhysicalAxesVertical";
  }
  if (axes == kPhysicalAxesBoth) {
    return "kPhysicalAxesBoth";
  }
  // Fallback: cast .value() to int so it prints a number, not an invisible char
  return String::Format("PhysicalAxes(%d)", static_cast<int>(axes.value()));
}

std::ostream& operator<<(std::ostream& os, LogicalAxes axes) {
  return os << ToString(axes);
}

std::ostream& operator<<(std::ostream& os, PhysicalAxes axes) {
  return os << ToString(axes);
}

}  // namespace blink
