// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_BOX_LAYOUT_EXTRA_INPUT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_BOX_LAYOUT_EXTRA_INPUT_H_

#include "third_party/blink/renderer/core/layout/ng/geometry/ng_box_strut.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutBox;

// Extra input data for laying out a LayoutBox. The object will automatically
// associate itself with the specified LayoutBox upon creation, and dissociate
// itself upon destruction.
struct BoxLayoutExtraInput {
  // BoxLayoutExtraInput is always allocated on the stack as it is scoped to
  // layout, but DISALLOW_NEW is used here since LayoutBox has a raw pointer to
  // it.
  DISALLOW_NEW();

  explicit BoxLayoutExtraInput(LayoutBox&);
  ~BoxLayoutExtraInput();

  void Trace(Visitor*) const;

  Member<LayoutBox> box;

  // The border-box size computed by NGReplacedLayoutAlgorithm.
  PhysicalSize size;

  // The content size of the containing block. These are somewhat vague legacy
  // layout values, that typically either mean available size or percentage
  // resolution size.
  LayoutUnit containing_block_content_inline_size;

  // Border and padding values. This field is set only for LayoutReplaced.
  NGPhysicalBoxStrut border_padding_for_replaced;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_BOX_LAYOUT_EXTRA_INPUT_H_
