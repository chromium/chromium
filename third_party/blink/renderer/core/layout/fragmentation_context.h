// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FRAGMENTATION_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FRAGMENTATION_CONTEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

// A fragmentation context is either established by a multicol container, or by
// pages when printing. A fragmentation context consists of a series of
// fragmentainers. A fragmentainer is simply a column or page, depending on the
// type of fragmentation context. [1]
//
// A couple of methods here take a |blockOffset| parameter. This is the offset
// from the start of the fragmentation context, pretending that everything is
// laid out in one single strip (and not sliced into pages or columns).
// In multicol, this is referred to as the flow thread coordinate space.
//
// It should be noted that a multicol container may be nested inside another
// fragmentation context (another multicol container, or the pages when
// printing), although this class doesn't deal with that (it's internal to the
// multicol implementation).
//
// [1] http://www.w3.org/TR/css3-break/#fragmentation-model
class CORE_EXPORT FragmentationContext {
 public:
  virtual ~FragmentationContext() = default;

  // The height of the fragmentainers may depend on the total height of the
  // contents (column balancing), in which case false is returned if we haven't
  // laid out yet. Otherwise, true is returned.
  virtual bool IsFragmentainerLogicalHeightKnown() = 0;

  // Return the height of the fragmentainer at the specified offset. The
  // fragmentainer height isn't necessarily uniform all across the
  // fragmentation context. This method may only be called if the logical
  // height has been calculated, i.e. if IsFragmentainerLogicalHeightKnown()
  // returns true.
  virtual LayoutUnit FragmentainerLogicalHeightAt(LayoutUnit block_offset) = 0;

  // Return how much is left of the fragmentainer at the specified offset.
  // Callers typically want this information to decide whether some piece of
  // content fits in this fragmentainer, or if it has to push the content to the
  // next fragmentainer.
  virtual LayoutUnit RemainingLogicalHeightAt(LayoutUnit block_offset) = 0;

  // Return the flow thread of the fragmentation context, if it is a multicol
  // fragmentation context. Since multicol containers may be nested inside other
  // fragmentation contexts, sometimes we need to know if it's a multicol
  // container that we're dealing with.
  virtual class LayoutMultiColumnFlowThread* AssociatedFlowThread() {
    return nullptr;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_FRAGMENTATION_CONTEXT_H_
