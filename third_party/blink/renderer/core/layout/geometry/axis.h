// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_AXIS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_AXIS_H_

#include "base/types/strong_alias.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

enum {
  kLogicalAxisNone = 0,
  kLogicalAxisInline = 1 << 0,
  kLogicalAxisBlock = 1 << 1,
  kLogicalAxisBoth = kLogicalAxisInline | kLogicalAxisBlock
};

enum {
  kPhysicalAxisNone = 0,
  kPhysicalAxisHorizontal = 1 << 0,
  kPhysicalAxisVertical = 1 << 1,
  kPhysicalAxisBoth = kPhysicalAxisHorizontal | kPhysicalAxisVertical
};

using PhysicalAxes = base::StrongAlias<class PhysicalAxesTag, uint8_t>;
using LogicalAxes = base::StrongAlias<class LogicalAxesTag, uint8_t>;

inline PhysicalAxes ToPhysicalAxes(LogicalAxes logical, WritingMode mode) {
  if (IsHorizontalWritingMode(mode))
    return PhysicalAxes(logical.value());
  if (logical == LogicalAxes(kLogicalAxisInline))
    return PhysicalAxes(kPhysicalAxisVertical);
  if (logical == LogicalAxes(kLogicalAxisBlock))
    return PhysicalAxes(kPhysicalAxisHorizontal);
  return PhysicalAxes(logical.value());
}

inline LogicalAxes operator|(LogicalAxes a, LogicalAxes b) {
  return LogicalAxes(a.value() | b.value());
}

inline LogicalAxes& operator|=(LogicalAxes& a, LogicalAxes b) {
  a.value() |= b.value();
  return a;
}

inline LogicalAxes operator&(LogicalAxes a, LogicalAxes b) {
  return LogicalAxes(a.value() & b.value());
}

inline LogicalAxes operator&=(LogicalAxes& a, LogicalAxes b) {
  a.value() &= b.value();
  return a;
}

inline PhysicalAxes operator|(PhysicalAxes a, PhysicalAxes b) {
  return PhysicalAxes(a.value() | b.value());
}

inline PhysicalAxes& operator|=(PhysicalAxes& a, PhysicalAxes b) {
  a.value() |= b.value();
  return a;
}

inline PhysicalAxes operator&(PhysicalAxes a, PhysicalAxes b) {
  return PhysicalAxes(a.value() & b.value());
}

inline PhysicalAxes operator&=(PhysicalAxes& a, PhysicalAxes b) {
  a.value() &= b.value();
  return a;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_AXIS_H_
