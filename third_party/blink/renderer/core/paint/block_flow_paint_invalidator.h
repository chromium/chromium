// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BLOCK_FLOW_PAINT_INVALIDATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_BLOCK_FLOW_PAINT_INVALIDATOR_H_

#include "third_party/blink/renderer/platform/graphics/paint_invalidation_reason.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutBlockFlow;

class BlockFlowPaintInvalidator {
  STACK_ALLOCATED();

 public:
  BlockFlowPaintInvalidator(const LayoutBlockFlow& block_flow)
      : block_flow_(block_flow) {}

  void InvalidatePaintForOverhangingFloats() {
    InvalidatePaintForOverhangingFloatsInternal(kInvalidateDescendants);
  }

  void InvalidateDisplayItemClients(PaintInvalidationReason);

 private:
  enum InvalidateDescendantMode {
    kDontInvalidateDescendants,
    kInvalidateDescendants
  };
  void InvalidatePaintForOverhangingFloatsInternal(InvalidateDescendantMode);

  const LayoutBlockFlow& block_flow_;
};

}  // namespace blink

#endif
