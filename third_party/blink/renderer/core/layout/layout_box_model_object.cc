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
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/layout/constraint_space.h"
#include "third_party/blink/renderer/core/layout/geometry/transform_state.h"
#include "third_party/blink/renderer/core/layout/inline/inline_cursor.h"
#include "third_party/blink/renderer/core/layout/layout_block.h"
#include "third_party/blink/renderer/core/layout/layout_flow_thread.h"
#include "third_party/blink/renderer/core/layout/layout_inline.h"
#include "third_party/blink/renderer/core/layout/layout_object_inlines.h"
#include "third_party/blink/renderer/core/layout/layout_result.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/layout/legacy_layout_tree_walking.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/table/layout_table_section.h"
#include "third_party/blink/renderer/core/page/scrolling/sticky_position_scrolling_constraints.h"
#include "third_party/blink/renderer/core/paint/inline_paint_context.h"
#include "third_party/blink/renderer/core/paint/object_paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_layer.h"
#include "third_party/blink/renderer/core/paint/paint_layer_scrollable_area.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/platform/geometry/length_functions.h"

namespace blink {

namespace {

void MarkBoxForRelayoutAfterSplit(LayoutBoxModelObject* box) {
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

bool NeedsAnchorPositionScrollData(Element& element,
                                   const ComputedStyle& style) {
  // `AnchorPositionScrollData` is for anchor positioned elements, which must be
  // absolutely positioned.
  if (!style.HasOutOfFlowPosition()) {
    return false;
  }
  // There's an explicitly set default anchor.
  if (style.PositionAnchor()) {
    return true;
  }
  // Now we have `position-anchor: auto`. We need `AnchorPositionScrollData`
  // only if there's an implicit anchor element to track.
  return element.ImplicitAnchorElement();
}

}  // namespace

LayoutBoxModelObject::LayoutBoxModelObject(ContainerNode* node)
    : LayoutObject(node) {}

LayoutBoxModelObject::~LayoutBoxModelObject() = default;

void LayoutBoxModelObject::WillBeDestroyed() {
  NOT_DESTROYED();

  if (!DocumentBeingDestroyed()) {
    GetDocument()
        .GetFrame()
        ->GetInputMethodController()
        .LayoutObjectWillBeDestroyed(*this);
  }

  LayoutObject::WillBeDestroyed();

  if (HasLayer())
    DestroyLayer();

  if (GetFrameView()) {
    SetIsBackgroundAttachmentFixedObject(false);
  }

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

  if (Style()) {
    LayoutFlowThread* flow_thread = FlowThreadContainingBlock();
    if (flow_thread && flow_thread != this) {
      flow_thread->FlowThreadDescendantStyleWillChange(this, diff, new_style);
    }
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

  // Clear our sticky constraints if we are no longer sticky.
  if (Layer() && old_style->HasStickyConstrainedPosition() &&
      !StyleRef().HasStickyConstrainedPosition()) {
    SetStickyConstraints(nullptr);
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
    Layer()->UpdateFilters(diff, old_style, StyleRef());
    Layer()->UpdateBackdropFilters(old_style, StyleRef());
    Layer()->UpdateClipPath(old_style, StyleRef());
    Layer()->UpdateOffsetPath(old_style, StyleRef());
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
    if (LayoutFlowThread* flow_thread = FlowThreadContainingBlock()) {
      if (flow_thread != this) {
        flow_thread->FlowThreadDescendantStyleDidChange(this, diff, *old_style);
      }
    }

    LayoutBlock* block = InclusiveContainingBlock();

    if ((could_contain_fixed && !can_contain_fixed) ||
        (could_contain_absolute && !can_contain_absolute)) {
      // Clear our positioned objects list. Our absolute and fixed positioned
      // descendants will be inserted into our containing block's positioned
      // objects list during layout.
      block->RemovePositionedObjects(nullptr);
    }
    if (!could_contain_absolute && can_contain_absolute) {
      // Remove our absolute positioned descendants from their current
      // containing block.
      // They will be inserted into our positioned objects list during layout.
      if (LayoutBlock* cb = block->ContainingBlockForAbsolutePosition())
        cb->RemovePositionedObjects(this);
    }
    if (!could_contain_fixed && can_contain_fixed) {
      // Remove our fixed positioned descendants from their current containing
      // block.
      // They will be inserted into our positioned objects list during layout.
      if (LayoutBlock* cb = block->ContainingBlockForFixedPosition())
        cb->RemovePositionedObjects(this);
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

  // The used style for body background may change due to computed style change
  // on the document element because of change of BackgroundTransfersToView()
  // which depends on the document element style.
  if (IsDocumentElement()) {
    if (const HTMLBodyElement* body = GetDocument().FirstBodyElement()) {
      if (auto* body_object =
              DynamicTo<LayoutBoxModelObject>(body->GetLayoutObject())) {
        bool new_body_background_transfers =
            body_object->BackgroundTransfersToView(Style());
        bool old_body_background_transfers =
            old_style && body_object->BackgroundTransfersToView(old_style);
        if (new_body_background_transfers != old_body_background_transfers &&
            body_object->Style() && body_object->StyleRef().HasBackground()) {
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

  if (Element* element = DynamicTo<Element>(GetNode())) {
    if (NeedsAnchorPositionScrollData(*element, StyleRef())) {
      element->EnsureAnchorPositionScrollData();
    } else {
      element->RemoveAnchorPositionScrollData();
    }
  }
}

void LayoutBoxModelObject::CreateLayerAfterStyleChange() {
  NOT_DESTROYED();
  DCHECK(!HasLayer() && !Layer());
  FragmentData& first_fragment = GetMutableForPainting().FirstFragment();
  first_fragment.EnsureId();
  first_fragment.SetLayer(MakeGarbageCollected<PaintLayer>(this));
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
    OutlineRectCollector& collector,
    const PhysicalOffset& additional_offset,
    OutlineType include_block_overflows) const {
  NOT_DESTROYED();
  for (LayoutObject* child = SlowFirstChild(); child;
       child = child->NextSibling()) {
    // Outlines of out-of-flow positioned descendants are handled in
    // LayoutBlock::AddOutlineRects().
    if (child->IsOutOfFlowPositioned())
      continue;

    AddOutlineRectsForDescendant(*child, collector, additional_offset,
                                 include_block_overflows);
  }
}

void LayoutBoxModelObject::AddOutlineRectsForDescendant(
    const LayoutObject& descendant,
    OutlineRectCollector& collector,
    const PhysicalOffset& additional_offset,
    OutlineType include_block_overflows) const {
  NOT_DESTROYED();
  if (descendant.IsText()) {
    return;
  }

  if (descendant.HasLayer()) {
    std::unique_ptr<OutlineRectCollector> descendant_collector =
        collector.ForDescendantCollector();
    descendant.AddOutlineRects(*descendant_collector, nullptr, PhysicalOffset(),
                               include_block_overflows);
    collector.Combine(descendant_collector.get(), descendant, this,
                      additional_offset);
    return;
  }

  if (descendant.IsBox()) {
    descendant.AddOutlineRects(
        collector, nullptr,
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
        .AddOutlineRectsForNormalChildren(collector, additional_offset,
                                          include_block_overflows);
    return;
  }

  descendant.AddOutlineRects(collector, nullptr, additional_offset,
                             include_block_overflows);
}

void LayoutBoxModelObject::RecalcVisualOverflow() {
  // |PaintLayer| calls this function when |HasSelfPaintingLayer|. When |this|
  // is an inline box or an atomic inline, its ink overflow is stored in
  // |FragmentItem| in the inline formatting context.
  if (IsInline() && IsInLayoutNGInlineFormattingContext()) {
    DCHECK(HasSelfPaintingLayer());
    InlineCursor cursor;
    InlinePaintContext inline_context;
    for (cursor.MoveTo(*this); cursor; cursor.MoveToNextForSameLayoutObject()) {
      InlinePaintContext::ScopedInlineBoxAncestors scoped_items(
          cursor, &inline_context);
      cursor.Current().RecalcInkOverflow(cursor, &inline_context);
    }
    return;
  }

  LayoutObject::RecalcVisualOverflow();
}

bool LayoutBoxModelObject::ShouldBeHandledAsInline(
    const ComputedStyle& style) const {
  if (style.IsDisplayInlineType()) {
    return true;
  }
  // Table-internal display types create anonymous inline or block <table>s
  // depending on the parent. But if an element with a table-internal display
  // type creates a domain-specific LayoutObject such as LayoutImage, such
  // anonymous <table> is not created, and the LayoutObject should adjust
  // IsInline flag for inlinifying.
  //
  // LayoutRubyBase and LayoutRubyText should be blocks even in a ruby.
  return style.IsInInlinifyingDisplay() && !IsTablePart() && !IsRubyBase() &&
         !IsRubyText();
}

void LayoutBoxModelObject::UpdateFromStyle() {
  NOT_DESTROYED();
  const ComputedStyle& style_to_use = StyleRef();
  SetHasBoxDecorationBackground(style_to_use.HasBoxDecorationBackground());
  SetInline(ShouldBeHandledAsInline(style_to_use));
  SetPositionState(style_to_use.GetPosition());
  SetHorizontalWritingMode(style_to_use.IsHorizontalWritingMode());
  SetCanContainAbsolutePositionObjects(
      ComputeIsAbsoluteContainer(&style_to_use));
  SetCanContainFixedPositionObjects(ComputeIsFixedContainer(&style_to_use));
  SetIsBackgroundAttachmentFixedObject(
      !BackgroundTransfersToView() &&
      StyleRef().HasFixedAttachmentBackgroundImage());
}

void LayoutBoxModelObject::UpdateCanCompositeBackgroundAttachmentFixed(
    bool enable_composited_background_attachment_fixed) {
  SetCanCompositeBackgroundAttachmentFixed(
      enable_composited_background_attachment_fixed &&
      ComputeCanCompositeBackgroundAttachmentFixed());
}

PhysicalRect LayoutBoxModelObject::VisualOverflowRectIncludingFilters() const {
  NOT_DESTROYED();
  return ApplyFiltersToRect(VisualOverflowRect());
}

PhysicalRect LayoutBoxModelObject::ApplyFiltersToRect(
    const PhysicalRect& rect) const {
  NOT_DESTROYED();
  if (!HasReflection() && !StyleRef().HasFilter()) {
    return rect;
  }
  gfx::RectF float_rect(rect);
  if (auto* layer = Layer()) {
    const gfx::RectF filter_reference_box = layer->FilterReferenceBox();
    if (!filter_reference_box.size().IsZero()) {
      float_rect.UnionEvenIfEmpty(filter_reference_box);
    }
    float_rect = layer->MapRectForFilter(float_rect);
  } else {
    CHECK(IsSVGChild());
    const gfx::RectF filter_reference_box =
        SVGResources::ReferenceBoxForEffects(*this);
    if (!filter_reference_box.size().IsZero()) {
      float_rect.UnionEvenIfEmpty(filter_reference_box);
    }
    float_rect = StyleRef().Filter().MapRect(float_rect);
  }
  return PhysicalRect::EnclosingRect(float_rect);
}

LayoutBlock* LayoutBoxModelObject::StickyContainer() const {
  return ContainingBlock();
}

StickyPositionScrollingConstraints*
LayoutBoxModelObject::ComputeStickyPositionConstraints() const {
  NOT_DESTROYED();
  DCHECK(StyleRef().HasStickyConstrainedPosition());

  StickyPositionScrollingConstraints* constraints =
      MakeGarbageCollected<StickyPositionScrollingConstraints>();

  bool is_fixed_to_view = false;
  const auto* scroll_container_layer =
      Layer()->ContainingScrollContainerLayer(&is_fixed_to_view);
  constraints->containing_scroll_container_layer = scroll_container_layer;
  constraints->is_fixed_to_view = is_fixed_to_view;

  // Skip anonymous containing blocks except for anonymous fieldset content box.
  LayoutBlock* sticky_container = StickyContainer();
  while (sticky_container->IsAnonymous()) {
    if (sticky_container->Parent() &&
        sticky_container->Parent()->IsFieldset()) {
      break;
    }
    sticky_container = sticky_container->ContainingBlock();
  }

  const auto* scroll_container = scroll_container_layer->GetLayoutBox();
  DCHECK(scroll_container);
  const PhysicalOffset scroll_container_border_offset(
      scroll_container->BorderLeft(), scroll_container->BorderTop());

  MapCoordinatesFlags flags =
      kIgnoreTransforms | kIgnoreScrollOffset | kIgnoreStickyOffset;

  // Compute the sticky-container rect.
  {
    PhysicalRect scroll_container_relative_containing_block_rect;
    if (sticky_container == scroll_container) {
      scroll_container_relative_containing_block_rect =
          sticky_container->ScrollableOverflowRect();
    } else {
      PhysicalRect local_rect = sticky_container->PhysicalPaddingBoxRect();
      scroll_container_relative_containing_block_rect =
          sticky_container->LocalToAncestorRect(local_rect, scroll_container,
                                                flags);
    }

    // Make relative to the padding-box instead of border-box.
    scroll_container_relative_containing_block_rect.Move(
        -scroll_container_border_offset);

    // This is removing the padding of the containing block's overflow rect to
    // get the flow box rectangle and removing the margin of the sticky element
    // to ensure that space between the sticky element and its containing flow
    // box. It is an open issue whether the margin should collapse. See
    // https://www.w3.org/TR/css-position-3/#sticky-pos
    scroll_container_relative_containing_block_rect.Contract(
        sticky_container->PaddingOutsets());
    if (!RuntimeEnabledFeatures::LayoutIgnoreMarginsForStickyEnabled()) {
      // Sticky positioned element ignore any override logical width on the
      // containing block, as they don't call
      // containingBlockLogicalWidthForContent.
      // It's unclear whether this is totally fine.
      // Compute the container-relative area within which the sticky element is
      // allowed to move.
      LayoutUnit max_width = sticky_container->AvailableLogicalWidth();
      scroll_container_relative_containing_block_rect.ContractEdges(
          MinimumValueForLength(StyleRef().MarginTop(), max_width),
          MinimumValueForLength(StyleRef().MarginRight(), max_width),
          MinimumValueForLength(StyleRef().MarginBottom(), max_width),
          MinimumValueForLength(StyleRef().MarginLeft(), max_width));
    }

    constraints->scroll_container_relative_containing_block_rect =
        scroll_container_relative_containing_block_rect;
  }

  // The location container for boxes is not always the containing block.
  LayoutObject* location_container =
      IsLayoutInline() ? Container() : To<LayoutBox>(this)->LocationContainer();

  // Compute the sticky-box rect.
  PhysicalRect sticky_box_rect;
  {
    if (IsLayoutInline()) {
      sticky_box_rect = To<LayoutInline>(this)->PhysicalLinesBoundingBox();
    } else {
      const LayoutBox& box = To<LayoutBox>(*this);
      sticky_box_rect = PhysicalRect(box.PhysicalLocation(), box.Size());
    }

    PhysicalRect scroll_container_relative_sticky_box_rect =
        location_container->LocalToAncestorRect(sticky_box_rect,
                                                scroll_container, flags);

    // Make relative to the padding-box instead of border-box.
    scroll_container_relative_sticky_box_rect.Move(
        -scroll_container_border_offset);
    constraints->scroll_container_relative_sticky_box_rect =
        scroll_container_relative_sticky_box_rect;
  }

  // To correctly compute the offsets, the constraints need to know about any
  // nested sticky elements between themselves and their sticky-container,
  // and between the sticky-container and their scroll-container.
  //
  // The respective search ranges are [location_container, sticky_container)
  // and [sticky_container, scroll_container).
  constraints->nearest_sticky_layer_shifting_sticky_box =
      location_container->FindFirstStickyContainer(sticky_container);
  constraints->nearest_sticky_layer_shifting_containing_block =
      sticky_container->FindFirstStickyContainer(scroll_container);

  constraints->constraining_rect =
      scroll_container->ComputeStickyConstrainingRect();

  // Compute the insets.
  {
    auto ResolveInset = [](const Length& length,
                           LayoutUnit size) -> std::optional<LayoutUnit> {
      if (length.IsAuto()) {
        return std::nullopt;
      }
      return MinimumValueForLength(length, size);
    };

    const PhysicalSize available_size = constraints->constraining_rect.size;
    const auto& style = StyleRef();
    std::optional<LayoutUnit> left =
        ResolveInset(style.Left(), available_size.width);
    std::optional<LayoutUnit> right =
        ResolveInset(style.Right(), available_size.width);
    std::optional<LayoutUnit> top =
        ResolveInset(style.Top(), available_size.height);
    std::optional<LayoutUnit> bottom =
        ResolveInset(style.Bottom(), available_size.height);

    // Skip the end inset if there is not enough space to honor both insets.
    if (left && right) {
      if (*left + *right + sticky_box_rect.Width() > available_size.width) {
        if (style.IsLeftToRightDirection()) {
          right = std::nullopt;
        } else {
          left = std::nullopt;
        }
      }
    }
    if (top && bottom) {
      // TODO(flackr): Exclude top or bottom edge offset depending on the
      // writing mode when related sections are fixed in spec. See
      // http://lists.w3.org/Archives/Public/www-style/2014May/0286.html
      if (*top + *bottom + sticky_box_rect.Height() > available_size.height) {
        bottom = std::nullopt;
      }
    }

    constraints->left_inset = left;
    constraints->right_inset = right;
    constraints->top_inset = top;
    constraints->bottom_inset = bottom;
  }

  return constraints;
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
      if (IsStickyPositioned()) {
        reference_point += StickyPositionOffset();
      }

      // Note that we may fail to find |offsetParent| while walking the
      // container chain, if |offsetParent| is an inline split into
      // continuations: <body style="display:inline;" id="offsetParent">
      // <div id="this">
      // This is why we have to do a nullptr check here.
      for (const LayoutObject* current = Container();
           current && current->GetNode() != offset_parent;
           current = current->Container()) {
        // FIXME: What are we supposed to do inside SVG content?
        reference_point += current->ColumnOffset(reference_point);
        if (current->IsBox()) {
          reference_point += To<LayoutBox>(current)->PhysicalLocation();
        }
      }

      if (offset_parent_object->IsBox() && offset_parent_object->IsBody() &&
          !offset_parent_object->IsPositioned()) {
        reference_point +=
            To<LayoutBox>(offset_parent_object)->PhysicalLocation();
      }
    } else if (IsBox() &&
               To<LayoutBox>(this)->NeedsAnchorPositionScrollAdjustment())
        [[unlikely]] {
      reference_point +=
          To<LayoutBox>(this)->AnchorPositionScrollTranslationOffset();
    }

    if (offset_parent_object->IsLayoutInline()) {
      const auto* inline_parent = To<LayoutInline>(offset_parent_object);
      reference_point -= inline_parent->FirstLineBoxTopLeft();
    }

    if (offset_parent_object->IsBox() && !offset_parent_object->IsBody()) {
      auto* box = To<LayoutBox>(offset_parent_object);
      reference_point -= PhysicalOffset(box->BorderLeft(), box->BorderTop());
    }
  }

  return reference_point;
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

LayoutUnit LayoutBoxModelObject::ComputedCSSPadding(
    const Length& padding) const {
  NOT_DESTROYED();
  LayoutUnit w;
  if (padding.HasPercent()) {
    w = ContainingBlockLogicalWidthForContent();
  }
  return MinimumValueForLength(padding, w);
}

LayoutUnit LayoutBoxModelObject::ContainingBlockLogicalWidthForContent() const {
  NOT_DESTROYED();
  return ContainingBlock()->AvailableLogicalWidth();
}

LogicalRect LayoutBoxModelObject::LocalCaretRectForEmptyElement(
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
  BoxStrut border_padding =
      (BorderOutsets() + PaddingOutsets())
          .ConvertToLogical(
              {current_style.GetWritingMode(), TextDirection::kLtr});
  x = border_padding.inline_start;
  max_x = width - border_padding.inline_end;
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
  LayoutUnit vertical_space = FirstLineHeight() - height;
  LayoutUnit block_start = border_padding.block_start + (vertical_space / 2);
  return LogicalRect(x, block_start, caret_width, height);
}

void LayoutBoxModelObject::MoveChildTo(
    LayoutBoxModelObject* to_box_model_object,
    LayoutObject* child,
    LayoutObject* before_child,
    bool full_remove_insert) {
  NOT_DESTROYED();
  DCHECK_EQ(this, child->Parent());
  DCHECK(!before_child || to_box_model_object == before_child->Parent());

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
      // dirty.
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

  if (box_at_top_of_new_branch) {
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
