// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TABLE_CELL_PAINT_INVALIDATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TABLE_CELL_PAINT_INVALIDATOR_H_

#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutObject;
class LayoutTableCell;
struct PaintInvalidatorContext;

class TableCellPaintInvalidator {
  STACK_ALLOCATED();

 public:
  TableCellPaintInvalidator(const LayoutTableCell& cell,
                            const PaintInvalidatorContext& context)
      : cell_(cell), context_(context) {}

  void InvalidatePaint();

 private:
  void InvalidateContainerForCellGeometryChange(
      const LayoutObject& container,
      const PaintInvalidatorContext& container_context);

  const LayoutTableCell& cell_;
  const PaintInvalidatorContext& context_;
};

}  // namespace blink

#endif
