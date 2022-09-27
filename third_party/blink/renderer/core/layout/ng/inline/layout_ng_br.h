// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_BR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_BR_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_br.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_span.h"

namespace blink {

// This class is identical to |LayoutBR| except for this class returns true
// for |IsLayoutNGObject()| and |NGInlineItem| support, to become child of
// |LayoutNGTextCombine|. See also |LayoutNGWordBreak|.
// TODO(yosin): Once we get rid of |IsLayoutNGObject()|, we should unify this
// class |LayoutBR|.
class CORE_EXPORT LayoutNGBR final : public LayoutBR {
 public:
  explicit LayoutNGBR(Node* node) : LayoutBR(node) {}

  bool IsLayoutNGObject() const final {
    NOT_DESTROYED();
    return true;
  }

  void Trace(Visitor* visitor) const final {
    visitor->Trace(inline_items_);
    LayoutBR::Trace(visitor);
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

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_BR_H_
