// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"

#include <limits>

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/layout/block_node.h"
#include "third_party/blink/renderer/core/layout/constraint_space_builder.h"
#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_info.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/paint/clip_path_clipper.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/paint/scoped_svg_paint_state.h"
#include "third_party/blink/renderer/core/paint/svg_model_object_painter.h"
#include "third_party/blink/renderer/core/svg/svg_text_element.h"

namespace blink {

namespace {

const LayoutSVGText* FindTextRoot(const LayoutObject* start) {
  DCHECK(start);
  for (; start; start = start->Parent()) {
    if (const auto* ng_text = DynamicTo<LayoutSVGText>(start)) {
      return ng_text;
    }
  }
  return nullptr;
}

}  // namespace

LayoutSVGText::LayoutSVGText(Element* element)
    : LayoutSVGBlock(element),
      needs_update_bounding_box_(true),
      needs_text_metrics_update_(true) {
  DCHECK(IsA<SVGTextElement>(element));
}

void LayoutSVGText::StyleDidChange(StyleDifference diff,
                                   const ComputedStyle* old_style) {
  NOT_DESTROYED();
  if (needs_text_metrics_update_ && diff.HasDifference() && old_style) {
    diff.SetNeedsFullLayout();
  }
  LayoutSVGBlock::StyleDidChange(diff, old_style);
  SVGResources::UpdatePaints(*this, old_style, StyleRef());

  if (old_style) {
    const ComputedStyle& style = StyleRef();
    if (transform_uses_reference_box_ && !needs_transform_update_) {
      if (TransformHelper::CheckReferenceBoxDependencies(*old_style, style)) {
        SetNeedsTransformUpdate();
        SetNeedsPaintPropertyUpdate();
      }
    }
  }
}

void LayoutSVGText::WillBeDestroyed() {
  NOT_DESTROYED();
  SVGResources::ClearPaints(*this, Style());
  LayoutSVGBlock::WillBeDestroyed();
}

const char* LayoutSVGText::GetName() const {
  NOT_DESTROYED();
  return "LayoutSVGText";
}

bool LayoutSVGText::CreatesNewFormattingContext() const {
  NOT_DESTROYED();
  return true;
}

void LayoutSVGText::UpdateFromStyle() {
  NOT_DESTROYED();
  LayoutSVGBlock::UpdateFromStyle();
  SetHasNonVisibleOverflow(false);
}

bool LayoutSVGText::IsChildAllowed(LayoutObject* child,
                                   const ComputedStyle&) const {
  NOT_DESTROYED();
  return child->IsSVGInline() ||
         (child->IsText() && SVGLayoutSupport::IsLayoutableTextNode(child));
}

void LayoutSVGText::AddChild(LayoutObject* child, LayoutObject* before_child) {
  NOT_DESTROYED();
  LayoutSVGBlock::AddChild(child, before_child);
  SubtreeStructureChanged(layout_invalidation_reason::kChildChanged);
}

void LayoutSVGText::RemoveChild(LayoutObject* child) {
  NOT_DESTROYED();
  SubtreeStructureChanged(layout_invalidation_reason::kChildChanged);
  LayoutSVGBlock::RemoveChild(child);
}

void LayoutSVGText::InsertedIntoTree() {
  NOT_DESTROYED();
  LayoutSVGBlock::InsertedIntoTree();
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

void LayoutSVGText::WillBeRemovedFromTree() {
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
  LayoutSVGBlock::WillBeRemovedFromTree();
}

void LayoutSVGText::SubtreeStructureChanged(
    LayoutInvalidationReasonForTracing) {
  NOT_DESTROYED();
  if (BeingDestroyed() || !EverHadLayout()) {
    return;
  }
  if (DocumentBeingDestroyed()) {
    return;
  }

  SetNeedsTextMetricsUpdate();
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(*this);
}

void LayoutSVGText::UpdateFont() {
  for (LayoutObject* descendant = FirstChild(); descendant;
       descendant = descendant->NextInPreOrder(this)) {
    if (auto* text = DynamicTo<LayoutSVGInlineText>(descendant)) {
      text->UpdateScaledFont();
    }
  }
}

void LayoutSVGText::UpdateTransformAffectsVectorEffect() {
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

void LayoutSVGText::Paint(const PaintInfo& paint_info) const {
  if (paint_info.phase != PaintPhase::kForeground &&
      paint_info.phase != PaintPhase::kForcedColorsModeBackplate &&
      paint_info.phase != PaintPhase::kSelectionDragImage) {
    return;
  }

  ScopedSVGTransformState transform_state(paint_info, *this);
  PaintInfo& block_info = transform_state.ContentPaintInfo();
  if (const auto* properties = FirstFragment().PaintProperties()) {
    // TODO(https://crbug.com/1278452): Also consider Translate, Rotate,
    // Scale, and Offset, probably via a single transform operation to
    // FirstFragment().PreTransform().
    if (const auto* transform = properties->Transform()) {
      block_info.TransformCullRect(*transform);
    }
  }

  if (block_info.phase == PaintPhase::kForeground) {
    SVGModelObjectPainter::RecordHitTestData(*this, block_info);
    SVGModelObjectPainter::RecordRegionCaptureData(*this, block_info);
  }
  LayoutSVGBlock::Paint(block_info);

  // Svg doesn't follow HTML PaintPhases, but is implemented with HTML classes.
  // The nearest self-painting layer is the containing <svg> element which is
  // painted using ReplacedPainter and ignores kDescendantOutlinesOnly.
  // Begin a fake kOutline to paint outlines, if any.
  if (paint_info.phase == PaintPhase::kForeground) {
    block_info.phase = PaintPhase::kOutline;
    LayoutSVGBlock::Paint(block_info);
  }
}

SVGLayoutResult LayoutSVGText::UpdateSVGLayout(
    const SVGLayoutInfo& layout_info) {
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
    UpdateTransformBeforeLayout();
    UpdateFont();
    SetNeedsCollectInlines(true);
    needs_text_metrics_update_ = false;
  }

  const gfx::RectF old_boundaries = ObjectBoundingBox();

  const ComputedStyle& style = StyleRef();
  ConstraintSpaceBuilder builder(
      style.GetWritingMode(), style.GetWritingDirection(),
      /* is_new_fc */ true, /* adjust_inline_size_if_needed */ false);
  builder.SetAvailableSize(LogicalSize());
  BlockNode(this).Layout(builder.ToConstraintSpace());

  needs_update_bounding_box_ = true;

  const gfx::RectF boundaries = ObjectBoundingBox();
  const bool bounds_changed = old_boundaries != boundaries;

  SVGLayoutResult result;
  if (bounds_changed) {
    result.bounds_changed = true;
  }
  if (UpdateAfterSVGLayout(layout_info, bounds_changed)) {
    result.bounds_changed = true;
  }

  if (result.bounds_changed) {
    DeprecatedInvalidateIntersectionObserverCachedRects();
  }

  return result;
}

bool LayoutSVGText::UpdateAfterSVGLayout(const SVGLayoutInfo& layout_info,
                                         bool bounds_changed) {
  if (bounds_changed) {
    // Invalidate all resources of this client if our reference box changed.
    SVGResourceInvalidator resource_invalidator(*this);
    resource_invalidator.InvalidateEffects();
    resource_invalidator.InvalidatePaints();
  }

  UpdateTransformAffectsVectorEffect();
  return UpdateTransformAfterLayout(layout_info, bounds_changed);
}

bool LayoutSVGText::IsObjectBoundingBoxValid() const {
  NOT_DESTROYED();
  return PhysicalFragments().HasFragmentItems();
}

gfx::RectF LayoutSVGText::ObjectBoundingBox() const {
  NOT_DESTROYED();
  if (needs_update_bounding_box_) {
    // Compute a box containing repositioned text in the non-scaled coordinate.
    // We don't need to take into account of ink overflow here. We should
    // return a union of "advance x EM height".
    // https://svgwg.org/svg2-draft/coords.html#BoundingBoxes
    gfx::RectF bbox;
    DCHECK_LE(PhysicalFragmentCount(), 1u);
    for (const auto& fragment : PhysicalFragments()) {
      if (!fragment.Items()) {
        continue;
      }
      for (const auto& item : fragment.Items()->Items()) {
        if (item.IsSvgText()) {
          // Do not use item.RectInContainerFragment() in order to avoid
          // precision loss.
          bbox.Union(item.ObjectBoundingBox(*fragment.Items()));
        }
      }
    }
    bounding_box_ = bbox;
    needs_update_bounding_box_ = false;
  }
  return bounding_box_;
}

gfx::RectF LayoutSVGText::StrokeBoundingBox() const {
  NOT_DESTROYED();
  gfx::RectF box = ObjectBoundingBox();
  if (box.IsEmpty()) {
    return gfx::RectF();
  }
  return SVGLayoutSupport::ExtendTextBBoxWithStroke(*this, box);
}

gfx::RectF LayoutSVGText::DecoratedBoundingBox() const {
  NOT_DESTROYED();
  return StrokeBoundingBox();
}

gfx::RectF LayoutSVGText::VisualRectInLocalSVGCoordinates() const {
  NOT_DESTROYED();
  // TODO(crbug.com/1179585): Just use ink overflow?
  gfx::RectF box = ObjectBoundingBox();
  if (box.IsEmpty()) {
    return gfx::RectF();
  }
  return SVGLayoutSupport::ComputeVisualRectForText(*this, box);
}

void LayoutSVGText::QuadsInAncestorInternal(
    Vector<gfx::QuadF>& quads,
    const LayoutBoxModelObject* ancestor,
    MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  quads.push_back(
      LocalToAncestorQuad(gfx::QuadF(DecoratedBoundingBox()), ancestor, mode));
}

gfx::RectF LayoutSVGText::LocalBoundingBoxRectForAccessibility() const {
  NOT_DESTROYED();
  return DecoratedBoundingBox();
}

bool LayoutSVGText::NodeAtPoint(HitTestResult& result,
                                const HitTestLocation& hit_test_location,
                                const PhysicalOffset& accumulated_offset,
                                HitTestPhase phase) {
  TransformedHitTestLocation local_location(hit_test_location,
                                            LocalToSVGParentTransform());
  if (!local_location) {
    return false;
  }

  if (HasClipPath() && !ClipPathClipper::HitTest(*this, *local_location)) {
    return false;
  }

  return LayoutSVGBlock::NodeAtPoint(result, *local_location,
                                     accumulated_offset, phase);
}

PositionWithAffinity LayoutSVGText::PositionForPoint(
    const PhysicalOffset& point_in_contents) const {
  NOT_DESTROYED();
  gfx::PointF point(point_in_contents.left, point_in_contents.top);
  float min_distance = std::numeric_limits<float>::max();
  const LayoutSVGInlineText* closest_inline_text = nullptr;
  for (const LayoutObject* descendant = FirstChild(); descendant;
       descendant = descendant->NextInPreOrder(this)) {
    const auto* text = DynamicTo<LayoutSVGInlineText>(descendant);
    if (!text) {
      continue;
    }
    float distance =
        (descendant->ObjectBoundingBox().ClosestPoint(point) - point)
            .LengthSquared();
    if (distance >= min_distance) {
      continue;
    }
    min_distance = distance;
    closest_inline_text = text;
  }
  if (!closest_inline_text) {
    return CreatePositionWithAffinity(0);
  }
  return closest_inline_text->PositionForPoint(point_in_contents);
}

void LayoutSVGText::SetNeedsPositioningValuesUpdate() {
  NOT_DESTROYED();
  // We resolve text layout attributes in CollectInlines().
  // Do not use SetNeedsCollectInlines() without arguments.
  SetNeedsCollectInlines(true);
}

void LayoutSVGText::SetNeedsTextMetricsUpdate() {
  NOT_DESTROYED();
  needs_text_metrics_update_ = true;
  // We need to re-shape text.
  SetNeedsCollectInlines(true);
}

bool LayoutSVGText::NeedsTextMetricsUpdate() const {
  NOT_DESTROYED();
  return needs_text_metrics_update_;
}

LayoutSVGText* LayoutSVGText::LocateLayoutSVGTextAncestor(LayoutObject* start) {
  return const_cast<LayoutSVGText*>(FindTextRoot(start));
}

const LayoutSVGText* LayoutSVGText::LocateLayoutSVGTextAncestor(
    const LayoutObject* start) {
  return FindTextRoot(start);
}

// static
void LayoutSVGText::NotifySubtreeStructureChanged(
    LayoutObject* object,
    LayoutInvalidationReasonForTracing reason) {
  if (auto* ng_text = LocateLayoutSVGTextAncestor(object)) {
    ng_text->SubtreeStructureChanged(reason);
  }
}

}  // namespace blink
