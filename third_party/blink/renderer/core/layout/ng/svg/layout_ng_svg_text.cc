// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_text.h"

#include <limits>

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_fragment_item.h"
#include "third_party/blink/renderer/core/layout/ng/ng_block_node.h"
#include "third_party/blink/renderer/core/layout/ng/ng_constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/scoped_svg_paint_state.h"
#include "third_party/blink/renderer/core/paint/svg_model_object_painter.h"
#include "third_party/blink/renderer/core/svg/svg_text_element.h"

namespace blink {

namespace {

const LayoutNGSVGText* FindTextRoot(const LayoutObject* start) {
  DCHECK(start);
  for (; start; start = start->Parent()) {
    if (const auto* ng_text = DynamicTo<LayoutNGSVGText>(start)) {
      return ng_text;
    }
  }
  return nullptr;
}

}  // namespace

LayoutNGSVGText::LayoutNGSVGText(Element* element)
    : LayoutNGBlockFlowMixin<LayoutSVGBlock>(element),
      needs_update_bounding_box_(true),
      needs_text_metrics_update_(true) {
  DCHECK(IsA<SVGTextElement>(element));
}

void LayoutNGSVGText::StyleDidChange(StyleDifference diff,
                                     const ComputedStyle* old_style) {
  NOT_DESTROYED();
  if (needs_text_metrics_update_ && diff.HasDifference() && old_style)
    diff.SetNeedsFullLayout();
  LayoutNGBlockFlowMixin<LayoutSVGBlock>::StyleDidChange(diff, old_style);
  SVGResources::UpdatePaints(*this, old_style, StyleRef());
}

void LayoutNGSVGText::WillBeDestroyed() {
  NOT_DESTROYED();
  SVGResources::ClearPaints(*this, Style());
  LayoutNGBlockFlowMixin<LayoutSVGBlock>::WillBeDestroyed();
}

const char* LayoutNGSVGText::GetName() const {
  NOT_DESTROYED();
  return "LayoutNGSVGText";
}

bool LayoutNGSVGText::IsOfType(LayoutObjectType type) const {
  NOT_DESTROYED();
  return type == kLayoutObjectNGSVGText ||
         LayoutNGBlockFlowMixin<LayoutSVGBlock>::IsOfType(type);
}

bool LayoutNGSVGText::CreatesNewFormattingContext() const {
  NOT_DESTROYED();
  return true;
}

void LayoutNGSVGText::UpdateFromStyle() {
  NOT_DESTROYED();
  LayoutNGBlockFlowMixin<LayoutSVGBlock>::UpdateFromStyle();
  SetHasNonVisibleOverflow(false);
}

bool LayoutNGSVGText::IsChildAllowed(LayoutObject* child,
                                     const ComputedStyle&) const {
  NOT_DESTROYED();
  return child->IsSVGInline() ||
         (child->IsText() && SVGLayoutSupport::IsLayoutableTextNode(child));
}

void LayoutNGSVGText::AddChild(LayoutObject* child,
                               LayoutObject* before_child) {
  NOT_DESTROYED();
  LayoutSVGBlock::AddChild(child, before_child);
  SubtreeStructureChanged(layout_invalidation_reason::kChildChanged);
}

void LayoutNGSVGText::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  SubtreeStructureChanged(layout_invalidation_reason::kChildChanged);
  LayoutSVGBlock::RemoveChild(child);
}

void LayoutNGSVGText::InsertedIntoTree() {
  NOT_DESTROYED();
  LayoutNGBlockFlowMixin<LayoutSVGBlock>::InsertedIntoTree();
  bool seen_svg_root = false;
  for (auto* ancestor = Parent(); ancestor; ancestor = ancestor->Parent()) {
    auto* root = DynamicTo<LayoutSVGRoot>(ancestor);
    if (!seen_svg_root && root) {
      root->AddSvgTextDescendant(*this);
      seen_svg_root = true;
    } else if (auto* block = DynamicTo<LayoutBlock>(ancestor)) {
      block->AddSvgTextDescendant(*this);
    }
  }
}

void LayoutNGSVGText::WillBeRemovedFromTree() {
  NOT_DESTROYED();
  bool seen_svg_root = false;
  for (auto* ancestor = Parent(); ancestor; ancestor = ancestor->Parent()) {
    auto* root = DynamicTo<LayoutSVGRoot>(ancestor);
    if (!seen_svg_root && root) {
      root->RemoveSvgTextDescendant(*this);
      seen_svg_root = true;
    } else if (auto* block = DynamicTo<LayoutBlock>(ancestor)) {
      block->RemoveSvgTextDescendant(*this);
    }
  }
  LayoutNGBlockFlowMixin<LayoutSVGBlock>::WillBeRemovedFromTree();
}

void LayoutNGSVGText::SubtreeStructureChanged(
    LayoutInvalidationReasonForTracing) {
  NOT_DESTROYED();
  if (BeingDestroyed() || !EverHadLayout())
    return;
  if (DocumentBeingDestroyed())
    return;

  SetNeedsTextMetricsUpdate();
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(*this);
}

void LayoutNGSVGText::UpdateFont() {
  for (LayoutObject* descendant = FirstChild(); descendant;
       descendant = descendant->NextInPreOrder(this)) {
    if (auto* text = DynamicTo<LayoutSVGInlineText>(descendant))
      text->UpdateScaledFont();
  }
}

void LayoutNGSVGText::UpdateTransformAffectsVectorEffect() {
  if (StyleRef().VectorEffect() == EVectorEffect::kNonScalingStroke) {
    SetTransformAffectsVectorEffect(true);
    return;
  }

  SetTransformAffectsVectorEffect(false);
  for (LayoutObject* descendant = FirstChild(); descendant;
       descendant = descendant->NextInPreOrder(this)) {
    if (descendant->IsSVGInline() && descendant->StyleRef().VectorEffect() ==
                                         EVectorEffect::kNonScalingStroke) {
      SetTransformAffectsVectorEffect(true);
      break;
    }
  }
}

void LayoutNGSVGText::Paint(const PaintInfo& paint_info) const {
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kForcedColorsModeBackplate &&
      paint_info.phase != PaintPhase::kSelectionDragImage) {
    return;
  }

  PaintInfo block_info(paint_info);
  if (const auto* properties = FirstFragment().PaintProperties()) {
    // TODO(https://crbug.com/1278452): Also consider Translate, Rotate,
    // Scale, and Offset, probably via a single transform operation to
    // FirstFragment().PreTransform().
    if (const auto* transform = properties->Transform())
      block_info.TransformCullRect(*transform);
  }
  ScopedSVGTransformState transform_state(block_info, *this);

  if (block_info.phase == PaintPhase::kForeground) {
    SVGModelObjectPainter::RecordHitTestData(*this, block_info);
    SVGModelObjectPainter::RecordRegionCaptureData(*this, block_info);
  }
  LayoutNGBlockFlowMixin<LayoutSVGBlock>::Paint(block_info);

  // Svg doesn't follow HTML PaintPhases, but is implemented with HTML classes.
  // The nearest self-painting layer is the containing <svg> element which is
  // painted using ReplacedPainter and ignores kDescendantOutlinesOnly.
  // Begin a fake kOutline to paint outlines, if any.
  if (paint_info.phase == PaintPhase::kForeground) {
    block_info.phase = PaintPhase::kOutline;
    LayoutNGBlockFlowMixin<LayoutSVGBlock>::Paint(block_info);
  }
}

void LayoutNGSVGText::UpdateBlockLayout(bool relayout_children) {
  NOT_DESTROYED();

  // If the root layout size changed (eg. window size changes), or the screen
  // scale factor has changed, then recompute the on-screen font size. Since
  // the computation of layout attributes uses the text metrics, we need to
  // update them before updating the layout attributes.
  if (needs_text_metrics_update_ || needs_transform_update_) {
    // Recompute the transform before updating font and corresponding
    // metrics. At this point our bounding box may be incorrect, so
    // any box relative transforms will be incorrect. Since the scaled
    // font size only needs the scaling components to be correct, this
    // should be fine. We update the transform again after computing
    // the bounding box below, and after that we clear the
    // |needs_transform_update_| flag.
    if (needs_transform_update_) {
      local_transform_ =
          GetElement()->CalculateTransform(SVGElement::kIncludeMotionTransform);
    }

    UpdateFont();
    SetNeedsCollectInlines(true);
    needs_text_metrics_update_ = false;
  }

  gfx::RectF old_boundaries = ObjectBoundingBox();

  if (RuntimeEnabledFeatures::LayoutNewSVGTextEntryEnabled()) {
    const ComputedStyle& style = StyleRef();
    NGConstraintSpaceBuilder builder(
        style.GetWritingMode(), style.GetWritingDirection(),
        /* is_new_fc */ true, /* adjust_inline_size_if_needed */ false);
    builder.SetAvailableSize(LogicalSize());
    NGBlockNode(this).Layout(builder.ToConstraintSpace());
  } else {
    UpdateNGBlockLayout();
  }

  needs_update_bounding_box_ = true;

  gfx::RectF boundaries = ObjectBoundingBox();
  const bool bounds_changed = old_boundaries != boundaries;
  if (bounds_changed) {
    // Invalidate all resources of this client if our reference box changed.
    SVGResourceInvalidator resource_invalidator(*this);
    resource_invalidator.InvalidateEffects();
    resource_invalidator.InvalidatePaints();
  }

  // If our bounds changed, notify the parents.
  if (UpdateTransformAfterLayout(bounds_changed) || bounds_changed)
    SetNeedsBoundariesUpdate();

  UpdateTransformAffectsVectorEffect();
}

bool LayoutNGSVGText::IsObjectBoundingBoxValid() const {
  NOT_DESTROYED();
  return PhysicalFragments().HasFragmentItems();
}

gfx::RectF LayoutNGSVGText::ObjectBoundingBox() const {
  NOT_DESTROYED();
  if (needs_update_bounding_box_) {
    // Compute a box containing repositioned text in the non-scaled coordinate.
    // We don't need to take into account of ink overflow here. We should
    // return a union of "advance x EM height".
    // https://svgwg.org/svg2-draft/coords.html#BoundingBoxes
    gfx::RectF bbox;
    DCHECK_LE(PhysicalFragmentCount(), 1u);
    for (const auto& fragment : PhysicalFragments()) {
      if (!fragment.Items())
        continue;
      for (const auto& item : fragment.Items()->Items()) {
        if (item.Type() != NGFragmentItem::kSvgText)
          continue;
        // Do not use item.RectInContainerFragment() in order to avoid
        // precision loss.
        bbox.Union(item.ObjectBoundingBox(*fragment.Items()));
      }
    }
    bounding_box_ = bbox;
    needs_update_bounding_box_ = false;
  }
  return bounding_box_;
}

gfx::RectF LayoutNGSVGText::StrokeBoundingBox() const {
  NOT_DESTROYED();
  gfx::RectF box = ObjectBoundingBox();
  if (box.IsEmpty())
    return gfx::RectF();
  return SVGLayoutSupport::ExtendTextBBoxWithStroke(*this, box);
}

gfx::RectF LayoutNGSVGText::VisualRectInLocalSVGCoordinates() const {
  NOT_DESTROYED();
  // TODO(crbug.com/1179585): Just use ink overflow?
  gfx::RectF box = ObjectBoundingBox();
  if (box.IsEmpty())
    return gfx::RectF();
  return SVGLayoutSupport::ComputeVisualRectForText(*this, box);
}

void LayoutNGSVGText::AbsoluteQuads(Vector<gfx::QuadF>& quads,
                                    MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  quads.push_back(LocalToAbsoluteQuad(gfx::QuadF(StrokeBoundingBox()), mode));
}

gfx::RectF LayoutNGSVGText::LocalBoundingBoxRectForAccessibility() const {
  NOT_DESTROYED();
  return StrokeBoundingBox();
}

bool LayoutNGSVGText::NodeAtPoint(HitTestResult& result,
                                  const HitTestLocation& hit_test_location,
                                  const PhysicalOffset& accumulated_offset,
                                  HitTestPhase phase) {
  TransformedHitTestLocation local_location(hit_test_location,
                                            LocalToSVGParentTransform());
  if (!local_location)
    return false;

  if (!SVGLayoutSupport::IntersectsClipPath(*this, ObjectBoundingBox(),
                                            *local_location))
    return false;

  return LayoutNGBlockFlowMixin<LayoutSVGBlock>::NodeAtPoint(
      result, *local_location, accumulated_offset, phase);
}

PositionWithAffinity LayoutNGSVGText::PositionForPoint(
    const PhysicalOffset& point_in_contents) const {
  NOT_DESTROYED();
  gfx::PointF point(point_in_contents.left, point_in_contents.top);
  float min_distance = std::numeric_limits<float>::max();
  const LayoutSVGInlineText* closest_inline_text = nullptr;
  for (const LayoutObject* descendant = FirstChild(); descendant;
       descendant = descendant->NextInPreOrder(this)) {
    const auto* text = DynamicTo<LayoutSVGInlineText>(descendant);
    if (!text)
      continue;
    float distance =
        (descendant->ObjectBoundingBox().ClosestPoint(point) - point)
            .LengthSquared();
    if (distance >= min_distance)
      continue;
    min_distance = distance;
    closest_inline_text = text;
  }
  if (!closest_inline_text)
    return CreatePositionWithAffinity(0);
  return closest_inline_text->PositionForPoint(point_in_contents);
}

void LayoutNGSVGText::SetNeedsPositioningValuesUpdate() {
  NOT_DESTROYED();
  // We resolve text layout attributes in CollectInlines().
  // Do not use SetNeedsCollectInlines() without arguments.
  SetNeedsCollectInlines(true);
}

void LayoutNGSVGText::SetNeedsTextMetricsUpdate() {
  NOT_DESTROYED();
  needs_text_metrics_update_ = true;
  // We need to re-shape text.
  SetNeedsCollectInlines(true);
}

bool LayoutNGSVGText::NeedsTextMetricsUpdate() const {
  NOT_DESTROYED();
  return needs_text_metrics_update_;
}

LayoutNGSVGText* LayoutNGSVGText::LocateLayoutSVGTextAncestor(
    LayoutObject* start) {
  return const_cast<LayoutNGSVGText*>(FindTextRoot(start));
}

const LayoutNGSVGText* LayoutNGSVGText::LocateLayoutSVGTextAncestor(
    const LayoutObject* start) {
  return FindTextRoot(start);
}

// static
void LayoutNGSVGText::NotifySubtreeStructureChanged(
    LayoutObject* object,
    LayoutInvalidationReasonForTracing reason) {
  if (auto* ng_text = LocateLayoutSVGTextAncestor(object)) {
    ng_text->SubtreeStructureChanged(reason);
  }
}

}  // namespace blink
