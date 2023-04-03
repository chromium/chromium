// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FRAGMENTATION_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FRAGMENTATION_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// A fragmentation context is either established by a multicol container, or by
// pages when printing. A fragmentation context consists of a series of
// fragmentainers. A fragmentainer is simply a column or page, depending on the
// type of fragmentation context. [1]
//
// It should be noted that a multicol container may be nested inside another
// fragmentation context (another multicol container, or the pages when
// printing), although this class doesn't deal with that (it's internal to the
// multicol implementation).
//
// [1] http://www.w3.org/TR/css3-break/#fragmentation-model
class CORE_EXPORT FragmentationContext : public GarbageCollectedMixin {
 public:
  virtual ~FragmentationContext() = default;

  // Return the flow thread of the fragmentation context, if it is a multicol
  // fragmentation context. Since multicol containers may be nested inside other
  // fragmentation contexts, sometimes we need to know if it's a multicol
  // container that we're dealing with.
  virtual class LayoutMultiColumnFlowThread* AssociatedFlowThread() {
    return nullptr;
  }

  void Trace(Visitor*) const override {}
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FRAGMENTATION_CONTEXT_H_
