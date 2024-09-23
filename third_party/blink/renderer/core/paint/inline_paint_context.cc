// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/inline_paint_context.h"

#include "base/containers/adapters.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"

namespace blink {

void InlinePaintContext::ClearDecoratingBoxes(
    DecoratingBoxList* saved_decorating_boxes) {
  if (saved_decorating_boxes) {
    DCHECK(saved_decorating_boxes->empty());
    decorating_boxes_.swap(*saved_decorating_boxes);
  } else {
    decorating_boxes_.Shrink(0);
  }
}

InlinePaintContext::ScopedInlineItem::ScopedInlineItem(
    const FragmentItem& item,
    InlinePaintContext* inline_context) {
  DCHECK(inline_context);
  inline_context_ = inline_context;
  last_decorations_ = inline_context->last_decorations_;
  push_count_ =
      inline_context->SyncDecoratingBox(item, &saved_decorating_boxes_);
  DCHECK_EQ(inline_context->decorating_boxes_.size(),
            item.Style().AppliedTextDecorations().size());
}

// Synchronize |decorating_boxes_| with the |AppliedTextDecorations|, including
// culled inline boxes in the ancestor chain.
//
// This function may push multiple decorating boxes, or clear if the propagation
// was stopped. See |StopPropagateTextDecorations|.
wtf_size_t InlinePaintContext::SyncDecoratingBox(
    const FragmentItem& item,
    DecoratingBoxList* saved_decorating_boxes) {
  DCHECK(!saved_decorating_boxes || saved_decorating_boxes->empty());

  // Compare the instance addresses of |AppliedTextDecorations| because it is
  // shared across |ComputedStyle|s when it is propagated without changes.
  const ComputedStyle* style = &item.Style();
  const Vector<AppliedTextDecoration, 1>* decorations =
      &style->AppliedTextDecorations();
  DCHECK(last_decorations_);
  if (decorations == last_decorations_)
    return 0;

  // This class keeps all the context data while making recursive calls.
  class DecorationBoxSynchronizer {
    STACK_ALLOCATED();

   public:
    DecorationBoxSynchronizer(InlinePaintContext* inline_context,
                              const FragmentItem& item,
                              const Vector<AppliedTextDecoration, 1>* stop_at,
                              DecoratingBoxList* saved_decorating_boxes)
        : inline_context_(inline_context),
          stop_at_(stop_at),
          saved_decorating_boxes_(saved_decorating_boxes),
          style_variant_(ToParentStyleVariant(item.GetStyleVariant())) {
      DCHECK(inline_context_);
      DCHECK(stop_at_);
    }

    wtf_size_t Sync(const FragmentItem* item,
                    const LayoutObject* layout_object,
                    const ComputedStyle* style,
                    const Vector<AppliedTextDecoration, 1>* decorations) {
      for (;;) {
        DCHECK(!item || item->GetLayoutObject() == layout_object);
        DCHECK_EQ(&layout_object->EffectiveStyle(style_variant_), style);
        DCHECK_EQ(&style->AppliedTextDecorations(), decorations);
        DCHECK_NE(decorations, stop_at_);
        const LayoutObject* parent = layout_object->Parent();
        DCHECK(parent);
        const ComputedStyle& parent_style =
            parent->EffectiveStyle(style_variant_);
        const Vector<AppliedTextDecoration, 1>& parent_decorations =
            parent_style.AppliedTextDecorations();

        if (decorations != &parent_decorations) {
          // It's a decorating box if it has more decorations than its parent.
          if (decorations->size() > parent_decorations.size()) {
            // Ensure the parent is in sync. Ancestors are pushed first.
            wtf_size_t num_pushes = 0;
            if (&parent_decorations != stop_at_) {
              num_pushes = Sync(/* item */ nullptr, parent, &parent_style,
                                &parent_decorations);
            }

            num_pushes += PushDecoratingBoxesUntilParent(
                item, *layout_object, *style, *decorations, parent_decorations);
            return num_pushes;
          }

          // Rare but sometimes |AppliedTextDecorations| is duplicated instead
          // of being shared. If duplicated, skip it.
          // e.g., fast/css/first-letter.html
          //       tables/mozilla/bugs/bug126742.html
          if (decorations->size() == parent_decorations.size() &&
              (style->GetTextDecorationLine() == TextDecorationLine::kNone ||
               // Conceptually text nodes don't have styles, but |LayoutText|
               // has a style of its parent. Ignore |GetTextDecorationLine| for
               // |LayoutText|.
               // http/tests/devtools/service-workers/service-workers-view.js
               IsA<LayoutText>(layout_object))) {
            if (&parent_decorations == stop_at_)
              return 0;
            return Sync(/* item */ nullptr, parent, &parent_style,
                        &parent_decorations);
          }

          // If the number of this node's decorations is equal to or less than
          // the parent's, this node stopped the propagation. Reset the
          // decorating boxes. In this case, this node has 0 or 1 decorations.
          if (decorations->size() <= 1) {
            inline_context_->ClearDecoratingBoxes(saved_decorating_boxes_);
            if (decorations->empty())
              return 0;
            DCHECK_NE(style->GetTextDecorationLine(),
                      TextDecorationLine::kNone);
            PushDecoratingBox(item, *layout_object, *style, *decorations);
            return 1;
          }

          // There are some edge cases where a style doesn't propagate
          // decorations from its parent. One known such case is a pseudo
          // element in a parent with a first-line style, but there can be more.
          // If this happens, consider it stopped the propagation.
          const Vector<AppliedTextDecoration, 1>* base_decorations =
              style->BaseAppliedTextDecorations();
          if (base_decorations != &parent_decorations) {
            inline_context_->ClearDecoratingBoxes(saved_decorating_boxes_);
            const wtf_size_t size =
                std::min(saved_decorating_boxes_->size(), decorations->size());
            inline_context_->PushDecoratingBoxes(
                base::span(*saved_decorating_boxes_).first(size));
            return size;
          }

#if DCHECK_IS_ON()
          ShowLayoutTree(layout_object);
#endif
          NOTREACHED_IN_MIGRATION()
              << "size=" << decorations->size()
              << ", parent=" << parent_decorations.size()
              << ", TextDecorationLine="
              << static_cast<int>(style->GetTextDecorationLine());
        }

        if (!IsA<LayoutInline>(parent)) [[unlikely]] {
          // This shouldn't happen, indicating text-decoration isn't propagated
          // as expected, but the logs indicate it does, though not too often.
          // Just abort the sync.
          return 0;
        }

#if DCHECK_IS_ON()
        // All non-culled inline boxes should have called |SyncDecoratingBox|,
        // so the loop should have stopped before seeing non-culled inline
        // boxes.
        const auto* layout_inline = To<LayoutInline>(parent);
        // Except when |AppliedTextDecorations| is duplicated instead of
        // shared, see above.
        if (!(parent_decorations.size() == parent->Parent()
                                               ->StyleRef()
                                               .AppliedTextDecorations()
                                               .size() &&
              parent_style.GetTextDecorationLine() ==
                  TextDecorationLine::kNone) &&
            !IsA<LayoutText>(layout_object)) {
          DCHECK(!layout_inline->ShouldCreateBoxFragment());
          DCHECK(!layout_inline->HasInlineFragments());
        }
#endif
        item = nullptr;
        layout_object = parent;
        style = &parent_style;
      }
    }

    wtf_size_t PushDecoratingBoxesUntilParent(
        const FragmentItem* item,
        const LayoutObject& layout_object,
        const ComputedStyle& style,
        const Vector<AppliedTextDecoration, 1>& decorations,
        const Vector<AppliedTextDecoration, 1>& parent_decorations) {
      const Vector<AppliedTextDecoration, 1>* base_decorations =
          style.BaseAppliedTextDecorations();
      if (base_decorations == &parent_decorations) {
        DCHECK_EQ(decorations.size(), parent_decorations.size() + 1);
        DCHECK_NE(style.GetTextDecorationLine(), TextDecorationLine::kNone);
        PushDecoratingBox(item, layout_object, style, decorations);
        return 1;
      }

      if (base_decorations && base_decorations != &decorations &&
          decorations.size() == parent_decorations.size() + 2) {
        // When the normal style and `::first-line` have different decorations,
        // the normal style inherits from the parent, and the `:first-line`
        // inherits from the normal style, resulting two decorating boxes.
        DCHECK_NE(style.GetTextDecorationLine(), TextDecorationLine::kNone);
        PushDecoratingBox(item, layout_object, style, *base_decorations);
        PushDecoratingBox(item, layout_object, style, decorations);
        return 2;
      }

      // The style engine may create a clone, not an inherited decorations,
      // such as a `<span>` in `::first-line`.
      DCHECK_EQ(decorations.size(), parent_decorations.size() + 1);
      PushDecoratingBox(item, layout_object, style, decorations);
      return 1;
    }

    void PushDecoratingBox(
        const FragmentItem* item,
        const LayoutObject& layout_object,
        const ComputedStyle& style,
        const Vector<AppliedTextDecoration, 1>& decorations) {
      DCHECK(!item || item->GetLayoutObject() == &layout_object);
      if (!item) {
        // If the item is not known, it is either a culled inline or it is found
        // while traversing the tree. Find the offset of the first fragment of
        // the |LayoutObject| in the current line.
        if (!line_cursor_)
          line_cursor_ = inline_context_->CursorForDescendantsOfLine();
        line_cursor_->MoveToIncludingCulledInline(layout_object);
        DCHECK(*line_cursor_);
        item = line_cursor_->Current().Item();
      }
      DCHECK(item);
      inline_context_->PushDecoratingBox(
          item->ContentOffsetInContainerFragment(), style, &decorations);
    }

    InlinePaintContext* inline_context_;
    const Vector<AppliedTextDecoration, 1>* stop_at_;
    std::optional<InlineCursor> line_cursor_;
    DecoratingBoxList* saved_decorating_boxes_;
    StyleVariant style_variant_;
  };

  const wtf_size_t push_count =
      DecorationBoxSynchronizer(this, item, last_decorations_,
                                saved_decorating_boxes)
          .Sync(&item, item.GetLayoutObject(), style, decorations);
  last_decorations_ = decorations;
  return push_count;
}

InlinePaintContext::ScopedInlineBoxAncestors::ScopedInlineBoxAncestors(
    const InlineCursor& inline_box,
    InlinePaintContext* inline_context) {
  DCHECK(inline_context);
  inline_context_ = inline_context;
  inline_context->PushDecoratingBoxAncestors(inline_box);
}

void InlinePaintContext::PushDecoratingBoxAncestors(
    const InlineCursor& inline_box) {
  DCHECK(inline_box.Current());
  DCHECK(inline_box.Current().IsInlineBox());
  DCHECK(decorating_boxes_.empty());

  Vector<const FragmentItem*, 16> ancestor_items;
  for (InlineCursor cursor = inline_box;;) {
    cursor.MoveToParent();
    const InlineCursorPosition& current = cursor.Current();
    DCHECK(current);

    if (current.IsLineBox()) {
      SetLineBox(cursor);
      for (const FragmentItem* item : base::Reversed(ancestor_items)) {
        SyncDecoratingBox(*item);
      }
      return;
    }

    DCHECK(current.IsInlineBox());
    ancestor_items.push_back(current.Item());
  }
}

void InlinePaintContext::PushDecoratingBoxes(
    const base::span<DecoratingBox>& boxes) {
  decorating_boxes_.AppendRange(boxes.begin(), boxes.end());
}

InlinePaintContext::ScopedLineBox::ScopedLineBox(
    const InlineCursor& line_cursor,
    InlinePaintContext* inline_context) {
  DCHECK(inline_context);
  inline_context_ = inline_context;
  inline_context->SetLineBox(line_cursor);
}

void InlinePaintContext::SetLineBox(const InlineCursor& line_cursor) {
  DCHECK_EQ(line_cursor.Current()->Type(), FragmentItem::kLine);
  line_cursor_ = line_cursor;
  DCHECK(decorating_boxes_.empty());

  const FragmentItem& line_item = *line_cursor.Current();
  const ComputedStyle& style = line_item.Style();
  const Vector<AppliedTextDecoration, 1>& applied_text_decorations =
      style.AppliedTextDecorations();
  line_decorations_ = last_decorations_ = &applied_text_decorations;
  if (applied_text_decorations.empty())
    return;

  // The decorating box of a block container is an anonymous inline box that
  // wraps all children of the block container.
  // https://drafts.csswg.org/css-text-decor-3/#decorating-box
  //
  // Compute the offset of the non-existent anonymous inline box.
  PhysicalOffset offset = line_item.OffsetInContainerFragment();
  if (const PhysicalLineBoxFragment* fragment = line_item.LineBoxFragment()) {
    if (const SimpleFontData* font = style.GetFont().PrimaryFont()) {
      offset.top += fragment->Metrics().ascent;
      offset.top -= font->GetFontMetrics().FixedAscent();
    }
  }

  // If the block has multiple decorations, all decorations have the same
  // decorating box, which is a non-existent anonymous inline box that wraps all
  // the in-flow children. See
  // https://drafts.csswg.org/css-text-decor-3/#line-decoration, EXAMPLE 1 in
  // the spec, and crbug.com/855589.
  for (wtf_size_t i = 0; i < applied_text_decorations.size(); ++i)
    decorating_boxes_.emplace_back(offset, style, &applied_text_decorations);
}

void InlinePaintContext::ClearLineBox() {
  last_decorations_ = nullptr;
  line_decorations_ = nullptr;
  line_cursor_.reset();
  decorating_boxes_.Shrink(0);
}

}  // namespace blink
