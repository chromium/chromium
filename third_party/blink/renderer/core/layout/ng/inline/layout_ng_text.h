// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_TEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_TEXT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_text.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_span.h"

namespace blink {

// This overrides the default LayoutText to reference LayoutNGInlineItems
// instead of InlineTextBoxes.
//
class CORE_EXPORT LayoutNGText : public LayoutText {
 public:
  LayoutNGText(Node* node, scoped_refptr<StringImpl> text)
      : LayoutText(node, text) {
    NOT_DESTROYED();
  }

  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectNGText || LayoutText::IsOfType(type);
  }
  bool IsLayoutNGObject() const override {
    NOT_DESTROYED();
    return true;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(inline_items_);
    LayoutText::Trace(visitor);
  }

 private:
  const NGInlineItemSpan* GetNGInlineItems() const final {
    NOT_DESTROYED();
    return &inline_items_;
  }
  NGInlineItemSpan* GetNGInlineItems() final {
    NOT_DESTROYED();
    return &inline_items_;
  }

  NGInlineItemSpan inline_items_;
};

template <>
struct DowncastTraits<LayoutNGText> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsLayoutNGText();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_TEXT_H_
