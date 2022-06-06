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
  NGDecoratingBox(const PhysicalOffset& content_offset_in_container,
                  const ComputedStyle& style)
      : content_offset_in_container_(content_offset_in_container),
        style_(&style) {}
  NGDecoratingBox(const NGFragmentItem& item, const ComputedStyle& style)
      : NGDecoratingBox(item.ContentOffsetInContainerFragment(), style) {
    // DCHECK(inline_box.IsInlineBox());
    DCHECK_EQ(&item.Style(), &style);
  }
  explicit NGDecoratingBox(const NGFragmentItem& item)
      : NGDecoratingBox(item, item.Style()) {}

  const PhysicalOffset& ContentOffsetInContainer() const {
    return content_offset_in_container_;
  }
  const ComputedStyle& Style() const { return *style_; }

 private:
  PhysicalOffset content_offset_in_container_;
  const ComputedStyle* style_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_DECORATING_BOX_H_
