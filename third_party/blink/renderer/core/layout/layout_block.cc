/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc.
 *               All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/layout_block.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/editing/drag_caret.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_marquee_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/box_layout_extra_input.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/flex/layout_ng_flexible_box.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/ng/mathml/layout_ng_mathml_block.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_text.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/block_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/ng/ng_box_fragment_painter.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

struct SameSizeAsLayoutBlock : public LayoutBox {
  LayoutObjectChildList children;
  uint32_t bitfields;
};

ASSERT_SIZE(LayoutBlock, SameSizeAsLayoutBlock);

// This map keeps track of the positioned objects associated with a containing
// block.
//
// This map is populated during layout. It is kept across layouts to handle
// that we skip unchanged sub-trees during layout, in such a way that we are
// able to lay out deeply nested out-of-flow descendants if their containing
// block got laid out. The map could be invalidated during style change but
// keeping track of containing blocks at that time is complicated (we are in
// the middle of recomputing the style so we can't rely on any of its
// information), which is why it's easier to just update it for every layout.
TrackedDescendantsMap& GetPositionedDescendantsMap() {
  DEFINE_STATIC_LOCAL(Persistent<TrackedDescendantsMap>, map,
                      (MakeGarbageCollected<TrackedDescendantsMap>()));
  return *map;
}

TrackedContainerMap& GetPositionedContainerMap() {
  DEFINE_STATIC_LOCAL(Persistent<TrackedContainerMap>, map,
                      (MakeGarbageCollected<TrackedContainerMap>()));
  return *map;
}

LayoutBlock::LayoutBlock(ContainerNode* node)
    : LayoutBox(node),
      descendants_with_floats_marked_for_layout_(false),
      has_positioned_objects_(false),
      has_svg_text_descendants_(false) {
  // LayoutBlockFlow calls setChildrenInline(true).
  // By default, subclasses do not have inline children.
}

void LayoutBlock::Trace(Visitor* visitor) const {
  visitor->Trace(children_);
  LayoutBox::Trace(visitor);
}

void LayoutBlock::RemoveFromGlobalMaps() {
  NOT_DESTROYED();
  if (HasPositionedObjects()) {
    TrackedLayoutBoxLinkedHashSet* descendants =
        GetPositionedDescendantsMap().Take(this);
    DCHECK(!descendants->empty());
    for (LayoutBox* descendant : *descendants) {
      DCHECK_EQ(GetPositionedContainerMap().at(descendant), this);
      GetPositionedContainerMap().erase(descendant);
    }
  }
  if (has_svg_text_descendants_) {
    View()->SvgTextDescendantsMap().erase(this);
    has_svg_text_descendants_ = false;
  }
}

void LayoutBlock::WillBeDestroyed() {
  NOT_DESTROYED();
  if (!DocumentBeingDestroyed() && Parent())
    Parent()->DirtyLinesFromChangedChild(this);

  if (LocalFrame* frame = GetFrame()) {
    frame->Selection().LayoutBlockWillBeDestroyed(*this);
    frame->GetPage()->GetDragCaret().LayoutBlockWillBeDestroyed(*this);
  }

  if (TextAutosizer* text_autosizer = GetDocument().GetTextAutosizer())
    text_autosizer->Destroy(this);

  RemoveFromGlobalMaps();

  LayoutBox::WillBeDestroyed();
}

void LayoutBlock::StyleWillChange(StyleDifference diff,
                                  const ComputedStyle& new_style) {
  NOT_DESTROYED();
  SetIsAtomicInlineLevel(new_style.IsDisplayInlineType());
  LayoutBox::StyleWillChange(diff, new_style);
}

// Compute a local version of the "font size scale factor" used by SVG
// <text>. Squared to avoid computing the square root. See
// SVGLayoutSupport::CalculateScreenFontSizeScalingFactor().
static double ComputeSquaredLocalFontSizeScalingFactor(
    const gfx::Transform* transform) {
  if (!transform)
    return 1;
  const auto affine = AffineTransform::FromTransform(*transform);
  return affine.XScaleSquared() + affine.YScaleSquared();
}

void LayoutBlock::StyleDidChange(StyleDifference diff,
                                 const ComputedStyle* old_style) {
  NOT_DESTROYED();
  // Computes old scaling factor before PaintLayer::UpdateTransform()
  // updates Layer()->Transform().
  double old_squared_scale = 1;
  if (Layer() && diff.TransformChanged() && has_svg_text_descendants_) {
    old_squared_scale =
        ComputeSquaredLocalFontSizeScalingFactor(Layer()->Transform());
  }

  LayoutBox::StyleDidChange(diff, old_style);

  const ComputedStyle& new_style = StyleRef();

  if (old_style && Parent()) {
    if (old_style->GetPosition() != new_style.GetPosition() &&
        new_style.GetPosition() != EPosition::kStatic) {
      // In LayoutObject::styleWillChange() we already removed ourself from our
      // old containing block's positioned descendant list, and we will be
      // inserted to the new containing block's list during layout. However the
      // positioned descendant layout logic assumes layout objects to obey
      // parent-child order in the list. Remove our descendants here so they
      // will be re-inserted after us.
      if (LayoutBlock* cb = ContainingBlock()) {
        cb->RemovePositionedObjects(this, kNewContainingBlock);
        if (IsOutOfFlowPositioned() && !cb->IsLayoutNGObject()) {
          // Insert this object into containing block's positioned descendants
          // list in case the parent won't layout. This is needed especially
          // there are descendants scheduled for overflow recalc.
          //
          // Only do this if the containing block is a legacy object, to let
          // LayoutNG decide when to insert positioned objects. In particular,
          // we don't want that if the OOF participates in block fragmentation,
          // since an OOF will then be laid out as a child of a fragmentainer,
          // rather than its actual containing block.
          cb->InsertPositionedObject(this);
        }
      }
    }
  }

  if (TextAutosizer* text_autosizer = GetDocument().GetTextAutosizer())
    text_autosizer->Record(this);

  PropagateStyleToAnonymousChildren();

  if (diff.TransformChanged() && has_svg_text_descendants_) {
    const double new_squared_scale = ComputeSquaredLocalFontSizeScalingFactor(
        Layer() ? Layer()->Transform() : nullptr);
    // Compare local scale before and after.
    if (old_squared_scale != new_squared_scale) {
      bool stacking_context_changed =
          old_style &&
          (IsStackingContext(*old_style) != IsStackingContext(new_style));
      for (LayoutBox* box : *View()->SvgTextDescendantsMap().at(this)) {
        To<LayoutNGSVGText>(box)->SetNeedsTextMetricsUpdate();
        if (GetNode() == GetDocument().documentElement() ||
            stacking_context_changed) {
          box->SetNeedsLayout(layout_invalidation_reason::kStyleChange);
        }
      }
    }
  }
}

bool LayoutBlock::RespectsCSSOverflow() const {
  NOT_DESTROYED();
  // If overflow has been propagated to the viewport, it has no effect here.
  return GetNode() != GetDocument().ViewportDefiningElement();
}

void LayoutBlock::AddChildBeforeDescendant(LayoutObject* new_child,
                                           LayoutObject* before_descendant) {
  NOT_DESTROYED();
  DCHECK_NE(before_descendant->Parent(), this);
  LayoutObject* before_descendant_container = before_descendant->Parent();
  while (before_descendant_container->Parent() != this)
    before_descendant_container = before_descendant_container->Parent();
  DCHECK(before_descendant_container);

  // We really can't go on if what we have found isn't anonymous. We're not
  // supposed to use some random non-anonymous object and put the child there.
  // That's a recipe for security issues.
  CHECK(before_descendant_container->IsAnonymous());

  // If the requested insertion point is not one of our children, then this is
  // because there is an anonymous container within this object that contains
  // the beforeDescendant.
  if (before_descendant_container->IsAnonymousBlock()) {
    // Insert the child into the anonymous block box instead of here.
    if (new_child->IsInline() ||
        (new_child->IsFloatingOrOutOfFlowPositioned() &&
         (StyleRef().IsDeprecatedFlexboxUsingFlexLayout() ||
          (!IsFlexibleBoxIncludingNG() && !IsLayoutNGGrid()))) ||
        before_descendant->Parent()->SlowFirstChild() != before_descendant) {
      before_descendant_container->AddChild(new_child, before_descendant);
    } else {
      AddChild(new_child, before_descendant->Parent());
    }
    return;
  }

  DCHECK(before_descendant_container->IsTable());
  if (new_child->IsTablePart()) {
    // Insert into the anonymous table.
    before_descendant_container->AddChild(new_child, before_descendant);
    return;
  }

  LayoutObject* before_child =
      SplitAnonymousBoxesAroundChild(before_descendant);

  DCHECK_EQ(before_child->Parent(), this);
  if (before_child->Parent() != this) {
    // We should never reach here. If we do, we need to use the
    // safe fallback to use the topmost beforeChild container.
    before_child = before_descendant_container;
  }

  AddChild(new_child, before_child);
}

void LayoutBlock::AddChild(LayoutObject* new_child,
                           LayoutObject* before_child) {
  NOT_DESTROYED();
  if (before_child && before_child->Parent() != this) {
    AddChildBeforeDescendant(new_child, before_child);
    return;
  }

  // Only LayoutBlockFlow should have inline children, and then we shouldn't be
  // here.
  DCHECK(!ChildrenInline());

  if (new_child->IsInline() ||
      (new_child->IsFloatingOrOutOfFlowPositioned() &&
       (StyleRef().IsDeprecatedFlexboxUsingFlexLayout() ||
        (!IsFlexibleBoxIncludingNG() && !IsLayoutNGGrid())))) {
    // If we're inserting an inline child but all of our children are blocks,
    // then we have to make sure it is put into an anomyous block box. We try to
    // use an existing anonymous box if possible, otherwise a new one is created
    // and inserted into our list of children in the appropriate position.
    LayoutObject* after_child =
        before_child ? before_child->PreviousSibling() : LastChild();

    if (after_child && after_child->IsAnonymousBlock()) {
      after_child->AddChild(new_child);
      return;
    }

    if (new_child->IsInline()) {
      // No suitable existing anonymous box - create a new one.
      LayoutBlock* new_box = CreateAnonymousBlock();
      LayoutBox::AddChild(new_box, before_child);
      new_box->AddChild(new_child);
      return;
    }
  }

  LayoutBox::AddChild(new_child, before_child);
}

void LayoutBlock::RemoveLeftoverAnonymousBlock(LayoutBlock* child) {
  NOT_DESTROYED();
  DCHECK(child->IsAnonymousBlock());
  DCHECK(!child->ChildrenInline());
  DCHECK_EQ(child->Parent(), this);

  // Promote all the leftover anonymous block's children (to become children of
  // this block instead). We still want to keep the leftover block in the tree
  // for a moment, for notification purposes done further below (flow threads
  // and grids).
  child->MoveAllChildrenTo(this, child->NextSibling());

  // Remove all the information in the flow thread associated with the leftover
  // anonymous block.
  child->RemoveFromLayoutFlowThread();

  // Now remove the leftover anonymous block from the tree, and destroy it.
  // We'll rip it out manually from the tree before destroying it, because we
  // don't want to trigger any tree adjustments with regards to anonymous blocks
  // (or any other kind of undesired chain-reaction).
  Children()->RemoveChildNode(this, child, false);
  child->Destroy();
}

void LayoutBlock::UpdateLayout() {
  NOT_DESTROYED();
  DCHECK(!GetScrollableArea() || GetScrollableArea()->GetScrollAnchor());

  bool needs_scroll_anchoring =
      IsScrollContainer() &&
      GetScrollableArea()->ShouldPerformScrollAnchoring();
  if (needs_scroll_anchoring)
    GetScrollableArea()->GetScrollAnchor()->NotifyBeforeLayout();

  // Table cells call UpdateBlockLayout directly, as does
  // PaintLayerScrollableArea for nested scrollbar layouts. Most logic should be
  // in UpdateBlockLayout instead of UpdateLayout.
  UpdateBlockLayout(false);

  // It's safe to check for control clip here, since controls can never be table
  // cells. If we have a lightweight clip, there can never be any overflow from
  // children.
  if (HasControlClip() && HasLayoutOverflow())
    ClearLayoutOverflow();
}

void LayoutBlock::UpdateBlockLayout(bool) {
  NOT_DESTROYED();
  ClearNeedsLayout();
  NOTREACHED_NORETURN();
}

void LayoutBlock::AddVisualOverflowFromChildren() {
  NOT_DESTROYED();
  // It is an error to call this function on a LayoutBlock that it itself inside
  // a display-locked subtree.
  DCHECK(!DisplayLockUtilities::LockedAncestorPreventingPrePaint(*this));
  if (ChildPrePaintBlockedByDisplayLock())
    return;

  DCHECK(!NeedsLayout());

  if (ChildrenInline())
    To<LayoutBlockFlow>(this)->AddVisualOverflowFromInlineChildren();
  else
    AddVisualOverflowFromBlockChildren();
}

void LayoutBlock::AddLayoutOverflowFromChildren() {
  NOT_DESTROYED();
  if (ChildLayoutBlockedByDisplayLock())
    return;

  DCHECK(!ChildrenInline());
  AddLayoutOverflowFromBlockChildren();
}

void LayoutBlock::ComputeVisualOverflow(bool) {
  NOT_DESTROYED();
  DCHECK(!SelfNeedsLayout());

  LayoutRect previous_visual_overflow_rect = VisualOverflowRect();
  ClearVisualOverflow();
  AddVisualOverflowFromChildren();
  AddVisualEffectOverflow();

  if (VisualOverflowRect() != previous_visual_overflow_rect) {
    InvalidateIntersectionObserverCachedRects();
    SetShouldCheckForPaintInvalidation();
    GetFrameView()->SetIntersectionObservationState(LocalFrameView::kDesired);
  }
}

DISABLE_CFI_PERF
void LayoutBlock::ComputeLayoutOverflow(LayoutUnit old_client_after_edge,
                                        bool) {
  NOT_DESTROYED();
  ClearSelfNeedsLayoutOverflowRecalc();
  ClearLayoutOverflow();
  AddLayoutOverflowFromChildren();
  AddLayoutOverflowFromPositionedObjects();

  if (IsScrollContainer()) {
    // When we have overflow clip, propagate the original spillout since it will
    // include collapsed bottom margins and bottom padding. Set the axis we
    // don't care about to be 1, since we want this overflow to always be
    // considered reachable.
    LayoutRect client_rect(NoOverflowRect());
    LayoutRect rect_to_apply;
    if (IsHorizontalWritingMode())
      rect_to_apply = LayoutRect(
          client_rect.X(), client_rect.Y(), LayoutUnit(1),
          (old_client_after_edge - client_rect.Y()).ClampNegativeToZero());
    else
      rect_to_apply = LayoutRect(
          client_rect.X(), client_rect.Y(),
          (old_client_after_edge - client_rect.X()).ClampNegativeToZero(),
          LayoutUnit(1));
    AddLayoutOverflow(rect_to_apply);
    SetLayoutClientAfterEdge(old_client_after_edge);

    if (PaddingEnd() && !ChildrenInline()) {
      EOverflow overflow = StyleRef().OverflowInlineDirection();
      if (overflow == EOverflow::kAuto) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kInlineOverflowAutoWithInlineEndPadding);
      } else if (overflow == EOverflow::kScroll) {
        UseCounter::Count(
            GetDocument(),
            WebFeature::kInlineOverflowScrollWithInlineEndPadding);
      }
    }
  }
}

void LayoutBlock::AddVisualOverflowFromBlockChildren() {
  NOT_DESTROYED();
  for (LayoutBox* child = FirstChildBox(); child;
       child = child->NextSiblingBox()) {
    if ((!IsLayoutNGContainingBlock(this) && child->IsFloating()) ||
        child->IsOutOfFlowPositioned() || child->IsColumnSpanAll())
      continue;

    AddVisualOverflowFromChild(*child);
  }
}

void LayoutBlock::AddLayoutOverflowFromBlockChildren() {
  NOT_DESTROYED();
  for (LayoutBox* child = FirstChildBox(); child;
       child = child->NextSiblingBox()) {
    if ((!IsLayoutNGContainingBlock(this) && child->IsFloating()) ||
        child->IsOutOfFlowPositioned() || child->IsColumnSpanAll())
      continue;

    AddLayoutOverflowFromChild(*child);
  }
}

void LayoutBlock::AddLayoutOverflowFromPositionedObjects() {
  NOT_DESTROYED();
  if (ChildLayoutBlockedByDisplayLock())
    return;

  TrackedLayoutBoxLinkedHashSet* positioned_descendants = PositionedObjects();
  if (!positioned_descendants)
    return;

  for (const auto& positioned_object : *positioned_descendants) {
    // Fixed positioned elements whose containing block is the LayoutView
    // don't contribute to layout overflow, since they don't scroll with the
    // content.
    if (!IsA<LayoutView>(this) ||
        positioned_object->StyleRef().GetPosition() != EPosition::kFixed) {
      AddLayoutOverflowFromChild(*positioned_object,
                                 ToLayoutSize(positioned_object->Location()));
    }
  }
}

LayoutUnit LayoutBlock::MarginIntrinsicLogicalWidthForChild(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  // A margin has three types: fixed, percentage, and auto (variable).
  // Auto and percentage margins become 0 when computing min/max width.
  // Fixed margins can be added in as is.
  const Length& margin_left = child.StyleRef().MarginStartUsing(StyleRef());
  const Length& margin_right = child.StyleRef().MarginEndUsing(StyleRef());
  LayoutUnit margin;
  if (margin_left.IsFixed())
    margin += margin_left.Value();
  if (margin_right.IsFixed())
    margin += margin_right.Value();
  return margin;
}

void LayoutBlock::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  NOTREACHED_NORETURN();
}

void LayoutBlock::PaintChildren(const PaintInfo& paint_info,
                                const PhysicalOffset&) const {
  NOT_DESTROYED();
  NOTREACHED_NORETURN();
}

void LayoutBlock::PaintObject(const PaintInfo& paint_info,
                              const PhysicalOffset& paint_offset) const {
  NOT_DESTROYED();
  NOTREACHED_NORETURN();
}

TrackedLayoutBoxLinkedHashSet* LayoutBlock::PositionedObjectsInternal() const {
  NOT_DESTROYED();
  auto it = GetPositionedDescendantsMap().find(this);
  return it != GetPositionedDescendantsMap().end() ? &*it->value : nullptr;
}

void LayoutBlock::InsertPositionedObject(LayoutBox* o) {
  NOT_DESTROYED();
  DCHECK(!IsAnonymousBlock() || IsAnonymousNGMulticolInlineWrapper());
  DCHECK_EQ(o->ContainingBlock(), this);

  o->ClearOverrideContainingBlockContentSize();

  auto container_map_it = GetPositionedContainerMap().find(o);
  if (container_map_it != GetPositionedContainerMap().end()) {
    if (container_map_it->value == this) {
      DCHECK(HasPositionedObjects());
      DCHECK(PositionedObjects()->Contains(o));
      PositionedObjects()->AppendOrMoveToLast(o);
      return;
    }
    RemovePositionedObject(o);
  }
  GetPositionedContainerMap().Set(o, this);

  auto it = GetPositionedDescendantsMap().find(this);
  TrackedLayoutBoxLinkedHashSet* descendant_set =
      it != GetPositionedDescendantsMap().end() ? &*it->value : nullptr;
  if (!descendant_set) {
    descendant_set = MakeGarbageCollected<TrackedLayoutBoxLinkedHashSet>();
    GetPositionedDescendantsMap().Set(this, descendant_set);
  }
  descendant_set->insert(o);

  has_positioned_objects_ = true;
}

void LayoutBlock::RemovePositionedObject(LayoutBox* o) {
  LayoutBlock* container = GetPositionedContainerMap().Take(o);
  if (!container)
    return;

  TrackedLayoutBoxLinkedHashSet* positioned_descendants =
      GetPositionedDescendantsMap().at(container);
  DCHECK(positioned_descendants);
  DCHECK(positioned_descendants->Contains(o));
  positioned_descendants->erase(o);
  if (positioned_descendants->empty()) {
    GetPositionedDescendantsMap().erase(container);
    container->has_positioned_objects_ = false;
  }
}

bool LayoutBlock::IsAnonymousNGFieldsetContentWrapper() const {
  NOT_DESTROYED();
  return Parent() && Parent()->IsFieldset() && IsAnonymous();
}

void LayoutBlock::InvalidatePaint(
    const PaintInvalidatorContext& context) const {
  NOT_DESTROYED();
  BlockPaintInvalidator(*this).InvalidatePaint(context);
}

void LayoutBlock::ImageChanged(WrappedImagePtr image,
                               CanDeferInvalidation defer) {
  NOT_DESTROYED();
  LayoutBox::ImageChanged(image, defer);

  if (!StyleRef().HasPseudoElementStyle(kPseudoIdFirstLine))
    return;

  const auto* first_line_style =
      StyleRef().GetCachedPseudoElementStyle(kPseudoIdFirstLine);
  if (!first_line_style)
    return;
  if (auto* first_line_container = NearestInnerBlockWithFirstLine()) {
    for (const auto* layer = &first_line_style->BackgroundLayers(); layer;
         layer = layer->Next()) {
      if (layer->GetImage() && image == layer->GetImage()->Data()) {
        first_line_container->SetShouldDoFullPaintInvalidationForFirstLine();
        break;
      }
    }
  }
}

void LayoutBlock::RemovePositionedObjects(
    LayoutObject* stay_within,
    ContainingBlockState containing_block_state) {
  NOT_DESTROYED();

  auto ProcessPositionedObjectRemoval = [&](LayoutObject* positioned_object) {
    if (stay_within && (!positioned_object->IsDescendantOf(stay_within) ||
                        stay_within == positioned_object)) {
      return false;
    }

    if (containing_block_state == kNewContainingBlock)
      positioned_object->SetChildNeedsLayout(kMarkOnlyThis);

    // It is parent blocks job to add positioned child to positioned objects
    // list of its containing block.
    // Parent layout needs to be invalidated to ensure this happens.
    positioned_object->MarkParentForSpannerOrOutOfFlowPositionedChange();
    return true;
  };

  TrackedLayoutBoxLinkedHashSet* positioned_descendants = PositionedObjects();
  HeapVector<Member<LayoutBox>, 16> dead_objects;
  bool has_positioned_children_in_fragment_tree = false;

  // PositionedObjects() is populated in legacy, and in NG when inside a
  // fragmentation context root. But in other NG cases it's empty as an
  // optimization, since we can just look at the children in the fragment tree.
  if (positioned_descendants) {
    for (const auto& positioned_object : *positioned_descendants) {
      if (ProcessPositionedObjectRemoval(positioned_object))
        dead_objects.push_back(positioned_object);
    }
  } else {
    for (const NGPhysicalBoxFragment& fragment : PhysicalFragments()) {
      if (!fragment.HasOutOfFlowFragmentChild())
        continue;
      for (const NGLink& fragment_child : fragment.Children()) {
        if (!fragment_child->IsOutOfFlowPositioned())
          continue;
        if (LayoutObject* child = fragment_child->GetMutableLayoutObject()) {
          if (ProcessPositionedObjectRemoval(child))
            has_positioned_children_in_fragment_tree = true;
        }
      }
    }
  }

  // Invalidate the nearest OOF container to ensure it is marked for layout.
  // Fixed containing blocks are always absolute containing blocks too,
  // so we only need to look for absolute containing blocks.
  if (dead_objects.size() > 0 || has_positioned_children_in_fragment_tree) {
    if (LayoutBlock* containing_block = ContainingBlockForAbsolutePosition())
      containing_block->SetChildNeedsLayout(kMarkContainerChain);
  }

  if (!positioned_descendants)
    return;

  for (const auto& object : dead_objects) {
    DCHECK_EQ(GetPositionedContainerMap().at(object), this);
    positioned_descendants->erase(object);
    GetPositionedContainerMap().erase(object);
  }
  if (positioned_descendants->empty()) {
    GetPositionedDescendantsMap().erase(this);
    has_positioned_objects_ = false;
  }
}

void LayoutBlock::AddSvgTextDescendant(LayoutBox& svg_text) {
  NOT_DESTROYED();
  DCHECK(IsA<LayoutNGSVGText>(svg_text));
  auto result = View()->SvgTextDescendantsMap().insert(this, nullptr);
  if (result.is_new_entry) {
    result.stored_value->value =
        MakeGarbageCollected<TrackedLayoutBoxLinkedHashSet>();
  }
  result.stored_value->value->insert(&svg_text);
  has_svg_text_descendants_ = true;
}

void LayoutBlock::RemoveSvgTextDescendant(LayoutBox& svg_text) {
  NOT_DESTROYED();
  DCHECK(IsA<LayoutNGSVGText>(svg_text));
  TrackedDescendantsMap& map = View()->SvgTextDescendantsMap();
  auto it = map.find(this);
  if (it == map.end())
    return;
  TrackedLayoutBoxLinkedHashSet* descendants = &*it->value;
  descendants->erase(&svg_text);
  if (descendants->empty()) {
    map.erase(this);
    has_svg_text_descendants_ = false;
  }
}

LayoutUnit LayoutBlock::TextIndentOffset() const {
  NOT_DESTROYED();
  LayoutUnit cw;
  if (StyleRef().TextIndent().IsPercentOrCalc())
    cw = ContentLogicalWidth();
  return MinimumValueForLength(StyleRef().TextIndent(), cw);
}

bool LayoutBlock::HitTestChildren(HitTestResult& result,
                                  const HitTestLocation& hit_test_location,
                                  const PhysicalOffset& accumulated_offset,
                                  HitTestPhase phase) {
  NOT_DESTROYED();
  DCHECK(!ChildrenInline());

  if (PhysicalFragmentCount() && CanTraversePhysicalFragments()) {
    DCHECK(!Parent()->CanTraversePhysicalFragments());
    DCHECK_LE(PhysicalFragmentCount(), 1u);
    const NGPhysicalBoxFragment* fragment = GetPhysicalFragment(0);
    DCHECK(fragment);
    DCHECK(!fragment->HasItems());
    return NGBoxFragmentPainter(*fragment).NodeAtPoint(
        result, hit_test_location, accumulated_offset, phase);
  }

  PhysicalOffset scrolled_offset = accumulated_offset;
  if (IsScrollContainer())
    scrolled_offset -= PhysicalOffset(PixelSnappedScrolledContentOffset());
  HitTestPhase child_hit_test = phase;
  if (phase == HitTestPhase::kDescendantBlockBackgrounds)
    child_hit_test = HitTestPhase::kSelfBlockBackground;
  for (LayoutBox* child = LastChildBox(); child;
       child = child->PreviousSiblingBox()) {
    if (child->HasSelfPaintingLayer() || child->IsColumnSpanAll())
      continue;

    PhysicalOffset child_accumulated_offset =
        scrolled_offset + child->PhysicalLocation(this);
    bool did_hit;
    if (child->IsFloating()) {
      if (phase != HitTestPhase::kFloat || !IsLayoutNGObject())
        continue;
      // Hit-test the floats in regular tree order if this is LayoutNG. Only
      // legacy layout uses the FloatingObjects list.
      did_hit = child->HitTestAllPhases(result, hit_test_location,
                                        child_accumulated_offset);
    } else {
      did_hit = child->NodeAtPoint(result, hit_test_location,
                                   child_accumulated_offset, child_hit_test);
    }
    if (did_hit) {
      UpdateHitTestResult(result,
                          hit_test_location.Point() - accumulated_offset);
      return true;
    }
  }

  return false;
}

PositionWithAffinity LayoutBlock::PositionForPointIfOutsideAtomicInlineLevel(
    const PhysicalOffset& point) const {
  NOT_DESTROYED();
  DCHECK(IsAtomicInlineLevel());
  LogicalOffset logical_offset =
      point.ConvertToLogical({StyleRef().GetWritingMode(), ResolvedDirection()},
                             PhysicalSize(Size()), PhysicalSize());
  if (logical_offset.inline_offset < 0)
    return FirstPositionInOrBeforeThis();
  if (logical_offset.inline_offset >= LogicalWidth())
    return LastPositionInOrAfterThis();
  if (logical_offset.block_offset < 0)
    return FirstPositionInOrBeforeThis();
  if (logical_offset.block_offset >= LogicalHeight())
    return LastPositionInOrAfterThis();
  return PositionWithAffinity();
}

PositionWithAffinity LayoutBlock::PositionForPoint(
    const PhysicalOffset& point) const {
  NOT_DESTROYED();
  // NG codepath requires |kPrePaintClean|.
  // |SelectionModifier| calls this only in legacy codepath.
  DCHECK(!IsLayoutNGObject() || GetDocument().Lifecycle().GetState() >=
                                    DocumentLifecycle::kPrePaintClean);

  if (IsAtomicInlineLevel()) {
    PositionWithAffinity position =
        PositionForPointIfOutsideAtomicInlineLevel(point);
    if (!position.IsNull())
      return position;
  }

  if (PhysicalFragmentCount()) {
    return PositionForPointInFragments(point);
  }

  return LayoutBox::PositionForPoint(point);
}

void LayoutBlock::OffsetForContents(PhysicalOffset& offset) const {
  NOT_DESTROYED();
  if (IsScrollContainer())
    offset += PhysicalOffset(PixelSnappedScrolledContentOffset());
}

MinMaxSizes LayoutBlock::ComputeIntrinsicLogicalWidths() const {
  NOT_DESTROYED();
  MinMaxSizes sizes;
  LayoutUnit scrollbar_thickness = ComputeLogicalScrollbars().InlineSum();
  sizes += BorderAndPaddingLogicalWidth() + scrollbar_thickness;

  // See if we can early out sooner if the logical width is overridden or we're
  // size contained. Note that for multicol containers we need the column gaps.
  // So allow descending into the flow thread, which will take care of that.
  const auto* block_flow = DynamicTo<LayoutBlockFlow>(this);
  if (!block_flow || !block_flow->MultiColumnFlowThread()) {
    if (HasOverrideIntrinsicContentLogicalWidth()) {
      sizes += OverrideIntrinsicContentLogicalWidth();
      return sizes;
    }
    LayoutUnit default_inline_size = DefaultIntrinsicContentInlineSize();
    if (default_inline_size != kIndefiniteSize) {
      sizes.max_size += default_inline_size;
      // <textarea>'s intrinsic size should ignore scrollbar existence.
      if (IsTextArea()) {
        sizes -= scrollbar_thickness;
      }
      if (!StyleRef().LogicalWidth().IsPercentOrCalc())
        sizes.min_size = sizes.max_size;
      return sizes;
    }
    if (ShouldApplySizeContainment())
      return sizes;
  }

  DCHECK(!ChildrenInline());
  MinMaxSizes child_sizes;
  ComputeBlockPreferredLogicalWidths(child_sizes.min_size,
                                     child_sizes.max_size);

  child_sizes.max_size = std::max(child_sizes.min_size, child_sizes.max_size);

  auto* html_marquee_element = DynamicTo<HTMLMarqueeElement>(GetNode());
  if (html_marquee_element && html_marquee_element->IsHorizontal())
    child_sizes.min_size = LayoutUnit();
  if (UNLIKELY(IsListBox(this) && StyleRef().LogicalWidth().IsPercentOrCalc()))
    child_sizes.min_size = LayoutUnit();

  sizes += child_sizes;
  return sizes;
}

DISABLE_CFI_PERF
MinMaxSizes LayoutBlock::PreferredLogicalWidths() const {
  NOT_DESTROYED();
  MinMaxSizes sizes;

  // FIXME: The isFixed() calls here should probably be checking for isSpecified
  // since you should be able to use percentage, calc or viewport relative
  // values for width.
  const ComputedStyle& style_to_use = StyleRef();
  if (!IsTableCell() && style_to_use.LogicalWidth().IsFixed() &&
      style_to_use.LogicalWidth().Value() >= 0) {
    sizes = AdjustBorderBoxLogicalWidthForBoxSizing(
        LayoutUnit(style_to_use.LogicalWidth().Value()));
  } else {
    sizes = IntrinsicLogicalWidths();
  }

  // This implements the transferred min/max sizes per
  // https://drafts.csswg.org/css-sizing-4/#aspect-ratio
  if (ShouldComputeLogicalHeightFromAspectRatio()) {
    MinMaxSizes transferred_min_max =
        ComputeMinMaxLogicalWidthFromAspectRatio();
    sizes.Encompass(transferred_min_max.min_size);
    sizes.Constrain(transferred_min_max.max_size);
  }
  if (style_to_use.LogicalMaxWidth().IsFixed()) {
    sizes.Constrain(AdjustBorderBoxLogicalWidthForBoxSizing(
        LayoutUnit(style_to_use.LogicalMaxWidth().Value())));
  }

  if (style_to_use.LogicalMinWidth().IsFixed() &&
      style_to_use.LogicalMinWidth().Value() > 0) {
    sizes.Encompass(AdjustBorderBoxLogicalWidthForBoxSizing(
        LayoutUnit(style_to_use.LogicalMinWidth().Value())));
  }

  // Table layout uses integers, ceil the preferred widths to ensure that they
  // can contain the contents.
  if (IsTableCell()) {
    sizes.min_size = LayoutUnit(sizes.min_size.Ceil());
    sizes.max_size = LayoutUnit(sizes.max_size.Ceil());
  }

  if (IsLayoutNGObject() && IsTable()) {
    sizes.Encompass(IntrinsicLogicalWidths().min_size);
  }

  return sizes;
}

void LayoutBlock::ComputeBlockPreferredLogicalWidths(
    LayoutUnit& min_logical_width,
    LayoutUnit& max_logical_width) const {
  NOT_DESTROYED();
  const ComputedStyle& style_to_use = StyleRef();
  bool nowrap = style_to_use.WhiteSpace() == EWhiteSpace::kNowrap;

  LayoutObject* child = FirstChild();
  LayoutBlock* containing_block = ContainingBlock();
  LayoutUnit float_left_width, float_right_width;
  while (child) {
    // Positioned children don't affect the min/max width. Spanners only affect
    // the min/max width of the multicol container, not the flow thread.
    if (child->IsOutOfFlowPositioned() || child->IsColumnSpanAll()) {
      child = child->NextSibling();
      continue;
    }

    if (child->IsBox() &&
        To<LayoutBox>(child)->NeedsPreferredWidthsRecalculation()) {
      // We don't really know whether the containing block of this child did
      // change or is going to change size. However, this is our only
      // opportunity to make sure that it gets its min/max widths calculated.
      // This is also an important hook for flow threads; if the container of a
      // flow thread needs its preferred logical widths recalculated, so does
      // the flow thread, potentially.
      child->SetIntrinsicLogicalWidthsDirty();
    }

    scoped_refptr<const ComputedStyle> child_style = child->Style();
    if (child->IsFloating() ||
        (child->IsBox() &&
         To<LayoutBox>(child)->CreatesNewFormattingContext())) {
      LayoutUnit float_total_width = float_left_width + float_right_width;
      EClear c = child_style->Clear(style_to_use);
      if (c == EClear::kBoth || c == EClear::kLeft) {
        max_logical_width = std::max(float_total_width, max_logical_width);
        float_left_width = LayoutUnit();
      }
      if (c == EClear::kBoth || c == EClear::kRight) {
        max_logical_width = std::max(float_total_width, max_logical_width);
        float_right_width = LayoutUnit();
      }
    }

    // A margin basically has three types: fixed, percentage, and auto
    // (variable).
    // Auto and percentage margins simply become 0 when computing min/max width.
    // Fixed margins can be added in as is.
    const Length& start_margin_length =
        child_style->MarginStartUsing(style_to_use);
    const Length& end_margin_length = child_style->MarginEndUsing(style_to_use);
    LayoutUnit margin;
    LayoutUnit margin_start;
    LayoutUnit margin_end;
    if (start_margin_length.IsFixed())
      margin_start += start_margin_length.Value();
    if (end_margin_length.IsFixed())
      margin_end += end_margin_length.Value();
    margin = margin_start + margin_end;

    LayoutUnit child_min_preferred_logical_width,
        child_max_preferred_logical_width;
    ComputeChildPreferredLogicalWidths(*child,
                                       child_min_preferred_logical_width,
                                       child_max_preferred_logical_width);

    LayoutUnit w = child_min_preferred_logical_width + margin;
    min_logical_width = std::max(w, min_logical_width);

    // IE ignores tables for calculation of nowrap. Makes some sense.
    if (nowrap && !child->IsTable())
      max_logical_width = std::max(w, max_logical_width);

    w = child_max_preferred_logical_width + margin;

    if (!child->IsFloating()) {
      if (child->IsBox() &&
          To<LayoutBox>(child)->CreatesNewFormattingContext()) {
        // Determine a left and right max value based off whether or not the
        // floats can fit in the margins of the object. For negative margins, we
        // will attempt to overlap the float if the negative margin is smaller
        // than the float width.
        bool ltr = containing_block
                       ? containing_block->StyleRef().IsLeftToRightDirection()
                       : style_to_use.IsLeftToRightDirection();
        LayoutUnit margin_logical_left = ltr ? margin_start : margin_end;
        LayoutUnit margin_logical_right = ltr ? margin_end : margin_start;
        LayoutUnit max_left =
            margin_logical_left > 0
                ? std::max(float_left_width, margin_logical_left)
                : float_left_width + margin_logical_left;
        LayoutUnit max_right =
            margin_logical_right > 0
                ? std::max(float_right_width, margin_logical_right)
                : float_right_width + margin_logical_right;
        w = child_max_preferred_logical_width + max_left + max_right;
        w = std::max(w, float_left_width + float_right_width);
      } else {
        max_logical_width =
            std::max(float_left_width + float_right_width, max_logical_width);
      }
      float_left_width = float_right_width = LayoutUnit();
    }

    if (child->IsFloating()) {
      if (child_style->Floating(style_to_use) == EFloat::kLeft)
        float_left_width += w;
      else
        float_right_width += w;
    } else {
      max_logical_width = std::max(w, max_logical_width);
    }

    child = child->NextSibling();
  }

  // Always make sure these values are non-negative.
  min_logical_width = min_logical_width.ClampNegativeToZero();
  max_logical_width = max_logical_width.ClampNegativeToZero();

  max_logical_width =
      std::max(float_left_width + float_right_width, max_logical_width);
}

DISABLE_CFI_PERF
void LayoutBlock::ComputeChildPreferredLogicalWidths(
    LayoutObject& child,
    LayoutUnit& min_preferred_logical_width,
    LayoutUnit& max_preferred_logical_width) const {
  NOT_DESTROYED();
  if (child.IsBox() &&
      child.IsHorizontalWritingMode() != IsHorizontalWritingMode()) {
    // If the child is an orthogonal flow, child's height determines the width,
    // but the height is not available until layout.
    // https://drafts.csswg.org/css-writing-modes/#orthogonal-shrink-to-fit
    if (!child.NeedsLayout()) {
      min_preferred_logical_width = max_preferred_logical_width =
          To<LayoutBox>(child).LogicalHeight();
      return;
    }
    min_preferred_logical_width = max_preferred_logical_width =
        To<LayoutBox>(child).ComputeLogicalHeightWithoutLayout();
    return;
  }

  MinMaxSizes child_preferred_logical_widths = child.PreferredLogicalWidths();
  min_preferred_logical_width = child_preferred_logical_widths.min_size;
  max_preferred_logical_width = child_preferred_logical_widths.max_size;

  // For non-replaced blocks if the inline size is min|max-content or a definite
  // size the min|max-content contribution is that size plus border, padding and
  // margin https://drafts.csswg.org/css-sizing/#block-intrinsic
  if (child.IsLayoutBlock()) {
    const Length& computed_inline_size = child.StyleRef().LogicalWidth();
    if (computed_inline_size.IsMaxContent())
      min_preferred_logical_width = max_preferred_logical_width;
    else if (computed_inline_size.IsMinContent() ||
             computed_inline_size.IsMinIntrinsic())
      max_preferred_logical_width = min_preferred_logical_width;
  }
}

bool LayoutBlock::HasLineIfEmpty() const {
  NOT_DESTROYED();
  if (GetNode()) {
    if (IsRootEditableElement(*GetNode()))
      return true;
  }
  return FirstLineStyleRef().HasLineIfEmpty();
}

LayoutUnit LayoutBlock::EmptyLineBaseline(
    LineDirectionMode line_direction) const {
  NOT_DESTROYED();
  if (!HasLineIfEmpty())
    return LayoutUnit(-1);
  const auto baseline_offset = BaselineForEmptyLine(line_direction);
  return baseline_offset ? *baseline_offset : LayoutUnit(-1);
}

absl::optional<LayoutUnit> LayoutBlock::BaselineForEmptyLine(
    LineDirectionMode line_direction) const {
  NOT_DESTROYED();
  const SimpleFontData* font_data = FirstLineStyle()->GetFont().PrimaryFont();
  if (!font_data)
    return absl::nullopt;
  const auto& font_metrics = font_data->GetFontMetrics();
  const LayoutUnit line_height =
      LineHeight(true, line_direction, kPositionOfInteriorLineBoxes);
  const LayoutUnit border_padding = line_direction == kHorizontalLine
                                        ? BorderTop() + PaddingTop()
                                        : BorderRight() + PaddingRight();
  return LayoutUnit((font_metrics.Ascent() +
                     (line_height - font_metrics.Height()) / 2 + border_padding)
                        .ToInt());
}

LayoutUnit LayoutBlock::LineHeight(bool first_line,
                                   LineDirectionMode direction,
                                   LinePositionMode line_position_mode) const {
  NOT_DESTROYED();
  // Inline blocks are replaced elements. Otherwise, just pass off to
  // the base class.  If we're being queried as though we're the root line
  // box, then the fact that we're an inline-block is irrelevant, and we behave
  // just like a block.
  if (IsAtomicInlineLevel() && line_position_mode == kPositionOnContainingLine)
    return LayoutBox::LineHeight(first_line, direction, line_position_mode);

  const ComputedStyle& style = StyleRef(
      first_line && GetDocument().GetStyleEngine().UsesFirstLineRules());
  return LayoutUnit(style.ComputedLineHeight());
}

LayoutUnit LayoutBlock::MinLineHeightForReplacedObject(
    bool is_first_line,
    LayoutUnit replaced_height) const {
  NOT_DESTROYED();
  if (!GetDocument().InNoQuirksMode() && replaced_height)
    return replaced_height;

  return std::max<LayoutUnit>(
      replaced_height,
      LineHeight(is_first_line,
                 IsHorizontalWritingMode() ? kHorizontalLine : kVerticalLine,
                 kPositionOfInteriorLineBoxes));
}

bool LayoutBlock::UseLogicalBottomMarginEdgeForInlineBlockBaseline() const {
  NOT_DESTROYED();
  // CSS2.1 states that the baseline of an 'inline-block' is:
  // the baseline of the last line box in the normal flow, unless it has
  // either no in-flow line boxes or if its 'overflow' property has a computed
  // value other than 'visible', in which case the baseline is the bottom
  // margin edge.
  // We likewise avoid using the last line box in the case of size containment,
  // where the block's contents shouldn't be considered when laying out its
  // ancestors or siblings.
  return (!StyleRef().IsOverflowVisibleOrClip() &&
          !StyleRef().ShouldIgnoreOverflowPropertyForInlineBlockBaseline()) ||
         ShouldApplyLayoutContainment();
}

const LayoutBlock* LayoutBlock::FirstLineStyleParentBlock() const {
  NOT_DESTROYED();
  const LayoutBlock* first_line_block = this;
  // Inline blocks do not get ::first-line style from its containing blocks.
  if (IsAtomicInlineLevel())
    return nullptr;
  // Floats and out of flow blocks do not get ::first-line style from its
  // containing blocks.
  if (IsFloatingOrOutOfFlowPositioned())
    return nullptr;

  LayoutObject* parent_block = first_line_block->Parent();
  if (!parent_block || !parent_block->BehavesLikeBlockContainer())
    return nullptr;

  const LayoutBlock* parent_layout_block = To<LayoutBlock>(parent_block);

  // If we are not the first in-flow child of our parent, we cannot get
  // ::first-line style from our ancestors.
  const LayoutObject* first_child = parent_layout_block->FirstChild();
  while (first_child->IsFloatingOrOutOfFlowPositioned())
    first_child = first_child->NextSibling();
  if (first_child != first_line_block)
    return nullptr;

  return parent_layout_block;
}

LayoutBlockFlow* LayoutBlock::NearestInnerBlockWithFirstLine() {
  NOT_DESTROYED();
  if (ChildrenInline())
    return To<LayoutBlockFlow>(this);
  for (LayoutObject* child = FirstChild();
       child && !child->IsFloatingOrOutOfFlowPositioned() &&
       child->IsLayoutBlockFlow();
       child = To<LayoutBlock>(child)->FirstChild()) {
    if (child->ChildrenInline())
      return To<LayoutBlockFlow>(child);
  }
  return nullptr;
}

// An inline-block uses its inlineBox as the inlineBoxWrapper,
// so the firstChild() is nullptr if the only child is an empty inline-block.
inline bool LayoutBlock::IsInlineBoxWrapperActuallyChild() const {
  NOT_DESTROYED();
  return IsInlineBlockOrInlineTable() && !Size().IsEmpty() && GetNode() &&
         EditingIgnoresContent(*GetNode());
}

LayoutRect LayoutBlock::LocalCaretRect(
    int caret_offset,
    LayoutUnit* extra_width_to_end_of_line) const {
  NOT_DESTROYED();
  // Do the normal calculation in most cases.
  if ((FirstChild() && !FirstChild()->IsPseudoElement()) ||
      IsInlineBoxWrapperActuallyChild()) {
    return LayoutBox::LocalCaretRect(caret_offset, extra_width_to_end_of_line);
  }

  LayoutRect caret_rect =
      LocalCaretRectForEmptyElement(Size().Width(), TextIndentOffset());

  if (extra_width_to_end_of_line)
    *extra_width_to_end_of_line = Size().Width() - caret_rect.MaxX();

  return caret_rect;
}

void LayoutBlock::AddOutlineRects(Vector<PhysicalRect>& rects,
                                  OutlineInfo* info,
                                  const PhysicalOffset& additional_offset,
                                  NGOutlineType include_block_overflows) const {
  NOT_DESTROYED();
#if DCHECK_IS_ON()
  // TODO(crbug.com/987836): enable this DCHECK universally.
  Page* page = GetDocument().GetPage();
  if (page && !page->GetSettings().GetSpatialNavigationEnabled()) {
    DCHECK_GE(GetDocument().Lifecycle().GetState(),
              DocumentLifecycle::kAfterPerformLayout);
  }
#endif  // DCHECK_IS_ON()

  if (!IsAnonymous())  // For anonymous blocks, the children add outline rects.
    rects.emplace_back(additional_offset, Size());

  if (ShouldIncludeBlockVisualOverflow(include_block_overflows) &&
      !HasNonVisibleOverflow() && !HasControlClip()) {
    AddOutlineRectsForNormalChildren(rects, additional_offset,
                                     include_block_overflows);
    if (TrackedLayoutBoxLinkedHashSet* positioned_objects =
            PositionedObjects()) {
      for (const auto& box : *positioned_objects)
        AddOutlineRectsForDescendant(*box, rects, additional_offset,
                                     include_block_overflows);
    }
  }
  if (info)
    *info = OutlineInfo::GetFromStyle(StyleRef());
}

LayoutBox* LayoutBlock::CreateAnonymousBoxWithSameTypeAs(
    const LayoutObject* parent) const {
  NOT_DESTROYED();
  return CreateAnonymousWithParentAndDisplay(parent, StyleRef().Display());
}

const char* LayoutBlock::GetName() const {
  NOT_DESTROYED();
  NOTREACHED();
  return "LayoutBlock";
}

LayoutBlock* LayoutBlock::CreateAnonymousWithParentAndDisplay(
    const LayoutObject* parent,
    EDisplay display) {
  // TODO(layout-dev): Do we need to convert all our inline displays to block
  // type in the anonymous logic?
  EDisplay new_display;
  switch (display) {
    case EDisplay::kFlex:
    case EDisplay::kInlineFlex:
      new_display = EDisplay::kFlex;
      break;
    case EDisplay::kGrid:
    case EDisplay::kInlineGrid:
      new_display = EDisplay::kGrid;
      break;
    case EDisplay::kFlowRoot:
      new_display = EDisplay::kFlowRoot;
      break;
    case EDisplay::kBlockMath:
      new_display = EDisplay::kBlockMath;
      break;
    default:
      new_display = EDisplay::kBlock;
      break;
  }
  ComputedStyleBuilder new_style_builder =
      parent->GetDocument()
          .GetStyleResolver()
          .CreateAnonymousStyleBuilderWithDisplay(parent->StyleRef(),
                                                  new_display);

  parent->UpdateAnonymousChildStyle(nullptr, new_style_builder);
  scoped_refptr<const ComputedStyle> new_style = new_style_builder.TakeStyle();

  LayoutBlock* layout_block;
  if (new_display == EDisplay::kFlex) {
    layout_block =
        MakeGarbageCollected<LayoutNGFlexibleBox>(/* element */ nullptr);
  } else if (new_display == EDisplay::kGrid) {
    layout_block = MakeGarbageCollected<LayoutNGGrid>(/* element */ nullptr);
  } else if (new_display == EDisplay::kBlockMath) {
    layout_block =
        MakeGarbageCollected<LayoutNGMathMLBlock>(/* element */ nullptr);
  } else {
    DCHECK(new_display == EDisplay::kBlock ||
           new_display == EDisplay::kFlowRoot);
    layout_block =
        LayoutObjectFactory::CreateBlockFlow(parent->GetDocument(), *new_style);
  }
  layout_block->SetDocumentForAnonymous(&parent->GetDocument());
  layout_block->SetStyle(std::move(new_style));
  return layout_block;
}

RecalcLayoutOverflowResult LayoutBlock::RecalcChildLayoutOverflow() {
  NOT_DESTROYED();
  DCHECK(!IsTable() || IsLayoutNGObject());
  DCHECK(ChildNeedsLayoutOverflowRecalc());
  ClearChildNeedsLayoutOverflowRecalc();

  RecalcLayoutOverflowResult result;

  DCHECK(!ChildrenInline());
  for (LayoutBox* box = FirstChildBox(); box; box = box->NextSiblingBox()) {
    if (box->IsOutOfFlowPositioned()) {
      continue;
    }

    result.Unite(box->RecalcLayoutOverflow());
  }

  result.Unite(RecalcPositionedDescendantsLayoutOverflow());
  return result;
}

void LayoutBlock::RecalcChildVisualOverflow() {
  NOT_DESTROYED();
  DCHECK(!IsTable() || IsLayoutNGObject());
  // It is an error to call this function on a LayoutBlock that it itself inside
  // a display-locked subtree.
  DCHECK(!DisplayLockUtilities::LockedAncestorPreventingPrePaint(*this));
  if (ChildPrePaintBlockedByDisplayLock())
    return;

  if (ChildrenInline()) {
    SECURITY_DCHECK(IsLayoutBlockFlow());
    To<LayoutBlockFlow>(this)->RecalcInlineChildrenVisualOverflow();
  } else {
    for (LayoutBox* box = FirstChildBox(); box; box = box->NextSiblingBox()) {
      box->RecalcNormalFlowChildVisualOverflowIfNeeded();
    }
  }
}

RecalcLayoutOverflowResult
LayoutBlock::RecalcPositionedDescendantsLayoutOverflow() {
  NOT_DESTROYED();
  RecalcLayoutOverflowResult result;

  TrackedLayoutBoxLinkedHashSet* positioned_descendants = PositionedObjects();
  if (!positioned_descendants)
    return result;

  for (auto& box : *positioned_descendants)
    result.Unite(box->RecalcLayoutOverflow());

  return result;
}

RecalcLayoutOverflowResult LayoutBlock::RecalcLayoutOverflow() {
  NOT_DESTROYED();
  bool children_layout_overflow_changed = false;
  if (ChildNeedsLayoutOverflowRecalc()) {
    children_layout_overflow_changed =
        RecalcChildLayoutOverflow().layout_overflow_changed;
  }

  bool self_needs_overflow_recalc = SelfNeedsLayoutOverflowRecalc();
  if (!self_needs_overflow_recalc && !children_layout_overflow_changed)
    return RecalcLayoutOverflowResult();

  // rebuild_fragment_tree can be false here as it is a legacy root.
  return {RecalcSelfLayoutOverflow(), /* rebuild_fragment_tree */ false};
}

void LayoutBlock::RecalcVisualOverflow() {
  NOT_DESTROYED();
  RecalcChildVisualOverflow();
  RecalcSelfVisualOverflow();
}

bool LayoutBlock::RecalcSelfLayoutOverflow() {
  NOT_DESTROYED();
  bool self_needs_layout_overflow_recalc = SelfNeedsLayoutOverflowRecalc();
  // If the current block needs layout, overflow will be recalculated during
  // layout time anyway. We can safely exit here.
  if (NeedsLayout())
    return false;

  LayoutUnit old_client_after_edge = LayoutClientAfterEdge();
  ComputeLayoutOverflow(old_client_after_edge, true);
  if (IsScrollContainer())
    Layer()->GetScrollableArea()->UpdateAfterOverflowRecalc();

  return !HasNonVisibleOverflow() || self_needs_layout_overflow_recalc;
}

void LayoutBlock::RecalcSelfVisualOverflow() {
  NOT_DESTROYED();
  ComputeVisualOverflow(true);
}

#if DCHECK_IS_ON()
void LayoutBlock::CheckPositionedObjectsNeedLayout() {
  NOT_DESTROYED();
  if (ChildLayoutBlockedByDisplayLock())
    return;

  if (TrackedLayoutBoxLinkedHashSet* positioned_descendant_set =
          PositionedObjects()) {
    TrackedLayoutBoxLinkedHashSet::const_iterator end =
        positioned_descendant_set->end();
    for (TrackedLayoutBoxLinkedHashSet::const_iterator it =
             positioned_descendant_set->begin();
         it != end; ++it) {
      LayoutBox* curr_box = *it;
      // An OOF positioned object may still need to be laid out in NG once it
      // reaches its containing block if it is inside a fragmentation context.
      // In such cases, we wait to perform layout of the OOF at the
      // fragmentation context root instead.
      if (!curr_box->MightBeInsideFragmentationContext()) {
        DCHECK(!curr_box->SelfNeedsLayout());
        DCHECK(curr_box->ChildLayoutBlockedByDisplayLock() ||
               !curr_box->NeedsLayout());
      }
    }
  }
}

#endif

LayoutUnit LayoutBlock::AvailableLogicalHeightForPercentageComputation() const {
  NOT_DESTROYED();
  LayoutUnit available_height(-1);

  // For anonymous blocks that are skipped during percentage height calculation,
  // we consider them to have an indefinite height.
  if (SkipContainingBlockForPercentHeightCalculation(this))
    return available_height;

  const ComputedStyle& style = StyleRef();

  // A positioned element that specified both top/bottom or that specifies
  // height should be treated as though it has a height explicitly specified
  // that can be used for any percentage computations.
  bool is_out_of_flow_positioned_with_specified_height =
      IsOutOfFlowPositioned() &&
      (!style.LogicalHeight().IsAuto() ||
       (!style.LogicalTop().IsAuto() && !style.LogicalBottom().IsAuto()));

  LayoutUnit stretched_flex_height(-1);
  if (HasOverrideLogicalHeight() && IsOverrideLogicalHeightDefinite()) {
    stretched_flex_height = OverrideContentLogicalHeight();
  }
  if (stretched_flex_height != LayoutUnit(-1)) {
    available_height = stretched_flex_height;
  } else if (style.LogicalHeight().IsFixed()) {
    LayoutUnit content_box_height = AdjustContentBoxLogicalHeightForBoxSizing(
        style.LogicalHeight().Value());
    available_height =
        std::max(LayoutUnit(),
                 ConstrainContentBoxLogicalHeightByMinMax(
                     content_box_height - ComputeLogicalScrollbars().BlockSum(),
                     LayoutUnit(-1)));
  } else if (ShouldComputeLogicalHeightFromAspectRatio()) {
    NGBoxStrut border_padding(BorderStart() + ComputedCSSPaddingStart(),
                              BorderEnd() + ComputedCSSPaddingEnd(),
                              BorderBefore() + ComputedCSSPaddingBefore(),
                              BorderAfter() + ComputedCSSPaddingAfter());
    available_height = BlockSizeFromAspectRatio(
        border_padding, StyleRef().LogicalAspectRatio(),
        StyleRef().BoxSizingForAspectRatio(), LogicalWidth());
  } else if (is_out_of_flow_positioned_with_specified_height) {
    // Don't allow this to affect the block' size() member variable, since this
    // can get called while the block is still laying out its kids.
    LogicalExtentComputedValues computed_values;
    ComputeLogicalHeight(LogicalHeight(), LayoutUnit(), computed_values);
    available_height = computed_values.extent_ -
                       BorderAndPaddingLogicalHeight() -
                       ComputeLogicalScrollbars().BlockSum();
  } else if (style.LogicalHeight().IsPercentOrCalc()) {
    LayoutUnit height_with_scrollbar =
        ComputePercentageLogicalHeight(style.LogicalHeight());
    if (height_with_scrollbar != -1) {
      LayoutUnit content_box_height_with_scrollbar =
          AdjustContentBoxLogicalHeightForBoxSizing(height_with_scrollbar);
      // We need to adjust for min/max height because this method does not
      // handle the min/max of the current block, its caller does. So the
      // return value from the recursive call will not have been adjusted
      // yet.
      LayoutUnit content_box_height = ConstrainContentBoxLogicalHeightByMinMax(
          content_box_height_with_scrollbar -
              ComputeLogicalScrollbars().BlockSum(),
          LayoutUnit(-1));
      available_height = std::max(LayoutUnit(), content_box_height);
    }
  } else if (IsA<LayoutView>(this)) {
    available_height = View()->ViewLogicalHeightForPercentages();
  }

  return available_height;
}

bool LayoutBlock::HasDefiniteLogicalHeight() const {
  NOT_DESTROYED();
  return AvailableLogicalHeightForPercentageComputation() != LayoutUnit(-1);
}

bool LayoutBlock::NeedsPreferredWidthsRecalculation() const {
  NOT_DESTROYED();
  return (HasRelativeLogicalHeight() && StyleRef().LogicalWidth().IsAuto()) ||
         LayoutBox::NeedsPreferredWidthsRecalculation();
}

}  // namespace blink
