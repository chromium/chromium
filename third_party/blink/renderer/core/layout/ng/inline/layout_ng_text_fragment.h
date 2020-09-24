// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_TEXT_FRAGMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_TEXT_FRAGMENT_H_

#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"

namespace blink {

// This overrides the default LayoutText to reference LayoutNGInlineItems
// instead of InlineTextBoxes.
//
class CORE_EXPORT LayoutNGTextFragment final : public LayoutTextFragment {
 public:
  LayoutNGTextFragment(Node* node,
                       StringImpl* text,
                       int start_offset,
                       int length)
      : LayoutTextFragment(node, text, start_offset, length) {}

  bool IsLayoutNGObject() const final { return true; }

 private:
  const base::span<NGInlineItem>* GetNGInlineItems() const final {
    return &inline_items_;
  }
  base::span<NGInlineItem>* GetNGInlineItems() final { return &inline_items_; }

  void InsertedIntoTree() final {
    valid_ng_items_ = false;
    LayoutText::InsertedIntoTree();
  }

  base::span<NGInlineItem> inline_items_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_TEXT_FRAGMENT_H_
