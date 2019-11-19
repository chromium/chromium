// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"

#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/inline_box_traversal.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_position.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_offset_mapping.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_line_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_physical_text_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/layout_ng_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_item.h"
#include "third_party/blink/renderer/core/layout/ng/list/layout_ng_list_marker.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_break_token.h"
#include "third_party/blink/renderer/core/layout/ng/ng_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_ink_overflow.h"
#include "third_party/blink/renderer/core/layout/ng/ng_outline_type.h"
#include "third_party/blink/renderer/core/layout/ng/ng_outline_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_container_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_fragment.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment_traversal.h"

namespace blink {

namespace {

struct SameSizeAsNGPaintFragment : public RefCounted<NGPaintFragment>,
                                   public DisplayItemClient {
  void* pointers[7];
  PhysicalOffset offsets[2];
  unsigned flags;
};

static_assert(sizeof(NGPaintFragment) == sizeof(SameSizeAsNGPaintFragment),
              "NGPaintFragment should stay small.");

LogicalRect ComputeLogicalRectFor(const PhysicalRect& physical_rect,
                                  WritingMode writing_mode,
                                  TextDirection text_direction,
                                  const PhysicalSize& outer_size) {
  const LogicalOffset logical_offset = physical_rect.offset.ConvertToLogical(
      writing_mode, text_direction, outer_size, physical_rect.size);
  const LogicalSize logical_size =
      physical_rect.size.ConvertToLogical(writing_mode);
  return {logical_offset, logical_size};
}

LogicalRect ComputeLogicalRectFor(const PhysicalRect& physical_rect,
                                  const NGPaintFragment& paint_fragment) {
  return ComputeLogicalRectFor(
      physical_rect, paint_fragment.Style().GetWritingMode(),
      paint_fragment.PhysicalFragment().ResolvedDirection(),
      paint_fragment.Size());
}

LogicalRect ComputeLogicalRectFor(const PhysicalRect& physical_rect,
                                  const NGInlineCursor& cursor) {
  if (const NGPaintFragment* paint_fragment = cursor.CurrentPaintFragment())
    return ComputeLogicalRectFor(physical_rect, *paint_fragment);

  const NGFragmentItem& item = *cursor.CurrentItem();
  return ComputeLogicalRectFor(physical_rect, item.GetWritingMode(),
                               item.ResolvedDirection(), item.Size());
}

PhysicalRect ComputePhysicalRectFor(const LogicalRect& logical_rect,
                                    WritingMode writing_mode,
                                    TextDirection text_direction,
                                    const PhysicalSize& outer_size) {
  const PhysicalSize physical_size =
      ToPhysicalSize(logical_rect.size, writing_mode);
  const PhysicalOffset physical_offset = logical_rect.offset.ConvertToPhysical(
      writing_mode, text_direction, outer_size, physical_size);

  return {physical_offset, physical_size};
}
PhysicalRect ComputePhysicalRectFor(const LogicalRect& logical_rect,
                                    const NGPaintFragment& paint_fragment) {
  return ComputePhysicalRectFor(
      logical_rect, paint_fragment.Style().GetWritingMode(),
      paint_fragment.PhysicalFragment().ResolvedDirection(),
      paint_fragment.Size());
}

PhysicalRect ComputePhysicalRectFor(const LogicalRect& logical_rect,
                                    const NGInlineCursor& cursor) {
  if (const NGPaintFragment* paint_fragment = cursor.CurrentPaintFragment())
    return ComputePhysicalRectFor(logical_rect, *paint_fragment);
  const NGFragmentItem& item = *cursor.CurrentItem();
  return ComputePhysicalRectFor(logical_rect, item.GetWritingMode(),
                                item.ResolvedDirection(), item.Size());
}

LogicalRect ExpandedSelectionRectForSoftLineBreakIfNeeded(
    const LogicalRect& rect,
    const NGInlineCursor& cursor,
    const LayoutSelectionStatus& selection_status) {
  // Expand paint rect if selection covers multiple lines and
  // this fragment is at the end of line.
  if (selection_status.line_break == SelectSoftLineBreak::kNotSelected)
    return rect;
  const LayoutBlockFlow* const layout_block_flow = cursor.GetLayoutBlockFlow();
  if (layout_block_flow && layout_block_flow->ShouldTruncateOverflowingText())
    return rect;
  // Copy from InlineTextBoxPainter::PaintSelection.
  const LayoutUnit space_width(cursor.CurrentStyle().GetFont().SpaceWidth());
  return {rect.offset,
          {rect.size.inline_size + space_width, rect.size.block_size}};
}

// Expands selection height so that the selection rect fills entire line.
LogicalRect ExpandSelectionRectToLineHeight(
    const LogicalRect& rect,
    const LogicalRect& line_logical_rect) {
  // Unite the rect only in the block direction.
  const LayoutUnit selection_top =
      std::min(rect.offset.block_offset, line_logical_rect.offset.block_offset);
  const LayoutUnit selection_bottom =
      std::max(rect.BlockEndOffset(), line_logical_rect.BlockEndOffset());
  return {{rect.offset.inline_offset, selection_top},
          {rect.size.inline_size, selection_bottom - selection_top}};
}

LogicalRect ExpandSelectionRectToLineHeight(const LogicalRect& rect,
                                            const NGInlineCursor& cursor) {
  NGInlineCursor line(cursor);
  line.MoveToContainingLine();
  const PhysicalRect line_physical_rect(
      line.CurrentOffset() - cursor.CurrentOffset(), line.CurrentSize());
  return ExpandSelectionRectToLineHeight(
      rect, ComputeLogicalRectFor(line_physical_rect, cursor));
}

LogicalOffset ChildLogicalOffsetInParent(const NGPaintFragment& child) {
  DCHECK(child.Parent());
  const NGPaintFragment& parent = *child.Parent();
  return child.Offset().ConvertToLogical(parent.Style().GetWritingMode(),
                                         parent.Style().Direction(),
                                         parent.Size(), child.Size());
}

LogicalSize ChildLogicalSizeInParent(const NGPaintFragment& child) {
  DCHECK(child.Parent());
  const NGPaintFragment& parent = *child.Parent();
  return NGFragment(parent.Style().GetWritingMode(), child.PhysicalFragment())
      .Size();
}

base::Optional<PositionWithAffinity> PositionForPointInChild(
    const NGPaintFragment& child,
    const PhysicalOffset& point) {
  const PhysicalOffset& child_point = point - child.Offset();
  // We must fallback to legacy for old layout roots. We also fallback (to
  // LayoutNGMixin::PositionForPoint()) for NG block layout, so that we can
  // utilize LayoutBlock::PositionForPoint() that resolves the position in block
  // layout.
  // TODO(xiaochengh): Don't fallback to legacy for NG block layout.
  const bool should_fallback = child.PhysicalFragment().IsBlockFlow() ||
                               child.PhysicalFragment().IsLegacyLayoutRoot();
  const PositionWithAffinity result =
      should_fallback ? child.GetLayoutObject()->PositionForPoint(child_point)
                      : child.PositionForPoint(child_point);
  if (result.IsNotNull())
    return result;
  return base::nullopt;
}

// ::before, ::after and ::first-letter can be hit test targets.
bool CanBeHitTestTargetPseudoNode(const Node& node) {
  auto* pseudo_element = DynamicTo<PseudoElement>(node);
  if (!pseudo_element)
    return false;
  switch (pseudo_element->GetPseudoId()) {
    case kPseudoIdBefore:
    case kPseudoIdAfter:
    case kPseudoIdFirstLetter:
      return true;
    default:
      return false;
  }
}

bool IsLastBRInPage(const LayoutObject& layout_object) {
  return layout_object.IsBR() && !layout_object.NextInPreOrder();
}

const LayoutObject* ListMarkerFromMarkerOrMarkerContent(
    const LayoutObject* object) {
  if (object->IsLayoutNGListMarkerIncludingInside())
    return object;

  // Check if this is a marker content.
  if (object->IsAnonymous()) {
    const LayoutObject* parent = object->Parent();
    if (parent && parent->IsLayoutNGListMarkerIncludingInside())
      return parent;
  }

  return nullptr;
}

}  // namespace

NGPaintFragment::NGPaintFragment(
    scoped_refptr<const NGPhysicalFragment> fragment,
    PhysicalOffset offset,
    NGPaintFragment* parent)
    : physical_fragment_(std::move(fragment)),
      offset_(offset),
      parent_(parent),
      is_dirty_inline_(false) {
  // TODO(crbug.com/924449): Once we get the caller passes null physical
  // fragment, we'll change to DCHECK().
  CHECK(physical_fragment_);
}

void NGPaintFragment::DestroyAll(scoped_refptr<NGPaintFragment> fragment) {
  DCHECK(fragment);
  while (fragment) {
    fragment = std::move(fragment->next_sibling_);
  }
}

void NGPaintFragment::RemoveChildren() {
  if (first_child_) {
    DestroyAll(std::move(first_child_));
    DCHECK(!first_child_);
  }
}

NGPaintFragment::~NGPaintFragment() {
  // The default destructor will deref |first_child_|, but because children are
  // in a linked-list, it will call this destructor recursively. Remove children
  // first non-recursively to avoid stack overflow when there are many chlidren.
  RemoveChildren();
}

void NGPaintFragment::CreateContext::SkipDestroyedPreviousInstances() {
  while (UNLIKELY(previous_instance && !previous_instance->IsAlive())) {
    previous_instance = std::move(previous_instance->next_sibling_);
    painting_layer_needs_repaint = true;
  }
}

void NGPaintFragment::CreateContext::DestroyPreviousInstances() {
  if (previous_instance) {
    DestroyAll(previous_instance);
    painting_layer_needs_repaint = true;
  }
}

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
  if (LayoutObject* layout_object = GetMutableLayoutObject())
    layout_object->SetShouldDoFullPaintInvalidation();
}

scoped_refptr<NGPaintFragment> NGPaintFragment::CreateOrReuse(
    scoped_refptr<const NGPhysicalFragment> fragment,
    PhysicalOffset offset,
    CreateContext* context) {
  DCHECK(fragment);

  // If the previous instance is given, check if it is re-usable.
  // Re-using NGPaintFragment allows the paint system to identify objects.
  context->SkipDestroyedPreviousInstances();
  if (context->previous_instance) {
    // Take the first instance of previous instances, leaving its following
    // siblings at |context->previous_instance|. There is a trade-off between
    // faster check of reusability and higher reuse ratio. The current algorithm
    // assumes that the index of children does not change. If there were
    // insertions/deletions, all following instances will not match.
    scoped_refptr<NGPaintFragment> previous_instance =
        std::move(context->previous_instance);
    context->previous_instance = std::move(previous_instance->next_sibling_);
    DCHECK_EQ(previous_instance->parent_, context->parent);
    DCHECK(!previous_instance->next_sibling_);

// TODO(kojii): This fails some tests when reusing line box was enabled.
// Investigate and re-enable.
#if 0
    // If the physical fragment was re-used, re-use the paint fragment as well.
    if (&previous_instance->PhysicalFragment() == fragment.get()) {
      previous_instance->offset_ = offset;
      previous_instance->next_for_same_layout_object_ = nullptr;
      previous_instance->is_dirty_inline_ = false;
      // No need to re-populate children because NGPhysicalFragment is
      // immutable and thus children should not have been changed.
      *populate_children = false;
      previous_instance->SetShouldDoFullPaintInvalidation();
      return previous_instance;
    }
#endif

    // If the LayoutObject are the same, the new paint fragment should have the
    // same DisplayItemClient identity as the previous instance.
    if (previous_instance->GetLayoutObject() == fragment->GetLayoutObject()) {
      previous_instance->physical_fragment_ = std::move(fragment);
      previous_instance->offset_ = offset;
      previous_instance->next_for_same_layout_object_ = nullptr;
      CHECK(previous_instance->IsAlive());
      previous_instance->is_dirty_inline_ = false;
      // Destroy children of previous instances if the new instance doesn't have
      // any children. Otherwise keep them in case these previous children maybe
      // reused to populate children.
      if (!context->populate_children && previous_instance->first_child_) {
        context->painting_layer_needs_repaint = true;
        previous_instance->RemoveChildren();
      }
      return previous_instance;
    }

    // Mark needing to repaint because exiting the scope here destroys an unused
    // previous instance.
    context->painting_layer_needs_repaint = true;
  }

  scoped_refptr<NGPaintFragment> new_instance = base::AdoptRef(
      new NGPaintFragment(std::move(fragment), offset, context->parent));
  return new_instance;
}

scoped_refptr<NGPaintFragment> NGPaintFragment::Create(
    scoped_refptr<const NGPhysicalFragment> fragment,
    const NGBlockBreakToken* block_break_token,
    scoped_refptr<NGPaintFragment> previous_instance) {
  DCHECK(fragment);

  bool has_previous_instance = previous_instance.get();
  CreateContext context(std::move(previous_instance), fragment->IsContainer());
  scoped_refptr<NGPaintFragment> paint_fragment =
      CreateOrReuse(std::move(fragment), PhysicalOffset(), &context);

  if (context.populate_children) {
    if (has_previous_instance) {
      NGInlineNode::ClearAssociatedFragments(paint_fragment->PhysicalFragment(),
                                             block_break_token);
    }
    HashMap<const LayoutObject*, NGPaintFragment*> last_fragment_map;
    context.last_fragment_map = &last_fragment_map;
    paint_fragment->PopulateDescendants(&context);
  }

  context.DestroyPreviousInstances();
  if (context.painting_layer_needs_repaint) {
    ObjectPaintInvalidator(*paint_fragment->GetLayoutObject())
        .SlowSetPaintingLayerNeedsRepaint();
  }

  return paint_fragment;
}

scoped_refptr<NGPaintFragment>* NGPaintFragment::Find(
    scoped_refptr<NGPaintFragment>* fragment,
    const NGBlockBreakToken* break_token) {
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
    auto* container =
        DynamicTo<NGPhysicalContainerFragment>((*fragment)->PhysicalFragment());
    if (container && container->BreakToken() == break_token)
      return next;
    fragment = next;
  }
  NOTREACHED();
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

bool NGPaintFragment::IsEllipsis() const {
  if (auto* text_fragment =
          DynamicTo<NGPhysicalTextFragment>(PhysicalFragment()))
    return text_fragment->IsEllipsis();
  return false;
}

bool NGPaintFragment::HasSelfPaintingLayer() const {
  return PhysicalFragment().HasSelfPaintingLayer();
}

bool NGPaintFragment::ShouldClipOverflow() const {
  auto* box_physical_fragment =
      DynamicTo<NGPhysicalBoxFragment>(&PhysicalFragment());
  return box_physical_fragment && box_physical_fragment->ShouldClipOverflow();
}

// Populate descendants from NGPhysicalFragment tree.
void NGPaintFragment::PopulateDescendants(CreateContext* parent_context) {
  const NGPhysicalFragment& fragment = PhysicalFragment();
  const auto& container = To<NGPhysicalContainerFragment>(fragment);
  CreateContext child_context(parent_context, this);
  // Children should have been moved to |child_context| for possible reuse.
  DCHECK(!first_child_);
  scoped_refptr<NGPaintFragment>* last_child_ptr = &first_child_;

  auto* box_physical_fragment = DynamicTo<NGPhysicalBoxFragment>(fragment);
  bool children_are_inline =
      !box_physical_fragment || box_physical_fragment->ChildrenInline();

  for (const NGLink& child_fragment : container.Children()) {
    child_fragment->CheckType();

    // OOF objects are not needed because they always have self painting layer.
    if (UNLIKELY(child_fragment->IsOutOfFlowPositioned()))
      continue;

    child_context.populate_children =
        child_fragment->IsContainer() &&
        !child_fragment->IsBlockFormattingContextRoot();
    scoped_refptr<NGPaintFragment> child = CreateOrReuse(
        child_fragment.get(), child_fragment.Offset(), &child_context);

    if (children_are_inline) {
      DCHECK(!child_fragment->IsOutOfFlowPositioned());
      if (child_fragment->IsText() || child_fragment->IsInlineBox() ||
          child_fragment->IsAtomicInline()) {
        child->AssociateWithLayoutObject(
            child_fragment->GetMutableLayoutObject(),
            child_context.last_fragment_map);
        child->inline_offset_to_container_box_ =
            inline_offset_to_container_box_ + child_fragment.Offset();
      } else if (child_fragment->IsLineBox()) {
        child->inline_offset_to_container_box_ =
            inline_offset_to_container_box_ + child_fragment.Offset();
      } else {
        DCHECK(child_fragment->IsFloating() || child_fragment->IsListMarker());
      }

      if (child_context.populate_children) {
        child->PopulateDescendants(&child_context);
      }
    }

    DCHECK(!*last_child_ptr);
    *last_child_ptr = std::move(child);
    last_child_ptr = &((*last_child_ptr)->next_sibling_);
  }

  // Destroy unused previous instances if any, and propagate states to the
  // parent context.
  child_context.DestroyPreviousInstances();
  parent_context->painting_layer_needs_repaint |=
      child_context.painting_layer_needs_repaint;
}

// Add to a linked list for each LayoutObject.
void NGPaintFragment::AssociateWithLayoutObject(
    LayoutObject* layout_object,
    HashMap<const LayoutObject*, NGPaintFragment*>* last_fragment_map) {
  DCHECK(layout_object);
  DCHECK(!next_for_same_layout_object_);
  DCHECK(layout_object->IsInline());
  DCHECK(PhysicalFragment().IsInline());

#if DCHECK_IS_ON()
  // Check we don't add the same fragment twice.
  for (const NGPaintFragment* fragment :
       FragmentRange(layout_object->FirstInlineFragment())) {
    DCHECK_NE(this, fragment);
  }
#endif

  auto add_result = last_fragment_map->insert(layout_object, this);
  if (add_result.is_new_entry) {
    NGPaintFragment* first_fragment = layout_object->FirstInlineFragment();
    if (!first_fragment) {
      layout_object->SetFirstInlineFragment(this);
      return;
    }
    // This |layout_object| was fragmented across multiple blocks.
    NGPaintFragment* last_fragment = first_fragment->LastForSameLayoutObject();
    last_fragment->next_for_same_layout_object_ = this;
    return;
  }
  DCHECK(add_result.stored_value->value);
  add_result.stored_value->value->next_for_same_layout_object_ = this;
  add_result.stored_value->value = this;
}

// TODO(kojii): Consider unifying this with
// NGInlineNode::ClearAssociatedFragments.
void NGPaintFragment::ClearAssociationWithLayoutObject() {
  // TODO(kojii): Support break_token for LayoutObject that spans across block
  // fragmentation boundaries.
  LayoutObject* last_object = nullptr;
  for (NGPaintFragment* child : Children()) {
    const NGPhysicalFragment& fragment = child->PhysicalFragment();
    if (fragment.IsInline()) {
      LayoutObject* object = fragment.GetMutableLayoutObject();
      if (object && object != last_object) {
        // |IsInLayoutNGInlineFormattingContext()| is cleared if its
        // NGInlineItem was invalidted.
        if (object->IsInLayoutNGInlineFormattingContext())
          object->SetFirstInlineFragment(nullptr);
        last_object = object;
      }
    }
    if (fragment.IsLineBox() || fragment.IsInlineBox() ||
        fragment.IsColumnBox()) {
      child->ClearAssociationWithLayoutObject();
    } else {
      DCHECK(fragment.IsText() || fragment.IsBlockFormattingContextRoot());
      DCHECK(child->Children().IsEmpty());
    }
  }
}

NGPaintFragment::FragmentRange NGPaintFragment::InlineFragmentsFor(
    const LayoutObject* layout_object) {
  DCHECK(layout_object);
  DCHECK(layout_object->IsInline());
  DCHECK(!layout_object->IsFloatingOrOutOfFlowPositioned());

  if (layout_object->IsInLayoutNGInlineFormattingContext())
    return FragmentRange(layout_object->FirstInlineFragment());
  return FragmentRange(nullptr, false);
}

const NGPaintFragment* NGPaintFragment::LastForSameLayoutObject() const {
  return const_cast<NGPaintFragment*>(this)->LastForSameLayoutObject();
}

NGPaintFragment* NGPaintFragment::LastForSameLayoutObject() {
  NGPaintFragment* fragment = this;
  while (fragment->next_for_same_layout_object_)
    fragment = fragment->next_for_same_layout_object_;
  return fragment;
}

void NGPaintFragment::LayoutObjectWillBeDestroyed() {
  for (NGPaintFragment* fragment = this; fragment;
       fragment = fragment->next_for_same_layout_object_) {
    fragment->PhysicalFragment().LayoutObjectWillBeDestroyed();
  }
}

const LayoutBox* NGPaintFragment::InkOverflowOwnerBox() const {
  const NGPhysicalFragment& fragment = PhysicalFragment();
  if (fragment.IsBox() && !fragment.IsInlineBox())
    return ToLayoutBox(fragment.GetLayoutObject());
  return nullptr;
}

PhysicalRect NGPaintFragment::SelfInkOverflow() const {
  // Get the cached value in |LayoutBox| if there is one.
  if (const LayoutBox* box = InkOverflowOwnerBox())
    return box->PhysicalSelfVisualOverflowRect();

  // NGPhysicalTextFragment caches ink overflow in layout.
  const NGPhysicalFragment& fragment = PhysicalFragment();
  if (const auto* text = DynamicTo<NGPhysicalTextFragment>(fragment))
    return text->SelfInkOverflow();

  if (!ink_overflow_)
    return fragment.LocalRect();
  return ink_overflow_->self_ink_overflow;
}

PhysicalRect NGPaintFragment::ContentsInkOverflow() const {
  // Get the cached value in |LayoutBox| if there is one.
  if (const LayoutBox* box = InkOverflowOwnerBox())
    return box->PhysicalContentsVisualOverflowRect();

  if (!ink_overflow_)
    return PhysicalFragment().LocalRect();
  return ink_overflow_->contents_ink_overflow;
}

PhysicalRect NGPaintFragment::InkOverflow() const {
  // Get the cached value in |LayoutBox| if there is one.
  if (const LayoutBox* box = InkOverflowOwnerBox())
    return box->PhysicalVisualOverflowRect();

  // NGPhysicalTextFragment caches ink overflow in layout.
  const NGPhysicalFragment& fragment = PhysicalFragment();
  if (const auto* text = DynamicTo<NGPhysicalTextFragment>(fragment))
    return text->SelfInkOverflow();

  if (!ink_overflow_)
    return fragment.LocalRect();

  if (HasOverflowClip())
    return ink_overflow_->self_ink_overflow;

  PhysicalRect rect = ink_overflow_->self_ink_overflow;
  rect.Unite(ink_overflow_->contents_ink_overflow);
  return rect;
}

void NGPaintFragment::RecalcInlineChildrenInkOverflow() const {
  DCHECK(GetLayoutObject()->ChildrenInline());
  RecalcContentsInkOverflow();
}

PhysicalRect NGPaintFragment::RecalcContentsInkOverflow() const {
  PhysicalRect contents_rect;
  for (NGPaintFragment* child : Children()) {
    const NGPhysicalFragment& child_fragment = child->PhysicalFragment();
    PhysicalRect child_rect;

    // A BFC root establishes a separate NGPaintFragment tree. Re-compute the
    // child tree using its LayoutObject, because it may not be NG.
    if (child_fragment.IsBlockFormattingContextRoot()) {
      LayoutBox* layout_box =
          ToLayoutBox(child_fragment.GetMutableLayoutObject());
      layout_box->RecalcVisualOverflow();
      child_rect = PhysicalRect(layout_box->VisualOverflowRect());
    } else {
      child_rect = child->RecalcInkOverflow();
    }
    if (child->HasSelfPaintingLayer())
      continue;
    if (!child_rect.IsEmpty()) {
      child_rect.offset += child->Offset();
      contents_rect.Unite(child_rect);
    }
  }
  return contents_rect;
}

PhysicalRect NGPaintFragment::RecalcInkOverflow() {
  const NGPhysicalFragment& fragment = PhysicalFragment();
  fragment.CheckCanUpdateInkOverflow();
  DCHECK(!fragment.IsBlockFormattingContextRoot());

  // NGPhysicalTextFragment caches ink overflow in layout. No need to recalc nor
  // to store in NGPaintFragment.
  if (const auto* text = DynamicTo<NGPhysicalTextFragment>(&fragment)) {
    DCHECK(!ink_overflow_);
    return text->SelfInkOverflow();
  }

  PhysicalRect self_rect;
  PhysicalRect contents_rect;
  PhysicalRect self_and_contents_rect;
  if (fragment.IsLineBox()) {
    // Line boxes don't have self overflow. Compute content overflow only.
    contents_rect = RecalcContentsInkOverflow();
    self_and_contents_rect = contents_rect;
  } else if (const auto* box_fragment =
                 DynamicTo<NGPhysicalBoxFragment>(fragment)) {
    contents_rect = RecalcContentsInkOverflow();
    self_rect = box_fragment->ComputeSelfInkOverflow();
    self_and_contents_rect = self_rect;
    self_and_contents_rect.Unite(contents_rect);
  } else {
    NOTREACHED();
  }

  DCHECK(!InkOverflowOwnerBox());
  if (fragment.LocalRect().Contains(self_and_contents_rect)) {
    ink_overflow_.reset();
  } else if (!ink_overflow_) {
    ink_overflow_ =
        std::make_unique<NGContainerInkOverflow>(self_rect, contents_rect);
  } else {
    ink_overflow_->self_ink_overflow = self_rect;
    ink_overflow_->contents_ink_overflow = contents_rect;
  }
  return self_and_contents_rect;
}

const LayoutObject& NGPaintFragment::VisualRectLayoutObject(
    bool& this_as_inline_box) const {
  const NGPhysicalFragment& fragment = PhysicalFragment();
  if (const LayoutObject* layout_object = fragment.GetLayoutObject()) {
    // For inline fragments, InlineBox uses one united rect for the LayoutObject
    // even when it is fragmented across lines. Use the same technique.
    //
    // Atomic inlines have two VisualRect; one for the LayoutBox and another as
    // InlineBox. NG creates two NGPaintFragment, one as the root of an inline
    // formatting context and another as a child of the inline formatting
    // context it participates. |Parent()| can distinguish them because a tree
    // is created for each inline formatting context.
    this_as_inline_box = Parent();
    return *layout_object;
  }

  // Line box does not have corresponding LayoutObject. Use VisualRect of the
  // containing LayoutBlockFlow as RootInlineBox does so.
  this_as_inline_box = true;
  DCHECK(fragment.IsLineBox());
  // Line box is always a direct child of its containing block.
  NGPaintFragment* containing_block_fragment = Parent();
  DCHECK(containing_block_fragment);
  DCHECK(containing_block_fragment->GetLayoutObject());
  return *containing_block_fragment->GetLayoutObject();
}

IntRect NGPaintFragment::VisualRect() const {
  // VisualRect is computed from fragment tree and set to LayoutObject in
  // pre-paint. Use the stored value in the LayoutObject.
  bool this_as_inline_box;
  const auto& layout_object = VisualRectLayoutObject(this_as_inline_box);
  return this_as_inline_box ? layout_object.VisualRectForInlineBox()
                            : layout_object.FragmentsVisualRectBoundingBox();
}

IntRect NGPaintFragment::PartialInvalidationVisualRect() const {
  bool this_as_inline_box;
  const auto& layout_object = VisualRectLayoutObject(this_as_inline_box);
  return this_as_inline_box
             ? layout_object.PartialInvalidationVisualRectForInlineBox()
             : layout_object.PartialInvalidationVisualRect();
}

base::Optional<PhysicalRect> NGPaintFragment::LocalVisualRectFor(
    const LayoutObject& layout_object) {
  auto fragments = InlineFragmentsFor(&layout_object);
  if (!fragments.IsInLayoutNGInlineFormattingContext())
    return base::nullopt;

  PhysicalRect visual_rect;
  for (NGPaintFragment* fragment : fragments) {
    if (fragment->PhysicalFragment().IsHiddenForPaint())
      continue;
    PhysicalRect child_visual_rect = fragment->SelfInkOverflow();
    child_visual_rect.offset += fragment->InlineOffsetToContainerBox();
    visual_rect.Unite(child_visual_rect);
  }
  return visual_rect;
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

const NGPaintFragment* NGPaintFragment::Root() const {
  DCHECK(PhysicalFragment().IsInline());
  const NGPaintFragment* root = this;
  for (const NGPaintFragment* fragment :
       NGPaintFragmentTraversal::InclusiveAncestorsOf(*this)) {
    root = fragment;
  }
  return root;
}

void NGPaintFragment::DirtyLinesFromChangedChild(LayoutObject* child) {
  if (!RuntimeEnabledFeatures::LayoutNGLineCacheEnabled())
    return;

  // This function should be called on every child that has
  // |IsInLayoutNGInlineFormattingContext()|, meaning it was once collected into
  // |NGInlineNode|.
  //
  // New LayoutObjects will be handled in the next |CollectInline()|.
  DCHECK(child && child->IsInLayoutNGInlineFormattingContext());

  if (child->IsInline() || child->IsFloatingOrOutOfFlowPositioned())
    MarkLineBoxesDirtyFor(*child);
}

void NGPaintFragment::MarkLineBoxesDirtyFor(const LayoutObject& layout_object) {
  DCHECK(RuntimeEnabledFeatures::LayoutNGLineCacheEnabled());
  DCHECK(layout_object.IsInline() ||
         layout_object.IsFloatingOrOutOfFlowPositioned())
      << layout_object;

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
  if (const NGPaintFragment* paint_fragment = parent.PaintFragment()) {
    if (NGPaintFragment* first_line = paint_fragment->FirstLineBox()) {
      first_line->is_dirty_inline_ = true;
      return;
    }
  }
}

void NGPaintFragment::MarkContainingLineBoxDirty() {
  DCHECK(RuntimeEnabledFeatures::LayoutNGLineCacheEnabled());
  DCHECK(PhysicalFragment().IsInline() || PhysicalFragment().IsLineBox());
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
    fragment->MarkContainingLineBoxDirty();
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
    fragment->LastForSameLayoutObject()->MarkContainingLineBoxDirty();
    return true;
  }
  return false;
}

void NGPaintFragment::SetShouldDoFullPaintInvalidationRecursively() {
  if (LayoutObject* layout_object = GetMutableLayoutObject()) {
    layout_object->StyleRef().ClearCachedPseudoElementStyles();
    layout_object->SetShouldDoFullPaintInvalidation();
  }
  for (NGPaintFragment* child : Children())
    child->SetShouldDoFullPaintInvalidationRecursively();
}

void NGPaintFragment::SetShouldDoFullPaintInvalidationForFirstLine() const {
  DCHECK(PhysicalFragment().IsBox() && GetLayoutObject() &&
         GetLayoutObject()->IsLayoutBlockFlow());

  if (NGPaintFragment* line_box = FirstLineBox()) {
    line_box->SetShouldDoFullPaintInvalidationRecursively();
    GetLayoutObject()->StyleRef().ClearCachedPseudoElementStyles();
    GetMutableLayoutObject()->SetShouldDoFullPaintInvalidation();
  }
}

// TODO(yosin): We should move |ComputeLocalSelectionRectForText()| to
// "ng_selection_painter.cc".
PhysicalRect ComputeLocalSelectionRectForText(
    const NGInlineCursor& cursor,
    const LayoutSelectionStatus& selection_status) {
  const PhysicalRect selection_rect =
      cursor.CurrentLocalRect(selection_status.start, selection_status.end);
  LogicalRect logical_rect = ComputeLogicalRectFor(selection_rect, cursor);
  // Let LocalRect for line break have a space width to paint line break
  // when it is only character in a line or only selected in a line.
  if (selection_status.start != selection_status.end && cursor.IsLineBreak() &&
      // This is for old compatible that old doesn't paint last br in a page.
      !IsLastBRInPage(*cursor.CurrentLayoutObject())) {
    DCHECK(!logical_rect.size.inline_size);
    logical_rect.size.inline_size =
        LayoutUnit(cursor.CurrentStyle().GetFont().SpaceWidth());
  }
  const LogicalRect line_break_extended_rect =
      cursor.IsLineBreak() ? logical_rect
                           : ExpandedSelectionRectForSoftLineBreakIfNeeded(
                                 logical_rect, cursor, selection_status);
  const LogicalRect line_height_expanded_rect =
      ExpandSelectionRectToLineHeight(line_break_extended_rect, cursor);
  const PhysicalRect physical_rect =
      ComputePhysicalRectFor(line_height_expanded_rect, cursor);
  return physical_rect;
}

// TODO(yosin): We should move |ComputeLocalSelectionRectForReplaced()| to
// "ng_selection_painter.cc".
PhysicalRect ComputeLocalSelectionRectForReplaced(
    const NGInlineCursor& cursor) {
  DCHECK(cursor.CurrentLayoutObject()->IsLayoutReplaced());
  const PhysicalRect selection_rect = PhysicalRect({}, cursor.CurrentSize());
  LogicalRect logical_rect = ComputeLogicalRectFor(selection_rect, cursor);
  const LogicalRect line_height_expanded_rect =
      ExpandSelectionRectToLineHeight(logical_rect, cursor);
  const PhysicalRect physical_rect =
      ComputePhysicalRectFor(line_height_expanded_rect, cursor);
  return physical_rect;
}

PositionWithAffinity NGPaintFragment::PositionForPointInText(
    const PhysicalOffset& point) const {
  const auto& text_fragment = To<NGPhysicalTextFragment>(PhysicalFragment());
  if (text_fragment.IsGeneratedText())
    return PositionWithAffinity();
  const unsigned text_offset = text_fragment.TextOffsetForPoint(point);
  NGInlineCursor cursor;
  cursor.MoveTo(*this);
  const NGCaretPosition unadjusted_position{
      cursor, NGCaretPositionType::kAtTextOffset, text_offset};
  if (RuntimeEnabledFeatures::BidiCaretAffinityEnabled())
    return unadjusted_position.ToPositionInDOMTreeWithAffinity();
  if (text_offset > text_fragment.StartOffset() &&
      text_offset < text_fragment.EndOffset()) {
    return unadjusted_position.ToPositionInDOMTreeWithAffinity();
  }
  return BidiAdjustment::AdjustForHitTest(unadjusted_position)
      .ToPositionInDOMTreeWithAffinity();
}

PositionWithAffinity NGPaintFragment::PositionForPointInInlineLevelBox(
    const PhysicalOffset& point) const {
  DCHECK(PhysicalFragment().IsInline() || PhysicalFragment().IsLineBox());
  DCHECK(!PhysicalFragment().IsBlockFlow());

  const LogicalOffset logical_point = point.ConvertToLogical(
      Style().GetWritingMode(), Style().Direction(), Size(),
      // |point| is actually a pixel with size 1x1.
      PhysicalSize(LayoutUnit(1), LayoutUnit(1)));
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
    if (child->PhysicalFragment().IsFloating())
      continue;

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
    const PhysicalOffset& point) const {
  DCHECK(PhysicalFragment().IsBlockFlow());
  DCHECK(PhysicalFragment().IsBox());
  DCHECK(To<NGPhysicalBoxFragment>(PhysicalFragment()).ChildrenInline());

  const LogicalOffset logical_point = point.ConvertToLogical(
      Style().GetWritingMode(), Style().Direction(), Size(),
      // |point| is actually a pixel with size 1x1.
      PhysicalSize(LayoutUnit(1), LayoutUnit(1)));
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
    const PhysicalOffset& point) const {
  if (PhysicalFragment().IsText())
    return PositionForPointInText(point);

  if (PhysicalFragment().IsBlockFlow()) {
    // We current fall back to legacy for block formatting contexts, so we
    // should reach here only for inline formatting contexts.
    // TODO(xiaochengh): Do not fall back.
    DCHECK(To<NGPhysicalBoxFragment>(PhysicalFragment()).ChildrenInline());
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
  if (const LayoutObject* object = GetLayoutObject()) {
    if (const LayoutObject* marker =
            ListMarkerFromMarkerOrMarkerContent(object)) {
      if (const LayoutNGListItem* list_item =
              LayoutNGListItem::FromMarker(*marker))
        return list_item->GetNode();
      return nullptr;
    }
  }

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

String NGPaintFragment::DebugName() const {
  StringBuilder name;

  DCHECK(physical_fragment_);
  const NGPhysicalFragment& physical_fragment = *physical_fragment_;
  if (physical_fragment.IsBox()) {
    name.Append("NGPhysicalBoxFragment");
    if (const LayoutObject* layout_object =
            physical_fragment.GetLayoutObject()) {
      DCHECK(physical_fragment.IsBox());
      name.Append(' ');
      name.Append(layout_object->DebugName());
    }
  } else if (physical_fragment.IsText()) {
    name.Append("NGPhysicalTextFragment '");
    name.Append(To<NGPhysicalTextFragment>(physical_fragment).Text());
    name.Append('\'');
  } else if (physical_fragment.IsLineBox()) {
    name.Append("NGPhysicalLineBoxFragment");
  } else {
    NOTREACHED();
  }

  return name.ToString();
}

template class CORE_TEMPLATE_EXPORT
    NGPaintFragment::List<NGPaintFragment::TraverseNextForSameLayoutObject>;
template class CORE_TEMPLATE_EXPORT
    NGPaintFragment::List<NGPaintFragment::TraverseNextSibling>;

}  // namespace blink
