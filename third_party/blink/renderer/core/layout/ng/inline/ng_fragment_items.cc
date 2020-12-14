// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items.h"

#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

NGFragmentItems::NGFragmentItems(NGFragmentItemsBuilder* builder)
    : text_content_(std::move(builder->text_content_)),
      first_line_text_content_(std::move(builder->first_line_text_content_)),
      size_(builder->items_.size()) {
  NGFragmentItemsBuilder::ItemWithOffsetList& source_items = builder->items_;
  for (unsigned i = 0; i < size_; ++i) {
    // Call the move constructor to move without |AddRef|. Items in
    // |NGFragmentItemsBuilder| are not used after |this| was constructed.
    new (&items_[i]) NGFragmentItem(std::move(source_items[i].item));
  }
}

NGFragmentItems::~NGFragmentItems() {
  for (unsigned i = 0; i < size_; ++i)
    items_[i].~NGFragmentItem();
}

bool NGFragmentItems::IsSubSpan(const Span& span) const {
  return span.empty() ||
         (span.data() >= ItemsData() && &span.back() < ItemsData() + Size());
}

void NGFragmentItems::FinalizeAfterLayout(
    const Vector<scoped_refptr<const NGLayoutResult>, 1>& results) {
  struct LastItem {
    const NGFragmentItem* item;
    wtf_size_t fragment_id;
    wtf_size_t item_index;
  };
  HashMap<const LayoutObject*, LastItem> last_items;
  for (const auto& result : results) {
    const auto& fragment =
        To<NGPhysicalBoxFragment>(result->PhysicalFragment());
    const NGFragmentItems* current = fragment.Items();
    if (UNLIKELY(!current))
      continue;

    // TODO(layout-dev): Make this work for multiple box fragments (block
    // fragmentation).
    const bool create_index_cache = fragment.IsFirstForNode();

    const Span items = current->Items();
    wtf_size_t index = 0;
    for (const NGFragmentItem& item : items) {
      ++index;
      if (item.Type() == NGFragmentItem::kLine) {
        DCHECK_EQ(item.DeltaToNextForSameLayoutObject(), 0u);
        continue;
      }
      LayoutObject* const layout_object = item.GetMutableLayoutObject();
      DCHECK(!layout_object->IsOutOfFlowPositioned());
      DCHECK(layout_object->IsInLayoutNGInlineFormattingContext());

      item.SetDeltaToNextForSameLayoutObject(0);
      item.SetIsLastForNode(false);

      // Fragments that aren't really on a line, such as floats, will have block
      // break tokens if they continue in a subsequent fragmentainer, so just
      // check that. Floats in particular will continue as regular box fragment
      // children in subsequent fragmentainers, i.e. they will not be fragment
      // items (even if we're in an inline formatting context). So we're not
      // going to find the last fragment by just looking for items.
      bool skip_last_items_map = false;
      if (const NGPhysicalBoxFragment* child_fragment = item.BoxFragment()) {
        if (!child_fragment->IsInline()) {
          item.SetIsLastForNode(!child_fragment->BreakToken());
          skip_last_items_map = true;
        }
      }

      bool is_first = skip_last_items_map;
      LastItem* last = nullptr;
      if (!skip_last_items_map) {
        const auto last_item_result =
            last_items.insert(layout_object, LastItem{&item, 0, index});
        is_first = last_item_result.is_new_entry;
        last = &last_item_result.stored_value->value;
      }

      if (is_first) {
        item.SetFragmentId(0);
        if (create_index_cache) {
          DCHECK_EQ(layout_object->FirstInlineFragmentItemIndex(), 0u);
          layout_object->SetFirstInlineFragmentItemIndex(index);
        }
        continue;
      }

      const NGFragmentItem* last_item = last->item;
      DCHECK_EQ(last_item->DeltaToNextForSameLayoutObject(), 0u);
      if (create_index_cache) {
        const wtf_size_t last_index = last->item_index;
        DCHECK_GT(last_index, 0u);
        DCHECK_LT(last_index, items.size());
        DCHECK_LT(last_index, index);
        last_item->SetDeltaToNextForSameLayoutObject(index - last_index);
      }
      item.SetFragmentId(++last->fragment_id);
      last->item = &item;
      last->item_index = index;
    }
  }
  for (const auto& iter : last_items)
    iter.value.item->SetIsLastForNode(true);
}

void NGFragmentItems::ClearAssociatedFragments(LayoutObject* container) {
  // Clear by traversing |LayoutObject| tree rather than |NGFragmentItem|
  // because a) we don't need to modify |NGFragmentItem|, and in general the
  // number of |LayoutObject| is less than the number of |NGFragmentItem|.
  for (LayoutObject* child = container->SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (UNLIKELY(!child->IsInLayoutNGInlineFormattingContext() ||
                 child->IsOutOfFlowPositioned()))
      continue;
    child->ClearFirstInlineFragmentItemIndex();

    // Children of |LayoutInline| are part of this inline formatting context,
    // but children of other |LayoutObject| (e.g., floats, oof, inline-blocks)
    // are not.
    if (child->IsLayoutInline())
      ClearAssociatedFragments(child);
  }
}

// static
bool NGFragmentItems::CanReuseAll(NGInlineCursor* cursor) {
  for (; *cursor; cursor->MoveToNext()) {
    const NGFragmentItem& item = *cursor->Current().Item();
    if (!item.CanReuse())
      return false;
  }
  return true;
}

const NGFragmentItem* NGFragmentItems::EndOfReusableItems() const {
  const NGFragmentItem* last_line_start = &front();
  for (NGInlineCursor cursor(*this); cursor;) {
    const NGFragmentItem& item = *cursor.Current();
    if (item.IsDirty())
      return &item;

    // Top-level fragments that are not line box cannot be reused; e.g., oof
    // or list markers.
    if (item.Type() != NGFragmentItem::kLine)
      return &item;

    const NGPhysicalLineBoxFragment* line_box_fragment = item.LineBoxFragment();
    DCHECK(line_box_fragment);

    // If there is a dirty item in the middle of a line, its previous line is
    // not reusable, because the dirty item may affect the previous line to wrap
    // differently.
    NGInlineCursor line = cursor.CursorForDescendants();
    if (!CanReuseAll(&line))
      return last_line_start;

    // Abort if the line propagated its descendants to outside of the line.
    // They are propagated through NGLayoutResult, which we don't cache.
    if (line_box_fragment->HasPropagatedDescendants())
      return &item;

    // TODO(kojii): Running the normal layout code at least once for this
    // child helps reducing the code to setup internal states after the
    // partial. Remove the last fragment if it is the end of the
    // fragmentation to do so, but we should figure out how to setup the
    // states without doing this.
    const NGBreakToken* break_token = line_box_fragment->BreakToken();
    DCHECK(break_token);
    if (break_token->IsFinished())
      return &item;

    last_line_start = &item;
    cursor.MoveToNextSkippingChildren();
  }
  return nullptr;  // all items are reusable.
}

bool NGFragmentItems::TryDirtyFirstLineFor(
    const LayoutObject& layout_object) const {
  DCHECK(layout_object.IsInLayoutNGInlineFormattingContext());
  DCHECK(!layout_object.IsFloatingOrOutOfFlowPositioned());
  if (wtf_size_t index = layout_object.FirstInlineFragmentItemIndex()) {
    const NGFragmentItem& item = Items()[index - 1];
    DCHECK_EQ(&layout_object, item.GetLayoutObject());
    item.SetDirty();
    return true;
  }
  return false;
}

bool NGFragmentItems::TryDirtyLastLineFor(
    const LayoutObject& layout_object) const {
  NGInlineCursor cursor(*this);
  cursor.MoveTo(layout_object);
  if (!cursor)
    return false;
  cursor.MoveToLastForSameLayoutObject();
  DCHECK(cursor.Current().Item());
  const NGFragmentItem& item = *cursor.Current().Item();
  DCHECK_EQ(&layout_object, item.GetLayoutObject());
  item.SetDirty();
  return true;
}

void NGFragmentItems::DirtyLinesFromChangedChild(
    const LayoutObject* child) const {
  if (UNLIKELY(!child)) {
    front().SetDirty();
    return;
  }

  if (child->IsInLayoutNGInlineFormattingContext() &&
      !child->IsFloatingOrOutOfFlowPositioned() && TryDirtyFirstLineFor(*child))
    return;

  // If |child| is new, or did not generate fragments, mark the fragments for
  // previous |LayoutObject| instead.
  while (true) {
    if (const LayoutObject* previous = child->PreviousSibling()) {
      while (const LayoutInline* layout_inline =
                 ToLayoutInlineOrNull(previous)) {
        if (const LayoutObject* last_child = layout_inline->LastChild())
          previous = last_child;
        else
          break;
      }
      child = previous;
      if (UNLIKELY(child->IsFloatingOrOutOfFlowPositioned()))
        continue;
      if (child->IsInLayoutNGInlineFormattingContext() &&
          TryDirtyLastLineFor(*child))
        return;
      continue;
    }

    child = child->Parent();
    if (!child || child->IsLayoutBlockFlow()) {
      front().SetDirty();
      return;
    }
    DCHECK(child->IsLayoutInline());
    if (child->IsInLayoutNGInlineFormattingContext() &&
        TryDirtyFirstLineFor(*child))
      return;
  }
}

void NGFragmentItems::DirtyLinesFromNeedsLayout(
    const LayoutBlockFlow* container) const {
  DCHECK_EQ(this, container->FragmentItems());
  // Mark dirty for the first top-level child that has |NeedsLayout|.
  //
  // TODO(kojii): We could mark first descendant to increase reuse
  // opportunities. Doing this complicates the logic, especially when culled
  // inline is involved, and common case is to append to large IFC. Choose
  // simpler logic and faster to check over more reuse opportunities.
  const auto writing_mode = container->StyleRef().GetWritingMode();
  for (LayoutObject* child = container->FirstChild(); child;
       child = child->NextSibling()) {
    // NeedsLayout is not helpful for an orthogonal writing-mode root because
    // its NeedsLayout flag is cleared during the ComputeMinMaxSizes() step of
    // the container.
    if (child->NeedsLayout() ||
        !IsParallelWritingMode(writing_mode,
                               child->StyleRef().GetWritingMode())) {
      DirtyLinesFromChangedChild(child);
      return;
    }
  }
}

// static
void NGFragmentItems::LayoutObjectWillBeMoved(
    const LayoutObject& layout_object) {
  NGInlineCursor cursor;
  cursor.MoveTo(layout_object);
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    const NGFragmentItem* item = cursor.Current().Item();
    item->LayoutObjectWillBeMoved();
  }
}

// static
void NGFragmentItems::LayoutObjectWillBeDestroyed(
    const LayoutObject& layout_object) {
  NGInlineCursor cursor;
  cursor.MoveTo(layout_object);
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    const NGFragmentItem* item = cursor.Current().Item();
    item->LayoutObjectWillBeDestroyed();
  }
}

#if DCHECK_IS_ON()
void NGFragmentItems::CheckAllItemsAreValid() const {
  for (const NGFragmentItem& item : Items())
    DCHECK(!item.IsLayoutObjectDestroyedOrMoved());
}
#endif

}  // namespace blink
