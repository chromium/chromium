// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_DECORATING_BOX_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_DECORATING_BOX_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/style/applied_text_decoration.h"
#include "third_party/blink/renderer/platform/geometry/physical_offset.h"

namespace blink {

class ComputedStyle;

// Represents a [decorating box].
// [decorating box]: https://drafts.csswg.org/css-text-decor-3/#decorating-box
class CORE_EXPORT DecoratingBox {
  DISALLOW_NEW();

 public:
  DecoratingBox(const PhysicalOffset& content_offset_in_container,
                const ComputedStyle& style,
                const AppliedTextDecorationVector* decorations)
      : content_offset_in_container_(content_offset_in_container),
        style_(&style),
        decorations_(decorations ? decorations
                                 : &style.AppliedTextDecorations()) {}
  DecoratingBox(const FragmentItem& item,
                const ComputedStyle& style,
                const AppliedTextDecorationVector* decorations)
      : DecoratingBox(item.ContentOffsetInContainerFragment(),
                      style,
                      decorations) {}
  explicit DecoratingBox(const FragmentItem& item)
      : DecoratingBox(item, item.Style(), /* decorations */ nullptr) {}

  void Trace(Visitor* visitor) const {
    visitor->Trace(style_);
    visitor->Trace(decorations_);
  }

  const PhysicalOffset& ContentOffsetInContainer() const {
    return content_offset_in_container_;
  }
  const ComputedStyle& Style() const { return *style_; }
  const AppliedTextDecorationVector* AppliedTextDecorations() const {
    return decorations_.Get();
  }

 private:
  PhysicalOffset content_offset_in_container_;
  Member<const ComputedStyle> style_;
  Member<const AppliedTextDecorationVector> decorations_;
};

}  // namespace blink

WTF_ALLOW_CLEAR_UNUSED_SLOTS_WITH_MEM_FUNCTIONS(blink::DecoratingBox)

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_DECORATING_BOX_H_
