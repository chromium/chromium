// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/writing_direction_mode.h"

#include <array>
#include <ostream>

namespace blink {

namespace {

using PhysicalDirectionMap =
    std::array<PhysicalDirection,
               static_cast<size_t>(WritingMode::kMaxWritingMode) + 1>;
// Following six arrays contain values for horizontal-tb, vertical-rl,
// vertical-lr, sideways-rl, and sideways-lr in this order.
constexpr PhysicalDirectionMap kInlineStartMap = {
    PhysicalDirection::kLeft, PhysicalDirection::kUp, PhysicalDirection::kUp,
    PhysicalDirection::kUp, PhysicalDirection::kDown};
constexpr PhysicalDirectionMap kInlineEndMap = {
    PhysicalDirection::kRight, PhysicalDirection::kDown,
    PhysicalDirection::kDown, PhysicalDirection::kDown, PhysicalDirection::kUp};
constexpr PhysicalDirectionMap kBlockStartMap = {
    PhysicalDirection::kUp, PhysicalDirection::kRight, PhysicalDirection::kLeft,
    PhysicalDirection::kRight, PhysicalDirection::kLeft};
constexpr PhysicalDirectionMap kBlockEndMap = {
    PhysicalDirection::kDown, PhysicalDirection::kLeft,
    PhysicalDirection::kRight, PhysicalDirection::kLeft,
    PhysicalDirection::kRight};
constexpr PhysicalDirectionMap kLineOverMap = {
    PhysicalDirection::kUp, PhysicalDirection::kRight,
    PhysicalDirection::kRight, PhysicalDirection::kRight,
    PhysicalDirection::kLeft};
constexpr PhysicalDirectionMap kLineUnderMap = {
    PhysicalDirection::kDown, PhysicalDirection::kLeft,
    PhysicalDirection::kLeft, PhysicalDirection::kLeft,
    PhysicalDirection::kRight};

}  // namespace

PhysicalDirection WritingDirectionMode::InlineStart() const {
  if (direction_ == TextDirection::kLtr) {
    return kInlineStartMap[static_cast<int>(writing_mode_)];
  }
  return kInlineEndMap[static_cast<int>(writing_mode_)];
}

PhysicalDirection WritingDirectionMode::InlineEnd() const {
  if (direction_ == TextDirection::kLtr) {
    return kInlineEndMap[static_cast<int>(writing_mode_)];
  }
  return kInlineStartMap[static_cast<int>(writing_mode_)];
}

PhysicalDirection WritingDirectionMode::BlockStart() const {
  return kBlockStartMap[static_cast<int>(writing_mode_)];
}

PhysicalDirection WritingDirectionMode::BlockEnd() const {
  return kBlockEndMap[static_cast<int>(writing_mode_)];
}

PhysicalDirection WritingDirectionMode::LineOver() const {
  return kLineOverMap[static_cast<int>(writing_mode_)];
}

PhysicalDirection WritingDirectionMode::LineUnder() const {
  return kLineUnderMap[static_cast<int>(writing_mode_)];
}

std::ostream& operator<<(std::ostream& ostream,
                         const WritingDirectionMode& writing_direction) {
  return ostream << writing_direction.GetWritingMode() << " "
                 << writing_direction.Direction();
}

}  // namespace blink
