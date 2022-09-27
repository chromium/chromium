// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_MATHML_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_MATHML_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace gfx {
class Rect;
}

namespace blink {

struct PaintInfo;
struct PhysicalOffset;
class NGPhysicalBoxFragment;

class NGMathMLPainter {
  STACK_ALLOCATED();

 public:
  explicit NGMathMLPainter(const NGPhysicalBoxFragment& box_fragment)
      : box_fragment_(box_fragment) {}
  void Paint(const PaintInfo&, PhysicalOffset);

 private:
  void PaintBar(const PaintInfo&, const gfx::Rect&);
  void PaintFractionBar(const PaintInfo&, PhysicalOffset);
  void PaintOperator(const PaintInfo&, PhysicalOffset);
  void PaintRadicalSymbol(const PaintInfo&, PhysicalOffset);
  void PaintStretchyOrLargeOperator(const PaintInfo&, PhysicalOffset);

  const NGPhysicalBoxFragment& box_fragment_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_MATHML_PAINTER_H_
