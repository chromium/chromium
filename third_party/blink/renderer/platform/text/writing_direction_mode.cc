// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/writing_direction_mode.h"

#include <array>
#include <ostream>

#include "third_party/blink/renderer/platform/geometry/logical_direction.h"

namespace blink {

namespace {

using PhysicalDirectionMap =
    std::array<PhysicalDirection,
               static_cast<size_t>(WritingMode::kMaxWritingMode) + 1>;
// Following ten arrays contain values for horizontal-tb, vertical-rl,
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

using LogicalDirectionMap =
    std::array<LogicalDirection,
               static_cast<size_t>(WritingMode::kMaxWritingMode) + 1>;
// Following ten arrays contain values for horizontal-tb, vertical-rl,
// vertical-lr, sideways-rl, and sideways-lr in this order.
constexpr LogicalDirectionMap kTopMap = {
    LogicalDirection::kBlockStart,  LogicalDirection::kInlineStart,
    LogicalDirection::kInlineStart, LogicalDirection::kInlineStart,
    LogicalDirection::kInlineEnd,
};
constexpr LogicalDirectionMap kRightMap = {
    LogicalDirection::kInlineEnd, LogicalDirection::kBlockStart,
    LogicalDirection::kBlockEnd,  LogicalDirection::kBlockStart,
    LogicalDirection::kBlockEnd,
};
constexpr LogicalDirectionMap kBottomMap = {
    LogicalDirection::kBlockEnd,    LogicalDirection::kInlineEnd,
    LogicalDirection::kInlineEnd,   LogicalDirection::kInlineEnd,
    LogicalDirection::kInlineStart,
};
constexpr LogicalDirectionMap kLeftMap = {
    LogicalDirection::kInlineStart, LogicalDirection::kBlockEnd,
    LogicalDirection::kBlockStart,  LogicalDirection::kBlockEnd,
    LogicalDirection::kBlockStart,
};

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

LogicalDirection WritingDirectionMode::Top() const {
  if (IsLtr() || IsHorizontalWritingMode(writing_mode_)) {
    return kTopMap[static_cast<int>(writing_mode_)];
  }
  return kBottomMap[static_cast<int>(writing_mode_)];
}

LogicalDirection WritingDirectionMode::Right() const {
  if (IsLtr() || !IsHorizontalWritingMode(writing_mode_)) {
    return kRightMap[static_cast<int>(writing_mode_)];
  }
  return kLeftMap[static_cast<int>(writing_mode_)];
}

LogicalDirection WritingDirectionMode::Bottom() const {
  if (IsLtr() || IsHorizontalWritingMode(writing_mode_)) {
    return kBottomMap[static_cast<int>(writing_mode_)];
  }
  return kTopMap[static_cast<int>(writing_mode_)];
}

LogicalDirection WritingDirectionMode::Left() const {
  if (IsLtr() || !IsHorizontalWritingMode(writing_mode_)) {
    return kLeftMap[static_cast<int>(writing_mode_)];
  }
  return kRightMap[static_cast<int>(writing_mode_)];
}

std::ostream& operator<<(std::ostream& ostream,
                         const WritingDirectionMode& writing_direction) {
  return ostream << writing_direction.GetWritingMode() << " "
                 << writing_direction.Direction();
}

}  // namespace blink
