// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FIELDSET_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FIELDSET_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutFieldset;
struct PaintInfo;
struct PhysicalOffset;

class FieldsetPainter {
  STACK_ALLOCATED();

 public:
  FieldsetPainter(const LayoutFieldset& layout_fieldset)
      : layout_fieldset_(layout_fieldset) {}

  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const PhysicalOffset& paint_offset);
  void PaintMask(const PaintInfo&, const PhysicalOffset& paint_offset);

 private:
  const LayoutFieldset& layout_fieldset_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_FIELDSET_PAINTER_H_
