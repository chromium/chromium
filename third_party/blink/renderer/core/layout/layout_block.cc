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
#include "third_party/blink/renderer/core/layout/api/line_layout_box.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/box_layout_extra_input.h"
#include "third_party/blink/renderer/core/layout/hit_test_location.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_grid.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_multi_column_spanner_placeholder.h"
#include "third_party/blink/renderer/core/layout/layout_object_factory.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_table_cell.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/line/inline_text_box.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_length_utils.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_text.h"
#include "third_party/blink/renderer/core/layout/text_autosizer.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/root_scroller_controller.h"
#include "third_party/blink/renderer/core/paint/block_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/block_painter.h"
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

// This map keeps track of the descendants whose 'height' is percentage
// associated with a containing block. Like |gPositionedDescendantsMap|, it is
// also recomputed for every layout (see the comment above about why).
static TrackedDescendantsMap& GetPercentHeightDescendantsMap() {
  DEFINE_STATIC_LOCAL(Persistent<TrackedDescendantsMap>, map,
                      (MakeGarbageCollected<TrackedDescendantsMap>()));
  return *map;
}

LayoutBlock::LayoutBlock(ContainerNode* node)
    : LayoutBox(node),
      has_margin_before_quirk_(false),
      has_margin_after_quirk_(false),
      has_markup_truncation_(false),
      width_available_to_children_changed_(false),
      height_available_to_children_changed_(false),
      is_self_collapsing_(false),
      descendants_with_floats_marked_for_layout_(false),
      has_positioned_objects_(false),
      has_percent_height_descendants_(false),
      has_svg_text_descendants_(false),
      pagination_state_changed_(false),
      is_legacy_initiated_out_of_flow_layout_(false) {
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
  if (HasPercentHeightDescendants()) {
    TrackedLayoutBoxLinkedHashSet* descendants =
        GetPercentHeightDescendantsMap().Take(this);
    DCHECK(!descendants->empty());
    for (LayoutBox* descendant : *descendants) {
      DCHECK_EQ(descendant->PercentHeightContainer(), this);
      descendant->SetPercentHeightContainer(nullptr);
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

enum LogicalExtent { kLogicalWidth, kLogicalHeight };
static bool BorderOrPaddingLogicalDimensionChanged(
    const ComputedStyle& old_style,
    const ComputedStyle& new_style,
    LogicalExtent logical_extent) {
  if (new_style.IsHorizontalWritingMode() ==
      (logical_extent == kLogicalWidth)) {
    return old_style.BorderLeftWidth() != new_style.BorderLeftWidth() ||
           old_style.BorderRightWidth() != new_style.BorderRightWidth() ||
           old_style.PaddingLeft() != new_style.PaddingLeft() ||
           old_style.PaddingRight() != new_style.PaddingRight();
  }

  return old_style.BorderTopWidth() != new_style.BorderTopWidth() ||
         old_style.BorderBottomWidth() != new_style.BorderBottomWidth() ||
         old_style.PaddingTop() != new_style.PaddingTop() ||
         old_style.PaddingBottom() != new_style.PaddingBottom();
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

  // It's possible for our border/padding to change, but for the overall logical
  // width or height of the block to end up being the same. We keep track of
  // this change so in layoutBlock, we can know to set relayoutChildren=true.
  width_available_to_children_changed_ |=
      old_style && NeedsLayout() &&
      (diff.NeedsFullLayout() || BorderOrPaddingLogicalDimensionChanged(
                                     *old_style, new_style, kLogicalWidth));
  height_available_to_children_changed_ |=
      old_style && diff.NeedsFullLayout() && NeedsLayout() &&
      BorderOrPaddingLogicalDimensionChanged(*old_style, new_style,
                                             kLogicalHeight);

  if (diff.TransformChanged() && has_svg_text_descendants_) {
    const double new_squared_scale = ComputeSquaredLocalFontSizeScalingFactor(
        Layer() ? Layer()->Transform() : nullptr);
    // Compare local scale before and after.
    if (old_squared_scale != new_squared_scale) {
      for (LayoutBox* box : *View()->SvgTextDescendantsMap().at(this)) {
        To<LayoutNGSVGText>(box)->SetNeedsTextMetricsUpdate();
        if (GetNode() == GetDocument().documentElement()) {
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
          (!IsFlexibleBoxIncludingNG() && !IsLayoutGridIncludingNG()))) ||
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
        (!IsFlexibleBoxIncludingNG() && !IsLayoutGridIncludingNG())))) {
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

  if (child->Continuation())
    return;

  // Promote all the leftover anonymous block's children (to become children of
  // this block instead). We still want to keep the leftover block in the tree
  // for a moment, for notification purposes done further below (flow threads
  // and grids).
  child->MoveAllChildrenTo(this, child->NextSibling());

  // Remove all the information in the flow thread associated with the leftover
  // anonymous block.
  child->RemoveFromLayoutFlowThread();

  // LayoutGrid keeps track of its children, we must notify it about changes in
  // the tree.
  if (child->Parent()->IsLayoutGrid())
    To<LayoutGrid>(child->Parent())->DirtyGrid();

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

  height_available_to_children_changed_ = false;
}

bool LayoutBlock::WidthAvailableToChildrenHasChanged() {
  NOT_DESTROYED();
  // TODO(robhogan): Does m_widthAvailableToChildrenChanged always get reset
  // when it needs to?
  bool width_available_to_children_has_changed =
      width_available_to_children_changed_;
  width_available_to_children_changed_ = false;

  // If we use border-box sizing, have percentage padding, and our parent has
  // changed width then the width available to our children has changed even
  // though our own width has remained the same.
  // TODO(mstensho): NeedsPreferredWidthsRecalculation() is used here to check
  // if we have percentage padding, which is rather non-obvious. That method
  // returns true in other cases as well.
  width_available_to_children_has_changed |=
      StyleRef().BoxSizing() == EBoxSizing::kBorderBox &&
      NeedsPreferredWidthsRecalculation() &&
      View()->GetLayoutState()->ContainingBlockLogicalWidthChanged();

  return width_available_to_children_has_changed;
}

DISABLE_CFI_PERF
bool LayoutBlock::UpdateLogicalWidthAndColumnWidth() {
  NOT_DESTROYED();
  LayoutUnit old_width = LogicalWidth();
  UpdateLogicalWidth();
  return old_width != LogicalWidth() || WidthAvailableToChildrenHasChanged();
}

void LayoutBlock::UpdateBlockLayout(bool) {
  NOT_DESTROYED();
  NOTREACHED();
  ClearNeedsLayout();
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

  if (ChildrenInline())
    To<LayoutBlockFlow>(this)->AddLayoutOverflowFromInlineChildren();
  else
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

    // If the child contains inline with outline and continuation, its
    // visual overflow computed during its layout might be inaccurate because
    // the layout of continuations might not be up-to-date at that time.
    // Re-add overflow from inline children to ensure its overflow covers
    // the outline which may enclose continuations.
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
    if (child_block_flow &&
        child_block_flow->ContainsInlineWithOutlineAndContinuation() &&
        !child_block_flow->ChildPrePaintBlockedByDisplayLock()) {
      child_block_flow->AddVisualOverflowFromInlineChildren();
    }
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

    // If the child contains inline with outline and continuation, its
    // visual overflow computed during its layout might be inaccurate because
    // the layout of continuations might not be up-to-date at that time.
    // Re-add overflow from inline children to ensure its overflow covers
    // the outline which may enclose continuations.
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
    if (child_block_flow &&
        child_block_flow->ContainsInlineWithOutlineAndContinuation() &&
        !child_block_flow->ChildPrePaintBlockedByDisplayLock()) {
      child_block_flow->AddLayoutOverflowFromInlineChildren();
    }

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

static inline bool ChangeInAvailableLogicalHeightAffectsChild(
    LayoutBlock* parent,
    LayoutBox& child) {
  if (parent->StyleRef().BoxSizing() != EBoxSizing::kBorderBox)
    return false;
  return parent->StyleRef().IsHorizontalWritingMode() &&
         !child.StyleRef().IsHorizontalWritingMode();
}

void LayoutBlock::UpdateBlockChildDirtyBitsBeforeLayout(bool relayout_children,
                                                        LayoutBox& child) {
  NOT_DESTROYED();
  if (child.IsOutOfFlowPositioned()) {
    // It's rather useless to mark out-of-flow children at this point. We may
    // not be their containing block (and if we are, it's just pure luck), so
    // this would be the wrong place for it. Furthermore, it would cause trouble
    // for out-of-flow descendants of column spanners, if the containing block
    // is outside the spanner but inside the multicol container.
    return;
  }

  // FIXME: Technically percentage height objects only need a relayout if
  // their percentage isn't going to be turned into an auto value. Add a
  // method to determine this, so that we can avoid the relayout.
  bool has_relative_logical_height =
      child.HasRelativeLogicalHeight() ||
      (child.IsAnonymous() && HasRelativeLogicalHeight()) ||
      child.StretchesToViewport();
  if (relayout_children ||
      (has_relative_logical_height && !IsA<LayoutView>(this)) ||
      (height_available_to_children_changed_ &&
       ChangeInAvailableLogicalHeightAffectsChild(this, child)) ||
      (child.IsListMarker() && IsListItem() &&
       To<LayoutBlockFlow>(this)->ContainsFloats())) {
    if (child.IsLayoutNGObject())
      child.SetSelfNeedsLayoutForAvailableSpace(true);
    else
      child.SetChildNeedsLayout(kMarkOnlyThis);
  }
}

void LayoutBlock::SimplifiedNormalFlowLayout() {
  NOT_DESTROYED();
  if (ChildrenInline()) {
    SECURITY_DCHECK(IsLayoutBlockFlow());
    auto* block_flow = To<LayoutBlockFlow>(this);
    block_flow->SimplifiedNormalFlowInlineLayout();
  } else {
    for (LayoutBox* box = FirstChildBox(); box; box = box->NextSiblingBox()) {
      if (!box->IsOutOfFlowPositioned()) {
        if (box->IsLayoutMultiColumnSpannerPlaceholder())
          To<LayoutMultiColumnSpannerPlaceholder>(box)
              ->MarkForLayoutIfObjectInFlowThreadNeedsLayout();
        box->LayoutIfNeeded();
      }
    }
  }
}

bool LayoutBlock::SimplifiedLayout() {
  NOT_DESTROYED();
  // Check if we need to do a full layout.
  if (NormalChildNeedsLayout() || SelfNeedsLayout())
    return false;

  // Check that we actually need to do a simplified layout.
  if (!PosChildNeedsLayout() &&
      !(NeedsSimplifiedNormalFlowLayout() || NeedsPositionedMovementLayout()))
    return false;

  {
    // LayoutState needs this deliberate scope to pop before paint invalidation.
    LayoutState state(*this);

    if (NeedsPositionedMovementLayout() &&
        !TryLayoutDoingPositionedMovementOnly())
      return false;

    // If this block is inside a multicol container, we may not be able to
    // perform simplified layout.
    if (LayoutFlowThread* flow_thread = FlowThreadContainingBlock()) {
      if (!flow_thread->CanSkipLayout(*this))
        return false;
    }
    // Additionally, if this block itself establishes a multicol container, we
    // may not be able to perform simplified layout inside it. This is really
    // only unsafe if there are spanners in there, but let's just bail.
    if (const auto* block_flow = DynamicTo<LayoutBlockFlow>(this)) {
      if (block_flow->MultiColumnFlowThread())
        return false;
    }

    if (ChildLayoutBlockedByDisplayLock())
      return false;

    TextAutosizer::LayoutScope text_autosizer_layout_scope(this);

    // Lay out positioned descendants or objects that just need to recompute
    // overflow.
    if (NeedsSimplifiedNormalFlowLayout())
      SimplifiedNormalFlowLayout();

    // Lay out our positioned objects if our positioned child bit is set.
    // Also, if an absolute position element inside a relative positioned
    // container moves, and the absolute element has a fixed position child
    // neither the fixed element nor its container learn of the movement since
    // posChildNeedsLayout() is only marked as far as the relative positioned
    // container. So if we can have fixed pos objects in our positioned objects
    // list check if any of them are statically positioned and thus need to move
    // with their absolute ancestors.
    bool can_contain_fixed_pos_objects = CanContainFixedPositionObjects();
    if (PosChildNeedsLayout() || NeedsPositionedMovementLayout() ||
        can_contain_fixed_pos_objects)
      LayoutPositionedObjects(
          false, NeedsPositionedMovementLayout()
                     ? kForcedLayoutAfterContainingBlockMoved
                     : (!PosChildNeedsLayout() && can_contain_fixed_pos_objects
                            ? kLayoutOnlyFixedPositionedObjects
                            : kDefaultLayout));

    // Recompute our overflow information.
    // FIXME: We could do better here by computing a temporary overflow object
    // from layoutPositionedObjects and only updating our overflow if we either
    // used to have overflow or if the new temporary object has overflow.
    // For now just always recompute overflow. This is no worse performance-wise
    // than the old code that called rightmostPosition and lowestPosition on
    // every relayout so it's not a regression. computeOverflow expects the
    // bottom edge before we clamp our height. Since this information isn't
    // available during simplifiedLayout, we cache the value in m_overflow.
    LayoutUnit old_client_after_edge = LayoutClientAfterEdge();
    ComputeLayoutOverflow(old_client_after_edge, true);
  }

  UpdateAfterLayout();
  ClearNeedsLayout();

  return true;
}

void LayoutBlock::MarkFixedPositionObjectForLayoutIfNeeded(
    LayoutObject* child,
    SubtreeLayoutScope& layout_scope) {
  NOT_DESTROYED();
  if (child->StyleRef().GetPosition() != EPosition::kFixed)
    return;

  bool has_static_block_position =
      child->StyleRef().HasStaticBlockPosition(IsHorizontalWritingMode());
  bool has_static_inline_position =
      child->StyleRef().HasStaticInlinePosition(IsHorizontalWritingMode());
  if (!has_static_block_position && !has_static_inline_position)
    return;

  LayoutObject* o = child->Parent();
  bool is_layout_view = IsA<LayoutView>(o);
  while (!is_layout_view && o->StyleRef().GetPosition() != EPosition::kAbsolute)
    o = o->Parent();
  // The LayoutView is absolute-positioned, but does not move.
  if (is_layout_view)
    return;

  // We must compute child's width and height, but not update them now.
  // The child will update its width and height when it gets laid out, and needs
  // to see them change there.
  auto* box = To<LayoutBox>(child);
  if (has_static_inline_position) {
    LogicalExtentComputedValues computed_values;
    box->ComputeLogicalWidth(computed_values);
    LayoutUnit new_left = computed_values.position_;
    if (new_left != box->LogicalLeft())
      layout_scope.SetChildNeedsLayout(child);
  }
  if (has_static_block_position) {
    LogicalExtentComputedValues computed_values;
    box->ComputeLogicalHeight(computed_values);
    LayoutUnit new_top = computed_values.position_;
    if (new_top != box->LogicalTop())
      layout_scope.SetChildNeedsLayout(child);
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

static bool NeedsLayoutDueToStaticPosition(LayoutBox* child) {
  // When a non-positioned block element moves, it may have positioned children
  // that are implicitly positioned relative to the non-positioned block.
  const ComputedStyle* style = child->Style();
  bool is_horizontal = style->IsHorizontalWritingMode();
  if (style->HasStaticBlockPosition(is_horizontal)) {
    LayoutBox::LogicalExtentComputedValues computed_values;
    LayoutUnit current_logical_top = child->LogicalTop();
    LayoutUnit current_logical_height = child->LogicalHeight();
    child->ComputeLogicalHeight(current_logical_height, current_logical_top,
                                computed_values);
    if (computed_values.position_ != current_logical_top ||
        computed_values.extent_ != current_logical_height)
      return true;
  }
  if (style->HasStaticInlinePosition(is_horizontal)) {
    LayoutBox::LogicalExtentComputedValues computed_values;
    LayoutUnit current_logical_left = child->LogicalLeft();
    LayoutUnit current_logical_width = child->LogicalWidth();
    child->ComputeLogicalWidth(computed_values);
    if (computed_values.position_ != current_logical_left ||
        computed_values.extent_ != current_logical_width)
      return true;
  }
  return false;
}

void LayoutBlock::LayoutPositionedObjects(bool relayout_children,
                                          PositionedLayoutBehavior info) {
  NOT_DESTROYED();
  if (ChildLayoutBlockedByDisplayLock())
    return;

  TrackedLayoutBoxLinkedHashSet* positioned_descendants = PositionedObjects();
  if (!positioned_descendants)
    return;

  for (const auto& positioned_object : *positioned_descendants) {
    LayoutPositionedObject(positioned_object, relayout_children, info);
  }
}

void LayoutBlock::LayoutPositionedObject(LayoutBox* positioned_object,
                                         bool relayout_children,
                                         PositionedLayoutBehavior info) {
  NOT_DESTROYED();
  positioned_object->SetShouldCheckForPaintInvalidation();

  SubtreeLayoutScope layout_scope(*positioned_object);
  // If positionedObject is fixed-positioned and moves with an absolute-
  // positioned ancestor (other than the LayoutView, which cannot move),
  // mark it for layout now.
  MarkFixedPositionObjectForLayoutIfNeeded(positioned_object, layout_scope);
  if (info == kLayoutOnlyFixedPositionedObjects) {
    positioned_object->LayoutIfNeeded();
    return;
  }

  if (!positioned_object->NormalChildNeedsLayout()) {
    bool update_child_needs_layout =
        relayout_children || height_available_to_children_changed_;
    if (!update_child_needs_layout) {
      if (!positioned_object->IsLayoutNGObject() ||
          To<LayoutBlock>(positioned_object)
              ->IsLegacyInitiatedOutOfFlowLayout()) {
        update_child_needs_layout |=
            NeedsLayoutDueToStaticPosition(positioned_object);
      }
    }
    if (update_child_needs_layout)
      layout_scope.SetChildNeedsLayout(positioned_object);
  }

  LayoutUnit logical_top_estimate;
  bool is_paginated = View()->GetLayoutState()->IsPaginated();
  bool needs_block_direction_location_set_before_layout =
      is_paginated &&
      positioned_object->GetLegacyPaginationBreakability() != kForbidBreaks;
  bool bogus_logical_top_estimate = false;
  if (needs_block_direction_location_set_before_layout) {
    // Out-of-flow objects are normally positioned after layout (while in-flow
    // objects are positioned before layout). If the child object is paginated
    // in the same context as we are, estimate its logical top now. We need to
    // know this up-front, to correctly evaluate if we need to mark for
    // relayout, and, if our estimate is correct, we'll even be able to insert
    // correct pagination struts on the first attempt.
    const ComputedStyle& style = positioned_object->StyleRef();
    if (!style.LogicalBottom().IsAuto() && style.LogicalTop().IsAuto() &&
        style.LogicalHeight().IsAuto()) {
      // This child is bottom-aligned with auto block size. We cannot make a
      // decent estimate before layout. Just estimate something as far above a
      // fragmentainer break as possible. This is a way to try our best to avoid
      // hitting fragmentainer breaks, as that could impact the block size of
      // the child (increase it if contents need to be pushed to the next
      // fragmentainer, or decrease it if a descendant margin collides into a
      // fragmentainer boundary), and thus give us a bad block-start offset.
      logical_top_estimate = -OffsetFromLogicalTopOfFirstPage();
      bogus_logical_top_estimate = true;
    } else {
      LogicalExtentComputedValues computed_values;
      positioned_object->ComputeLogicalHeight(
          positioned_object->LogicalHeight(), positioned_object->LogicalTop(),
          computed_values);
      logical_top_estimate = computed_values.position_;
    }
    positioned_object->SetLogicalTop(logical_top_estimate);
  }

  if (!positioned_object->NeedsLayout()) {
    MarkChildForPaginationRelayoutIfNeeded(*positioned_object, layout_scope);
    // If we're not able to set a decent block start estimate, we need to force
    // layout to figure it out.
    if (bogus_logical_top_estimate)
      layout_scope.SetChildNeedsLayout(positioned_object);
  }

  // FIXME: We should be able to do a r->setNeedsPositionedMovementLayout()
  // here instead of a full layout. Need to investigate why it does not
  // trigger the correct invalidations in that case. crbug.com/350756
  if (info == kForcedLayoutAfterContainingBlockMoved) {
    positioned_object->SetNeedsLayout(
        layout_invalidation_reason::kAncestorMoved, kMarkOnlyThis);
  }

  if (positioned_object->NeedsLayout())
    positioned_object->UpdateLayout();

  LayoutObject* parent = positioned_object->Parent();
  bool layout_changed = false;
  if ((parent->IsLayoutNGFlexibleBox() &&
       !positioned_object->IsLayoutNGObject() &&
       LayoutFlexibleBox::SetStaticPositionForChildInFlexNGContainer(
           *positioned_object, To<LayoutBlock>(parent))) ||
      (parent->IsFlexibleBox() &&
       To<LayoutFlexibleBox>(parent)->SetStaticPositionForPositionedLayout(
           *positioned_object))) {
    // The static position of an abspos child of a flexbox depends on its size
    // (for example, they can be centered). So we may have to reposition the
    // item after layout.
    // TODO(cbiesinger): We could probably avoid a layout here and just
    // reposition?
    positioned_object->ForceLayout();
    layout_changed = true;
  }

  // Lay out again if our estimate was wrong.
  if (!layout_changed && needs_block_direction_location_set_before_layout &&
      logical_top_estimate != LogicalTopForChild(*positioned_object)) {
    positioned_object->ForceLayout();
  }

  if (is_paginated)
    UpdateFragmentationInfoForChild(*positioned_object);
}

void LayoutBlock::MarkPositionedObjectsForLayout() {
  NOT_DESTROYED();
  if (TrackedLayoutBoxLinkedHashSet* positioned_descendants =
          PositionedObjects()) {
    for (const auto& descendant : *positioned_descendants)
      descendant->SetChildNeedsLayout();
  }
}

void LayoutBlock::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  BlockPainter(*this).Paint(paint_info);
}

void LayoutBlock::PaintChildren(const PaintInfo& paint_info,
                                const PhysicalOffset&) const {
  NOT_DESTROYED();
  BlockPainter(*this).PaintChildren(paint_info);
}

void LayoutBlock::PaintObject(const PaintInfo& paint_info,
                              const PhysicalOffset& paint_offset) const {
  NOT_DESTROYED();
  BlockPainter(*this).PaintObject(paint_info, paint_offset);
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
  return Parent() && Parent()->IsLayoutNGFieldset() && IsAnonymous();
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

void LayoutBlock::AddPercentHeightDescendant(LayoutBox* descendant) {
  NOT_DESTROYED();
  // A replaced object is incapable of properly acting as a containing block for
  // its children. This is an issue with VIDEO elements, for instance, which
  // insert some percentage height flexbox children. It is also very easily
  // achievable with a foreignObject inside an SVG. Detect this situation and
  // bail. The assumption is that there is no situation where we require quirky
  // percentage height behavior inside replaced content.
  if (UNLIKELY(descendant->Container()->IsLayoutReplaced()))
    return;

  if (descendant->PercentHeightContainer()) {
    if (descendant->PercentHeightContainer() == this) {
      DCHECK(HasPercentHeightDescendant(descendant));
      return;
    }
    descendant->RemoveFromPercentHeightContainer();
  }
  descendant->SetPercentHeightContainer(this);

  // Mark our containing block chain as potentially having a percent height
  // descendant.
  LayoutBlock* cb = descendant->ContainingBlock();
  while (cb) {
    cb->SetMaybeHasPercentHeightDescendant();
    if (cb == this)
      break;
    cb = cb->ContainingBlock();
  }

  auto it = GetPercentHeightDescendantsMap().find(this);
  TrackedLayoutBoxLinkedHashSet* descendant_set =
      it != GetPercentHeightDescendantsMap().end() ? &*it->value : nullptr;
  if (!descendant_set) {
    descendant_set = MakeGarbageCollected<TrackedLayoutBoxLinkedHashSet>();
    GetPercentHeightDescendantsMap().Set(this, descendant_set);
  }
  descendant_set->insert(descendant);

  has_percent_height_descendants_ = true;
}

void LayoutBlock::RemovePercentHeightDescendant(LayoutBox* descendant) {
  NOT_DESTROYED();
  if (TrackedLayoutBoxLinkedHashSet* descendants = PercentHeightDescendants()) {
    descendants->erase(descendant);
    descendant->SetPercentHeightContainer(nullptr);
    if (descendants->empty()) {
      GetPercentHeightDescendantsMap().erase(this);
      has_percent_height_descendants_ = false;
    }
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

TrackedLayoutBoxLinkedHashSet* LayoutBlock::PercentHeightDescendantsInternal()
    const {
  NOT_DESTROYED();
  auto it = GetPercentHeightDescendantsMap().find(this);
  return it != GetPercentHeightDescendantsMap().end() ? &*it->value : nullptr;
}

void LayoutBlock::DirtyForLayoutFromPercentageHeightDescendants(
    SubtreeLayoutScope& layout_scope) {
  NOT_DESTROYED();
  TrackedLayoutBoxLinkedHashSet* descendants = PercentHeightDescendants();
  if (!descendants)
    return;

  for (LayoutBox* box : *descendants) {
    DCHECK(box->IsDescendantOf(this));
    while (box != this) {
      if (box->NormalChildNeedsLayout())
        break;
      layout_scope.SetChildNeedsLayout(box);
      box = box->ContainingBlock();
      DCHECK(box);
      if (!box)
        break;
    }
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

Position LayoutBlock::PositionForBox(InlineBox* box, bool start) const {
  NOT_DESTROYED();
  if (!box)
    return Position();

  Node* const node = box->GetLineLayoutItem().NonPseudoNode();
  if (!node) {
    if (!NonPseudoNode())
      return Position();
    if (start)
      return Position::FirstPositionInOrBeforeNode(*NonPseudoNode());
    return Position::LastPositionInOrAfterNode(*NonPseudoNode());
  }

  if (!box->IsInlineTextBox()) {
    if (start)
      return Position::FirstPositionInOrBeforeNode(*node);
    return Position::LastPositionInOrAfterNode(*node);
  }

  auto* text_box = To<InlineTextBox>(box);
  return Position::EditingPositionOf(
      box->GetLineLayoutItem().NonPseudoNode(),
      start ? text_box->Start() : text_box->Start() + text_box->Len());
}

static inline bool IsEditingBoundary(const LayoutObject* ancestor,
                                     LineLayoutBox child) {
  DCHECK(!ancestor || ancestor->NonPseudoNode());
  DCHECK(child);
  DCHECK(child.NonPseudoNode());
  return !ancestor || !ancestor->Parent() ||
         (ancestor->HasLayer() && IsA<LayoutView>(ancestor->Parent())) ||
         IsEditable(*ancestor->NonPseudoNode()) ==
             IsEditable(*child.NonPseudoNode());
}

// FIXME: This function should go on LayoutObject.
// Then all cases in which positionForPoint recurs could call this instead to
// prevent crossing editable boundaries. This would require many tests.
PositionWithAffinity LayoutBlock::PositionForPointRespectingEditingBoundaries(
    LineLayoutBox child,
    const PhysicalOffset& point_in_parent_coordinates) const {
  NOT_DESTROYED();
  PhysicalOffset child_location = child.PhysicalLocation();
  if (child.IsInFlowPositioned())
    child_location += child.OffsetForInFlowPosition();

  PhysicalOffset point_in_child_coordinates =
      point_in_parent_coordinates - child_location;

  // If this is an anonymous layoutObject, we just recur normally
  const Node* child_node = child.NonPseudoNode();
  if (!child_node)
    return child.PositionForPoint(point_in_child_coordinates);

  // Otherwise, first make sure that the editability of the parent and child
  // agree. If they don't agree, then we return a visible position just before
  // or after the child
  const LayoutObject* ancestor = this;
  while (ancestor && !ancestor->NonPseudoNode())
    ancestor = ancestor->Parent();

  // If we can't find an ancestor to check editability on, or editability is
  // unchanged, we recur like normal
  if (IsEditingBoundary(ancestor, child))
    return child.PositionForPoint(point_in_child_coordinates);

  // Otherwise return before or after the child, depending on if the click was
  // to the logical left or logical right of the child
  LayoutUnit child_middle = LogicalWidthForChildSize(child.Size()) / 2;
  LayoutUnit logical_left = IsHorizontalWritingMode()
                                ? point_in_child_coordinates.left
                                : point_in_child_coordinates.top;
  if (logical_left < child_middle) {
    if (IsUserSelectContain(*ancestor->NonPseudoNode()))
      return ancestor->PositionBeforeThis();
    return child.PositionBeforeThis();
  }
  if (IsUserSelectContain(*ancestor->NonPseudoNode()))
    return ancestor->PositionAfterThis();
  return child.PositionAfterThis();
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

static inline bool IsChildHitTestCandidate(LayoutBox* box) {
  return box->Size().Height() &&
         box->StyleRef().Visibility() == EVisibility::kVisible &&
         !box->IsFloatingOrOutOfFlowPositioned() && !box->IsLayoutFlowThread();
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

  if (IsLayoutNGObject() && PhysicalFragmentCount())
    return PositionForPointInFragments(point);

  if (IsTable())
    return LayoutBox::PositionForPoint(point);

  PhysicalOffset point_in_contents = point;
  OffsetForContents(point_in_contents);
  LayoutPoint point_in_logical_contents = FlipForWritingMode(point_in_contents);
  if (!IsHorizontalWritingMode())
    point_in_logical_contents = point_in_logical_contents.TransposedPoint();

  DCHECK(!ChildrenInline());

  LayoutBox* last_candidate_box = LastChildBox();
  while (last_candidate_box && !IsChildHitTestCandidate(last_candidate_box))
    last_candidate_box = last_candidate_box->PreviousSiblingBox();

  bool blocks_are_flipped = StyleRef().IsFlippedBlocksWritingMode();
  if (last_candidate_box) {
    if (point_in_logical_contents.Y() >
            LogicalTopForChild(*last_candidate_box) ||
        (!blocks_are_flipped && point_in_logical_contents.Y() ==
                                    LogicalTopForChild(*last_candidate_box)))
      return PositionForPointRespectingEditingBoundaries(
          LineLayoutBox(last_candidate_box), point_in_contents);

    for (LayoutBox* child_box = FirstChildBox(); child_box;
         child_box = child_box->NextSiblingBox()) {
      if (!IsChildHitTestCandidate(child_box))
        continue;
      LayoutUnit child_logical_bottom =
          LogicalTopForChild(*child_box) + LogicalHeightForChild(*child_box);
      // We hit child if our click is above the bottom of its padding box (like
      // IE6/7 and FF3).
      if (point_in_logical_contents.Y() < child_logical_bottom ||
          (blocks_are_flipped &&
           point_in_logical_contents.Y() == child_logical_bottom)) {
        return PositionForPointRespectingEditingBoundaries(
            LineLayoutBox(child_box), point_in_contents);
      }
    }
  }

  // We only get here if there are no hit test candidate children below the
  // click.
  return LayoutBox::PositionForPoint(point);
}

void LayoutBlock::OffsetForContents(PhysicalOffset& offset) const {
  NOT_DESTROYED();
  if (IsScrollContainer())
    offset += PhysicalOffset(PixelSnappedScrolledContentOffset());
}

void LayoutBlock::ScrollbarsChanged(bool horizontal_scrollbar_changed,
                                    bool vertical_scrollbar_changed,
                                    ScrollbarChangeContext context) {
  NOT_DESTROYED();
  width_available_to_children_changed_ |= vertical_scrollbar_changed;
  height_available_to_children_changed_ |= horizontal_scrollbar_changed;
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
      if (IsTextAreaIncludingNG())
        sizes -= scrollbar_thickness;
      if (!StyleRef().LogicalWidth().IsPercentOrCalc())
        sizes.min_size = sizes.max_size;
      return sizes;
    }
    if (ShouldApplySizeContainment())
      return sizes;
  }

  MinMaxSizes child_sizes;
  if (ChildrenInline()) {
    // FIXME: Remove this const_cast.
    To<LayoutBlockFlow>(const_cast<LayoutBlock*>(this))
        ->ComputeInlinePreferredLogicalWidths(child_sizes.min_size,
                                              child_sizes.max_size);
  } else {
    ComputeBlockPreferredLogicalWidths(child_sizes.min_size,
                                       child_sizes.max_size);
  }

  child_sizes.max_size = std::max(child_sizes.min_size, child_sizes.max_size);

  auto* html_marquee_element = DynamicTo<HTMLMarqueeElement>(GetNode());
  if (html_marquee_element && html_marquee_element->IsHorizontal())
    child_sizes.min_size = LayoutUnit();
  if (UNLIKELY(IsListBox(this) && StyleRef().LogicalWidth().IsPercentOrCalc()))
    child_sizes.min_size = LayoutUnit();

  if (IsTableCellLegacy()) {
    Length table_cell_width =
        To<LayoutTableCell>(this)->StyleOrColLogicalWidth();
    if (table_cell_width.IsFixed() && table_cell_width.Value() > 0) {
      child_sizes.max_size = std::max(
          child_sizes.min_size, AdjustContentBoxLogicalWidthForBoxSizing(
                                    LayoutUnit(table_cell_width.Value())));
    }
  }

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
      style_to_use.LogicalWidth().Value() >= 0 &&
      !(IsFlexItemCommon() && Parent()->StyleRef().IsDeprecatedWebkitBox() &&
        !style_to_use.LogicalWidth().IntValue())) {
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

LayoutUnit LayoutBlock::BeforeMarginInLineDirection(
    LineDirectionMode direction) const {
  NOT_DESTROYED();
  // InlineFlowBox::placeBoxesInBlockDirection will flip lines in
  // case of verticalLR mode, so we can assume verticalRL for now.
  return direction == kHorizontalLine ? MarginTop() : MarginRight();
}

LayoutUnit LayoutBlock::BaselinePosition(
    FontBaseline baseline_type,
    bool first_line,
    LineDirectionMode direction,
    LinePositionMode line_position_mode) const {
  NOT_DESTROYED();
  // Inline blocks are replaced elements. Otherwise, just pass off to
  // the base class.  If we're being queried as though we're the root line
  // box, then the fact that we're an inline-block is irrelevant, and we behave
  // just like a block.
  if (IsInline() && line_position_mode == kPositionOnContainingLine) {
    // For checkbox and radio controls, we always use the border edge instead
    // of the margin edge.
    // FIXME: Might be better to have a custom CSS property instead, so that if
    //        the theme is turned off, checkboxes/radios will still have decent
    //        baselines.
    // FIXME: Need to patch form controls to deal with vertical lines.
    if (StyleRef().IsCheckboxOrRadioPart())
      return MarginTop() + Size().Height();

    LayoutUnit baseline_pos = (IsWritingModeRoot() && !IsRubyRun())
                                  ? LayoutUnit(-1)
                                  : InlineBlockBaseline(direction);

    if (IsDeprecatedFlexibleBox()) {
      // Historically, we did this check for all baselines. But we can't
      // remove this code from deprecated flexbox, because it effectively
      // breaks -webkit-line-clamp, which is used in the wild -- we would
      // calculate the baseline as if -webkit-line-clamp wasn't used.
      // For simplicity, we use this for all uses of deprecated flexbox.
      LayoutUnit bottom_of_content =
          direction == kHorizontalLine
              ? Size().Height() - BorderBottom() - PaddingBottom() -
                    ComputeScrollbars().bottom
              : Size().Width() - BorderLeft() - PaddingLeft() -
                    ComputeScrollbars().left;
      if (baseline_pos > bottom_of_content)
        baseline_pos = LayoutUnit(-1);
    }
    if (baseline_pos != -1)
      return BeforeMarginInLineDirection(direction) + baseline_pos;

    return LayoutBox::BaselinePosition(baseline_type, first_line, direction,
                                       line_position_mode);
  }

  // If we're not replaced, we'll only get called with
  // PositionOfInteriorLineBoxes.
  // Note that inline-block counts as replaced here.
  DCHECK_EQ(line_position_mode, kPositionOfInteriorLineBoxes);

  const SimpleFontData* font_data = Style(first_line)->GetFont().PrimaryFont();
  DCHECK(font_data);
  if (!font_data)
    return LayoutUnit(-1);

  const FontMetrics& font_metrics = font_data->GetFontMetrics();
  return LayoutUnit((font_metrics.Ascent(baseline_type) +
                     (LineHeight(first_line, direction, line_position_mode) -
                      font_metrics.Height()) /
                         2)
                        .ToInt());
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

// TODO(mstensho): Figure out if all of this baseline code is needed here, or if
// it should be moved down to LayoutBlockFlow. LayoutDeprecatedFlexibleBox and
// LayoutGrid lack baseline calculation overrides, so the code is here just for
// them. Just walking the block children in logical order seems rather wrong for
// those two layout modes, though.

absl::optional<LayoutUnit> LayoutBlock::FirstLineBoxBaselineOverride() const {
  NOT_DESTROYED();
  if (ShouldApplyLayoutContainment())
    return LayoutUnit(-1);

  // Orthogonal grid items can participate in baseline alignment along column
  // axis.
  if (IsWritingModeRoot() && !IsRubyRun() && !IsGridItem())
    return LayoutUnit(-1);

  return absl::nullopt;
}

LayoutUnit LayoutBlock::FirstLineBoxBaseline() const {
  NOT_DESTROYED();
  DCHECK(!ChildrenInline());
  if (const absl::optional<LayoutUnit> baseline =
          FirstLineBoxBaselineOverride())
    return *baseline;

  for (LayoutBox* curr = FirstChildBox(); curr; curr = curr->NextSiblingBox()) {
    if (!curr->IsFloatingOrOutOfFlowPositioned()) {
      LayoutUnit result = curr->FirstLineBoxBaseline();
      if (result != -1) {
        // Translate to our coordinate space.
        return curr->LogicalTop() + result;
      }
    }
  }
  return LayoutUnit(-1);
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

absl::optional<LayoutUnit> LayoutBlock::InlineBlockBaselineOverride(
    LineDirectionMode line_direction) const {
  NOT_DESTROYED();
  if (UseLogicalBottomMarginEdgeForInlineBlockBaseline()) {
    // We are not calling LayoutBox::baselinePosition here because the caller
    // should add the margin-top/margin-right, not us.
    return line_direction == kHorizontalLine ? Size().Height() + MarginBottom()
                                             : Size().Width() + MarginLeft();
  }

  if (IsWritingModeRoot() && !IsRubyRun())
    return LayoutUnit(-1);

  return absl::nullopt;
}

LayoutUnit LayoutBlock::InlineBlockBaseline(
    LineDirectionMode line_direction) const {
  NOT_DESTROYED();
  DCHECK(!ChildrenInline());
  if (const absl::optional<LayoutUnit> baseline =
          InlineBlockBaselineOverride(line_direction))
    return *baseline;

  bool have_normal_flow_child = false;
  for (LayoutBox* curr = LastChildBox(); curr;
       curr = curr->PreviousSiblingBox()) {
    if (!curr->IsFloatingOrOutOfFlowPositioned()) {
      have_normal_flow_child = true;
      LayoutUnit result = curr->InlineBlockBaseline(line_direction);
      if (result != -1) {
        // Translate to our coordinate space.
        return curr->LogicalTop() + result;
      }
    }
  }
  if (!have_normal_flow_child)
    return EmptyLineBaseline(line_direction);
  return LayoutUnit(-1);
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
    const InlineBox* inline_box,
    int caret_offset,
    LayoutUnit* extra_width_to_end_of_line) const {
  NOT_DESTROYED();
  // Do the normal calculation in most cases.
  if ((FirstChild() && !FirstChild()->IsPseudoElement()) ||
      IsInlineBoxWrapperActuallyChild())
    return LayoutBox::LocalCaretRect(inline_box, caret_offset,
                                     extra_width_to_end_of_line);

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

void LayoutBlock::PaginatedContentWasLaidOut(
    LayoutUnit logical_bottom_offset_after_pagination) {
  NOT_DESTROYED();
  if (LayoutFlowThread* flow_thread = FlowThreadContainingBlock())
    flow_thread->ContentWasLaidOut(OffsetFromLogicalTopOfFirstPage() +
                                   logical_bottom_offset_after_pagination);
}

LayoutUnit LayoutBlock::CollapsedMarginBeforeForChild(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  // If the child has the same directionality as we do, then we can just return
  // its collapsed margin.
  if (!child.IsWritingModeRoot())
    return child.CollapsedMarginBefore();

  // The child has a different directionality.  If the child is parallel, then
  // it's just flipped relative to us.  We can use the collapsed margin for the
  // opposite edge.
  if (child.IsHorizontalWritingMode() == IsHorizontalWritingMode())
    return child.CollapsedMarginAfter();

  // The child is perpendicular to us, which means its margins don't collapse
  // but are on the "logical left/right" sides of the child box. We can just
  // return the raw margin in this case.
  return MarginBeforeForChild(child);
}

LayoutUnit LayoutBlock::CollapsedMarginAfterForChild(
    const LayoutBox& child) const {
  NOT_DESTROYED();
  // If the child has the same directionality as we do, then we can just return
  // its collapsed margin.
  if (!child.IsWritingModeRoot())
    return child.CollapsedMarginAfter();

  // The child has a different directionality.  If the child is parallel, then
  // it's just flipped relative to us.  We can use the collapsed margin for the
  // opposite edge.
  if (child.IsHorizontalWritingMode() == IsHorizontalWritingMode())
    return child.CollapsedMarginBefore();

  // The child is perpendicular to us, which means its margins don't collapse
  // but are on the "logical left/right" side of the child box. We can just
  // return the raw margin in this case.
  return MarginAfterForChild(child);
}

bool LayoutBlock::HasMarginBeforeQuirk(const LayoutBox* child) const {
  NOT_DESTROYED();
  // If the child has the same directionality as we do, then we can just return
  // its margin quirk.
  auto* child_layout_block = DynamicTo<LayoutBlock>(child);
  if (!child->IsWritingModeRoot()) {
    return child_layout_block ? child_layout_block->HasMarginBeforeQuirk()
                              : child->StyleRef().HasMarginBeforeQuirk();
  }

  // The child has a different directionality. If the child is parallel, then
  // it's just flipped relative to us. We can use the opposite edge.
  if (child->IsHorizontalWritingMode() == IsHorizontalWritingMode()) {
    return child_layout_block ? child_layout_block->HasMarginAfterQuirk()
                              : child->StyleRef().HasMarginAfterQuirk();
  }

  // The child is perpendicular to us and box sides are never quirky in
  // html.css, and we don't really care about whether or not authors specified
  // quirky ems, since they're an implementation detail.
  return false;
}

bool LayoutBlock::HasMarginAfterQuirk(const LayoutBox* child) const {
  NOT_DESTROYED();
  // If the child has the same directionality as we do, then we can just return
  // its margin quirk.
  auto* child_layout_block = DynamicTo<LayoutBlock>(child);
  if (!child->IsWritingModeRoot()) {
    return child_layout_block ? child_layout_block->HasMarginAfterQuirk()
                              : child->StyleRef().HasMarginAfterQuirk();
  }

  // The child has a different directionality. If the child is parallel, then
  // it's just flipped relative to us. We can use the opposite edge.
  if (child->IsHorizontalWritingMode() == IsHorizontalWritingMode()) {
    return child_layout_block ? child_layout_block->HasMarginBeforeQuirk()
                              : child->StyleRef().HasMarginBeforeQuirk();
  }

  // The child is perpendicular to us and box sides are never quirky in
  // html.css, and we don't really care about whether or not authors specified
  // quirky ems, since they're an implementation detail.
  return false;
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

  LegacyLayout legacy =
      parent->ForceLegacyLayout() ? LegacyLayout::kForce : LegacyLayout::kAuto;

  parent->UpdateAnonymousChildStyle(nullptr, new_style_builder);
  scoped_refptr<const ComputedStyle> new_style = new_style_builder.TakeStyle();

  LayoutBlock* layout_block;
  if (new_display == EDisplay::kFlex) {
    layout_block = LayoutObjectFactory::CreateFlexibleBox(parent->GetDocument(),
                                                          *new_style, legacy);
  } else if (new_display == EDisplay::kGrid) {
    layout_block = LayoutObjectFactory::CreateGrid(parent->GetDocument(),
                                                   *new_style, legacy);
  } else if (new_display == EDisplay::kBlockMath) {
    layout_block = LayoutObjectFactory::CreateMath(parent->GetDocument(),
                                                   *new_style, legacy);
  } else {
    DCHECK(new_display == EDisplay::kBlock ||
           new_display == EDisplay::kFlowRoot);
    layout_block = LayoutObjectFactory::CreateBlockFlow(parent->GetDocument(),
                                                        *new_style, legacy);
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

  if (ChildrenInline()) {
    SECURITY_DCHECK(IsLayoutBlockFlow());
    result.Unite(
        To<LayoutBlockFlow>(this)->RecalcInlineChildrenLayoutOverflow());
  } else {
    for (LayoutBox* box = FirstChildBox(); box; box = box->NextSiblingBox()) {
      if (box->IsOutOfFlowPositioned())
        continue;

      result.Unite(box->RecalcLayoutOverflow());
    }
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

// Called when a positioned object moves but doesn't necessarily change size.
// A simplified layout is attempted that just updates the object's position.
// If the size does change, the object remains dirty.
bool LayoutBlock::TryLayoutDoingPositionedMovementOnly() {
  NOT_DESTROYED();
  LayoutUnit old_width = LogicalWidth();
  LogicalExtentComputedValues computed_values;
  LogicalExtentAfterUpdatingLogicalWidth(LogicalTop(), computed_values);
  // If we shrink to fit our width may have changed, so we still need full
  // layout.
  if (old_width != computed_values.extent_)
    return false;
  SetLogicalWidth(computed_values.extent_);
  SetLogicalLeft(computed_values.position_);
  SetMarginStart(computed_values.margins_.start_);
  SetMarginEnd(computed_values.margins_.end_);

  LayoutUnit old_height = LogicalHeight();
  LayoutUnit old_intrinsic_content_logical_height =
      IntrinsicContentLogicalHeight();

  SetIntrinsicContentLogicalHeight(ContentLogicalHeight());
  ComputeLogicalHeight(old_height, LogicalTop(), computed_values);

  if (old_height != computed_values.extent_ &&
      (HasPercentHeightDescendants() || IsFlexibleBoxIncludingNG())) {
    SetIntrinsicContentLogicalHeight(old_intrinsic_content_logical_height);
    return false;
  }

  SetLogicalHeight(computed_values.extent_);
  SetLogicalTop(computed_values.position_);
  SetMarginBefore(computed_values.margins_.before_);
  SetMarginAfter(computed_values.margins_.after_);

  return true;
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
  if (IsFlexItem()) {
    const auto* flex_box = To<LayoutFlexibleBox>(Parent());
    if (flex_box->UseOverrideLogicalHeightForPerentageResolution(*this))
      stretched_flex_height = OverrideContentLogicalHeight();
  } else if (HasOverrideLogicalHeight() && IsOverrideLogicalHeightDefinite()) {
    stretched_flex_height = OverrideContentLogicalHeight();
  }
  if (stretched_flex_height != LayoutUnit(-1)) {
    available_height = stretched_flex_height;
  } else if (IsGridItem() && HasOverrideLogicalHeight()) {
    available_height = OverrideContentLogicalHeight();
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
