// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"

#include "third_party/blink/renderer/core/dom/pseudo_element.h"
#include "third_party/blink/renderer/core/editing/bidi_adjustment.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
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
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

namespace {

struct SameSizeAsNGPaintFragment : public RefCounted<NGPaintFragment>,
                                   public DisplayItemClient {
  void* pointers[7];
  PhysicalOffset offsets[2];
  unsigned flags;
};

ASSERT_SIZE(NGPaintFragment, SameSizeAsNGPaintFragment);

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
  const LayoutUnit space_width(cursor.Current().Style().GetFont().SpaceWidth());
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
      line.Current().OffsetInContainerBlock() -
          cursor.Current().OffsetInContainerBlock(),
      line.Current().Size());
  return ExpandSelectionRectToLineHeight(
      rect, cursor.Current().ConvertChildToLogical(line_physical_rect));
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

bool IsLastBRInPage(const LayoutObject& layout_object) {
  return layout_object.IsBR() && !layout_object.NextInPreOrder();
}

}  // namespace

NGPaintFragment::NGPaintFragment(
    scoped_refptr<const NGPhysicalFragment> fragment,
    PhysicalOffset offset,
    NGPaintFragment* parent)
    : physical_fragment_(std::move(fragment)),
      offset_(offset),
      parent_(parent),
      is_layout_object_destroyed_(false),
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

bool NGPaintFragment::ShouldClipOverflowAlongBothAxis() const {
  auto* box_physical_fragment =
      DynamicTo<NGPhysicalBoxFragment>(&PhysicalFragment());
  return box_physical_fragment &&
         box_physical_fragment->ShouldClipOverflowAlongBothAxis();
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
  bool is_inline_fc = !box_physical_fragment ||
                      box_physical_fragment->IsInlineFormattingContext();

  for (const NGLink& child_fragment : container.Children()) {
    child_fragment->CheckType();

    // OOF objects are not needed because they always have self painting layer.
    if (UNLIKELY(child_fragment->IsOutOfFlowPositioned()))
      continue;

    child_context.populate_children =
        child_fragment->IsContainer() &&
        !child_fragment->IsFormattingContextRoot();
    scoped_refptr<NGPaintFragment> child = CreateOrReuse(
        child_fragment.get(), child_fragment.Offset(), &child_context);

    if (is_inline_fc) {
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

  auto add_result = last_fragment_map->insert(layout_object, this);
  NGPaintFragment* last_fragment;
  if (add_result.is_new_entry) {
    NGPaintFragment* first_fragment = layout_object->FirstInlineFragment();
    if (!first_fragment) {
      layout_object->SetFirstInlineFragment(this);
      return;
    }
    // This |layout_object| was fragmented across multiple blocks.
    last_fragment = first_fragment->LastForSameLayoutObject();
  } else {
    last_fragment = add_result.stored_value->value;
    DCHECK(last_fragment) << layout_object;
    add_result.stored_value->value = this;
  }
  DCHECK_EQ(layout_object, last_fragment->GetLayoutObject());
  DCHECK_NE(this, last_fragment);
  last_fragment->next_for_same_layout_object_ = this;
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
        fragment.IsFragmentainerBox()) {
      child->ClearAssociationWithLayoutObject();
    } else {
      DCHECK(fragment.IsText() || fragment.IsFormattingContextRoot());
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
    fragment->is_layout_object_destroyed_ = true;
    // TODO(crbug.com/1033203): We should call this, but this seems to crash.
    // fragment->PhysicalFragment().LayoutObjectWillBeDestroyed();
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
  return ink_overflow_->ink_overflow;
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

  if (HasNonVisibleOverflow())
    return ink_overflow_->ink_overflow;

  PhysicalRect rect = ink_overflow_->ink_overflow;
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
    if (child_fragment.IsFormattingContextRoot()) {
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
  DCHECK(!fragment.IsFormattingContextRoot());

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
    ink_overflow_->ink_overflow = self_rect;
    ink_overflow_->contents_ink_overflow = contents_rect;
  }
  return self_and_contents_rect;
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
    child_visual_rect.offset += fragment->OffsetInContainerBlock();
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
  // Because of this function can be called during |LayoutObject::Destroy()|,
  // we use |physical_fragment_| to avoid calling |IsAlive()|.
  DCHECK(physical_fragment_->IsInline());
  const NGPaintFragment* root = this;
  for (const NGPaintFragment* fragment :
       NGPaintFragmentTraversal::InclusiveAncestorsOf(*this)) {
    root = fragment;
  }
  return root;
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
  LogicalRect logical_rect =
      cursor.Current().ConvertChildToLogical(selection_rect);
  // Let LocalRect for line break have a space width to paint line break
  // when it is only character in a line or only selected in a line.
  if (selection_status.start != selection_status.end &&
      cursor.Current().IsLineBreak() &&
      // This is for old compatible that old doesn't paint last br in a page.
      !IsLastBRInPage(*cursor.Current().GetLayoutObject())) {
    DCHECK(!logical_rect.size.inline_size);
    logical_rect.size.inline_size =
        LayoutUnit(cursor.Current().Style().GetFont().SpaceWidth());
  }
  const LogicalRect line_break_extended_rect =
      cursor.Current().IsLineBreak()
          ? logical_rect
          : ExpandedSelectionRectForSoftLineBreakIfNeeded(logical_rect, cursor,
                                                          selection_status);
  const LogicalRect line_height_expanded_rect =
      ExpandSelectionRectToLineHeight(line_break_extended_rect, cursor);
  const PhysicalRect physical_rect =
      cursor.Current().ConvertChildToPhysical(line_height_expanded_rect);
  return physical_rect;
}

// TODO(yosin): We should move |ComputeLocalSelectionRectForReplaced()| to
// "ng_selection_painter.cc".
PhysicalRect ComputeLocalSelectionRectForReplaced(
    const NGInlineCursor& cursor) {
  DCHECK(cursor.Current().GetLayoutObject()->IsLayoutReplaced());
  const PhysicalRect selection_rect = PhysicalRect({}, cursor.Current().Size());
  LogicalRect logical_rect =
      cursor.Current().ConvertChildToLogical(selection_rect);
  const LogicalRect line_height_expanded_rect =
      ExpandSelectionRectToLineHeight(logical_rect, cursor);
  const PhysicalRect physical_rect =
      cursor.Current().ConvertChildToPhysical(line_height_expanded_rect);
  return physical_rect;
}

PositionWithAffinity NGPaintFragment::PositionForPointInText(
    const PhysicalOffset& point) const {
  const auto& text_fragment = To<NGPhysicalTextFragment>(PhysicalFragment());
  if (text_fragment.IsGeneratedText())
    return PositionWithAffinity();
  return PositionForPointInText(text_fragment.TextOffsetForPoint(point));
}

PositionWithAffinity NGPaintFragment::PositionForPointInText(
    unsigned text_offset) const {
  const auto& text_fragment = To<NGPhysicalTextFragment>(PhysicalFragment());
  DCHECK(!text_fragment.IsGeneratedText());
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

  const LogicalOffset logical_point =
      point.ConvertToLogical(Style().GetWritingDirection(), Size(),
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

    const LogicalRect logical_child_rect =
        PhysicalFragment().ConvertChildToLogical(child->Rect());
    const LayoutUnit child_inline_min = logical_child_rect.offset.inline_offset;
    const LayoutUnit child_inline_max =
        child_inline_min + logical_child_rect.size.inline_size;

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
  DCHECK(To<NGPhysicalBoxFragment>(PhysicalFragment())
             .GetLayoutObject()
             ->ChildrenInline());

  const LogicalOffset logical_point =
      point.ConvertToLogical(Style().GetWritingDirection(), Size(),
                             // |point| is actually a pixel with size 1x1.
                             PhysicalSize(LayoutUnit(1), LayoutUnit(1)));
  const LayoutUnit block_point = logical_point.block_offset;

  // Stores the closest line box child below |point| in the block direction.
  // Used if we can't find any child |point| falls in to resolve the position.
  const NGPaintFragment* closest_line_below = nullptr;
  LayoutUnit closest_line_below_block_offset = LayoutUnit::Min();

  // Stores the closest line box child above |point| in the block direction.
  // Used if we can't find any child |point| falls in to resolve the position.
  const NGPaintFragment* closest_line_above = nullptr;
  LayoutUnit closest_line_above_block_offset = LayoutUnit::Max();

  for (const NGPaintFragment* child : Children()) {
    if (!child->PhysicalFragment().IsLineBox())
      continue;
    if (!NGInlineCursor(*child).TryToMoveToFirstInlineLeafChild()) {
      // editing/selection/last-empty-inline.html requires this to skip
      // empty <span> with padding.
      continue;
    }

    const LogicalRect logical_child_rect =
        PhysicalFragment().ConvertChildToLogical(child->Rect());
    const LayoutUnit line_min = logical_child_rect.offset.block_offset;
    const LayoutUnit line_max = line_min + logical_child_rect.size.block_size;

    // Try to resolve if |point| falls in a line box in block direction.
    // Hitting on line bottom doesn't count, to match legacy behavior.
    // TODO(xiaochengh): Consider floats.
    if (block_point >= line_min && block_point < line_max) {
      if (auto child_position = PositionForPointInChild(*child, point))
        return child_position.value();
      continue;
    }

    if (block_point < line_min) {
      if (line_min < closest_line_above_block_offset) {
        closest_line_above = child;
        closest_line_above_block_offset = line_min;
      }
    }

    if (block_point >= line_max) {
      if (line_max > closest_line_below_block_offset) {
        closest_line_below = child;
        closest_line_below_block_offset = line_max;
      }
    }
  }

  // Note: |move_caret_to_boundary| is true for Mac and Unix.
  const bool move_caret_to_boundary =
      To<LayoutBlockFlow>(GetLayoutObject())
          ->ShouldMoveCaretToHorizontalBoundaryWhenPastTopOrBottom();

  // At here, |point| is not inside any line in |this|:
  //   |closest_line_above|
  //   |point|
  //   |closest_line_below|
  if (closest_line_above) {
    if (move_caret_to_boundary) {
      // Tests[1-3] reach here.
      // [1] editing/selection/click-in-margins-inside-editable-div.html
      // [2] fast/writing-mode/flipped-blocks-hit-test-line-edges.html
      // [3] All/LayoutViewHitTestTest.HitTestHorizontal/4
      NGInlineCursor line_box(*this);
      line_box.MoveTo(*closest_line_above);
      if (auto first_position = line_box.PositionForStartOfLine())
        return PositionWithAffinity(first_position.GetPosition());
    } else if (auto child_position =
                   PositionForPointInChild(*closest_line_above, point)) {
      return child_position.value();
    }
  }

  if (closest_line_below) {
    if (move_caret_to_boundary) {
      // Tests[1-3] reach here.
      // [1] editing/selection/click-in-margins-inside-editable-div.html
      // [2] fast/writing-mode/flipped-blocks-hit-test-line-edges.html
      // [3] All/LayoutViewHitTestTest.HitTestHorizontal/4
      NGInlineCursor line_box(*this);
      line_box.MoveTo(*closest_line_below);
      if (auto last_position = line_box.PositionForEndOfLine())
        return PositionWithAffinity(last_position.GetPosition());
    } else if (auto child_position =
                   PositionForPointInChild(*closest_line_below, point)) {
      // Test[1] reaches here.
      // [1] editing/selection/last-empty-inline.html
      return child_position.value();
    }
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
    const LayoutObject& layout_object = *PhysicalFragment().GetLayoutObject();
    if (layout_object.ChildrenInline())
      return PositionForPointInInlineFormattingContext(point);
    // |NGInlineCursor::PositionForPointInChild()| calls this function with
    // inline block with with block formatting context that has block
    // children[1], e.g: <b style="display:inline-block"><div>b</div></b>
    // [1] NGInlineCursorTest.PositionForPointInChildBlockChildren
    return layout_object.PositionForPoint(point);
  }

  DCHECK(PhysicalFragment().IsInline() || PhysicalFragment().IsLineBox());
  return PositionForPointInInlineLevelBox(point);
}

String NGPaintFragment::DebugName() const {
  StringBuilder name;

  DCHECK(physical_fragment_);
  const NGPhysicalFragment& physical_fragment = *physical_fragment_;
  if (physical_fragment.IsBox()) {
    const LayoutObject* layout_object = physical_fragment.GetLayoutObject();
    if (!layout_object)
      return "NGPhysicalBoxFragment";
    // For the root |NGPaintFragment|, return the name of the |LayoutObject| to
    // ease the transition to |NGFragmentItem|.
    if (!Parent())
      return layout_object->DebugName();
    name.Append("NGPhysicalBoxFragment");
    name.Append(' ');
    name.Append(layout_object->DebugName());
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
