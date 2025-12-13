// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_HIGH_ENTROPY_OP_TYPE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_HIGH_ENTROPY_OP_TYPE_H_

#include <concepts>
#include <cstdint>

namespace blink {

// The types of operations performed on the canvas that would result in a high
// entropy operation, and thus might result in the canvas being noised.
enum class HighEntropyCanvasOpType {
  kNone = 0,
  kArc = 1,
  kEllipse = 2,
  kSetShadowBlur = 4,
  kSetShadowColor = 8,
  kGlobalCompositionOperation = 16,
  kFillText = 32,
  kStrokeText = 64,
  kCopyFromCanvas = 128,
  kMaxValue = kCopyFromCanvas
};

inline constexpr HighEntropyCanvasOpType operator|(HighEntropyCanvasOpType a,
                                                   HighEntropyCanvasOpType b) {
  return static_cast<HighEntropyCanvasOpType>(static_cast<int>(a) |
                                              static_cast<int>(b));
}
inline constexpr HighEntropyCanvasOpType& operator|=(
    HighEntropyCanvasOpType& a,
    HighEntropyCanvasOpType b) {
  a = a | b;
  return a;
}

inline constexpr HighEntropyCanvasOpType operator&(HighEntropyCanvasOpType a,
                                                   HighEntropyCanvasOpType b) {
  return static_cast<HighEntropyCanvasOpType>(static_cast<int>(a) &
                                              static_cast<int>(b));
}
inline constexpr HighEntropyCanvasOpType& operator&=(
    HighEntropyCanvasOpType& a,
    HighEntropyCanvasOpType b) {
  a = a & b;
  return a;
}

inline constexpr bool ShouldPropagateHighEntropyCanvasOpTypes(
    HighEntropyCanvasOpType op_types,
    bool has_accelerated_rendering) {
  return has_accelerated_rendering ||
         static_cast<int>(op_types &
                          HighEntropyCanvasOpType::kCopyFromCanvas) != 0;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_HIGH_ENTROPY_OP_TYPE_H_
