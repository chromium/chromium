// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_BOX_LAYOUT_EXTRA_INPUT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_BOX_LAYOUT_EXTRA_INPUT_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
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

  // When set, no attempt should be be made to resolve the inline size. Use this
  // one instead.
  absl::optional<LayoutUnit> override_inline_size;

  // When set, no attempt should be be made to resolve the block size. Use this
  // one instead.
  absl::optional<LayoutUnit> override_block_size;

  // If the |override_block_size| should be treated as definite for the
  // purposes of percent block-size resolution.
  bool is_override_block_size_definite = true;

  // If an 'auto' inline/block-size should stretch to the available size.
  bool stretch_inline_size_if_auto = false;
  bool stretch_block_size_if_auto = false;

  // Available inline size. https://drafts.csswg.org/css-sizing/#available
  LayoutUnit available_inline_size;

  // The content size of the containing block. These are somewhat vague legacy
  // layout values, that typically either mean available size or percentage
  // resolution size.
  LayoutUnit containing_block_content_inline_size;
  LayoutUnit containing_block_content_block_size;

  // Border and padding values. This field is set only for LayoutReplaced.
  NGPhysicalBoxStrut border_padding_for_replaced;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_BOX_LAYOUT_EXTRA_INPUT_H_
