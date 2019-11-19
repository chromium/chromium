// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_FIELDSET_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_FIELDSET_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class NGPhysicalBoxFragment;
struct NGLink;
struct PaintInfo;
struct PhysicalOffset;

class NGFieldsetPainter {
  STACK_ALLOCATED();

 public:
  NGFieldsetPainter(const NGPhysicalBoxFragment& fieldset)
      : fieldset_(fieldset) {}

  void PaintBoxDecorationBackground(const PaintInfo&, const PhysicalOffset&);

 private:
  void PaintFieldsetDecorationBackground(const NGLink* legend,
                                         const PaintInfo&,
                                         const PhysicalOffset&);
  void PaintLegend(const NGPhysicalBoxFragment& legend, const PaintInfo&);

  const NGPhysicalBoxFragment& fieldset_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_FIELDSET_PAINTER_H_
