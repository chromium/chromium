// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_INLINE_PAINT_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_NG_NG_INLINE_PAINT_CONTEXT_H_

#include "base/auto_reset.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/paint/ng/ng_decorating_box.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// Carries contextual information shared across multiple inline fragments within
// an inline formatting context.
class CORE_EXPORT NGInlinePaintContext {
  STACK_ALLOCATED();

 public:
  using DecoratingBoxList = Vector<NGDecoratingBox, 4>;
  const DecoratingBoxList& DecoratingBoxes() const { return decorating_boxes_; }

  NGInlineCursor CursorForDescendantsOfLine() const {
    return line_cursor_->CursorForDescendants();
  }

  template <class... Args>
  void PushDecoratingBox(Args&&... args) {
    decorating_boxes_.emplace_back(std::forward<Args>(args)...);
    UpdateLastDecorations();
  }
  void PushDecoratingBoxAncestors(const NGInlineCursor& inline_box);
  void PopDecoratingBox();
  void PopDecoratingBox(wtf_size_t size);
  void ClearDecoratingBoxes(
      DecoratingBoxList* saved_decorating_boxes = nullptr);

  void SetLineBox(const NGInlineCursor& line_cursor);
  void ClearLineBox();

  const PhysicalOffset& PaintOffset() const { return paint_offset_; }
  void SetPaintOffset(const PhysicalOffset& paint_offset) {
    paint_offset_ = paint_offset;
  }

  class ScopedBatchChanges {
    STACK_ALLOCATED();

   public:
    explicit ScopedBatchChanges(NGInlinePaintContext* inline_context);
    ~ScopedBatchChanges();

   private:
    NGInlinePaintContext* inline_context_ = nullptr;
    bool saved_is_in_batch_changes_;
  };

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
    wtf_size_t push_count_ = 0;
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
    ScopedLineBox(const NGInlineCursor& line_cursor,
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
  void UpdateLastDecorations();
  void SetIsInBatchChanges(bool value);

  wtf_size_t SyncDecoratingBox(
      const NGFragmentItem& item,
      DecoratingBoxList* saved_decorating_boxes = nullptr);

  DecoratingBoxList decorating_boxes_;
  // The last |AppliedTextDecorations| |this| was synchronized with.
  const Vector<AppliedTextDecoration>* last_decorations_ = nullptr;
  const Vector<AppliedTextDecoration>* line_decorations_ = nullptr;
  absl::optional<NGInlineCursor> line_cursor_;
  PhysicalOffset paint_offset_;
  bool is_in_batch_changes_ = false;
};

inline void NGInlinePaintContext::UpdateLastDecorations() {
  if (is_in_batch_changes_)
    return;
  DCHECK(line_decorations_);
  if (decorating_boxes_.IsEmpty())
    last_decorations_ = line_decorations_;
  else
    last_decorations_ = decorating_boxes_.back().AppliedTextDecorations();
}

inline void NGInlinePaintContext::PopDecoratingBox() {
  decorating_boxes_.pop_back();
  UpdateLastDecorations();
}

inline void NGInlinePaintContext::PopDecoratingBox(wtf_size_t size) {
  DCHECK_LE(size, decorating_boxes_.size());
  decorating_boxes_.Shrink(decorating_boxes_.size() - size);
  UpdateLastDecorations();
}

inline NGInlinePaintContext::ScopedInlineItem::~ScopedInlineItem() {
  if (!inline_context_)
    return;
  if (!saved_decorating_boxes_.IsEmpty()) {
    inline_context_->decorating_boxes_.swap(saved_decorating_boxes_);
    inline_context_->UpdateLastDecorations();
    return;
  }
  if (push_count_)
    inline_context_->PopDecoratingBox(push_count_);
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
