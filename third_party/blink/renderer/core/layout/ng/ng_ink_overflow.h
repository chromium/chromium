// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_INK_OVERFLOW_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_INK_OVERFLOW_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"

namespace blink {

class ComputedStyle;
struct NGTextFragmentPaintInfo;

// Represents an ink-overflow for objects without children, such as text runs.
struct CORE_EXPORT NGInkOverflow {
  USING_FAST_MALLOC(NGInkOverflow);

 public:
  NGInkOverflow(const PhysicalRect& self_ink_overflow)
      : self_ink_overflow(self_ink_overflow) {}

  static void ComputeTextInkOverflow(
      const NGTextFragmentPaintInfo& text_info,
      const ComputedStyle& style,
      const PhysicalSize& size,
      std::unique_ptr<NGInkOverflow>* ink_overflow_out);

  PhysicalRect self_ink_overflow;
};

// Represents an ink-overflow for objects with children, such as boxes. Keeps
// ink-overflow for self and contents separately.
struct CORE_EXPORT NGContainerInkOverflow : NGInkOverflow {
  USING_FAST_MALLOC(NGContainerInkOverflow);

 public:
  NGContainerInkOverflow(const PhysicalRect& self_ink_overflow,
                         const PhysicalRect& contents_ink_overflow)
      : NGInkOverflow(self_ink_overflow),
        contents_ink_overflow(contents_ink_overflow) {}

  PhysicalRect SelfAndContentsInkOverflow() const {
    return UnionRect(self_ink_overflow, contents_ink_overflow);
  }

  PhysicalRect contents_ink_overflow;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_INK_OVERFLOW_H_
