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
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_flexible_box.h"
#include "third_party/blink/renderer/core/layout/layout_geometry_map.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_cursor.h"
#include "third_party/blink/renderer/core/layout/ng/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space.h"
#include "third_party/blink/renderer/core/layout/ng/ng_layout_result.h"
#include "third_party/blink/renderer/core/paint/compositing/composited_layer_mapping.h"
#include "third_party/blink/renderer/core/paint/compositing/paint_layer_compositor.h"
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

    maybe_sticky_ancestor =
        maybe_sticky_ancestor->IsLayoutInline()
            ? maybe_sticky_ancestor->Container()
            : To<LayoutBox>(maybe_sticky_ancestor)->StickyContainer();
  }
  return nullptr;
}
}  // namespace

// The HashMap for storing continuation pointers.
// The continuation chain is a singly linked list. As such, the HashMap's value
// is the next pointer associated with the key.
typedef HashMap<const LayoutBoxModelObject*, LayoutBoxModelObject*>
    ContinuationMap;
static ContinuationMap& GetContinuationMap() {
  DEFINE_STATIC_LOCAL(ContinuationMap, map, ());
  return map;
}

void LayoutBoxModelObject::ContentChanged(ContentChangeType change_type) {
  NOT_DESTROYED();
  if (!HasLayer())
    return;

  Layer()->ContentChanged(change_type);
}

LayoutBoxModelObject::LayoutBoxModelObject(ContainerNode* node)
    : LayoutObject(node) {}

bool LayoutBoxModelObject::UsesCompositedScrolling() const {
  NOT_DESTROYED();
  return IsScrollContainer() && HasLayer() &&
         Layer()->GetScrollableArea()->UsesCompositedScrolling();
}

static bool HasInsetBoxShadow(const ComputedStyle& style) {
  if (!style.BoxShadow())
    return false;
  for (const ShadowData& shadow : style.BoxShadow()->Shadows()) {
    if (shadow.Style() == ShadowStyle::kInset)
      return true;
  }
  return false;
}

BackgroundPaintLocation
LayoutBoxModelObject::ComputeBackgroundPaintLocationIfComposited() const {
  NOT_DESTROYED();
  bool may_have_scrolling_layers_without_scrolling = IsA<LayoutView>(this);
  const auto* scrollable_area = GetScrollableArea();
  bool scrolls_overflow = scrollable_area && scrollable_area->ScrollsOverflow();
  if (!scrolls_overflow && !may_have_scrolling_layers_without_scrolling)
    return kBackgroundPaintInGraphicsLayer;

  // If we care about LCD text, paint root backgrounds into scrolling contents
  // layer even if style suggests otherwise. (For non-root scrollers, we just
  // avoid compositing - see PLSA::ComputeNeedsCompositedScrolling.)
  if (IsA<LayoutView>(this)) {
    if (!GetDocument().GetSettings()->GetPreferCompositingToLCDTextEnabled())
      return kBackgroundPaintInScrollingContents;
  }

  // Inset box shadow is painted in the scrolling area above the background, and
  // it doesn't scroll, so the background can only be painted in the main layer.
  if (HasInsetBoxShadow(StyleRef()))
    return kBackgroundPaintInGraphicsLayer;

  // TODO(flackr): Detect opaque custom scrollbars which would cover up a
  // border-box background.
  bool has_custom_scrollbars =
      scrollable_area &&
      ((scrollable_area->HorizontalScrollbar() &&
        scrollable_area->HorizontalScrollbar()->IsCustomScrollbar()) ||
       (scrollable_area->VerticalScrollbar() &&
        scrollable_area->VerticalScrollbar()->IsCustomScrollbar()));

  // Assume optimistically that the background can be painted in the scrolling
  // contents until we find otherwise.
  BackgroundPaintLocation paint_location = kBackgroundPaintInScrollingContents;

  const FillLayer* layer = &(StyleRef().BackgroundLayers());
  for (; layer; layer = layer->Next()) {
    if (layer->Attachment() == EFillAttachment::kLocal)
      continue;

    // Solid color layers with an effective background clip of the padding box
    // can be treated as local.
    if (!layer->GetImage() && !layer->Next() &&
        ResolveColor(GetCSSPropertyBackgroundColor()).Alpha() > 0) {
      EFillBox clip = layer->Clip();
      if (clip == EFillBox::kPadding)
        continue;
      // A border box can be treated as a padding box if the border is opaque or
      // there is no border and we don't have custom scrollbars.
      if (clip == EFillBox::kBorder) {
        if (!has_custom_scrollbars &&
            (StyleRef().BorderTopWidth() == 0 ||
             (!ResolveColor(GetCSSPropertyBorderTopColor()).HasAlpha() &&
              StyleRef().BorderTopStyle() == EBorderStyle::kSolid)) &&
            (StyleRef().BorderLeftWidth() == 0 ||
             (!ResolveColor(GetCSSPropertyBorderLeftColor()).HasAlpha() &&
              StyleRef().BorderLeftStyle() == EBorderStyle::kSolid)) &&
            (StyleRef().BorderRightWidth() == 0 ||
             (!ResolveColor(GetCSSPropertyBorderRightColor()).HasAlpha() &&
              StyleRef().BorderRightStyle() == EBorderStyle::kSolid)) &&
            (StyleRef().BorderBottomWidth() == 0 ||
             (!ResolveColor(GetCSSPropertyBorderBottomColor()).HasAlpha() &&
              StyleRef().BorderBottomStyle() == EBorderStyle::kSolid))) {
          continue;
        }
        // If we have an opaque background color only, we can safely paint it
        // into both the scrolling contents layer and the graphics layer to
        // preserve LCD text.
        if (layer == (&StyleRef().BackgroundLayers()) &&
            ResolveColor(GetCSSPropertyBackgroundColor()).Alpha() < 255)
          return kBackgroundPaintInGraphicsLayer;
        paint_location |= kBackgroundPaintInGraphicsLayer;
        continue;
      }
      // A content fill box can be treated as a padding fill box if there is no
      // padding.
      if (clip == EFillBox::kContent && StyleRef().PaddingTop().IsZero() &&
          StyleRef().PaddingLeft().IsZero() &&
          StyleRef().PaddingRight().IsZero() &&
          StyleRef().PaddingBottom().IsZero()) {
        continue;
      }
    }
    return kBackgroundPaintInGraphicsLayer;
  }

  // It can't paint in the scrolling contents because it has different 3d
  // context than the scrolling contents.
  if (!StyleRef().Preserves3D() && Parent() &&
      Parent()->StyleRef().Preserves3D()) {
    return kBackgroundPaintInGraphicsLayer;
  }

  return paint_location;
}

LayoutBoxModelObject::~LayoutBoxModelObject() {
  // Our layer should have been destroyed and cleared by now
  DCHECK(!HasLayer());
  DCHECK(!Layer());
}

void LayoutBoxModelObject::WillBeDestroyed() {
  NOT_DESTROYED();
  // A continuation of this LayoutObject should be destroyed at subclasses.
  DCHECK(!Continuation());

  if (IsPositioned()) {
    // Don't use view() because the document's layoutView has been set to
    // 0 during destruction.
    if (LocalFrame* frame = GetFrame()) {
      if (LocalFrameView* frame_view = frame->View()) {
        if (StyleRef().HasViewportConstrainedPosition()) {
          frame_view->RemoveViewportConstrainedObject(
              *this, LocalFrameView::ViewportConstrainedType::kFixed);
        } else if (StyleRef().HasStickyConstrainedPosition()) {
          frame_view->RemoveViewportConstrainedObject(
              *this, LocalFrameView::ViewportConstrainedType::kSticky);
        }
      }
    }
  }

  LayoutObject::WillBeDestroyed();

  if (HasLayer())
    DestroyLayer();
}

void LayoutBoxModelObject::StyleWillChange(StyleDifference diff,
                                           const ComputedStyle& new_style) {
  NOT_DESTROYED();
  // SPv1:
  // This object's layer may begin or cease to be stacked or stacking context,
  // in which case the paint invalidation container of this object and
  // descendants may change. Thus we need to invalidate paint eagerly for all
  // such children. PaintLayerCompositor::paintInvalidationOnCompositingChange()
  // doesn't work for the case because we can only see the new
  // paintInvalidationContainer during compositing update.
  // SPv1 and v2:
  // Change of stacked/stacking context status may cause change of this or
  // descendant PaintLayer's CompositingContainer, so we need to eagerly
  // invalidate the current compositing container chain which may have painted
  // cached subsequences containing this object or descendant objects.
  if (Style() &&
      (IsStacked() != IsStacked(new_style) ||
       IsStackingContext() != IsStackingContext(new_style)) &&
      // ObjectPaintInvalidator requires this.
      IsRooted()) {
    if (RuntimeEnabledFeatures::CompositeAfterPaintEnabled()) {
      ObjectPaintInvalidator(*this).SlowSetPaintingLayerNeedsRepaint();
    } else {
      // We need to invalidate based on the current compositing status.
      DisableCompositingQueryAsserts compositing_disabler;
      ObjectPaintInvalidator(*this)
          .InvalidatePaintIncludingNonCompositingDescendants();
    }
  }

  if (HasLayer() && diff.CssClipChanged())
    Layer()->ClearClipRects();

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
  bool could_contain_fixed = ComputeIsFixedContainer(old_style);
  bool could_contain_absolute =
      could_contain_fixed || ComputeIsAbsoluteContainer(old_style);

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
    PaintLayer* parent_layer = Layer()->Parent();
    // Either a transform wasn't specified or the object doesn't support
    // transforms, so just null out the bit.
    SetHasTransformRelatedProperty(false);
    SetHasReflection(false);
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
    if (!NeedsLayout()) {
      // FIXME: We should call a specialized version of this function.
      parent_layer->UpdateLayerPositionsAfterLayout();
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
    if (Layer())
      Layer()->SetNeedsCompositingInputsUpdate();
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

  if (LocalFrameView* frame_view = View()->GetFrameView()) {
    bool new_style_is_viewport_constained =
        StyleRef().GetPosition() == EPosition::kFixed;
    bool old_style_is_viewport_constrained =
        old_style && old_style->GetPosition() == EPosition::kFixed;
    bool new_style_is_sticky = StyleRef().HasStickyConstrainedPosition();
    bool old_style_is_sticky =
        old_style && old_style->HasStickyConstrainedPosition();

    if (new_style_is_sticky != old_style_is_sticky) {
      if (new_style_is_sticky) {
        // During compositing inputs update we'll have the scroll ancestor
        // without having to walk up the tree and can compute the sticky
        // position constraints then.
        if (Layer())
          Layer()->SetNeedsCompositingInputsUpdate();

        // TODO(pdr): When CompositeAfterPaint is enabled, we will need to
        // invalidate the scroll paint property subtree for this so main thread
        // scroll reasons are recomputed.
      } else {
        // This may get re-added to viewport constrained objects if the object
        // went from sticky to fixed.
        frame_view->RemoveViewportConstrainedObject(
            *this, LocalFrameView::ViewportConstrainedType::kSticky);

        // Remove sticky constraints for this layer.
        if (Layer()) {
          if (const PaintLayer* ancestor_scroll_container_layer =
                  Layer()->AncestorScrollContainerLayer()) {
            if (PaintLayerScrollableArea* scrollable_area =
                    ancestor_scroll_container_layer->GetScrollableArea())
              scrollable_area->InvalidateStickyConstraintsFor(Layer());
          }
        }

        // TODO(pdr): When CompositeAfterPaint is enabled, we will need to
        // invalidate the scroll paint property subtree for this so main thread
        // scroll reasons are recomputed.
      }
    }

    if (new_style_is_viewport_constained != old_style_is_viewport_constrained) {
      if (new_style_is_viewport_constained && Layer()) {
        frame_view->AddViewportConstrainedObject(
            *this, LocalFrameView::ViewportConstrainedType::kFixed);
      } else {
        frame_view->RemoveViewportConstrainedObject(
            *this, LocalFrameView::ViewportConstrainedType::kFixed);
      }
    }
  }

  if (old_style &&
      old_style->BackfaceVisibility() != StyleRef().BackfaceVisibility()) {
    SetNeedsPaintPropertyUpdate();
  }

  if (old_style && HasLayer() && !Layer()->SelfNeedsRepaint() &&
      diff.TransformChanged() &&
      (RuntimeEnabledFeatures::CompositeAfterPaintEnabled() ||
       !Layer()->HasStyleDeterminedDirectCompositingReasons())) {
    // PaintLayerPainter::PaintLayerWithAdjustedRoot skips painting of a layer
    // whose transform is not invertible, so we need to repaint the layer when
    // invertible status changes.
    TransformationMatrix old_transform;
    TransformationMatrix new_transform;
    old_style->ApplyTransform(
        old_transform, LayoutSize(), ComputedStyle::kExcludeTransformOrigin,
        ComputedStyle::kExcludeMotionPath,
        ComputedStyle::kIncludeIndependentTransformProperties);
    StyleRef().ApplyTransform(
        new_transform, LayoutSize(), ComputedStyle::kExcludeTransformOrigin,
        ComputedStyle::kExcludeMotionPath,
        ComputedStyle::kIncludeIndependentTransformProperties);
    if (old_transform.IsInvertible() != new_transform.IsInvertible())
      Layer()->SetNeedsRepaint();
  }

  // We can't squash across a layout containment boundary. So, if the
  // containment changes, we need to update the compositing inputs.
  if (old_style &&
      ShouldApplyLayoutContainment(*old_style) !=
          ShouldApplyLayoutContainment() &&
      Layer()) {
    Layer()->SetNeedsCompositingInputsUpdate();
  }
}

void LayoutBoxModelObject::InvalidateStickyConstraints() {
  NOT_DESTROYED();
  PaintLayer* enclosing = EnclosingLayer();

  if (PaintLayerScrollableArea* scrollable_area =
          enclosing->GetScrollableArea()) {
    scrollable_area->InvalidateAllStickyConstraints();
    // If this object doesn't have a layer and its enclosing layer is a scroller
    // then we don't need to invalidate the sticky constraints on the ancestor
    // scroller because the enclosing scroller won't have changed size.
    if (!Layer())
      return;
  }

  // This intentionally uses the stale ancestor overflow layer compositing input
  // as if we have saved constraints for this layer they were saved in the
  // previous frame.
  if (const PaintLayer* ancestor_scroll_container_layer =
          enclosing->AncestorScrollContainerLayer()) {
    if (PaintLayerScrollableArea* ancestor_scrollable_area =
            ancestor_scroll_container_layer->GetScrollableArea())
      ancestor_scrollable_area->InvalidateAllStickyConstraints();
  }
}

void LayoutBoxModelObject::CreateLayerAfterStyleChange() {
  NOT_DESTROYED();
  DCHECK(!HasLayer() && !Layer());
  GetMutableForPainting().FirstFragment().SetLayer(
      std::make_unique<PaintLayer>(*this));
  SetHasLayer(true);
  Layer()->InsertOnlyThisLayerAfterStyleChange();
  // Creating a layer may affect existence of the LocalBorderBoxProperties, so
  // we need to ensure that we update paint properties.
  SetNeedsPaintPropertyUpdate();
  if (GetScrollableArea())
    GetScrollableArea()->InvalidateScrollTimeline();
}

void LayoutBoxModelObject::DestroyLayer() {
  NOT_DESTROYED();
  DCHECK(HasLayer() && Layer());
  SetHasLayer(false);
  GetMutableForPainting().FirstFragment().SetLayer(nullptr);
  // Removing a layer may affect existence of the LocalBorderBoxProperties, so
  // we need to ensure that we update paint properties.
  SetNeedsPaintPropertyUpdate();
  SetBackgroundPaintLocation(kBackgroundPaintInGraphicsLayer);
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
    descendant.AddOutlineRects(layer_outline_rects, PhysicalOffset(),
                               include_block_overflows);
    descendant.LocalToAncestorRects(layer_outline_rects, this, PhysicalOffset(),
                                    additional_offset);
    rects.AppendVector(layer_outline_rects);
    return;
  }

  if (descendant.IsBox()) {
    descendant.AddOutlineRects(
        rects, additional_offset + To<LayoutBox>(descendant).PhysicalLocation(),
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

  descendant.AddOutlineRects(rects, additional_offset, include_block_overflows);
}

void LayoutBoxModelObject::RecalcVisualOverflow() {
  // |PaintLayer| calls this function when |HasSelfPaintingLayer|. When |this|
  // is an inline box or an atomic inline, its ink overflow is stored in
  // |NGFragmentItem| in the inline formatting context.
  if (IsInline() && IsInLayoutNGInlineFormattingContext() &&
      RuntimeEnabledFeatures::LayoutNGFragmentItemEnabled()) {
    DCHECK(HasSelfPaintingLayer());
    NGInlineCursor cursor;
    for (cursor.MoveTo(*this); cursor; cursor.MoveToNextForSameLayoutObject())
      cursor.Current().RecalcInkOverflow(cursor);
    return;
  }

  LayoutObject::RecalcVisualOverflow();
}

void LayoutBoxModelObject::AbsoluteQuadsForSelf(
    Vector<FloatQuad>& quads,
    MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  NOTREACHED();
}

void LayoutBoxModelObject::AbsoluteQuads(Vector<FloatQuad>& quads,
                                         MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  AbsoluteQuadsForSelf(quads, mode);

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
    continuation_object->AbsoluteQuadsForSelf(quads, mode);
  }
}

void LayoutBoxModelObject::UpdateFromStyle() {
  NOT_DESTROYED();
  const ComputedStyle& style_to_use = StyleRef();
  SetHasBoxDecorationBackground(style_to_use.HasBoxDecorationBackground());
  SetInline(style_to_use.IsDisplayInlineType());
  SetPositionState(style_to_use.GetPosition());
  SetHorizontalWritingMode(style_to_use.IsHorizontalWritingMode());
  SetCanContainFixedPositionObjects(ComputeIsFixedContainer(&style_to_use));
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
    } else if (this_box->GetCachedLayoutResult()) {
      // TODO(dgrogan): We won't get here when laying out the FlexNG item and
      // its descendant(s) for the first time because the item (|this_box|)
      // doesn't have anything in its cache. That seems bad because this method
      // returns true even when the item has a fixed definite height. There
      // doesn't seem to be an easy way to check the flex item's definiteness
      // here because the flex item's LayoutObject doesn't have a
      // BoxLayoutExtraInput that we could add a flag to.
      const NGConstraintSpace& space =
          this_box->GetCachedLayoutResult()->GetConstraintSpaceForCaching();
      if (space.IsFixedBlockSize() && !space.IsFixedBlockSizeIndefinite())
        return false;
    }
  }
  if (this_box && this_box->IsGridItem() &&
      this_box->HasOverrideContainingBlockContentLogicalHeight())
    return false;
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
      } else if (this_box && this_box->GetCachedLayoutResult() &&
                 !this_box->GetBoxLayoutExtraInput()) {
        return this_box->GetCachedLayoutResult()
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
  base::Optional<LayoutUnit> left;
  base::Optional<LayoutUnit> right;
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

  base::Optional<LayoutUnit> top;
  base::Optional<LayoutUnit> bottom;
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

void LayoutBoxModelObject::UpdateStickyPositionConstraints() const {
  NOT_DESTROYED();
  DCHECK(StyleRef().HasStickyConstrainedPosition());

  const PhysicalSize constraining_size = ComputeStickyConstrainingRect().size;

  StickyPositionScrollingConstraints constraints;
  PhysicalOffset skipped_containers_offset;
  LayoutBlock* sticky_container = StickyContainer();
  // The location container for boxes is not always the containing block.
  LayoutObject* location_container =
      IsLayoutInline() ? Container() : To<LayoutBox>(this)->LocationContainer();
  // Skip anonymous containing blocks.
  while (sticky_container->IsAnonymous()) {
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
  auto& scroll_ancestor =
      To<LayoutBox>(Layer()->AncestorScrollContainerLayer()->GetLayoutObject());

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
  if (sticky_container != &scroll_ancestor) {
    PhysicalRect local_rect = sticky_container->PhysicalPaddingBoxRect();
    scroll_container_relative_padding_box_rect =
        sticky_container->LocalToAncestorRect(local_rect, &scroll_ancestor,
                                              flags);
  }

  // Remove top-left border offset from overflow scroller.
  PhysicalOffset scroll_container_border_offset(scroll_ancestor.BorderLeft(),
                                                scroll_ancestor.BorderTop());
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

  constraints.scroll_container_relative_containing_block_rect =
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

  // The scrollContainerRelativePaddingBoxRect's position is the padding box so
  // we need to remove the border when finding the position of the sticky box
  // within the scroll ancestor if the container is not our scroll ancestor. If
  // the container is our scroll ancestor, we also need to remove the border
  // box because we want the position from within the scroller border.
  PhysicalOffset container_border_offset(sticky_container->BorderLeft(),
                                         sticky_container->BorderTop());
  sticky_location -= container_border_offset;
  constraints.scroll_container_relative_sticky_box_rect = PhysicalRect(
      scroll_container_relative_padding_box_rect.offset + sticky_location,
      sticky_box_rect.size);

  // To correctly compute the offsets, the constraints need to know about any
  // nested position:sticky elements between themselves and their
  // containingBlock, and between the containingBlock and their scrollAncestor.
  //
  // The respective search ranges are [container, containingBlock) and
  // [containingBlock, scrollAncestor).
  constraints.nearest_sticky_layer_shifting_sticky_box =
      FindFirstStickyBetween(location_container, sticky_container);
  // We cannot use |scrollAncestor| here as it disregards the root
  // ancestorOverflowLayer(), which we should include.
  constraints.nearest_sticky_layer_shifting_containing_block =
      FindFirstStickyBetween(
          sticky_container,
          &Layer()->AncestorScrollContainerLayer()->GetLayoutObject());

  // We skip the right or top sticky offset if there is not enough space to
  // honor both the left/right or top/bottom offsets.
  LayoutUnit horizontal_offsets =
      MinimumValueForLength(StyleRef().Right(), constraining_size.width) +
      MinimumValueForLength(StyleRef().Left(), constraining_size.width);
  bool skip_right = false;
  bool skip_left = false;
  if (!StyleRef().Left().IsAuto() && !StyleRef().Right().IsAuto()) {
    if (horizontal_offsets >
            scroll_container_relative_containing_block_rect.Width() ||
        horizontal_offsets + sticky_box_rect.Width() >
            constraining_size.width) {
      skip_right = StyleRef().IsLeftToRightDirection();
      skip_left = !skip_right;
    }
  }

  if (!StyleRef().Left().IsAuto() && !skip_left) {
    constraints.left_offset =
        MinimumValueForLength(StyleRef().Left(), constraining_size.width);
    constraints.is_anchored_left = true;
  }

  if (!StyleRef().Right().IsAuto() && !skip_right) {
    constraints.right_offset =
        MinimumValueForLength(StyleRef().Right(), constraining_size.width);
    constraints.is_anchored_right = true;
  }

  bool skip_bottom = false;
  // TODO(flackr): Exclude top or bottom edge offset depending on the writing
  // mode when related sections are fixed in spec.
  // See http://lists.w3.org/Archives/Public/www-style/2014May/0286.html
  LayoutUnit vertical_offsets =
      MinimumValueForLength(StyleRef().Top(), constraining_size.height) +
      MinimumValueForLength(StyleRef().Bottom(), constraining_size.height);
  if (!StyleRef().Top().IsAuto() && !StyleRef().Bottom().IsAuto()) {
    if (vertical_offsets >
            scroll_container_relative_containing_block_rect.Height() ||
        vertical_offsets + sticky_box_rect.Height() >
            constraining_size.height) {
      skip_bottom = true;
    }
  }

  if (!StyleRef().Top().IsAuto()) {
    constraints.top_offset =
        MinimumValueForLength(StyleRef().Top(), constraining_size.height);
    constraints.is_anchored_top = true;
  }

  if (!StyleRef().Bottom().IsAuto() && !skip_bottom) {
    constraints.bottom_offset =
        MinimumValueForLength(StyleRef().Bottom(), constraining_size.height);
    constraints.is_anchored_bottom = true;
  }
  PaintLayerScrollableArea* scrollable_area =
      Layer()->AncestorScrollContainerLayer()->GetScrollableArea();
  scrollable_area->AddStickyConstraints(Layer(), constraints);
}

bool LayoutBoxModelObject::IsSlowRepaintConstrainedObject() const {
  NOT_DESTROYED();
  if (!HasLayer() || (StyleRef().GetPosition() != EPosition::kFixed &&
                      StyleRef().GetPosition() != EPosition::kSticky)) {
    return false;
  }

  PaintLayer* layer = Layer();

  // Whether the Layer sticks to the viewport is a tree-depenent
  // property and our viewportConstrainedObjects collection is maintained
  // with only LayoutObject-level information.
  if (!layer->FixedToViewport() && !layer->SticksToScroller())
    return false;

  // If the whole subtree is invisible, there's no reason to scroll on
  // the main thread because we don't need to generate invalidations
  // for invisible content.
  if (layer->SubtreeIsInvisible())
    return false;

  // We're only smart enough to scroll viewport-constrainted objects
  // in the compositor if they are directly composited.
  return !layer->CanBeCompositedForDirectReasons();
}

PhysicalRect LayoutBoxModelObject::ComputeStickyConstrainingRect() const {
  NOT_DESTROYED();
  LayoutBox* scroll_container_box =
      Layer()->AncestorScrollContainerLayer()->GetLayoutBox();
  DCHECK(scroll_container_box);
  // That |scroll_container_box| is a scroll-container is ensured by
  // Layer::AncestorScrollContainerLayer().
  DCHECK(scroll_container_box->IsScrollContainer());
  PhysicalRect constraining_rect;
  constraining_rect =
      PhysicalRect(scroll_container_box->OverflowClipRect(LayoutPoint()));
  constraining_rect.Move(PhysicalOffset(
      -scroll_container_box->BorderLeft() + scroll_container_box->PaddingLeft(),
      -scroll_container_box->BorderTop() + scroll_container_box->PaddingTop()));
  constraining_rect.ContractEdges(LayoutUnit(),
                                  scroll_container_box->PaddingLeft() +
                                      scroll_container_box->PaddingRight(),
                                  scroll_container_box->PaddingTop() +
                                      scroll_container_box->PaddingBottom(),
                                  LayoutUnit());
  return constraining_rect;
}

PhysicalOffset LayoutBoxModelObject::StickyPositionOffset() const {
  NOT_DESTROYED();
  // TODO(chrishtr): StickyPositionOffset depends on compositing at present,
  // but there are callsites within Layout for it.

  const PaintLayer* ancestor_scroll_container_layer =
      Layer()->AncestorScrollContainerLayer();
  // TODO: Force compositing input update if we ask for offset before
  // compositing inputs have been computed?
  if (!ancestor_scroll_container_layer ||
      !ancestor_scroll_container_layer->GetScrollableArea()) {
    return PhysicalOffset();
  }

  auto* constraints = ancestor_scroll_container_layer->GetScrollableArea()
                          ->GetStickyConstraints(Layer());
  if (!constraints)
    return PhysicalOffset();

  // The sticky offset is physical, so we can just return the delta computed in
  // absolute coords (though it may be wrong with transforms).
  PhysicalRect constraining_rect = ComputeStickyConstrainingRect();
  FloatPoint scroll_position =
      ancestor_scroll_container_layer->GetScrollableArea()->ScrollPosition();
  constraining_rect.Move(PhysicalOffset(LayoutUnit(scroll_position.X()),
                                        LayoutUnit(scroll_position.Y())));
  return constraints->ComputeStickyOffset(
      constraining_rect, ancestor_scroll_container_layer->GetScrollableArea()
                             ->GetStickyConstraintsMap());
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
  return GetContinuationMap().at(this);
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

const LayoutObject* LayoutBoxModelObject::PushMappingToContainer(
    const LayoutBoxModelObject* ancestor_to_stop_at,
    LayoutGeometryMap& geometry_map) const {
  NOT_DESTROYED();
  DCHECK_NE(ancestor_to_stop_at, this);

  AncestorSkipInfo skip_info(ancestor_to_stop_at);
  LayoutObject* container = Container(&skip_info);
  if (!container)
    return nullptr;

  bool is_inline = IsLayoutInline();
  bool is_fixed_pos =
      !is_inline && StyleRef().GetPosition() == EPosition::kFixed;
  bool contains_fixed_position = CanContainFixedPositionObjects();

  TransformationMatrix adjustment_for_skipped_ancestor;
  bool adjustment_for_skipped_ancestor_is_translate_2d = true;
  if (skip_info.AncestorSkipped()) {
    // There can't be a transform between container and ancestor_to_stop_at,
    // because transforms create containers, so it should be safe to just
    // subtract the delta between the container and ancestor_to_stop_at.
    PhysicalOffset ancestor_offset =
        ancestor_to_stop_at->OffsetFromAncestor(container);
    adjustment_for_skipped_ancestor.Translate(-ancestor_offset.left.ToFloat(),
                                              -ancestor_offset.top.ToFloat());
  }

  PhysicalOffset container_offset = OffsetFromContainer(container);
  bool offset_depends_on_point;
  if (IsLayoutFlowThread()) {
    container_offset += PhysicalOffsetToBeNoop(ColumnOffset(LayoutPoint()));
    offset_depends_on_point = true;
  } else {
    offset_depends_on_point =
        container->StyleRef().IsFlippedBlocksWritingMode() &&
        container->IsBox();
  }

  bool preserve3d =
      container->StyleRef().Preserves3D() || StyleRef().Preserves3D();
  GeometryInfoFlags flags = 0;
  if (preserve3d)
    flags |= kAccumulatingTransform;
  if (offset_depends_on_point)
    flags |= kIsNonUniform;
  if (is_fixed_pos)
    flags |= kIsFixedPosition;
  if (contains_fixed_position)
    flags |= kContainsFixedPosition;
  if (ShouldUseTransformFromContainer(container)) {
    TransformationMatrix t;
    GetTransformFromContainer(container, container_offset, t);
    adjustment_for_skipped_ancestor.Multiply(t);
    geometry_map.Push(this, adjustment_for_skipped_ancestor, flags,
                      PhysicalOffset());
  } else if (adjustment_for_skipped_ancestor_is_translate_2d) {
    container_offset += PhysicalOffset::FromFloatSizeRound(
        adjustment_for_skipped_ancestor.To2DTranslation());
    geometry_map.Push(this, container_offset, flags, PhysicalOffset());
  } else {
    adjustment_for_skipped_ancestor.Translate(container_offset.left,
                                              container_offset.top);
    geometry_map.Push(this, adjustment_for_skipped_ancestor, flags,
                      PhysicalOffset());
  }

  return skip_info.AncestorSkipped() ? ancestor_to_stop_at : container;
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

  return true;
}

}  // namespace blink
