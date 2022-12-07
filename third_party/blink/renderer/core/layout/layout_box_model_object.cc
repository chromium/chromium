/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 *
 */

#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"

#include "cc/input/main_thread_scrolling_reason.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/layout/deferred_shaping.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_interface.h"
#include "third_party/blink/renderer/core/layout/ng/table/layout_ng_table_section_interface.h"
#include "third_party/blink/renderer/core/page/scrolling/sticky_position_scrolling_constraints.h"
#include "third_party/blink/renderer/core/paint/ng/ng_inline_paint_context.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {
inline bool IsOutOfFlowPositionedWithImplicitHeight(
    const LayoutBoxModelObject* child) {
  return child->IsOutOfFlowPositioned() &&
         !child->StyleRef().LogicalTop().IsAuto() &&
         !child->StyleRef().LogicalBottom().IsAuto();
}

// Inclusive of |from|, exclusive of |to|.
PaintLayer* FindFirstStickyBetween(LayoutObject* from, LayoutObject* to) {
  LayoutObject* maybe_sticky_ancestor = from;
  while (maybe_sticky_ancestor && maybe_sticky_ancestor != to) {
    if (maybe_sticky_ancestor->StyleRef().HasStickyConstrainedPosition()) {
      return To<LayoutBoxModelObject>(maybe_sticky_ancestor)->Layer();
    }

    // We use LocationContainer here to find the nearest sticky ancestor which
    // shifts the given element's position so that the sticky positioning code
    // is aware ancestor sticky position shifts.
    maybe_sticky_ancestor =
        maybe_sticky_ancestor->IsLayoutInline()
            ? maybe_sticky_ancestor->Container()
            : To<LayoutBox>(maybe_sticky_ancestor)->LocationContainer();
  }
  return nullptr;
}

void MarkBoxForRelayoutAfterSplit(LayoutBoxModelObject* box) {
  // FIXME: The table code should handle that automatically. If not,
  // we should fix it and remove the table part checks.
  if (box->IsTable()) {
    // Because we may have added some sections with already computed column
    // structures, we need to sync the table structure with them now. This
    // avoids crashes when adding new cells to the table.
    ToInterface<LayoutNGTableInterface>(box)->ForceSectionsRecalc();
  } else if (box->IsTableSection()) {
    ToInterface<LayoutNGTableSectionInterface>(box)->SetNeedsCellRecalc();
  }

  box->SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
      layout_invalidation_reason::kAnonymousBlockChange);
}

void CollapseLoneAnonymousBlockChild(LayoutBox* parent, LayoutObject* child) {
  auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
  auto* parent_block_flow = DynamicTo<LayoutBlockFlow>(parent);
  if (!child->IsAnonymousBlock() || !child_block_flow)
    return;
  if (!parent_block_flow)
    return;
  parent_block_flow->CollapseAnonymousBlockChild(child_block_flow);
}

}  // namespace

// The HashMap for storing continuation pointers.
// The continuation chain is a singly linked list. As such, the HashMap's value
// is the next pointer associated with the key.
typedef HeapHashMap<WeakMember<const LayoutBoxModelObject>,
                    Member<LayoutBoxModelObject>>
    ContinuationMap;
static ContinuationMap& GetContinuationMap() {
  DEFINE_STATIC_LOCAL(Persistent<ContinuationMap>, map,
                      (MakeGarbageCollected<ContinuationMap>()));
  return *map;
}

LayoutBoxModelObject::LayoutBoxModelObject(ContainerNode* node)
    : LayoutObject(node) {}

bool LayoutBoxModelObject::UsesCompositedScrolling() const {
  NOT_DESTROYED();

  const auto* properties = FirstFragment().PaintProperties();
  return properties && properties->ScrollTranslation() &&
         properties->ScrollTranslation()->HasDirectCompositingReasons();
}

LayoutBoxModelObject::~LayoutBoxModelObject() = default;

void LayoutBoxModelObject::WillBeDestroyed() {
  NOT_DESTROYED();
  // A continuation of this LayoutObject should be destroyed at subclasses.
  DCHECK(!Continuation());

  if (!DocumentBeingDestroyed()) {
    GetDocument()
        .GetFrame()
        ->GetInputMethodController()
        .LayoutObjectWillBeDestroyed(*this);
  }

  LayoutObject::WillBeDestroyed();

  if (HasLayer())
    DestroyLayer();

  // Our layer should have been destroyed and cleared by now
  DCHECK(!HasLayer());
  DCHECK(!Layer());
}

void LayoutBoxModelObject::StyleWillChange(StyleDifference diff,
                                           const ComputedStyle& new_style) {
  NOT_DESTROYED();
  // Change of stacked/stacking context status may cause change of this or
  // descendant PaintLayer's CompositingContainer, so we need to eagerly
  // invalidate the current compositing container chain which may have painted
  // cached subsequences containing this object or descendant objects.
  if (Style() &&
      (IsStacked() != IsStacked(new_style) ||
       IsStackingContext() != IsStackingContext(new_style)) &&
      // ObjectPaintInvalidator requires this.
      IsRooted()) {
    ObjectPaintInvalidator(*this).SlowSetPaintingLayerNeedsRepaint();
  }

  LayoutObject::StyleWillChange(diff, new_style);
}

DISABLE_CFI_PERF
void LayoutBoxModelObject::StyleDidChange(StyleDifference diff,
                                          const ComputedStyle* old_style) {
  NOT_DESTROYED();
  bool had_transform_related_property = HasTransformRelatedProperty();
  bool had_filter_inducing_property = HasFilterInducingProperty();
  bool had_non_initial_backdrop_filter = HasNonInitialBackdropFilter();
  bool had_layer = HasLayer();
  bool layer_was_self_painting = had_layer && Layer()->IsSelfPaintingLayer();
  bool was_horizontal_writing_mode = IsHorizontalWritingMode();
  bool could_contain_fixed = CanContainFixedPositionObjects();
  bool could_contain_absolute = CanContainAbsolutePositionObjects();

  LayoutObject::StyleDidChange(diff, old_style);
  UpdateFromStyle();

  // When an out-of-flow-positioned element changes its display between block
  // and inline-block, then an incremental layout on the element's containing
  // block lays out the element through LayoutPositionedObjects, which skips
  // laying out the element's parent.
  // The element's parent needs to relayout so that it calls LayoutBlockFlow::
  // setStaticInlinePositionForChild with the out-of-flow-positioned child, so
  // that when it's laid out, its LayoutBox::computePositionedLogicalWidth/
  // Height takes into account its new inline/block position rather than its old
  // block/inline position.
  // Position changes and other types of display changes are handled elsewhere.
  if (old_style && IsOutOfFlowPositioned() && Parent() &&
      (StyleRef().GetPosition() == old_style->GetPosition()) &&
      (StyleRef().IsOriginalDisplayInlineType() !=
       old_style->IsOriginalDisplayInlineType()))
    Parent()->SetNeedsLayout(layout_invalidation_reason::kChildChanged,
                             kMarkContainerChain);

  if (Layer() && old_style->HasStickyConstrainedPosition() &&
      !StyleRef().HasStickyConstrainedPosition()) {
    if (const auto* scroll_container =
            Layer()->ContainingScrollContainerLayer()) {
      scroll_container->GetScrollableArea()->InvalidateAllStickyConstraints();
    }
  }

  PaintLayerType type = LayerTypeRequired();
  if (type != kNoPaintLayer) {
    if (!Layer()) {
      // In order to update this object properly, we need to lay it out again.
      // However, if we have never laid it out, don't mark it for layout. If
      // this is a new object, it may not yet have been inserted into the tree,
      // and if we mark it for layout then, we risk upsetting the tree
      // insertion machinery.
      if (EverHadLayout())
        SetChildNeedsLayout();

      CreateLayerAfterStyleChange();
    }
  } else if (Layer() && Layer()->Parent()) {
    Layer()->UpdateFilters(old_style, StyleRef());
    Layer()->UpdateBackdropFilters(old_style, StyleRef());
    Layer()->UpdateClipPath(old_style, StyleRef());
    // Calls DestroyLayer() which clears the layer.
    Layer()->RemoveOnlyThisLayerAfterStyleChange(old_style);
    if (EverHadLayout())
      SetChildNeedsLayout();
    if (had_transform_related_property || had_filter_inducing_property ||
        had_non_initial_backdrop_filter) {
      SetNeedsLayoutAndIntrinsicWidthsRecalcAndFullPaintInvalidation(
          layout_invalidation_reason::kStyleChange);
    }
  }

  bool can_contain_fixed = CanContainFixedPositionObjects();
  bool can_contain_absolute = CanContainAbsolutePositionObjects();

  if (old_style && (could_contain_fixed != can_contain_fixed ||
                    could_contain_absolute != can_contain_absolute)) {
    // If out of flow element containment changed, then we need to force a
    // subtree paint property update, since the children elements may now be
    // referencing a different container.
    AddSubtreePaintPropertyUpdateReason(
        SubtreePaintPropertyUpdateReason::kContainerChainMayChange);
  } else if (had_layer == HasLayer() &&
             (had_transform_related_property != HasTransformRelatedProperty() ||
              had_filter_inducing_property != HasFilterInducingProperty() ||
              had_non_initial_backdrop_filter !=
                  HasNonInitialBackdropFilter())) {
    // This affects whether to create transform, filter, or effect nodes. Note
    // that if the HasLayer() value changed, then all of this was already set in
    // CreateLayerAfterStyleChange() or DestroyLayer().
    SetNeedsPaintPropertyUpdate();
  }

  if (old_style && Parent()) {
    LayoutBlock* block = FindNonAnonymousContainingBlock(this);

    if ((could_contain_fixed && !can_contain_fixed) ||
        (could_contain_absolute && !can_contain_absolute)) {
      // Clear our positioned objects list. Our absolute and fixed positioned
      // descendants will be inserted into our containing block's positioned
      // objects list during layout.
      block->RemovePositionedObjects(nullptr, kNewContainingBlock);
    }
    if (!could_contain_absolute && can_contain_absolute) {
      // Remove our absolute positioned descendants from their current
      // containing block.
      // They will be inserted into our positioned objects list during layout.
      if (LayoutBlock* cb = block->ContainingBlockForAbsolutePosition())
        cb->RemovePositionedObjects(this, kNewContainingBlock);
    }
    if (!could_contain_fixed && can_contain_fixed) {
      // Remove our fixed positioned descendants from their current containing
      // block.
      // They will be inserted into our positioned objects list during layout.
      if (LayoutBlock* cb = block->ContainingBlockForFixedPosition())
        cb->RemovePositionedObjects(this, kNewContainingBlock);
    }
  }

  if (Layer()) {
    // The previous CompositingContainer chain was marked for repaint via
    // |LayoutBoxModelObject::StyleWillChange| but changes to stacking can
    // change the compositing container so we need to ensure the new
    // CompositingContainer is also marked for repaint.
    if (old_style &&
        (IsStacked() != IsStacked(*old_style) ||
         IsStackingContext() != IsStackingContext(*old_style)) &&
        // ObjectPaintInvalidator requires this.
        IsRooted()) {
      ObjectPaintInvalidator(*this).SlowSetPaintingLayerNeedsRepaint();
    }

    Layer()->StyleDidChange(diff, old_style);
    if (had_layer && Layer()->IsSelfPaintingLayer() != layer_was_self_painting)
      SetChildNeedsLayout();
  }

  if (old_style && was_horizontal_writing_mode != IsHorizontalWritingMode()) {
    // Changing the getWritingMode() may change isOrthogonalWritingModeRoot()
    // of children. Make sure all children are marked/unmarked as orthogonal
    // writing-mode roots.
    bool new_horizontal_writing_mode = IsHorizontalWritingMode();
    for (LayoutObject* child = SlowFirstChild(); child;
         child = child->NextSibling()) {
      if (!child->IsBox())
        continue;
      if (new_horizontal_writing_mode != child->IsHorizontalWritingMode())
        To<LayoutBox>(child)->MarkOrthogonalWritingModeRoot();
      else
        To<LayoutBox>(child)->UnmarkOrthogonalWritingModeRoot();
    }
  }

  // The used style for body background may change due to computed style change
  // on the document element because of change of BackgroundTransfersToView()
  // which depends on the document element style.
  if (IsDocumentElement()) {
    if (HTMLBodyElement* body = GetDocument().FirstBodyElement()) {
      if (auto* body_object = body->GetLayoutObject()) {
        if (body_object->IsBoxModelObject()) {
          auto* body_box_model = To<LayoutBoxModelObject>(body_object);
          bool new_body_background_transfers =
              body_box_model->BackgroundTransfersToView(Style());
          bool old_body_background_transfers =
              old_style && body_box_model->BackgroundTransfersToView(old_style);
          if (new_body_background_transfers != old_body_background_transfers &&
              body_object->Style() && body_object->StyleRef().HasBackground())
            body_object->SetBackgroundNeedsFullPaintInvalidation();
        }
      }
    }
  }

  if (old_style &&
      old_style->BackfaceVisibility() != StyleRef().BackfaceVisibility()) {
    SetNeedsPaintPropertyUpdate();
  }

  // We can't squash across a layout containment boundary. So, if the
  // containment changes, we need to update the compositing inputs.
  if (old_style &&
      ShouldApplyLayoutContainment(*old_style) !=
          ShouldApplyLayoutContainment() &&
      Layer()) {
    Layer()->SetNeedsCompositingInputsUpdate();
  }

  if ((IsOutOfFlowPositioned() || IsRelPositioned()) && Parent())
    DisallowDeferredShapingIfNegativePositioned();

  if (Element* element = DynamicTo<Element>(GetNode())) {
    if (IsOutOfFlowPositioned() && StyleRef().AnchorScroll())
      element->EnsureAnchorScrollData();
    else
      element->RemoveAnchorScrollData();
  }
}

void LayoutBoxModelObject::InsertedIntoTree() {
  LayoutObject::InsertedIntoTree();
  if (IsOutOfFlowPositioned() || IsRelPositioned())
    DisallowDeferredShapingIfNegativePositioned();
}

void LayoutBoxModelObject::DisallowDeferredShapingIfNegativePositioned() const {
}

void LayoutBoxModelObject::CreateLayerAfterStyleChange() {
  NOT_DESTROYED();
  DCHECK(!HasLayer() && !Layer());
  GetMutableForPainting().FirstFragment().SetLayer(
      MakeGarbageCollected<PaintLayer>(this));
  SetHasLayer(true);
  Layer()->InsertOnlyThisLayerAfterStyleChange();
  // Creating a layer may affect existence of the LocalBorderBoxProperties, so
  // we need to ensure that we update paint properties.
  SetNeedsPaintPropertyUpdate();
}

void LayoutBoxModelObject::DestroyLayer() {
  NOT_DESTROYED();
  DCHECK(HasLayer() && Layer());
  SetHasLayer(false);
  GetMutableForPainting().FirstFragment().SetLayer(nullptr);
  // Removing a layer may affect existence of the LocalBorderBoxProperties, so
  // we need to ensure that we update paint properties.
  SetNeedsPaintPropertyUpdate();
  SetBackgroundPaintLocation(kBackgroundPaintInBorderBoxSpace);
}

bool LayoutBoxModelObject::HasSelfPaintingLayer() const {
  NOT_DESTROYED();
  return Layer() && Layer()->IsSelfPaintingLayer();
}

PaintLayerScrollableArea* LayoutBoxModelObject::GetScrollableArea() const {
  NOT_DESTROYED();
  return Layer() ? Layer()->GetScrollableArea() : nullptr;
}

void LayoutBoxModelObject::AddOutlineRectsForNormalChildren(
    Vector<PhysicalRect>& rects,
    const PhysicalOffset& additional_offset,
    NGOutlineType include_block_overflows) const {
  NOT_DESTROYED();
  for (LayoutObject* child = SlowFirstChild(); child;
       child = child->NextSibling()) {
    // Outlines of out-of-flow positioned descendants are handled in
    // LayoutBlock::AddOutlineRects().
    if (child->IsOutOfFlowPositioned())
      continue;

    // Outline of an element continuation or anonymous block continuation is
    // added when we iterate the continuation chain.
    // See LayoutBlock::AddOutlineRects() and LayoutInline::AddOutlineRects().
    auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
    if (child->IsElementContinuation() ||
        (child_block_flow && child_block_flow->IsAnonymousBlockContinuation()))
      continue;

    AddOutlineRectsForDescendant(*child, rects, additional_offset,
                                 include_block_overflows);
  }
}

void LayoutBoxModelObject::AddOutlineRectsForDescendant(
    const LayoutObject& descendant,
    Vector<PhysicalRect>& rects,
    const PhysicalOffset& additional_offset,
    NGOutlineType include_block_overflows) const {
  NOT_DESTROYED();
  if (descendant.IsText() || descendant.IsListMarkerForNormalContent())
    return;

  if (descendant.HasLayer()) {
    Vector<PhysicalRect> layer_outline_rects;
    descendant.AddOutlineRects(layer_outline_rects, nullptr, PhysicalOffset(),
                               include_block_overflows);
    descendant.LocalToAncestorRects(layer_outline_rects, this, PhysicalOffset(),
                                    additional_offset);
    rects.AppendVector(layer_outline_rects);
    return;
  }

  if (descendant.IsBox()) {
    descendant.AddOutlineRects(
        rects, nullptr,
        additional_offset + To<LayoutBox>(descendant).PhysicalLocation(),
        include_block_overflows);
    return;
  }

  if (descendant.IsLayoutInline()) {
    // As an optimization, an ancestor has added rects for its line boxes
    // covering descendants' line boxes, so descendants don't need to add line
    // boxes again. For example, if the parent is a LayoutBlock, it adds rects
    // for its RootOutlineBoxes which cover the line boxes of this LayoutInline.
    // So the LayoutInline needs to add rects for children and continuations
    // only.
    To<LayoutInline>(descendant)
        .AddOutlineRectsForChildrenAndContinuations(rects, additional_offset,
                                                    include_block_overflows);
    return;
  }

  descendant.AddOutlineRects(rects, nullptr, additional_offset,
                             include_block_overflows);
}

void LayoutBoxModelObject::RecalcVisualOverflow() {
  // |PaintLayer| calls this function when |HasSelfPaintingLayer|. When |this|
  // is an inline box or an atomic inline, its ink overflow is stored in
  // |NGFragmentItem| in the inline formatting context.
  if (IsInline() && IsInLayoutNGInlineFormattingContext()) {
    DCHECK(HasSelfPaintingLayer());
    NGInlineCursor cursor;
    NGInlinePaintContext inline_context;
    for (cursor.MoveTo(*this); cursor; cursor.MoveToNextForSameLayoutObject()) {
      NGInlinePaintContext::ScopedInlineBoxAncestors scoped_items(
          cursor, &inline_context);
      cursor.Current().RecalcInkOverflow(cursor, &inline_context);
    }
    return;
  }

  LayoutObject::RecalcVisualOverflow();
}

void LayoutBoxModelObject::AbsoluteQuadsForSelf(
    Vector<gfx::QuadF>& quads,
    MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  NOTREACHED();
}

void LayoutBoxModelObject::LocalQuadsForSelf(Vector<gfx::QuadF>& quads) const {
  NOT_DESTROYED();
  NOTREACHED();
}

void LayoutBoxModelObject::AbsoluteQuads(Vector<gfx::QuadF>& quads,
                                         MapCoordinatesFlags mode) const {
  QuadsInternal(quads, mode, true);
}

void LayoutBoxModelObject::LocalQuads(Vector<gfx::QuadF>& quads) const {
  QuadsInternal(quads, 0, false);
}

void LayoutBoxModelObject::QuadsInternal(Vector<gfx::QuadF>& quads,
                                         MapCoordinatesFlags mode,
                                         bool map_to_absolute) const {
  NOT_DESTROYED();
  if (map_to_absolute)
    AbsoluteQuadsForSelf(quads, mode);
  else
    LocalQuadsForSelf(quads);

  // Iterate over continuations, avoiding recursion in case there are
  // many of them. See crbug.com/653767.
  for (const LayoutBoxModelObject* continuation_object = Continuation();
       continuation_object;
       continuation_object = continuation_object->Continuation()) {
    auto* continuation_block_flow =
        DynamicTo<LayoutBlockFlow>(continuation_object);
    DCHECK(continuation_object->IsLayoutInline() ||
           (continuation_block_flow &&
            continuation_block_flow->IsAnonymousBlockContinuation()));
    if (map_to_absolute)
      continuation_object->AbsoluteQuadsForSelf(quads);
    else
      continuation_object->LocalQuadsForSelf(quads);
  }
}

gfx::RectF LayoutBoxModelObject::LocalBoundingBoxRectF() const {
  NOT_DESTROYED();
  Vector<gfx::QuadF> quads;
  LocalQuads(quads);

  wtf_size_t n = quads.size();
  if (n == 0)
    return gfx::RectF();

  gfx::RectF result = quads[0].BoundingBox();
  for (wtf_size_t i = 1; i < n; ++i)
    result.Union(quads[i].BoundingBox());
  return result;
}

void LayoutBoxModelObject::UpdateFromStyle() {
  NOT_DESTROYED();
  const ComputedStyle& style_to_use = StyleRef();
  SetHasBoxDecorationBackground(style_to_use.HasBoxDecorationBackground());
  SetInline(style_to_use.IsDisplayInlineType());
  SetPositionState(style_to_use.GetPosition());
  SetHorizontalWritingMode(style_to_use.IsHorizontalWritingMode());
  SetCanContainAbsolutePositionObjects(
      ComputeIsAbsoluteContainer(&style_to_use));
  SetCanContainFixedPositionObjects(ComputeIsFixedContainer(&style_to_use));
}

PhysicalRect LayoutBoxModelObject::PhysicalVisualOverflowRectIncludingFilters()
    const {
  NOT_DESTROYED();
  PhysicalRect bounds_rect = PhysicalVisualOverflowRect();
  if (!StyleRef().HasFilter())
    return bounds_rect;
  gfx::RectF float_rect(bounds_rect);
  gfx::RectF filter_reference_box = Layer()->FilterReferenceBox();
  if (!filter_reference_box.size().IsZero())
    float_rect.UnionEvenIfEmpty(filter_reference_box);
  float_rect = Layer()->MapRectForFilter(float_rect);
  return PhysicalRect::EnclosingRect(float_rect);
}

LayoutBlock* LayoutBoxModelObject::ContainingBlockForAutoHeightDetection(
    const Length& logical_height) const {
  NOT_DESTROYED();
  // For percentage heights: The percentage is calculated with respect to the
  // height of the generated box's containing block. If the height of the
  // containing block is not specified explicitly (i.e., it depends on content
  // height), and this element is not absolutely positioned, the used height is
  // calculated as if 'auto' was specified.
  if (!logical_height.IsPercentOrCalc() || IsOutOfFlowPositioned())
    return nullptr;

  // Anonymous block boxes are ignored when resolving percentage values that
  // would refer to it: the closest non-anonymous ancestor box is used instead.
  LayoutBlock* cb = ContainingBlock();
  while (cb->IsAnonymous())
    cb = cb->ContainingBlock();

  // Matching LayoutBox::percentageLogicalHeightIsResolvableFromBlock() by
  // ignoring table cell's attribute value, where it says that table cells
  // violate what the CSS spec says to do with heights. Basically we don't care
  // if the cell specified a height or not.
  if (cb->IsTableCell())
    return nullptr;

  // Match LayoutBox::availableLogicalHeightUsing by special casing the layout
  // view. The available height is taken from the frame.
  if (IsA<LayoutView>(cb))
    return nullptr;

  if (IsOutOfFlowPositionedWithImplicitHeight(cb))
    return nullptr;

  return cb;
}

bool LayoutBoxModelObject::HasAutoHeightOrContainingBlockWithAutoHeight(
    RegisterPercentageDescendant register_percentage_descendant) const {
  NOT_DESTROYED();
  // TODO(rego): Check if we can somehow reuse LayoutBlock::
  // availableLogicalHeightForPercentageComputation() (see crbug.com/635655).
  const auto* this_box = DynamicTo<LayoutBox>(this);
  const Length& logical_height_length = StyleRef().LogicalHeight();
  LayoutBlock* cb =
      ContainingBlockForAutoHeightDetection(logical_height_length);
  if (register_percentage_descendant == kRegisterPercentageDescendant &&
      logical_height_length.IsPercentOrCalc() && cb && IsBox()) {
    cb->AddPercentHeightDescendant(const_cast<LayoutBox*>(To<LayoutBox>(this)));
  }
  if (this_box && this_box->IsFlexItemIncludingNG()) {
    if (this_box->IsFlexItem()) {
      const auto& flex_box = To<LayoutFlexibleBox>(*Parent());
      if (flex_box.UseOverrideLogicalHeightForPerentageResolution(*this_box))
        return false;
    } else if (const NGLayoutResult* result =
                   this_box->GetSingleCachedLayoutResult()) {
      // TODO(dgrogan): We won't get here when laying out the FlexNG item and
      // its descendant(s) for the first time because the item (|this_box|)
      // doesn't have anything in its cache. That seems bad because this method
      // returns true even when the item has a fixed definite height. There
      // doesn't seem to be an easy way to check the flex item's definiteness
      // here because the flex item's LayoutObject doesn't have a
      // BoxLayoutExtraInput that we could add a flag to.
      const NGConstraintSpace& space = result->GetConstraintSpaceForCaching();
      if (space.IsFixedBlockSize() && !space.IsInitialBlockSizeIndefinite())
        return false;
    }
  }
  if (this_box && this_box->IsGridItem() &&
      this_box->HasOverrideContainingBlockContentLogicalHeight()) {
    return this_box->OverrideContainingBlockContentLogicalHeight() ==
           kIndefiniteSize;
  }
  if (this_box && this_box->IsCustomItem() &&
      (this_box->HasOverrideContainingBlockContentLogicalHeight() ||
       this_box->HasOverridePercentageResolutionBlockSize()))
    return false;

  if ((logical_height_length.IsAutoOrContentOrIntrinsic() ||
       logical_height_length.IsFillAvailable()) &&
      !IsOutOfFlowPositionedWithImplicitHeight(this))
    return true;

  if (cb) {
    // We need the containing block to have a definite block-size in order to
    // resolve the block-size of the descendant, except when in quirks mode.
    // Flexboxes follow strict behavior even in quirks mode, though.
    if (!GetDocument().InQuirksMode() ||
        cb->IsFlexibleBoxIncludingDeprecatedAndNG()) {
      if (this_box &&
          this_box->HasOverrideContainingBlockContentLogicalHeight()) {
        return this_box->OverrideContainingBlockContentLogicalHeight() ==
               LayoutUnit(-1);
      } else if (this_box && this_box->GetSingleCachedLayoutResult() &&
                 !this_box->GetBoxLayoutExtraInput()) {
        return this_box->GetSingleCachedLayoutResult()
                   ->GetConstraintSpaceForCaching()
                   .AvailableSize()
                   .block_size == LayoutUnit(-1);
      }
      return !cb->HasDefiniteLogicalHeight();
    }
  }

  return false;
}

PhysicalOffset LayoutBoxModelObject::RelativePositionOffset() const {
  NOT_DESTROYED();
  DCHECK(IsRelPositioned());
  LayoutBlock* containing_block = ContainingBlock();

  // If this object was placed by LayoutNG it's offset already includes the
  // relative adjustment.
  if (IsLayoutNGContainingBlock(containing_block))
    return PhysicalOffset();

  PhysicalOffset offset = AccumulateRelativePositionOffsets();

  // Objects that shrink to avoid floats normally use available line width when
  // computing containing block width. However in the case of relative
  // positioning using percentages, we can't do this. The offset should always
  // be resolved using the available width of the containing block. Therefore we
  // don't use containingBlockLogicalWidthForContent() here, but instead
  // explicitly call availableWidth on our containing block.
  // https://drafts.csswg.org/css-position-3/#rel-pos
  // However for grid items the containing block is the grid area, so offsets
  // should be resolved against that:
  // https://drafts.csswg.org/css-grid/#grid-item-sizing
  absl::optional<LayoutUnit> left;
  absl::optional<LayoutUnit> right;
  if (!StyleRef().Left().IsAuto() || !StyleRef().Right().IsAuto()) {
    LayoutUnit available_width = HasOverrideContainingBlockContentWidth()
                                     ? OverrideContainingBlockContentWidth()
                                     : containing_block->AvailableWidth();
    if (!StyleRef().Left().IsAuto())
      left = ValueForLength(StyleRef().Left(), available_width);
    if (!StyleRef().Right().IsAuto())
      right = ValueForLength(StyleRef().Right(), available_width);
  }
  if (!left && !right) {
    left = LayoutUnit();
    right = LayoutUnit();
  }
  if (!left)
    left = -right.value();
  if (!right)
    right = -left.value();
  bool is_ltr = containing_block->StyleRef().IsLeftToRightDirection();
  WritingMode writing_mode = containing_block->StyleRef().GetWritingMode();
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      if (is_ltr)
        offset.left += left.value();
      else
        offset.left = -right.value();
      break;
    case WritingMode::kVerticalRl:
      offset.left = -right.value();
      break;
    case WritingMode::kVerticalLr:
      offset.left += left.value();
      break;
    // TODO(layout-dev): Sideways-lr and sideways-rl are not yet supported.
    default:
      break;
  }

  // If the containing block of a relatively positioned element does not specify
  // a height, a percentage top or bottom offset should be resolved as auto.
  // An exception to this is if the containing block has the WinIE quirk where
  // <html> and <body> assume the size of the viewport. In this case, calculate
  // the percent offset based on this height.
  // See <https://bugs.webkit.org/show_bug.cgi?id=26396>.
  // Another exception is a grid item, as the containing block is the grid area:
  // https://drafts.csswg.org/css-grid/#grid-item-sizing

  absl::optional<LayoutUnit> top;
  absl::optional<LayoutUnit> bottom;
  bool has_override_containing_block_content_height =
      HasOverrideContainingBlockContentHeight();
  if (!StyleRef().Top().IsAuto() &&
      (!containing_block->HasAutoHeightOrContainingBlockWithAutoHeight() ||
       !StyleRef().Top().IsPercentOrCalc() ||
       containing_block->StretchesToViewport() ||
       has_override_containing_block_content_height)) {
    // TODO(rego): The computation of the available height is repeated later for
    // "bottom". We could refactor this and move it to some common code for both
    // ifs, however moving it outside of the ifs is not possible as it'd cause
    // performance regressions (see crbug.com/893884).
    top = ValueForLength(StyleRef().Top(),
                         has_override_containing_block_content_height
                             ? OverrideContainingBlockContentHeight()
                             : containing_block->AvailableHeight());
  }
  if (!StyleRef().Bottom().IsAuto() &&
      (!containing_block->HasAutoHeightOrContainingBlockWithAutoHeight() ||
       !StyleRef().Bottom().IsPercentOrCalc() ||
       containing_block->StretchesToViewport() ||
       has_override_containing_block_content_height)) {
    // TODO(rego): Check comment above for "top", it applies here too.
    bottom = ValueForLength(StyleRef().Bottom(),
                            has_override_containing_block_content_height
                                ? OverrideContainingBlockContentHeight()
                                : containing_block->AvailableHeight());
  }
  if (!top && !bottom) {
    top = LayoutUnit();
    bottom = LayoutUnit();
  }
  if (!top)
    top = -bottom.value();
  if (!bottom)
    bottom = -top.value();
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      offset.top += top.value();
      break;
    case WritingMode::kVerticalRl:
      if (is_ltr)
        offset.top += top.value();
      else
        offset.top = -bottom.value();
      break;
    case WritingMode::kVerticalLr:
      if (is_ltr)
        offset.top += top.value();
      else
        offset.top = -bottom.value();
      break;
    // TODO(layout-dev): Sideways-lr and sideways-rl are not yet supported.
    default:
      break;
  }
  return offset;
}

LayoutBlock* LayoutBoxModelObject::StickyContainer() const {
  return ContainingBlock();
}

bool LayoutBoxModelObject::UpdateStickyPositionConstraints() {
  NOT_DESTROYED();

  if (!StyleRef().HasStickyConstrainedPosition()) {
    if (StickyConstraints()) {
      SetStickyConstraints(nullptr);
      SetNeedsPaintPropertyUpdate();
      return true;
    }
    return false;
  }

  bool is_fixed_to_view = false;
  const auto* scroll_container_layer =
      Layer()->ContainingScrollContainerLayer(&is_fixed_to_view);
  auto* scrollable_area = scroll_container_layer->GetScrollableArea();
  DCHECK(scrollable_area);
  // Check if sticky constraints are invalidated.
  if (scrollable_area->HasStickyLayer(Layer()) && StickyConstraints()) {
    DCHECK_EQ(scroll_container_layer,
              StickyConstraints()->containing_scroll_container_layer);
    DCHECK_EQ(is_fixed_to_view, StickyConstraints()->is_fixed_to_view);
    return false;
  }

  StickyPositionScrollingConstraints* constraints =
      MakeGarbageCollected<StickyPositionScrollingConstraints>();
  constraints->containing_scroll_container_layer = scroll_container_layer;
  constraints->is_fixed_to_view = is_fixed_to_view;

  PhysicalOffset skipped_containers_offset;
  LayoutBlock* sticky_container = StickyContainer();
  // The location container for boxes is not always the containing block.
  LayoutObject* location_container =
      IsLayoutInline() ? Container() : To<LayoutBox>(this)->LocationContainer();
  // Skip anonymous containing blocks except for anonymous fieldset content box.
  while (sticky_container->IsAnonymous()) {
    if (sticky_container->Parent() &&
        sticky_container->Parent()->IsLayoutNGFieldset())
      break;
    sticky_container = sticky_container->ContainingBlock();
  }

  // The sticky position constraint rects should be independent of the current
  // scroll position therefore we should ignore the scroll offset when
  // calculating the quad.
  // TODO(crbug.com/966131): Is kIgnoreTransforms correct here?
  MapCoordinatesFlags flags =
      kIgnoreTransforms | kIgnoreScrollOffset | kIgnoreStickyOffset;
  skipped_containers_offset = location_container->LocalToAncestorPoint(
      PhysicalOffset(), sticky_container, flags);
  DCHECK(scroll_container_layer);
  DCHECK(scroll_container_layer->GetLayoutBox());
  auto& scroll_container = *scroll_container_layer->GetLayoutBox();

  constraints->constraining_rect =
      scroll_container.ComputeStickyConstrainingRect();

  LayoutUnit max_container_width =
      IsA<LayoutView>(sticky_container)
          ? sticky_container->LogicalWidth()
          : sticky_container->ContainingBlockLogicalWidthForContent();
  // Sticky positioned element ignore any override logical width on the
  // containing block, as they don't call containingBlockLogicalWidthForContent.
  // It's unclear whether this is totally fine.
  // Compute the container-relative area within which the sticky element is
  // allowed to move.
  LayoutUnit max_width = sticky_container->AvailableLogicalWidth();

  // Map the containing block to the inner corner of the scroll ancestor without
  // transforms.
  PhysicalRect scroll_container_relative_padding_box_rect(
      sticky_container->LayoutOverflowRect());
  if (sticky_container != &scroll_container) {
    PhysicalRect local_rect = sticky_container->PhysicalPaddingBoxRect();
    scroll_container_relative_padding_box_rect =
        sticky_container->LocalToAncestorRect(local_rect, &scroll_container,
                                              flags);
  }

  // Remove top-left border offset from overflow scroller.
  PhysicalOffset scroll_container_border_offset(scroll_container.BorderLeft(),
                                                scroll_container.BorderTop());
  scroll_container_relative_padding_box_rect.Move(
      -scroll_container_border_offset);

  PhysicalRect scroll_container_relative_containing_block_rect(
      scroll_container_relative_padding_box_rect);

  // This is removing the padding of the containing block's overflow rect to get
  // the flow box rectangle and removing the margin of the sticky element to
  // ensure that space between the sticky element and its containing flow box.
  // It is an open issue whether the margin should collapse.
  // See https://www.w3.org/TR/css-position-3/#sticky-pos
  scroll_container_relative_containing_block_rect.ContractEdges(
      MinimumValueForLength(sticky_container->StyleRef().PaddingTop(),
                            max_container_width) +
          MinimumValueForLength(StyleRef().MarginTop(), max_width),
      MinimumValueForLength(sticky_container->StyleRef().PaddingRight(),
                            max_container_width) +
          MinimumValueForLength(StyleRef().MarginRight(), max_width),
      MinimumValueForLength(sticky_container->StyleRef().PaddingBottom(),
                            max_container_width) +
          MinimumValueForLength(StyleRef().MarginBottom(), max_width),
      MinimumValueForLength(sticky_container->StyleRef().PaddingLeft(),
                            max_container_width) +
          MinimumValueForLength(StyleRef().MarginLeft(), max_width));

  constraints->scroll_container_relative_containing_block_rect =
      scroll_container_relative_containing_block_rect;

  PhysicalRect sticky_box_rect;
  if (IsLayoutInline()) {
    sticky_box_rect = To<LayoutInline>(this)->PhysicalLinesBoundingBox();
  } else {
    sticky_box_rect =
        sticky_container->FlipForWritingMode(To<LayoutBox>(this)->FrameRect());
  }
  PhysicalOffset sticky_location =
      sticky_box_rect.offset + skipped_containers_offset;

  // The scroll_container_relative_sticky_box_rect position is the padding box
  // so we need to remove the border when finding the position of the sticky box
  // within the scroll ancestor if the container is not our scroll ancestor. If
  // the container is our scroll ancestor, we also need to remove the border
  // box because we want the position from within the scroller border.
  PhysicalOffset container_border_offset(sticky_container->BorderLeft(),
                                         sticky_container->BorderTop());
  sticky_location -= container_border_offset;
  constraints->scroll_container_relative_sticky_box_rect = PhysicalRect(
      scroll_container_relative_padding_box_rect.offset + sticky_location,
      sticky_box_rect.size);

  // To correctly compute the offsets, the constraints need to know about any
  // nested position:sticky elements between themselves and their
  // location container, and between the location container and their
  // ContainingScrollContainerLayer.
  //
  // The respective search ranges are [container, containingBlock) and
  // [containingBlock, scrollAncestor).
  constraints->nearest_sticky_layer_shifting_sticky_box =
      FindFirstStickyBetween(location_container, sticky_container);
  constraints->nearest_sticky_layer_shifting_containing_block =
      FindFirstStickyBetween(sticky_container, &scroll_container);

  // We skip the right or top sticky offset if there is not enough space to
  // honor both the left/right or top/bottom offsets.
  LayoutUnit constraining_width = constraints->constraining_rect.Width();
  LayoutUnit constraining_height = constraints->constraining_rect.Height();
  LayoutUnit horizontal_offsets =
      MinimumValueForLength(StyleRef().Right(), constraining_width) +
      MinimumValueForLength(StyleRef().Left(), constraining_width);
  bool skip_right = false;
  bool skip_left = false;
  if (!StyleRef().Left().IsAuto() && !StyleRef().Right().IsAuto()) {
    if (horizontal_offsets + sticky_box_rect.Width() > constraining_width) {
      skip_right = StyleRef().IsLeftToRightDirection();
      skip_left = !skip_right;
    }
  }

  if (!StyleRef().Left().IsAuto() && !skip_left) {
    constraints->left_offset =
        MinimumValueForLength(StyleRef().Left(), constraining_width);
    constraints->is_anchored_left = true;
  }

  if (!StyleRef().Right().IsAuto() && !skip_right) {
    constraints->right_offset =
        MinimumValueForLength(StyleRef().Right(), constraining_width);
    constraints->is_anchored_right = true;
  }

  bool skip_bottom = false;
  // TODO(flackr): Exclude top or bottom edge offset depending on the writing
  // mode when related sections are fixed in spec.
  // See http://lists.w3.org/Archives/Public/www-style/2014May/0286.html
  LayoutUnit vertical_offsets =
      MinimumValueForLength(StyleRef().Top(), constraining_height) +
      MinimumValueForLength(StyleRef().Bottom(), constraining_height);
  if (!StyleRef().Top().IsAuto() && !StyleRef().Bottom().IsAuto() &&
      vertical_offsets + sticky_box_rect.Height() > constraining_height) {
    skip_bottom = true;
  }

  if (!StyleRef().Top().IsAuto()) {
    constraints->top_offset =
        MinimumValueForLength(StyleRef().Top(), constraining_height);
    constraints->is_anchored_top = true;
  }

  if (!StyleRef().Bottom().IsAuto() && !skip_bottom) {
    constraints->bottom_offset =
        MinimumValueForLength(StyleRef().Bottom(), constraining_height);
    constraints->is_anchored_bottom = true;
  }

  scrollable_area->AddStickyLayer(Layer());
  constraints->ComputeStickyOffset(scrollable_area->ScrollPosition());
  SetStickyConstraints(constraints);
  SetNeedsPaintPropertyUpdate();
  return true;
}

PhysicalOffset LayoutBoxModelObject::StickyPositionOffset() const {
  NOT_DESTROYED();
  // TODO(chrishtr): StickyPositionOffset depends data updated after layout at
  // present, but there are callsites within Layout for it.
  auto* constraints = StickyConstraints();
  return constraints ? constraints->StickyOffset() : PhysicalOffset();
}

PhysicalOffset LayoutBoxModelObject::AdjustedPositionRelativeTo(
    const PhysicalOffset& start_point,
    const Element* offset_parent) const {
  NOT_DESTROYED();
  // If the element is the HTML body element or doesn't have a parent
  // return 0 and stop this algorithm.
  if (IsBody() || !Parent())
    return PhysicalOffset();

  PhysicalOffset reference_point = start_point;

  // If the offsetParent is null, return the distance between the canvas origin
  // and the left/top border edge of the element and stop this algorithm.
  if (!offset_parent)
    return reference_point;

  if (const LayoutBoxModelObject* offset_parent_object =
          offset_parent->GetLayoutBoxModelObject()) {
    if (!IsOutOfFlowPositioned()) {
      if (IsInFlowPositioned())
        reference_point += OffsetForInFlowPosition();

      // Note that we may fail to find |offsetParent| while walking the
      // container chain, if |offsetParent| is an inline split into
      // continuations: <body style="display:inline;" id="offsetParent">
      // <div id="this">
      // This is why we have to do a nullptr check here.
      for (const LayoutObject* current = Container();
           current && current->GetNode() != offset_parent;
           current = current->Container()) {
        // FIXME: What are we supposed to do inside SVG content?
        reference_point += PhysicalOffsetToBeNoop(
            current->ColumnOffset(reference_point.ToLayoutPoint()));
        if (current->IsBox() && !current->IsLegacyTableRow())
          reference_point += To<LayoutBox>(current)->PhysicalLocation();
      }

      if (offset_parent_object->IsBox() && offset_parent_object->IsBody() &&
          !offset_parent_object->IsPositioned()) {
        reference_point +=
            To<LayoutBox>(offset_parent_object)->PhysicalLocation();
      }
    } else if (UNLIKELY(IsBox() &&
                        To<LayoutBox>(this)->HasAnchorScrollTranslation())) {
      reference_point += To<LayoutBox>(this)->AnchorScrollTranslationOffset();
    }

    if (offset_parent_object->IsLayoutInline()) {
      const auto* inline_parent = To<LayoutInline>(offset_parent_object);

      if (IsBox() && IsOutOfFlowPositioned() &&
          inline_parent->CanContainOutOfFlowPositionedElement(
              StyleRef().GetPosition())) {
        // Offset for out of flow positioned elements with inline containers is
        // a special case in the CSS spec
        reference_point += inline_parent->OffsetForInFlowPositionedInline(
            *To<LayoutBox>(this));
      }

      reference_point -= inline_parent->FirstLineBoxTopLeft();
    }

    if (offset_parent_object->IsBox() && !offset_parent_object->IsBody()) {
      auto* box = To<LayoutBox>(offset_parent_object);
      reference_point -= PhysicalOffset(box->BorderLeft(), box->BorderTop());
    }
  }

  return reference_point;
}

PhysicalOffset LayoutBoxModelObject::OffsetForInFlowPosition() const {
  NOT_DESTROYED();
  if (IsRelPositioned())
    return RelativePositionOffset();

  if (IsStickyPositioned())
    return StickyPositionOffset();

  return PhysicalOffset();
}

LayoutUnit LayoutBoxModelObject::OffsetLeft(const Element* parent) const {
  NOT_DESTROYED();
  // Note that LayoutInline and LayoutBox override this to pass a different
  // startPoint to adjustedPositionRelativeTo.
  return AdjustedPositionRelativeTo(PhysicalOffset(), parent).left;
}

LayoutUnit LayoutBoxModelObject::OffsetTop(const Element* parent) const {
  NOT_DESTROYED();
  // Note that LayoutInline and LayoutBox override this to pass a different
  // startPoint to adjustedPositionRelativeTo.
  return AdjustedPositionRelativeTo(PhysicalOffset(), parent).top;
}

int LayoutBoxModelObject::PixelSnappedOffsetWidth(const Element* parent) const {
  NOT_DESTROYED();
  return SnapSizeToPixel(OffsetWidth(), OffsetLeft(parent));
}

int LayoutBoxModelObject::PixelSnappedOffsetHeight(
    const Element* parent) const {
  NOT_DESTROYED();
  return SnapSizeToPixel(OffsetHeight(), OffsetTop(parent));
}

LayoutUnit LayoutBoxModelObject::ComputedCSSPadding(
    const Length& padding) const {
  NOT_DESTROYED();
  LayoutUnit w;
  if (padding.IsPercentOrCalc())
    w = ContainingBlockLogicalWidthForContent();
  return MinimumValueForLength(padding, w);
}

LayoutUnit LayoutBoxModelObject::ContainingBlockLogicalWidthForContent() const {
  NOT_DESTROYED();
  return ContainingBlock()->AvailableLogicalWidth();
}

LayoutBoxModelObject* LayoutBoxModelObject::Continuation() const {
  NOT_DESTROYED();
  auto it = GetContinuationMap().find(this);
  return it != GetContinuationMap().end() ? it->value : nullptr;
}

void LayoutBoxModelObject::SetContinuation(LayoutBoxModelObject* continuation) {
  NOT_DESTROYED();
  if (continuation) {
    DCHECK(continuation->IsLayoutInline() || continuation->IsLayoutBlockFlow());
    GetContinuationMap().Set(this, continuation);
  } else {
    GetContinuationMap().erase(this);
  }
}

LayoutRect LayoutBoxModelObject::LocalCaretRectForEmptyElement(
    LayoutUnit width,
    LayoutUnit text_indent_offset) const {
  NOT_DESTROYED();
  DCHECK(!SlowFirstChild() || SlowFirstChild()->IsPseudoElement());

  // FIXME: This does not take into account either :first-line or :first-letter
  // However, as soon as some content is entered, the line boxes will be
  // constructed and this kludge is not called any more. So only the caret size
  // of an empty :first-line'd block is wrong. I think we can live with that.
  const ComputedStyle& current_style = FirstLineStyleRef();

  enum CaretAlignment { kAlignLeft, kAlignRight, kAlignCenter };

  CaretAlignment alignment = kAlignLeft;

  switch (current_style.GetTextAlign()) {
    case ETextAlign::kLeft:
    case ETextAlign::kWebkitLeft:
      break;
    case ETextAlign::kCenter:
    case ETextAlign::kWebkitCenter:
      alignment = kAlignCenter;
      break;
    case ETextAlign::kRight:
    case ETextAlign::kWebkitRight:
      alignment = kAlignRight;
      break;
    case ETextAlign::kJustify:
    case ETextAlign::kStart:
      if (!current_style.IsLeftToRightDirection())
        alignment = kAlignRight;
      break;
    case ETextAlign::kEnd:
      if (current_style.IsLeftToRightDirection())
        alignment = kAlignRight;
      break;
  }

  LayoutUnit x = BorderLeft() + PaddingLeft();
  LayoutUnit max_x = width - BorderRight() - PaddingRight();
  LayoutUnit caret_width = GetFrameView()->CaretWidth();

  switch (alignment) {
    case kAlignLeft:
      if (current_style.IsLeftToRightDirection())
        x += text_indent_offset;
      break;
    case kAlignCenter:
      x = (x + max_x) / 2;
      if (current_style.IsLeftToRightDirection())
        x += text_indent_offset / 2;
      else
        x -= text_indent_offset / 2;
      break;
    case kAlignRight:
      x = max_x - caret_width;
      if (!current_style.IsLeftToRightDirection())
        x -= text_indent_offset;
      break;
  }
  x = std::min(x, (max_x - caret_width).ClampNegativeToZero());

  const Font& font = StyleRef().GetFont();
  const SimpleFontData* font_data = font.PrimaryFont();
  LayoutUnit height;
  // crbug.com/595692 This check should not be needed but sometimes
  // primaryFont is null.
  if (font_data)
    height = LayoutUnit(font_data->GetFontMetrics().Height());
  LayoutUnit vertical_space =
      LineHeight(true,
                 current_style.IsHorizontalWritingMode() ? kHorizontalLine
                                                         : kVerticalLine,
                 kPositionOfInteriorLineBoxes) -
      height;
  LayoutUnit y = PaddingTop() + BorderTop() + (vertical_space / 2);
  return current_style.IsHorizontalWritingMode()
             ? LayoutRect(x, y, caret_width, height)
             : LayoutRect(y, x, height, caret_width);
}

void LayoutBoxModelObject::MoveChildTo(
    LayoutBoxModelObject* to_box_model_object,
    LayoutObject* child,
    LayoutObject* before_child,
    bool full_remove_insert) {
  NOT_DESTROYED();
  // We assume that callers have cleared their positioned objects list for child
  // moves (!fullRemoveInsert) so the positioned layoutObject maps don't become
  // stale. It would be too slow to do the map lookup on each call.
  DCHECK(!full_remove_insert || !IsLayoutBlock() ||
         !To<LayoutBlock>(this)->HasPositionedObjects());

  DCHECK_EQ(this, child->Parent());
  DCHECK(!before_child || to_box_model_object == before_child->Parent());

  // If a child is moving from a block-flow to an inline-flow parent then any
  // floats currently intruding into the child can no longer do so. This can
  // happen if a block becomes floating or out-of-flow and is moved to an
  // anonymous block. Remove all floats from their float-lists immediately as
  // markAllDescendantsWithFloatsForLayout won't attempt to remove floats from
  // parents that have inline-flow if we try later.
  auto* child_block_flow = DynamicTo<LayoutBlockFlow>(child);
  if (child_block_flow && to_box_model_object->ChildrenInline() &&
      !ChildrenInline()) {
    child_block_flow->RemoveFloatingObjectsFromDescendants();
    DCHECK(!child_block_flow->ContainsFloats());
  }

  if (full_remove_insert && IsLayoutBlock() && child->IsBox())
    To<LayoutBox>(child)->RemoveFromPercentHeightContainer();

  if (full_remove_insert && (to_box_model_object->IsLayoutBlock() ||
                             to_box_model_object->IsLayoutInline())) {
    // Takes care of adding the new child correctly if toBlock and fromBlock
    // have different kind of children (block vs inline).
    to_box_model_object->AddChild(
        VirtualChildren()->RemoveChildNode(this, child), before_child);
  } else {
    to_box_model_object->VirtualChildren()->InsertChildNode(
        to_box_model_object,
        VirtualChildren()->RemoveChildNode(this, child, full_remove_insert),
        before_child, full_remove_insert);
  }
}

void LayoutBoxModelObject::MoveChildrenTo(
    LayoutBoxModelObject* to_box_model_object,
    LayoutObject* start_child,
    LayoutObject* end_child,
    LayoutObject* before_child,
    bool full_remove_insert) {
  NOT_DESTROYED();
  // This condition is rarely hit since this function is usually called on
  // anonymous blocks which can no longer carry positioned objects (see r120761)
  // or when fullRemoveInsert is false.
  auto* block = DynamicTo<LayoutBlock>(this);
  if (full_remove_insert && block) {
    block->RemovePositionedObjects(nullptr);
    block->RemoveFromPercentHeightContainer();
    auto* block_flow = DynamicTo<LayoutBlockFlow>(block);
    if (block_flow)
      block_flow->RemoveFloatingObjects();
  }

  DCHECK(!before_child || to_box_model_object == before_child->Parent());
  for (LayoutObject* child = start_child; child && child != end_child;) {
    // Save our next sibling as moveChildTo will clear it.
    LayoutObject* next_sibling = child->NextSibling();
    MoveChildTo(to_box_model_object, child, before_child, full_remove_insert);
    child = next_sibling;
  }
}

LayoutObject* LayoutBoxModelObject::SplitAnonymousBoxesAroundChild(
    LayoutObject* before_child) {
  NOT_DESTROYED();
  LayoutBox* box_at_top_of_new_branch = nullptr;

  while (before_child->Parent() != this) {
    auto* box_to_split = To<LayoutBox>(before_child->Parent());
    if (box_to_split->SlowFirstChild() != before_child &&
        box_to_split->IsAnonymous()) {
      // We have to split the parent box into two boxes and move children
      // from |beforeChild| to end into the new post box.
      LayoutBox* post_box = CreateAnonymousBoxToSplit(box_to_split);
      post_box->SetChildrenInline(box_to_split->ChildrenInline());
      auto* parent_box = To<LayoutBoxModelObject>(box_to_split->Parent());
      // We need to invalidate the |parentBox| before inserting the new node
      // so that the table paint invalidation logic knows the structure is
      // dirty. See for example LayoutTableCell:localVisualRect().
      MarkBoxForRelayoutAfterSplit(parent_box);
      parent_box->VirtualChildren()->InsertChildNode(
          parent_box, post_box, box_to_split->NextSibling());
      box_to_split->MoveChildrenTo(post_box, before_child, nullptr, true);

      LayoutObject* child = post_box->SlowFirstChild();
      DCHECK(child);
      if (child && !child->NextSibling())
        CollapseLoneAnonymousBlockChild(post_box, child);
      child = box_to_split->SlowFirstChild();
      DCHECK(child);
      if (child && !child->NextSibling())
        CollapseLoneAnonymousBlockChild(box_to_split, child);

      MarkBoxForRelayoutAfterSplit(box_to_split);
      MarkBoxForRelayoutAfterSplit(post_box);
      box_at_top_of_new_branch = post_box;

      before_child = post_box;
    } else {
      before_child = box_to_split;
    }
  }

  // Splitting the box means the left side of the container chain will lose any
  // percent height descendants below |boxAtTopOfNewBranch| on the right hand
  // side.
  if (box_at_top_of_new_branch) {
    box_at_top_of_new_branch->ClearPercentHeightDescendants();
    MarkBoxForRelayoutAfterSplit(this);
  }

  DCHECK_EQ(before_child->Parent(), this);
  return before_child;
}

LayoutBox* LayoutBoxModelObject::CreateAnonymousBoxToSplit(
    const LayoutBox* box_to_split) const {
  NOT_DESTROYED();
  return box_to_split->CreateAnonymousBoxWithSameTypeAs(this);
}

bool LayoutBoxModelObject::BackgroundTransfersToView(
    const ComputedStyle* document_element_style) const {
  NOT_DESTROYED();
  // In our painter implementation, ViewPainter instead of the painter of the
  // layout object of the document element paints the view background.
  if (IsDocumentElement())
    return true;

  // http://www.w3.org/TR/css3-background/#body-background
  // If the document element is <html> with no background, and a <body> child
  // element exists, the <body> element's background transfers to the document
  // element which in turn transfers to the view in our painter implementation.
  if (!IsBody())
    return false;

  Element* document_element = GetDocument().documentElement();
  if (!IsA<HTMLHtmlElement>(document_element))
    return false;

  if (!document_element_style)
    document_element_style = document_element->GetComputedStyle();
  DCHECK(document_element_style);
  if (document_element_style->HasBackground())
    return false;
  if (GetNode() != GetDocument().FirstBodyElement())
    return false;
  if (document_element_style->ShouldApplyAnyContainment(*document_element))
    return false;
  if (StyleRef().ShouldApplyAnyContainment(*To<Element>(GetNode())))
    return false;
  return true;
}

}  // namespace blink
