// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_DECORATING_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_DECORATING_BOX_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"

namespace blink {

class ComputedStyle;

// Represents a [decorating box].
// [decorating box]: https://drafts.csswg.org/css-text-decor-3/#decorating-box
class CORE_EXPORT NGDecoratingBox {
  DISALLOW_NEW();

 public:
  NGDecoratingBox(const PhysicalOffset& content_offset_in_container,
                  const ComputedStyle& style,
                  const Vector<AppliedTextDecoration, 1>* decorations)
      : content_offset_in_container_(content_offset_in_container),
        style_(&style),
        decorations_(decorations ? decorations
                                 : &style.AppliedTextDecorations()) {
    DCHECK_EQ(&style.AppliedTextDecorations(), decorations_);
  }
  NGDecoratingBox(const FragmentItem& item,
                  const ComputedStyle& style,
                  const Vector<AppliedTextDecoration, 1>* decorations)
      : NGDecoratingBox(item.ContentOffsetInContainerFragment(),
                        style,
                        decorations) {}
  explicit NGDecoratingBox(const FragmentItem& item)
      : NGDecoratingBox(item, item.Style(), /* decorations */ nullptr) {}

  void Trace(Visitor* visitor) const { visitor->Trace(style_); }

  const PhysicalOffset& ContentOffsetInContainer() const {
    return content_offset_in_container_;
  }
  const ComputedStyle& Style() const { return *style_; }
  const Vector<AppliedTextDecoration, 1>* AppliedTextDecorations() const {
    return decorations_;
  }

 private:
  PhysicalOffset content_offset_in_container_;
  Member<const ComputedStyle> style_;
  const Vector<AppliedTextDecoration, 1>* decorations_;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::NGDecoratingBox)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_DECORATING_BOX_H_
