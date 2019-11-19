// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LINE_BOX_LIST_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LINE_BOX_LIST_PAINTER_H_

#include "third_party/blink/renderer/core/style/computed_style_constants.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class LayoutBoxModelObject;
class LineBoxList;
struct PaintInfo;
struct PhysicalOffset;
struct PhysicalRect;

class LineBoxListPainter {
  STACK_ALLOCATED();

 public:
  LineBoxListPainter(const LineBoxList& line_box_list)
      : line_box_list_(line_box_list) {}

  void Paint(const LayoutBoxModelObject&,
             const PaintInfo&,
             const PhysicalOffset& paint_offset) const;
  void PaintBackplate(const LayoutBoxModelObject& layout_object,
                      const PaintInfo&,
                      const PhysicalOffset& paint_offset) const;

 private:
  const LineBoxList& line_box_list_;

  bool ShouldPaint(const LayoutBoxModelObject& layout_object,
                   const PaintInfo& paint_info,
                   const PhysicalOffset& paint_offset) const;

  // Returns a vector of backplates that surround the paragraphs of text within
  // line_box_list_.
  Vector<PhysicalRect> GetBackplates(const PhysicalOffset& paint_offset) const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LINE_BOX_LIST_PAINTER_H_
