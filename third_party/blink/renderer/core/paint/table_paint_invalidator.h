// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TABLE_PAINT_INVALIDATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_TABLE_PAINT_INVALIDATOR_H_

#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutTable;
struct PaintInvalidatorContext;

class TablePaintInvalidator {
  STACK_ALLOCATED();

 public:
  TablePaintInvalidator(const LayoutTable& table,
                        const PaintInvalidatorContext& context)
      : table_(table), context_(context) {}

  void InvalidatePaint();

 private:
  const LayoutTable& table_;
  const PaintInvalidatorContext& context_;
};

}  // namespace blink

#endif
