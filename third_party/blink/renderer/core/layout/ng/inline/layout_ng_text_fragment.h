// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_TEXT_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_TEXT_FRAGMENT_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_span.h"

namespace blink {

// This overrides the default LayoutText to reference LayoutNGInlineItems
// instead of InlineTextBoxes.
class CORE_EXPORT LayoutNGTextFragment final : public LayoutTextFragment {
 public:
  LayoutNGTextFragment(Node* node,
                       StringImpl* text,
                       int start_offset,
                       int length)
      : LayoutTextFragment(node, text, start_offset, length) {
    NOT_DESTROYED();
  }

  bool IsLayoutNGObject() const final {
    NOT_DESTROYED();
    return true;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(inline_items_);
    LayoutTextFragment::Trace(visitor);
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

  void InsertedIntoTree() final {
    NOT_DESTROYED();
    valid_ng_items_ = false;
    LayoutText::InsertedIntoTree();
  }

  NGInlineItemSpan inline_items_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_TEXT_FRAGMENT_H_
