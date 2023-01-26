// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_OFFSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_OFFSET_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_timeline_range.h"
#include "third_party/blink/renderer/platform/geometry/length.h"

namespace blink {

struct TimelineOffset {
  using NamedRange = V8TimelineRange::Enum;

  TimelineOffset() = default;
  TimelineOffset(NamedRange name, Length offset) : name(name), offset(offset) {}

  NamedRange name = NamedRange::kNone;
  Length offset = Length::Fixed();

  bool operator==(const TimelineOffset& other) const {
    return name == other.name && offset == other.offset;
  }

  bool operator!=(const TimelineOffset& other) const {
    return !(*this == other);
  }

  String ToString() const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_ANIMATION_TIMELINE_OFFSET_H_
