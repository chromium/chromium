// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TABLE_CELL_PAINT_INVALIDATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TABLE_CELL_PAINT_INVALIDATOR_H_

#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutNGTableCell;
struct PaintInvalidatorContext;

class NGTableCellPaintInvalidator {
  STACK_ALLOCATED();

 public:
  NGTableCellPaintInvalidator(const LayoutNGTableCell& cell,
                              const PaintInvalidatorContext& context)
      : cell_(cell), context_(context) {}

  void InvalidatePaint();

 private:
  const LayoutNGTableCell& cell_;
  const PaintInvalidatorContext& context_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_TABLE_CELL_PAINT_INVALIDATOR_H_
