// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/fragment_items.h"

#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_items_builder.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/platform/heap/collection_support/clear_collection_scope.h"

namespace blink {

namespace {

#if DCHECK_IS_ON()
void CheckNoItemsAreAssociated(const PhysicalBoxFragment& fragment) {
  if (const FragmentItems* fragment_items = fragment.Items()) {
    for (const FragmentItem& item : fragment_items->Items()) {
      if (item.Type() == FragmentItem::kLine) {
        continue;
      }
      if (const LayoutObject* layout_object = item.GetLayoutObject())
        DCHECK(!layout_object->FirstInlineFragmentItemIndex());
    }
  }
}

void CheckIsLast(const FragmentItem& item) {
  if (const PhysicalBoxFragment* fragment = item.BoxFragment()) {
    if (!fragment->IsInline()) {
      DCHECK(!fragment->IsInlineBox());
      DCHECK_EQ(item.IsLastForNode(), !fragment->GetBreakToken());
    }
  }
}
#endif

}  // namespace

FragmentItems::FragmentItems(FragmentItemsBuilder* builder)
    : text_content_(std::move(builder->text_content_)),
      first_line_text_content_(std::move(builder->first_line_text_content_)) {
  items_.ReserveInitialCapacity(builder->items_.size());
  std::transform(builder->items_.begin(), builder->items_.end(),
                 std::back_inserter(items_),
                 [](auto& item) { return std::move(item.item); });
}

FragmentItems::FragmentItems(const FragmentItems& other)
    : text_content_(other.text_content_),
      first_line_text_content_(other.first_line_text_content_),
      size_of_earlier_fragments_(other.size_of_earlier_fragments_),
      items_(other.items_) {
  for (const auto& other_item : other.items_) {
    // The |other| object is likely going to be freed after this copy. Detach
    // any |AbstractInlineTextBox|, as they store a pointer to an individual
    // |FragmentItem|.
    if (auto* layout_text =
            DynamicTo<LayoutText>(other_item.GetMutableLayoutObject()))
      layout_text->DetachAbstractInlineTextBoxesIfNeeded();
  }
}

bool FragmentItems::IsSubSpan(const Span& span) const {
  return span.empty() || (span.data() >= ItemsData() && !items_.empty() &&
                          &span.back() <= &items_.back());
}

void FragmentItems::FinalizeAfterLayout(
    const HeapVector<Member<const LayoutResult>, 1>& results,
    LayoutBlockFlow& container) {
  struct LastItem {
    const FragmentItem* item;
    wtf_size_t fragment_id;
    wtf_size_t item_index;
  };
  HeapHashMap<Member<const LayoutObject>, LastItem> last_items;
  ClearCollectionScope<HeapHashMap<Member<const LayoutObject>, LastItem>>
      clear_scope(&last_items);
  wtf_size_t item_index = 0;
  wtf_size_t line_fragment_id = FragmentItem::kInitialLineFragmentId;

  // If there are container fragments that don't have fragment items, or if
  // there are just floats there, the inline formatting context may be
  // non-contiguous, which means that a non-atomic inline may be non-contiguous
  // (e.g. it may exist in fragment 1, be absent in fragment 2, present again in
  // fragment 3). This requires some quite expensive calculations when setting
  // up the FragmentData objects.
  bool may_be_non_contiguous_ifc = false;

  for (const auto& result : results) {
    const auto& fragment =
        To<PhysicalBoxFragment>(result->GetPhysicalFragment());
    const FragmentItems* fragment_items = fragment.Items();
    if (!fragment_items) [[unlikely]] {
      may_be_non_contiguous_ifc = true;
      continue;
    }

    bool found_inflow_content = false;
    fragment_items->size_of_earlier_fragments_ = item_index;
    const Span items = fragment_items->Items();
    for (const FragmentItem& item : items) {
      ++item_index;
      if (item.Type() == FragmentItem::kLine) {
        DCHECK_EQ(item.DeltaToNextForSameLayoutObject(), 0u);
        item.SetFragmentId(line_fragment_id++);
        continue;
      } else if (!found_inflow_content) {
        // Resumed floats may take up all the space in the containing block
        // fragment, leaving no room for actual content inside the inline
        // formatting context. The non-atomic inline boxes themselves also don't
        // contribute to having inflow content, as they may just be wrappers
        // around such floats. We need something "real", such as text or a
        // non-atomic inline.
        found_inflow_content = !item.IsFloating() && !item.IsInlineBox();
      }
      LayoutObject* const layout_object = item.GetMutableLayoutObject();
      DCHECK(!layout_object->IsOutOfFlowPositioned());
      DCHECK(layout_object->IsInLayoutNGInlineFormattingContext());

      item.SetDeltaToNextForSameLayoutObject(0);
      const bool use_break_token =
          layout_object->IsFloating() || !layout_object->IsInline();
      if (use_break_token) [[unlikely]] {
        // Fragments that aren't really on a line, such as floats, will have
        // block break tokens if they continue in a subsequent fragmentainer, so
        // just check that. Floats in particular will continue as regular box
        // fragment children in subsequent fragmentainers, i.e. they will not be
        // fragment items (even if we're in an inline formatting context). So
        // we're not going to find the last fragment by just looking for items.
        DCHECK(item.BoxFragment() && !item.BoxFragment()->IsInlineBox());
        item.SetIsLastForNode(!item.BoxFragment()->GetBreakToken());
      } else {
        DCHECK(layout_object->IsInline());
        // This will be updated later if following fragments are found.
        item.SetIsLastForNode(true);
      }

      // If this is the first fragment, associate with |layout_object|.
      const auto last_item_result =
          last_items.insert(layout_object, LastItem{&item, 0, item_index});
      const bool is_first = last_item_result.is_new_entry;
      if (is_first) {
        item.SetFragmentId(0);
        layout_object->SetFirstInlineFragmentItemIndex(item_index);
        continue;
      }

      // Update the last item for |layout_object|.
      LastItem* last = &last_item_result.stored_value->value;
      const FragmentItem* last_item = last->item;
      DCHECK_EQ(last_item->DeltaToNextForSameLayoutObject(), 0u);
      const wtf_size_t last_index = last->item_index;
      DCHECK_GT(last_index, 0u);
      DCHECK_LT(last_index, fragment_items->EndItemIndex());
      DCHECK_LT(last_index, item_index);
      last_item->SetDeltaToNextForSameLayoutObject(item_index - last_index);
      // Because we found a following fragment, reset |IsLastForNode| for the
      // last item except:
      // a. |IsLastForNode| is computed from break token. The last item already
      //    has the correct value.
      // b. Ellipses for atomic inlines. |IsLastForNode| of the last box item
      //    should be set to ease handling of this edge case.
      if (!use_break_token && !(layout_object->IsBox() && item.IsEllipsis()))
        last_item->SetIsLastForNode(false);
#if DCHECK_IS_ON()
      CheckIsLast(*last_item);
#endif

      // Update this item.
      item.SetFragmentId(++last->fragment_id);
      last->item = &item;
      last->item_index = item_index;
    }

    if (!found_inflow_content) {
      may_be_non_contiguous_ifc = true;
    }
  }

  container.SetMayBeNonContiguousIfc(may_be_non_contiguous_ifc);

#if DCHECK_IS_ON()
  for (const auto& iter : last_items)
    CheckIsLast(*iter.value.item);
#endif
}

void FragmentItems::ClearAssociatedFragments(LayoutObject* container) {
  // Clear by traversing |LayoutObject| tree rather than |FragmentItem|
  // because a) we don't need to modify |FragmentItem|, and in general the
  // number of |LayoutObject| is less than the number of |FragmentItem|.
  for (LayoutObject* child = container->SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (!child->IsInLayoutNGInlineFormattingContext() ||
        child->IsOutOfFlowPositioned()) [[unlikely]] {
      continue;
    }
    child->ClearFirstInlineFragmentItemIndex();

    // Children of |LayoutInline| are part of this inline formatting context,
    // but children of other |LayoutObject| (e.g., floats, oof, inline-blocks)
    // are not.
    if (child->IsLayoutInline())
      ClearAssociatedFragments(child);
  }
#if DCHECK_IS_ON()
  if (const auto* box = DynamicTo<LayoutBox>(container)) {
    for (const PhysicalBoxFragment& fragment : box->PhysicalFragments()) {
      CheckNoItemsAreAssociated(fragment);
    }
  }
#endif
}

// static
bool FragmentItems::CanReuseAll(InlineCursor* cursor) {
  for (; *cursor; cursor->MoveToNext()) {
    const FragmentItem& item = *cursor->Current().Item();
    // Ignore nested kLine items though their descendants affect the result.
    if (item.Type() == FragmentItem::kLine) {
      continue;
    }
    if (!item.CanReuse())
      return false;
  }
  return true;
}

const FragmentItem* FragmentItems::EndOfReusableItems(
    const PhysicalBoxFragment& container) const {
  const FragmentItem* last_line_start = &front();
  for (InlineCursor cursor(container, *this); cursor;) {
    const FragmentItem& item = *cursor.Current();
    if (item.IsDirty())
      return &item;

    // Top-level fragments that are not line box cannot be reused; e.g., oof
    // or list markers.
    if (item.Type() != FragmentItem::kLine) {
      return &item;
    }

    // If there is a dirty item in the middle of a line, its previous line is
    // not reusable, because the dirty item may affect the previous line to wrap
    // differently.
    InlineCursor line = cursor.CursorForDescendants();
    if (!CanReuseAll(&line))
      return last_line_start;

    const PhysicalLineBoxFragment& line_box_fragment = *item.LineBoxFragment();

    // Abort if the line propagated its descendants to outside of the line.
    // They are propagated through LayoutResult, which we don't cache.
    if (line_box_fragment.HasPropagatedDescendants())
      return &item;

    // Abort if we are an empty line-box. We don't have any content, and might
    // resolve the BFC block-offset at the incorrect position.
    if (line_box_fragment.IsEmptyLineBox())
      return &item;

    // Abort reusing block-in-inline because it may need to set
    // |PreviousInflowData|.
    if (line_box_fragment.IsBlockInInline()) [[unlikely]] {
      return &item;
    }

    // TODO(kojii): Running the normal layout code at least once for this
    // child helps reducing the code to setup internal states after the
    // partial. Remove the last fragment if it is the end of the
    // fragmentation to do so, but we should figure out how to setup the
    // states without doing this.
    if (!line_box_fragment.GetBreakToken()) {
      return &item;
    }

    last_line_start = &item;
    cursor.MoveToNextSkippingChildren();
  }
  return nullptr;  // all items are reusable.
}

bool FragmentItems::IsContainerForCulledInline(
    const LayoutInline& layout_inline,
    bool* is_first_container,
    bool* is_last_container,
    bool* child_has_any_child_items) const {
  DCHECK(!layout_inline.HasInlineFragments());
  const wtf_size_t start_idx = size_of_earlier_fragments_;
  const wtf_size_t end_idx = EndItemIndex();
  const LayoutObject* next_descendant;
  bool found_item = false;
  bool has_float_ahead = false;
  *is_first_container = true;
  *child_has_any_child_items = false;
  for (const LayoutObject* descendant = layout_inline.FirstChild(); descendant;
       descendant = next_descendant) {
    wtf_size_t item_idx = descendant->FirstInlineFragmentItemIndex();
    if (descendant->IsBox() || item_idx)
      next_descendant = descendant->NextInPreOrderAfterChildren(&layout_inline);
    else
      next_descendant = descendant->NextInPreOrder(&layout_inline);
    if (!item_idx)
      continue;
    *child_has_any_child_items = true;

    // |FirstInlineFragmentItemIndex| is 1-based. Convert to 0-based index.
    item_idx--;

    if (item_idx >= end_idx) {
      if (!found_item && descendant->IsFloating()) {
        // Keep looking if we haven't found anything here. Even if this float
        // starts in a later container, there may still be something to be found
        // in this container. A float may be pushed to the next fragmentainer,
        // while subsequent in-flow content may still fit in this container.
        has_float_ahead = true;
        continue;
      }
      // This descendant starts in a later container. So this isn't the last
      // container for the culled inline.
      *is_last_container = false;
      return found_item;
    }

    if (item_idx < start_idx) {
      // This descendant doesn't start here. But does it occur here?
      *is_first_container = false;
      InlineCursor cursor;
      for (cursor.MoveTo(*descendant); cursor.Current() && item_idx < end_idx;
           cursor.MoveToNextForSameLayoutObject()) {
        item_idx += cursor.Current()->DeltaToNextForSameLayoutObject();
        if (item_idx >= start_idx) {
          if (item_idx >= end_idx) {
            // The descendant occurs in a later container. So this isn't the
            // last container for the culled inline.
            *is_last_container = false;
            return found_item;
          }
          // The descendant occurs here. Proceed to figure out if it ends here
          // as well.
          found_item = true;
        }
      }
      continue;
    }

    // This descendant starts here. Does it end here as well?
    found_item = true;
    const FragmentItem* item = &items_[item_idx - start_idx];
    do {
      if (const wtf_size_t delta = item->DeltaToNextForSameLayoutObject()) {
        item_idx += delta;
        if (item_idx >= end_idx) {
          // This descendant also occurs in a later container. So this isn't the
          // last container for the culled inline.
          *is_last_container = false;
          return true;
        }
        item = &items_[item_idx - start_idx];
      } else {
        item = nullptr;
      }
    } while (item);
  }

  // If we didn't find anything that occurs in a later container, this is the
  // last container for the culled inline.
  *is_last_container = !has_float_ahead;
  return found_item;
}

// static
bool FragmentItems::TryDirtyFirstLineFor(const LayoutObject& layout_object,
                                         const LayoutBlockFlow& container) {
  DCHECK(layout_object.IsDescendantOf(&container));
  InlineCursor cursor(container);
  cursor.MoveTo(layout_object);
  if (!cursor)
    return false;
  DCHECK(cursor.Current().Item());
  DCHECK_EQ(&layout_object, cursor.Current().GetLayoutObject());
  cursor.Current()->SetDirty();
  return true;
}

// static
bool FragmentItems::TryDirtyLastLineFor(const LayoutObject& layout_object,
                                        const LayoutBlockFlow& container) {
  DCHECK(layout_object.IsDescendantOf(&container));
  InlineCursor cursor(container);
  cursor.MoveTo(layout_object);
  if (!cursor)
    return false;
  cursor.MoveToLastForSameLayoutObject();
  DCHECK(cursor.Current().Item());
  DCHECK_EQ(&layout_object, cursor.Current().GetLayoutObject());
  cursor.Current()->SetDirty();
  return true;
}

// static
void FragmentItems::DirtyLinesFromChangedChild(
    const LayoutObject& child,
    const LayoutBlockFlow& container) {
  if (child.IsInLayoutNGInlineFormattingContext() &&
      !child.IsFloatingOrOutOfFlowPositioned()) {
    if (TryDirtyFirstLineFor(child, container))
      return;
  }

  // If |child| is new, or did not generate fragments, mark the fragments for
  // previous |LayoutObject| instead.
  for (const LayoutObject* current = &child;;) {
    if (const LayoutObject* previous = current->PreviousSibling()) {
      while (const auto* layout_inline = DynamicTo<LayoutInline>(previous)) {
        if (const LayoutObject* last_child = layout_inline->LastChild())
          previous = last_child;
        else
          break;
      }
      current = previous;
      if (current->IsFloatingOrOutOfFlowPositioned()) [[unlikely]] {
        continue;
      }
      if (current->IsInLayoutNGInlineFormattingContext()) {
        if (TryDirtyLastLineFor(*current, container))
          return;
      }
      continue;
    }

    current = current->Parent();
    if (!current || current->IsLayoutBlockFlow()) {
      DirtyFirstItem(container);
      return;
    }
    DCHECK(current->IsLayoutInline());
    if (current->IsInLayoutNGInlineFormattingContext()) {
      if (TryDirtyFirstLineFor(*current, container))
        return;
    }
  }
}

// static
void FragmentItems::DirtyFirstItem(const LayoutBlockFlow& container) {
  for (const PhysicalBoxFragment& fragment : container.PhysicalFragments()) {
    if (const FragmentItems* items = fragment.Items()) {
      items->front().SetDirty();
      return;
    }
  }
}

// static
void FragmentItems::DirtyLinesFromNeedsLayout(
    const LayoutBlockFlow& container) {
  DCHECK(base::ranges::any_of(
      container.PhysicalFragments(),
      [](const PhysicalBoxFragment& fragment) { return fragment.HasItems(); }));

  // Mark dirty for the first top-level child that has |NeedsLayout|.
  //
  // TODO(kojii): We could mark first descendant to increase reuse
  // opportunities. Doing this complicates the logic, especially when culled
  // inline is involved, and common case is to append to large IFC. Choose
  // simpler logic and faster to check over more reuse opportunities.
  const auto writing_mode = container.StyleRef().GetWritingMode();
  for (LayoutObject* child = container.FirstChild(); child;
       child = child->NextSibling()) {
    // NeedsLayout is not helpful for an orthogonal writing-mode root because
    // its NeedsLayout flag is cleared during the ComputeMinMaxSizes() step of
    // the container.
    if (child->NeedsLayout() ||
        !IsParallelWritingMode(writing_mode,
                               child->StyleRef().GetWritingMode())) {
      DirtyLinesFromChangedChild(*child, container);
      return;
    }
  }
}

// static
bool FragmentItems::ReplaceBoxFragment(
    const PhysicalBoxFragment& old_fragment,
    const PhysicalBoxFragment& new_fragment,
    const PhysicalBoxFragment& containing_fragment) {
  for (InlineCursor cursor(containing_fragment); cursor; cursor.MoveToNext()) {
    const FragmentItem* item = cursor.Current().Item();
    if (item->BoxFragment() != &old_fragment)
      continue;
    item->GetMutableForCloning().ReplaceBoxFragment(new_fragment);
    return true;
  }
  return false;
}

// static
void FragmentItems::LayoutObjectWillBeMoved(const LayoutObject& layout_object) {
  InlineCursor cursor;
  cursor.MoveTo(layout_object);
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    const FragmentItem* item = cursor.Current().Item();
    item->LayoutObjectWillBeMoved();
  }
}

// static
void FragmentItems::LayoutObjectWillBeDestroyed(
    const LayoutObject& layout_object) {
  InlineCursor cursor;
  cursor.MoveTo(layout_object);
  for (; cursor; cursor.MoveToNextForSameLayoutObject()) {
    const FragmentItem* item = cursor.Current().Item();
    item->LayoutObjectWillBeDestroyed();
  }
}

#if DCHECK_IS_ON()
void FragmentItems::CheckAllItemsAreValid() const {
  for (const FragmentItem& item : Items()) {
    DCHECK(!item.IsLayoutObjectDestroyedOrMoved());
  }
}
#endif

void FragmentItems::Trace(Visitor* visitor) const {
  visitor->Trace(items_);
}

}  // namespace blink
