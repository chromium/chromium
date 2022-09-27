// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_COUNTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_COUNTER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/layout_counter.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_item_span.h"

namespace blink {

// This class is identical to |LayoutCounter| except for this class returns
// true for |IsLayoutNGObject()| and |NGInlineItem| support, to become child of
// |LayoutNGTextCombine|. See also |LayoutNGWordBreak|.
// TODO(yosin): Once we get rid of |IsLayoutNGObject()|, we should unify this
// class |LayoutCounter|.
class CORE_EXPORT LayoutNGCounter final : public LayoutCounter {
 public:
  explicit LayoutNGCounter(PseudoElement& pseudo,
                           const CounterContentData& counter)
      : LayoutCounter(pseudo, counter) {}

  bool IsLayoutNGObject() const final {
    NOT_DESTROYED();
    return true;
  }

  void Trace(Visitor* visitor) const final {
    visitor->Trace(inline_items_);
    LayoutCounter::Trace(visitor);
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

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_LAYOUT_NG_COUNTER_H_
