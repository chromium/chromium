// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_SIZE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_SIZE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

class LayoutSize;
struct LogicalSize;

// PhysicalSize is the size of a rect (typically a fragment) in the physical
// coordinate system.
// For more information about physical and logical coordinate systems, see:
// https://chromium.googlesource.com/chromium/src/+/master/third_party/blink/renderer/core/layout/README.md#coordinate-spaces
struct CORE_EXPORT PhysicalSize {
  constexpr PhysicalSize() = default;
  constexpr PhysicalSize(LayoutUnit width, LayoutUnit height)
      : width(width), height(height) {}

  // For testing only. It's defined in core/testing/core_unit_test_helpers.h.
  inline PhysicalSize(int width, int height);

  LayoutUnit width;
  LayoutUnit height;

  LogicalSize ConvertToLogical(WritingMode mode) const {
    return mode == WritingMode::kHorizontalTb ? LogicalSize(width, height)
                                              : LogicalSize(height, width);
  }

  PhysicalSize operator+(const PhysicalSize& other) const {
    return PhysicalSize{this->width + other.width, this->height + other.height};
  }
  PhysicalSize& operator+=(const PhysicalSize& other) {
    *this = *this + other;
    return *this;
  }

  PhysicalSize operator-() const {
    return PhysicalSize{-this->width, -this->height};
  }
  PhysicalSize operator-(const PhysicalSize& other) const {
    return PhysicalSize{this->width - other.width, this->height - other.height};
  }
  PhysicalSize& operator-=(const PhysicalSize& other) {
    *this = *this - other;
    return *this;
  }

  constexpr bool operator==(const PhysicalSize& other) const {
    return std::tie(other.width, other.height) == std::tie(width, height);
  }
  constexpr bool operator!=(const PhysicalSize& other) const {
    return !(*this == other);
  }

  constexpr bool IsEmpty() const {
    return width == LayoutUnit() || height == LayoutUnit();
  }
  constexpr bool IsZero() const {
    return width == LayoutUnit() && height == LayoutUnit();
  }
  constexpr bool HasFraction() const {
    return width.HasFraction() || height.HasFraction();
  }

  void Scale(float s) {
    width *= s;
    height *= s;
  }
  void Scale(LayoutUnit s) {
    width *= s;
    height *= s;
  }

  void ClampNegativeToZero() {
    width = std::max(width, LayoutUnit());
    height = std::max(height, LayoutUnit());
  }

  PhysicalSize FitToAspectRatio(const PhysicalSize& aspect_ratio,
                                AspectRatioFit fit) const;

  // Conversions from/to existing code. New code prefers type safety for
  // logical/physical distinctions.
  constexpr explicit PhysicalSize(const LayoutSize& size)
      : width(size.Width()), height(size.Height()) {}
  constexpr LayoutSize ToLayoutSize() const { return {width, height}; }

  static PhysicalSize FromFloatSizeRound(const FloatSize& size) {
    return {LayoutUnit::FromFloatRound(size.Width()),
            LayoutUnit::FromFloatRound(size.Height())};
  }
  constexpr explicit operator FloatSize() const { return {width, height}; }

  explicit PhysicalSize(const IntSize& size)
      : width(size.Width()), height(size.Height()) {}

  String ToString() const;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&, const PhysicalSize&);

inline PhysicalSize ToPhysicalSize(const LogicalSize& other, WritingMode mode) {
  return mode == WritingMode::kHorizontalTb
             ? PhysicalSize(other.inline_size, other.block_size)
             : PhysicalSize(other.block_size, other.inline_size);
}

// TODO(crbug.com/962299): These functions should upgraded to force correct
// pixel snapping in a type-safe way.
inline IntSize RoundedIntSize(const PhysicalSize& s) {
  return {s.width.Round(), s.height.Round()};
}
inline IntSize FlooredIntSize(const PhysicalSize& s) {
  return {s.width.Floor(), s.height.Floor()};
}
inline IntSize CeiledIntSize(const PhysicalSize& s) {
  return {s.width.Ceil(), s.height.Ceil()};
}

// TODO(wangxianzhu): For temporary conversion from LayoutSize to PhysicalSize,
// where the input will be changed to PhysicalSize soon, to avoid redundant
// PhysicalSize() which can't be discovered by the compiler.
inline PhysicalSize PhysicalSizeToBeNoop(const LayoutSize& s) {
  return PhysicalSize(s);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_PHYSICAL_SIZE_H_
