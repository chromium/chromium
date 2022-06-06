// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_INLINE_PAINT_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_INLINE_PAINT_CONTEXT_H_

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/paint/ng/ng_decorating_box.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Carries contextual information shared across multiple inline fragments within
// an inline formatting context.
class CORE_EXPORT NGInlinePaintContext {
  STACK_ALLOCATED();

 public:
  using DecoratingBoxList = Vector<NGDecoratingBox, 1>;
  const DecoratingBoxList& DecoratingBoxes() const { return decorating_boxes_; }

  void PushDecoratingBox(const NGFragmentItem& item,
                         const ComputedStyle& style);
  void PushDecoratingBox(const NGFragmentItem& item);
  void PushDecoratingBoxAncestors(const NGInlineCursor& inline_box);
  void PopDecoratingBox() { decorating_boxes_.pop_back(); }

  void SetLineBox(const NGFragmentItem& line_item);
  void ClearLineBox() { line_item_ = nullptr; }

  const PhysicalOffset& PaintOffset() const { return paint_offset_; }
  void SetPaintOffset(const PhysicalOffset& paint_offset) {
    paint_offset_ = paint_offset;
  }

  // Pushes a decorating box if the item is a decorating box.
  class ScopedInlineItem {
    STACK_ALLOCATED();

   public:
    ScopedInlineItem(const NGFragmentItem& inline_item,
                     NGInlinePaintContext* inline_context);
    ~ScopedInlineItem();

   private:
    NGInlinePaintContext* inline_context_ = nullptr;
    DecoratingBoxList saved_decorating_boxes_;
    bool is_pushed_ = false;
  };

  // Pushes all decorating boxes in the ancestor chain.
  class ScopedInlineBoxAncestors {
    STACK_ALLOCATED();

   public:
    ScopedInlineBoxAncestors(const NGInlineCursor& inline_box,
                             NGInlinePaintContext* inline_context);
    ~ScopedInlineBoxAncestors();

   private:
    NGInlinePaintContext* inline_context_ = nullptr;
  };

  // Pushes all decorating boxes for a line box.
  class ScopedLineBox {
    STACK_ALLOCATED();

   public:
    ScopedLineBox(const NGFragmentItem& line_item,
                  NGInlinePaintContext* inline_context);
    ~ScopedLineBox();

   private:
    NGInlinePaintContext* inline_context_ = nullptr;
  };

  // Set |PaintOffset| while the instance of this class is in the scope.
  class ScopedPaintOffset {
    STACK_ALLOCATED();

   public:
    ScopedPaintOffset(const PhysicalOffset& paint_offset,
                      NGInlinePaintContext* inline_context)
        : paint_offset_(&inline_context->paint_offset_, paint_offset) {}

   private:
    base::AutoReset<PhysicalOffset> paint_offset_;
  };

 private:
  bool PushDecoratingBox(const NGFragmentItem& item,
                         DecoratingBoxList* saved_decorating_boxes);

  DecoratingBoxList decorating_boxes_;
  const NGFragmentItem* line_item_ = nullptr;
  PhysicalOffset paint_offset_;
};

inline void NGInlinePaintContext::PushDecoratingBox(
    const NGFragmentItem& inline_item,
    const ComputedStyle& style) {
  DCHECK(RuntimeEnabledFeatures::TextDecoratingBoxEnabled());
  DCHECK_EQ(&inline_item.Style(), &style);
  decorating_boxes_.emplace_back(inline_item, style);
}

inline void NGInlinePaintContext::PushDecoratingBox(
    const NGFragmentItem& item) {
  DCHECK(RuntimeEnabledFeatures::TextDecoratingBoxEnabled());
  decorating_boxes_.emplace_back(item);
}

inline NGInlinePaintContext::ScopedInlineItem::~ScopedInlineItem() {
  if (!inline_context_)
    return;
  if (!saved_decorating_boxes_.IsEmpty())
    inline_context_->decorating_boxes_.swap(saved_decorating_boxes_);
  else if (is_pushed_)
    inline_context_->PopDecoratingBox();
}

inline NGInlinePaintContext::ScopedInlineBoxAncestors::
    ~ScopedInlineBoxAncestors() {
  if (inline_context_)
    inline_context_->ClearLineBox();
}

inline NGInlinePaintContext::ScopedLineBox::~ScopedLineBox() {
  if (inline_context_)
    inline_context_->ClearLineBox();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_INLINE_PAINT_CONTEXT_H_
