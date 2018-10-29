// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"

#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/inline_box_traversal.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_logical_rect.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_physical_offset_rect.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_position.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_outline_type.h"
#include "third_party/blink/renderer/core/layout/ng/ng_outline_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_inline_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment_traversal.h"

namespace blink {

namespace {

NGLogicalRect ComputeLogicalRectFor(const NGPhysicalOffsetRect& physical_rect,
                                    const NGPaintFragment& paint_fragment) {
  const WritingMode writing_mode = paint_fragment.Style().GetWritingMode();
  const TextDirection text_direction =
      paint_fragment.PhysicalFragment().ResolvedDirection();
  const NGPhysicalSize outer_size = paint_fragment.Size();
  const NGLogicalOffset logical_offset = physical_rect.offset.ConvertToLogical(
      writing_mode, text_direction, outer_size, physical_rect.size);
  const NGLogicalSize logical_size =
      physical_rect.size.ConvertToLogical(writing_mode);
  return {logical_offset, logical_size};
}

NGPhysicalOffsetRect ComputePhysicalRectFor(
    const NGLogicalRect& logical_rect,
    const NGPaintFragment& paint_fragment) {
  const WritingMode writing_mode = paint_fragment.Style().GetWritingMode();
  const TextDirection text_direction =
      paint_fragment.PhysicalFragment().ResolvedDirection();
  const NGPhysicalSize outer_size = paint_fragment.Size();
  const NGPhysicalSize physical_size =
      logical_rect.size.ConvertToPhysical(writing_mode);
  const NGPhysicalOffset physical_offset =
      logical_rect.offset.ConvertToPhysical(writing_mode, text_direction,
                                            outer_size, physical_size);

  return {physical_offset, physical_size};
}

NGLogicalRect ExpandedSelectionRectForSoftLineBreakIfNeeded(
    const NGLogicalRect& rect,
    const NGPaintFragment& paint_fragment,
    const LayoutSelectionStatus& selection_status) {
  // Expand paint rect if selection covers multiple lines and
  // this fragment is at the end of line.
  if (selection_status.line_break == SelectSoftLineBreak::kNotSelected)
    return rect;
  if (paint_fragment.GetLayoutObject()
          ->ContainingNGBlockFlow()
          ->ShouldTruncateOverflowingText())
    return rect;
  // Copy from InlineTextBoxPainter::PaintSelection.
  const LayoutUnit space_width(paint_fragment.Style().GetFont().SpaceWidth());
  return {rect.offset,
          {rect.size.inline_size + space_width, rect.size.block_size}};
}

// Expands selection height so that the selection rect fills entire line.
NGLogicalRect ExpandSelectionRectToLineHeight(
    const NGLogicalRect& rect,
    const NGPaintFragment& paint_fragment) {
  const NGPaintFragment* current_line = paint_fragment.ContainerLineBox();
  DCHECK(current_line);
  const NGPhysicalOffsetRect line_physical_rect(
      current_line->InlineOffsetToContainerBox() -
          paint_fragment.InlineOffsetToContainerBox(),
      current_line->Size());
  const NGLogicalRect line_logical_rect =
      ComputeLogicalRectFor(line_physical_rect, paint_fragment);
  return {{rect.offset.inline_offset, line_logical_rect.offset.block_offset},
          {rect.size.inline_size, line_logical_rect.size.block_size}};
}

NGLogicalOffset ChildLogicalOffsetInParent(const NGPaintFragment& child) {
  DCHECK(child.Parent());
  const NGPaintFragment& parent = *child.Parent();
  return child.Offset().ConvertToLogical(parent.Style().GetWritingMode(),
                                         parent.Style().Direction(),
                                         parent.Size(), child.Size());
}

NGLogicalSize ChildLogicalSizeInParent(const NGPaintFragment& child) {
  DCHECK(child.Parent());
  const NGPaintFragment& parent = *child.Parent();
  return NGFragment(parent.Style().GetWritingMode(), child.PhysicalFragment())
      .Size();
}

base::Optional<PositionWithAffinity> PositionForPointInChild(
    const NGPaintFragment& child,
    const NGPhysicalOffset& point) {
  const NGPhysicalOffset& child_point = point - child.Offset();
  // We must fallback to legacy for old layout roots. We also fallback (to
  // LayoutNGMixin::PositionForPoint()) for NG block layout, so that we can
  // utilize LayoutBlock::PositionForPoint() that resolves the position in block
  // layout.
  // TODO(xiaochengh): Don't fallback to legacy for NG block layout.
  const bool should_fallback = child.PhysicalFragment().IsBlockFlow() ||
                               child.PhysicalFragment().IsOldLayoutRoot();
  const PositionWithAffinity result =
      should_fallback ? child.GetLayoutObject()->PositionForPoint(
                            child_point.ToLayoutPoint())
                      : child.PositionForPoint(child_point);
  if (result.IsNotNull())
    return result;
  return base::nullopt;
}

// ::before, ::after and ::first-letter can be hit test targets.
bool CanBeHitTestTargetPseudoNode(const Node& node) {
  if (!node.IsPseudoElement())
    return false;
  switch (ToPseudoElement(node).GetPseudoId()) {
    case kPseudoIdBefore:
    case kPseudoIdAfter:
    case kPseudoIdFirstLetter:
      return true;
    default:
      return false;
  }
}

bool IsLastBRInPage(const NGPhysicalTextFragment& text_fragment) {
  return text_fragment.GetLayoutObject()->IsBR() &&
         !text_fragment.GetLayoutObject()->NextInPreOrder();
}

}  // namespace

NGPaintFragment::NGPaintFragment(
    scoped_refptr<const NGPhysicalFragment> fragment,
    NGPhysicalOffset offset,
    NGPaintFragment* parent)
    : physical_fragment_(std::move(fragment)),
      offset_(offset),
      parent_(parent),
      is_dirty_inline_(false) {
  DCHECK(physical_fragment_);
}

NGPaintFragment::~NGPaintFragment() = default;

template <typename Traverse>
NGPaintFragment& NGPaintFragment::List<Traverse>::front() const {
  DCHECK(first_);
  return *first_;
}

template <typename Traverse>
NGPaintFragment& NGPaintFragment::List<Traverse>::back() const {
  DCHECK(first_);
  NGPaintFragment* last = first_;
  for (NGPaintFragment* fragment : *this)
    last = fragment;
  return *last;
}

template <typename Traverse>
wtf_size_t NGPaintFragment::List<Traverse>::size() const {
  wtf_size_t size = 0;
  for (NGPaintFragment* fragment : *this) {
    ANALYZER_ALLOW_UNUSED(fragment);
    ++size;
  }
  return size;
}

template <typename Traverse>
void NGPaintFragment::List<Traverse>::ToList(
    Vector<NGPaintFragment*, 16>* list) const {
  if (UNLIKELY(!list->IsEmpty()))
    list->Shrink(0);
  if (IsEmpty())
    return;
  list->ReserveCapacity(size());
  for (NGPaintFragment* fragment : *this)
    list->push_back(fragment);
}

void NGPaintFragment::SetShouldDoFullPaintInvalidation() {
  if (LayoutObject* layout_object = GetLayoutObject())
    layout_object->SetShouldDoFullPaintInvalidation();
}

scoped_refptr<NGPaintFragment> NGPaintFragment::CreateOrReuse(
    scoped_refptr<const NGPhysicalFragment> fragment,
    NGPhysicalOffset offset,
    NGPaintFragment* parent,
    scoped_refptr<NGPaintFragment> previous_instance,
    bool* populate_children) {
  DCHECK(fragment);

  // If the previous instance is given, check if it is re-usable.
  // Re-using NGPaintFragment allows the paint system to identify objects.
  if (previous_instance) {
    DCHECK_EQ(previous_instance->parent_, parent);

    // If the physical fragment was re-used, re-use the paint fragment as well.
    if (&previous_instance->PhysicalFragment() == fragment.get()) {
      previous_instance->offset_ = offset;
      previous_instance->next_for_same_layout_object_ = nullptr;
      // No need to re-populate children because NGPhysicalFragment is
      // immutable and thus children should not have been changed.
      *populate_children = false;
      previous_instance->SetShouldDoFullPaintInvalidation();
      return previous_instance;
    }

    // If the LayoutObject are the same, the new paint fragment should have the
    // same DisplayItemClient identity as the previous instance.
    if (previous_instance->GetLayoutObject() == fragment->GetLayoutObject()) {
      previous_instance->physical_fragment_ = std::move(fragment);
      previous_instance->offset_ = offset;
      previous_instance->next_for_same_layout_object_ = nullptr;
      if (!*populate_children)
        previous_instance->first_child_ = nullptr;
      previous_instance->SetShouldDoFullPaintInvalidation();
      return previous_instance;
    }
  }

  scoped_refptr<NGPaintFragment> new_instance =
      base::AdoptRef(new NGPaintFragment(std::move(fragment), offset, parent));
  new_instance->SetShouldDoFullPaintInvalidation();
  return new_instance;
}

scoped_refptr<NGPaintFragment> NGPaintFragment::Create(
    scoped_refptr<const NGPhysicalFragment> fragment,
    NGPhysicalOffset offset,
    scoped_refptr<NGPaintFragment> previous_instance) {
  DCHECK(fragment);

  bool populate_children = fragment->IsContainer();
  scoped_refptr<NGPaintFragment> paint_fragment =
      CreateOrReuse(std::move(fragment), offset, nullptr,
                    std::move(previous_instance), &populate_children);

  if (populate_children) {
    HashMap<const LayoutObject*, NGPaintFragment*> last_fragment_map;
    paint_fragment->PopulateDescendants(NGPhysicalOffset(), &last_fragment_map);
  }

  return paint_fragment;
}

void NGPaintFragment::UpdateFromCachedLayoutResult(
    scoped_refptr<const NGPhysicalFragment> fragment,
    NGPhysicalOffset offset) {
  DCHECK(fragment);

#if DCHECK_IS_ON()
  // When updating to a cached layout result, only offset can change. Check
  // children do not change.
  const NGPhysicalContainerFragment& container_fragment =
      ToNGPhysicalContainerFragment(*fragment);
  NGPaintFragment* child = FirstChild();
  for (unsigned i = 0; i < container_fragment.Children().size();
       i++, child = child->NextSibling()) {
    DCHECK(child);
    DCHECK_EQ(child->physical_fragment_.get(),
              container_fragment.Children()[i].get());
  }
  DCHECK(!child);
#endif

  DCHECK_EQ(physical_fragment_.get(), fragment.get());
  physical_fragment_ = std::move(fragment);
  offset_ = offset;
}

NGPaintFragment* NGPaintFragment::Last(const NGBreakToken& break_token) {
  for (NGPaintFragment* fragment = this; fragment;
       fragment = fragment->next_fragmented_.get()) {
    if (fragment->PhysicalFragment().BreakToken() == &break_token)
      return fragment;
  }
  return nullptr;
}

NGPaintFragment* NGPaintFragment::Last() {
  for (NGPaintFragment* fragment = this;;) {
    NGPaintFragment* next = fragment->next_fragmented_.get();
    if (!next)
      return fragment;
    fragment = next;
  }
}

scoped_refptr<NGPaintFragment>* NGPaintFragment::Find(
    scoped_refptr<NGPaintFragment>* fragment,
    const NGBreakToken* break_token) {
  DCHECK(fragment);

  if (!break_token)
    return fragment;

  while (true) {
    // TODO(kojii): Sometimes an unknown break_token is given. Need to
    // investigate why, and handle appropriately. For now, just keep it to avoid
    // crashes and use-after-free.
    if (!*fragment)
      return fragment;

    scoped_refptr<NGPaintFragment>* next = &(*fragment)->next_fragmented_;
    if ((*fragment)->PhysicalFragment().BreakToken() == break_token)
      return next;
    fragment = next;
  }
  NOTREACHED();
}

void NGPaintFragment::SetNext(scoped_refptr<NGPaintFragment> fragment) {
  next_fragmented_ = std::move(fragment);
}

bool NGPaintFragment::IsDescendantOfNotSelf(
    const NGPaintFragment& ancestor) const {
  for (const NGPaintFragment* fragment = Parent(); fragment;
       fragment = fragment->Parent()) {
    if (fragment == &ancestor)
      return true;
  }
  return false;
}

bool NGPaintFragment::HasSelfPaintingLayer() const {
  return physical_fragment_->IsBox() &&
         ToNGPhysicalBoxFragment(*physical_fragment_).HasSelfPaintingLayer();
}

bool NGPaintFragment::HasOverflowClip() const {
  return physical_fragment_->IsBox() &&
         ToNGPhysicalBoxFragment(*physical_fragment_).HasOverflowClip();
}

bool NGPaintFragment::ShouldClipOverflow() const {
  return physical_fragment_->IsBox() &&
         ToNGPhysicalBoxFragment(*physical_fragment_).ShouldClipOverflow();
}

LayoutRect NGPaintFragment::SelfInkOverflow() const {
  return physical_fragment_->InkOverflow().ToLayoutRect();
}

LayoutRect NGPaintFragment::ChildrenInkOverflow() const {
  return physical_fragment_->InkOverflow(false).ToLayoutRect();
}

// Populate descendants from NGPhysicalFragment tree.
void NGPaintFragment::PopulateDescendants(
    const NGPhysicalOffset inline_offset_to_container_box,
    HashMap<const LayoutObject*, NGPaintFragment*>* last_fragment_map) {
  const NGPhysicalFragment& fragment = PhysicalFragment();
  DCHECK(fragment.IsContainer());
  const NGPhysicalContainerFragment& container =
      ToNGPhysicalContainerFragment(fragment);
  scoped_refptr<NGPaintFragment> previous_children = std::move(first_child_);
  scoped_refptr<NGPaintFragment>* last_child_ptr = &first_child_;

  bool children_are_inline =
      !fragment.IsBox() || ToNGPhysicalBoxFragment(fragment).ChildrenInline();

  for (const NGLink& child_fragment : container.Children()) {
    bool populate_children = child_fragment->IsContainer() &&
                             !child_fragment->IsBlockFormattingContextRoot();
    scoped_refptr<NGPaintFragment> previous_child;
    if (previous_children) {
      previous_child = std::move(previous_children);
      previous_children = std::move(previous_child->next_sibling_);
    }
    scoped_refptr<NGPaintFragment> child =
        CreateOrReuse(child_fragment.get(), child_fragment.Offset(), this,
                      std::move(previous_child), &populate_children);

    if (children_are_inline) {
      if (!child_fragment->IsFloating() &&
          !child_fragment->IsOutOfFlowPositioned() &&
          !child_fragment->IsListMarker()) {
        if (LayoutObject* layout_object = child_fragment->GetLayoutObject())
          child->AssociateWithLayoutObject(layout_object, last_fragment_map);

        child->inline_offset_to_container_box_ =
            inline_offset_to_container_box + child_fragment.Offset();
      }

      if (populate_children) {
        child->PopulateDescendants(child->inline_offset_to_container_box_,
                                   last_fragment_map);
      }
    }

    DCHECK(!*last_child_ptr);
    *last_child_ptr = std::move(child);
    last_child_ptr = &((*last_child_ptr)->next_sibling_);
  }
}

// Add to a linked list for each LayoutObject.
void NGPaintFragment::AssociateWithLayoutObject(
    LayoutObject* layout_object,
    HashMap<const LayoutObject*, NGPaintFragment*>* last_fragment_map) {
  DCHECK(layout_object);
  DCHECK(!next_for_same_layout_object_);
  DCHECK(layout_object->IsInline());

  auto add_result = last_fragment_map->insert(layout_object, this);
  if (add_result.is_new_entry) {
    DCHECK(!layout_object->FirstInlineFragment());
    layout_object->SetFirstInlineFragment(this);
  } else {
    DCHECK(add_result.stored_value->value);
    add_result.stored_value->value->next_for_same_layout_object_ = this;
    add_result.stored_value->value = this;
  }
}

NGPaintFragment* NGPaintFragment::GetForInlineContainer(
    const LayoutObject* layout_object) {
  DCHECK(layout_object && layout_object->IsInline());
  if (LayoutBlockFlow* block_flow = layout_object->ContainingNGBlockFlow()) {
    if (NGPaintFragment* fragment = block_flow->PaintFragment())
      return fragment;

    // TODO(kojii): IsLayoutFlowThread should probably be done in
    // ContainingNGBlockFlow(), but there seem to be both expectations today.
    // This needs cleanup.
    if (block_flow->IsLayoutFlowThread()) {
      DCHECK(block_flow->Parent() && block_flow->Parent()->IsLayoutBlockFlow());
      return ToLayoutBlockFlow(block_flow->Parent())->PaintFragment();
    }
  }
  return nullptr;
}

NGPaintFragment::FragmentRange NGPaintFragment::InlineFragmentsFor(
    const LayoutObject* layout_object) {
  DCHECK(layout_object && layout_object->IsInline() &&
         !layout_object->IsFloatingOrOutOfFlowPositioned());

  if (layout_object->IsInLayoutNGInlineFormattingContext())
    return FragmentRange(layout_object->FirstInlineFragment());
  return FragmentRange(nullptr, false);
}

NGPaintFragment* NGPaintFragment::LastForSameLayoutObject() {
  NGPaintFragment* fragment = this;
  while (fragment->next_for_same_layout_object_)
    fragment = fragment->next_for_same_layout_object_;
  return fragment;
}

void NGPaintFragment::DirtyLinesFromChangedChild(LayoutObject* child) {
  if (child->IsInline())
    MarkLineBoxesDirtyFor(*child);
}

bool NGPaintFragment::FlippedLocalVisualRectFor(
    const LayoutObject* layout_object,
    LayoutRect* visual_rect) {
  auto fragments = InlineFragmentsFor(layout_object);
  if (!fragments.IsInLayoutNGInlineFormattingContext())
    return false;

  for (NGPaintFragment* fragment : fragments) {
    NGPhysicalOffsetRect child_visual_rect =
        fragment->PhysicalFragment().SelfInkOverflow();
    child_visual_rect.offset += fragment->InlineOffsetToContainerBox();
    visual_rect->Unite(child_visual_rect.ToLayoutRect());
  }
  if (!layout_object->HasFlippedBlocksWritingMode())
    return true;

  NGPaintFragment* container = GetForInlineContainer(layout_object);
  DCHECK(container);
  ToLayoutBox(container->GetLayoutObject())->FlipForWritingMode(*visual_rect);
  return true;
}

void NGPaintFragment::UpdateVisualRectForNonLayoutObjectChildren() {
  // Scan direct children only beause line boxes are always direct children of
  // the inline formatting context.
  for (NGPaintFragment* child : Children()) {
    if (!child->PhysicalFragment().IsLineBox())
      continue;
    LayoutRect union_of_children;
    for (const NGPaintFragment* descendant : child->Children())
      union_of_children.Unite(descendant->VisualRect());
    child->SetVisualRect(union_of_children);
  }
}

void NGPaintFragment::AddSelfOutlineRect(Vector<LayoutRect>* outline_rects,
                                         const LayoutPoint& additional_offset,
                                         NGOutlineType outline_type) const {
  DCHECK(outline_rects);
  const NGPhysicalFragment& fragment = PhysicalFragment();
  if (fragment.IsBox()) {
    if (NGOutlineUtils::IsInlineOutlineNonpaintingFragment(PhysicalFragment()))
      return;
    ToNGPhysicalBoxFragment(fragment).AddSelfOutlineRects(
        outline_rects, additional_offset, outline_type);
  }
}

void NGPaintFragment::PaintInlineBoxForDescendants(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset,
    const LayoutInline* layout_object,
    NGPhysicalOffset offset) const {
  DCHECK(layout_object);
  for (const NGPaintFragment* child : Children()) {
    if (child->GetLayoutObject() == layout_object) {
      NGInlineBoxFragmentPainter(*child).Paint(
          paint_info, paint_offset + offset.ToLayoutPoint() /*, paint_offset*/);
      continue;
    }

    child->PaintInlineBoxForDescendants(paint_info, paint_offset, layout_object,
                                        offset + child->Offset());
  }
}

const NGPaintFragment* NGPaintFragment::ContainerLineBox() const {
  DCHECK(PhysicalFragment().IsInline());
  for (const NGPaintFragment* fragment :
       NGPaintFragmentTraversal::InclusiveAncestorsOf(*this)) {
    if (fragment->PhysicalFragment().IsLineBox())
      return fragment;
  }
  NOTREACHED();
  return nullptr;
}

NGPaintFragment* NGPaintFragment::FirstLineBox() const {
  for (NGPaintFragment* child : Children()) {
    if (child->PhysicalFragment().IsLineBox())
      return child;
  }
  return nullptr;
}

void NGPaintFragment::MarkLineBoxesDirtyFor(const LayoutObject& layout_object) {
  DCHECK(layout_object.IsInline()) << layout_object;

  if (TryMarkFirstLineBoxDirtyFor(layout_object))
    return;

  // Since |layout_object| isn't in fragment tree, check preceding siblings.
  // Note: Once we reuse lines below dirty lines, we should check next siblings.
  for (LayoutObject* previous = layout_object.PreviousSibling(); previous;
       previous = previous->PreviousSibling()) {
    // If the previoius object had never been laid out, it should have already
    // marked the line box dirty.
    if (!previous->EverHadLayout())
      return;

    if (previous->IsFloatingOrOutOfFlowPositioned())
      continue;

    // |previous| may not be in inline formatting context, e.g. <object>.
    if (TryMarkLastLineBoxDirtyFor(*previous))
      return;
  }

  // There is no siblings, try parent. If it's a non-atomic inline (e.g., span),
  // mark dirty for it, but if it's an atomic inline (e.g., inline block), do
  // not propagate across inline formatting context boundary.
  const LayoutObject& parent = *layout_object.Parent();
  if (parent.IsInline() && !parent.IsAtomicInlineLevel())
    return MarkLineBoxesDirtyFor(parent);

  // The |layout_object| is inserted into an empty block.
  // Mark the first line box dirty.
  if (parent.IsLayoutNGMixin()) {
    const LayoutBlockFlow& block = ToLayoutBlockFlow(parent);
    if (NGPaintFragment* paint_fragment = block.PaintFragment()) {
      if (NGPaintFragment* first_line = paint_fragment->FirstLineBox()) {
        first_line->is_dirty_inline_ = true;
        return;
      }
    }
  }
}

void NGPaintFragment::MarkLineBoxDirty() {
  for (NGPaintFragment* fragment :
       NGPaintFragmentTraversal::InclusiveAncestorsOf(*this)) {
    if (fragment->is_dirty_inline_)
      return;
    fragment->is_dirty_inline_ = true;
    if (fragment->PhysicalFragment().IsLineBox())
      return;
  }
  NOTREACHED() << this;  // Should have a line box ancestor.
}

bool NGPaintFragment::TryMarkFirstLineBoxDirtyFor(
    const LayoutObject& layout_object) {
  if (!layout_object.IsInLayoutNGInlineFormattingContext())
    return false;
  // Once we reuse lines below dirty lines, we should mark lines for all
  // inline fragments.
  if (NGPaintFragment* const fragment = layout_object.FirstInlineFragment()) {
    fragment->MarkLineBoxDirty();
    return true;
  }
  return false;
}

bool NGPaintFragment::TryMarkLastLineBoxDirtyFor(
    const LayoutObject& layout_object) {
  if (!layout_object.IsInLayoutNGInlineFormattingContext())
    return false;
  // Once we reuse lines below dirty lines, we should mark lines for all
  // inline fragments.
  if (NGPaintFragment* const fragment = layout_object.FirstInlineFragment()) {
    fragment->LastForSameLayoutObject()->MarkLineBoxDirty();
    return true;
  }
  return false;
}

void NGPaintFragment::SetShouldDoFullPaintInvalidationRecursively() {
  if (LayoutObject* layout_object = GetLayoutObject())
    layout_object->SetShouldDoFullPaintInvalidation();

  for (NGPaintFragment* child : Children())
    child->SetShouldDoFullPaintInvalidationRecursively();
}

void NGPaintFragment::SetShouldDoFullPaintInvalidationForFirstLine() {
  DCHECK(PhysicalFragment().IsBox() && GetLayoutObject() &&
         GetLayoutObject()->IsLayoutBlockFlow());

  if (NGPaintFragment* line_box = FirstLineBox())
    line_box->SetShouldDoFullPaintInvalidationRecursively();
}

NGPhysicalOffsetRect NGPaintFragment::ComputeLocalSelectionRectForText(
    const LayoutSelectionStatus& selection_status) const {
  const NGPhysicalTextFragment& text_fragment =
      ToNGPhysicalTextFragmentOrDie(PhysicalFragment());
  NGPhysicalOffsetRect selection_rect =
      text_fragment.LocalRect(selection_status.start, selection_status.end);
  NGLogicalRect logical_rect = ComputeLogicalRectFor(selection_rect, *this);
  // Let LocalRect for line break have a space width to paint line break
  // when it is only character in a line or only selected in a line.
  if (text_fragment.IsLineBreak() &&
      selection_status.start != selection_status.end &&
      // This is for old compatible that old doesn't paint last br in a page.
      !IsLastBRInPage(text_fragment)) {
    DCHECK(!logical_rect.size.inline_size);
    logical_rect.size.inline_size = LayoutUnit(Style().GetFont().SpaceWidth());
  }
  const NGLogicalRect line_break_extended_rect =
      text_fragment.IsLineBreak()
          ? logical_rect
          : ExpandedSelectionRectForSoftLineBreakIfNeeded(logical_rect, *this,
                                                          selection_status);
  const NGLogicalRect line_height_expanded_rect =
      ExpandSelectionRectToLineHeight(line_break_extended_rect, *this);
  const NGPhysicalOffsetRect physical_rect =
      ComputePhysicalRectFor(line_height_expanded_rect, *this);
  return physical_rect;
}

NGPhysicalOffsetRect NGPaintFragment::ComputeLocalSelectionRectForReplaced()
    const {
  DCHECK(GetLayoutObject()->IsLayoutReplaced());
  const NGPhysicalOffsetRect selection_rect = PhysicalFragment().LocalRect();
  NGLogicalRect logical_rect = ComputeLogicalRectFor(selection_rect, *this);
  const NGLogicalRect line_height_expanded_rect =
      ExpandSelectionRectToLineHeight(logical_rect, *this);
  const NGPhysicalOffsetRect physical_rect =
      ComputePhysicalRectFor(line_height_expanded_rect, *this);
  return physical_rect;
}

PositionWithAffinity NGPaintFragment::PositionForPointInText(
    const NGPhysicalOffset& point) const {
  DCHECK(PhysicalFragment().IsText());
  const NGPhysicalTextFragment& text_fragment =
      ToNGPhysicalTextFragment(PhysicalFragment());
  if (text_fragment.IsAnonymousText())
    return PositionWithAffinity();
  const unsigned text_offset = text_fragment.TextOffsetForPoint(point);
  const NGCaretPosition unadjusted_position{
      this, NGCaretPositionType::kAtTextOffset, text_offset};
  if (text_offset > text_fragment.StartOffset() &&
      text_offset < text_fragment.EndOffset()) {
    return unadjusted_position.ToPositionInDOMTreeWithAffinity();
  }
  return BidiAdjustment::AdjustForHitTest(unadjusted_position)
      .ToPositionInDOMTreeWithAffinity();
}

PositionWithAffinity NGPaintFragment::PositionForPointInInlineLevelBox(
    const NGPhysicalOffset& point) const {
  DCHECK(PhysicalFragment().IsInline() || PhysicalFragment().IsLineBox());
  DCHECK(!PhysicalFragment().IsBlockFlow());

  const NGLogicalOffset logical_point = point.ConvertToLogical(
      Style().GetWritingMode(), Style().Direction(), Size(),
      // |point| is actually a pixel with size 1x1.
      NGPhysicalSize(LayoutUnit(1), LayoutUnit(1)));
  const LayoutUnit inline_point = logical_point.inline_offset;

  // Stores the closest child before |point| in the inline direction. Used if we
  // can't find any child |point| falls in to resolve the position.
  const NGPaintFragment* closest_child_before = nullptr;
  LayoutUnit closest_child_before_inline_offset = LayoutUnit::Min();

  // Stores the closest child after |point| in the inline direction. Used if we
  // can't find any child |point| falls in to resolve the position.
  const NGPaintFragment* closest_child_after = nullptr;
  LayoutUnit closest_child_after_inline_offset = LayoutUnit::Max();

  for (const NGPaintFragment* child : Children()) {
    const LayoutUnit child_inline_min =
        ChildLogicalOffsetInParent(*child).inline_offset;
    const LayoutUnit child_inline_max =
        child_inline_min + ChildLogicalSizeInParent(*child).inline_size;

    // Try to resolve if |point| falls in any child in inline direction.
    if (inline_point >= child_inline_min && inline_point <= child_inline_max) {
      if (auto child_position = PositionForPointInChild(*child, point))
        return child_position.value();
      continue;
    }

    if (inline_point < child_inline_min) {
      if (child_inline_min < closest_child_after_inline_offset) {
        closest_child_after = child;
        closest_child_after_inline_offset = child_inline_min;
      }
    }

    if (inline_point > child_inline_max) {
      if (child_inline_max > closest_child_before_inline_offset) {
        closest_child_before = child;
        closest_child_before_inline_offset = child_inline_max;
      }
    }
  }

  if (closest_child_after) {
    if (auto child_position =
            PositionForPointInChild(*closest_child_after, point))
      return child_position.value();
  }

  if (closest_child_before) {
    if (auto child_position =
            PositionForPointInChild(*closest_child_before, point))
      return child_position.value();
  }

  return PositionWithAffinity();
}

PositionWithAffinity NGPaintFragment::PositionForPointInInlineFormattingContext(
    const NGPhysicalOffset& point) const {
  DCHECK(PhysicalFragment().IsBlockFlow());
  DCHECK(PhysicalFragment().IsBox());
  DCHECK(ToNGPhysicalBoxFragment(PhysicalFragment()).ChildrenInline());

  const NGLogicalOffset logical_point = point.ConvertToLogical(
      Style().GetWritingMode(), Style().Direction(), Size(),
      // |point| is actually a pixel with size 1x1.
      NGPhysicalSize(LayoutUnit(1), LayoutUnit(1)));
  const LayoutUnit block_point = logical_point.block_offset;

  // Stores the closest line box child above |point| in the block direction.
  // Used if we can't find any child |point| falls in to resolve the position.
  const NGPaintFragment* closest_line_before = nullptr;
  LayoutUnit closest_line_before_block_offset = LayoutUnit::Min();

  // Stores the closest line box child below |point| in the block direction.
  // Used if we can't find any child |point| falls in to resolve the position.
  const NGPaintFragment* closest_line_after = nullptr;
  LayoutUnit closest_line_after_block_offset = LayoutUnit::Max();

  for (const NGPaintFragment* child : Children()) {
    if (!child->PhysicalFragment().IsLineBox() || child->Children().IsEmpty())
      continue;

    const LayoutUnit line_min = ChildLogicalOffsetInParent(*child).block_offset;
    const LayoutUnit line_max =
        line_min + ChildLogicalSizeInParent(*child).block_size;

    // Try to resolve if |point| falls in a line box in block direction.
    // Hitting on line bottom doesn't count, to match legacy behavior.
    // TODO(xiaochengh): Consider floats.
    if (block_point >= line_min && block_point < line_max) {
      if (auto child_position = PositionForPointInChild(*child, point))
        return child_position.value();
      continue;
    }

    if (block_point < line_min) {
      if (line_min < closest_line_after_block_offset) {
        closest_line_after = child;
        closest_line_after_block_offset = line_min;
      }
    }

    if (block_point >= line_max) {
      if (line_max > closest_line_before_block_offset) {
        closest_line_before = child;
        closest_line_before_block_offset = line_max;
      }
    }
  }

  if (closest_line_after) {
    if (auto child_position =
            PositionForPointInChild(*closest_line_after, point))
      return child_position.value();
  }

  if (closest_line_before) {
    if (auto child_position =
            PositionForPointInChild(*closest_line_before, point))
      return child_position.value();
  }

  // TODO(xiaochengh): Looking at only the closest lines may not be enough,
  // when we have multiple lines full of pseudo elements. Fix it.

  // TODO(xiaochengh): Consider floats. See crbug.com/758526.

  return PositionWithAffinity();
}

PositionWithAffinity NGPaintFragment::PositionForPoint(
    const NGPhysicalOffset& point) const {
  if (PhysicalFragment().IsText())
    return PositionForPointInText(point);

  if (PhysicalFragment().IsBlockFlow()) {
    // We current fall back to legacy for block formatting contexts, so we
    // should reach here only for inline formatting contexts.
    // TODO(xiaochengh): Do not fall back.
    DCHECK(ToNGPhysicalBoxFragment(PhysicalFragment()).ChildrenInline());
    return PositionForPointInInlineFormattingContext(point);
  }

  DCHECK(PhysicalFragment().IsInline() || PhysicalFragment().IsLineBox());
  return PositionForPointInInlineLevelBox(point);
}

Node* NGPaintFragment::NodeForHitTest() const {
  if (GetNode())
    return GetNode();

  if (PhysicalFragment().IsLineBox())
    return Parent()->NodeForHitTest();

  // When the fragment is a list marker, return the list item.
  if (GetLayoutObject() && GetLayoutObject()->IsLayoutNGListMarker())
    return ToLayoutNGListMarker(GetLayoutObject())->ListItem()->GetNode();

  for (const NGPaintFragment* runner = Parent(); runner;
       runner = runner->Parent()) {
    // When the fragment is inside a ::first-letter, ::before or ::after pseudo
    // node, return the pseudo node.
    if (Node* node = runner->GetNode()) {
      if (CanBeHitTestTargetPseudoNode(*node))
        return node;
      return nullptr;
    }

    // When the fragment is inside a list marker, return the list item.
    if (runner->GetLayoutObject() &&
        runner->GetLayoutObject()->IsLayoutNGListMarker()) {
      return runner->NodeForHitTest();
    }
  }

  return nullptr;
}

bool NGPaintFragment::ShouldPaintCursorCaret() const {
  // TODO(xiaochengh): Merge cursor caret painting functions from LayoutBlock to
  // FrameSelection.
  if (!GetLayoutObject()->IsLayoutBlock())
    return false;
  return ToLayoutBlock(GetLayoutObject())->ShouldPaintCursorCaret();
}

bool NGPaintFragment::ShouldPaintDragCaret() const {
  // TODO(xiaochengh): Merge drag caret painting functions from LayoutBlock to
  // DragCaret.
  if (!GetLayoutObject()->IsLayoutBlock())
    return false;
  return ToLayoutBlock(GetLayoutObject())->ShouldPaintDragCaret();
}

// ----

NGPaintFragment& NGPaintFragment::FragmentRange::front() const {
  DCHECK(first_);
  return *first_;
}

NGPaintFragment& NGPaintFragment::FragmentRange::back() const {
  DCHECK(first_);
  NGPaintFragment* last = first_;
  for (NGPaintFragment* fragment : *this)
    last = fragment;
  return *last;
}

wtf_size_t NGPaintFragment::FragmentRange::size() const {
  wtf_size_t size = 0;
  for (NGPaintFragment* fragment : *this) {
    ANALYZER_ALLOW_UNUSED(fragment);
    ++size;
  }
  return size;
}

String NGPaintFragment::DebugName() const {
  StringBuilder name;

  DCHECK(physical_fragment_);
  const NGPhysicalFragment& physical_fragment = *physical_fragment_;
  if (physical_fragment.IsBox()) {
    name.Append("NGPhysicalBoxFragment");
    if (LayoutObject* layout_object = physical_fragment.GetLayoutObject()) {
      DCHECK(physical_fragment.IsBox());
      name.Append(' ');
      name.Append(layout_object->DebugName());
    }
  } else if (physical_fragment.IsText()) {
    name.Append("NGPhysicalTextFragment '");
    name.Append(ToNGPhysicalTextFragment(physical_fragment).Text());
    name.Append('\'');
  } else if (physical_fragment.IsLineBox()) {
    name.Append("NGPhysicalLineBoxFragment");
  } else {
    NOTREACHED();
  }

  return name.ToString();
}

template class CORE_TEMPLATE_EXPORT
    NGPaintFragment::List<NGPaintFragment::TraverseNextSibling>;

}  // namespace blink
