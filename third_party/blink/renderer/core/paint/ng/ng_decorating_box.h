// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_DECORATING_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_DECORATING_BOX_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"

namespace blink {

class ComputedStyle;

// Represents a [decorating box].
// [decorating box]: https://drafts.csswg.org/css-text-decor-3/#decorating-box
class CORE_EXPORT NGDecoratingBox {
 public:
  NGDecoratingBox(const PhysicalOffset& offset_in_container,
                  const ComputedStyle& style)
      : offset_in_container_(offset_in_container), style_(style) {}
  NGDecoratingBox(const NGFragmentItem& inline_box, const ComputedStyle& style)
      : NGDecoratingBox(inline_box.OffsetInContainerFragment(), style) {
    DCHECK(inline_box.IsInlineBox());
    DCHECK_EQ(&inline_box.Style(), &style_);
  }

  const ComputedStyle& Style() const { return style_; }

 private:
  PhysicalOffset offset_in_container_;
  const ComputedStyle& style_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_DECORATING_BOX_H_
