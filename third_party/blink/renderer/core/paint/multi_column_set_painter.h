// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_MULTI_COLUMN_SET_PAINTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_MULTI_COLUMN_SET_PAINTER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutMultiColumnSet;
struct PaintInfo;
struct PhysicalOffset;

class MultiColumnSetPainter {
  STACK_ALLOCATED();

 public:
  MultiColumnSetPainter(const LayoutMultiColumnSet& layout_multi_column_set)
      : layout_multi_column_set_(layout_multi_column_set) {}
  void PaintObject(const PaintInfo&, const PhysicalOffset& paint_offset);

 private:
  void PaintColumnRules(const PaintInfo&, const PhysicalOffset& paint_offset);

  const LayoutMultiColumnSet& layout_multi_column_set_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_MULTI_COLUMN_SET_PAINTER_H_
