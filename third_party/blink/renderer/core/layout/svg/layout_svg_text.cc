/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010-2012. All rights reserved.
 * Copyright (C) 2012 Google Inc.
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_text.h"

#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_state.h"
#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_text.h"
#include "third_party/blink/renderer/core/layout/pointer_events_hit_rules.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/line/svg_root_inline_box.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_text_layout_attributes_builder.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/paint/svg_text_painter.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/core/svg/svg_text_element.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "ui/gfx/geometry/quad_f.h"

namespace blink {

namespace {

const LayoutSVGBlock* FindTextRoot(const LayoutObject* start) {
  DCHECK(start);
  for (; start; start = start->Parent()) {
    if (const auto* text = DynamicTo<LayoutSVGText>(start))
      return text;
    if (const auto* ng_text = DynamicTo<LayoutNGSVGText>(start))
      return ng_text;
  }
  return nullptr;
}

}  // namespace

LayoutSVGText::LayoutSVGText(Element* node)
    : LayoutSVGBlock(To<SVGElement>(node)),
      needs_reordering_(false),
      needs_positioning_values_update_(false),
      needs_text_metrics_update_(false) {
  DCHECK(IsA<SVGTextElement>(node));
  UseCounter::Count(GetDocument(), WebFeature::kSVGText);
}

LayoutSVGText::~LayoutSVGText() {
  DCHECK(descendant_text_nodes_.empty());
}

void LayoutSVGText::Trace(Visitor* visitor) const {
  visitor->Trace(descendant_text_nodes_);
  LayoutSVGBlock::Trace(visitor);
}

void LayoutSVGText::StyleDidChange(StyleDifference diff,
                                   const ComputedStyle* old_style) {
  NOT_DESTROYED();
  LayoutSVGBlock::StyleDidChange(diff, old_style);
  SVGResources::UpdatePaints(*this, old_style, StyleRef());
}

void LayoutSVGText::WillBeDestroyed() {
  NOT_DESTROYED();
  descendant_text_nodes_.clear();
  SVGResources::ClearPaints(*this, Style());
  LayoutSVGBlock::WillBeDestroyed();
}

bool LayoutSVGText::IsChildAllowed(LayoutObject* child,
                                   const ComputedStyle&) const {
  NOT_DESTROYED();
  return child->IsSVGInline() ||
         (child->IsText() && SVGLayoutSupport::IsLayoutableTextNode(child));
}

LayoutSVGBlock* LayoutSVGText::LocateLayoutSVGTextAncestor(
    LayoutObject* start) {
  return const_cast<LayoutSVGBlock*>(FindTextRoot(start));
}

const LayoutSVGBlock* LayoutSVGText::LocateLayoutSVGTextAncestor(
    const LayoutObject* start) {
  return FindTextRoot(start);
}

static inline void CollectDescendantTextNodes(
    LayoutSVGText& text_root,
    HeapVector<Member<LayoutSVGInlineText>>& descendant_text_nodes) {
  for (LayoutObject* descendant = text_root.FirstChild(); descendant;
       descendant = descendant->NextInPreOrder(&text_root)) {
    if (descendant->IsSVGInlineText())
      descendant_text_nodes.push_back(To<LayoutSVGInlineText>(descendant));
  }
}

void LayoutSVGText::SubtreeStructureChanged(
    LayoutInvalidationReasonForTracing reason) {
  NOT_DESTROYED();
  if (BeingDestroyed() || !EverHadLayout()) {
    DCHECK(descendant_text_nodes_.empty());
    return;
  }
  if (DocumentBeingDestroyed())
    return;

  // The positioning elements cache depends on the size of each text
  // LayoutObject in the subtree. If this changes, clear the cache. It will be
  // rebuilt on the next layout.
  descendant_text_nodes_.clear();
  SetNeedsPositioningValuesUpdate();
  SetNeedsTextMetricsUpdate();
  // TODO(fs): Restore the passing of |reason| here.
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(*this);

  if (StyleRef().UsedUserModify() != EUserModify::kReadOnly)
    UseCounter::Count(GetDocument(), WebFeature::kSVGTextEdited);
}

void LayoutSVGText::NotifySubtreeStructureChanged(
    LayoutObject* object,
    LayoutInvalidationReasonForTracing reason) {
  if (LayoutSVGBlock* text_or_ng_text = LocateLayoutSVGTextAncestor(object)) {
    if (auto* layout_text = DynamicTo<LayoutSVGText>(text_or_ng_text))
      layout_text->SubtreeStructureChanged(reason);
    else
      To<LayoutNGSVGText>(text_or_ng_text)->SubtreeStructureChanged(reason);
  }
}

static inline void UpdateFontAndMetrics(LayoutSVGText& text_root) {
  bool last_character_was_white_space = true;
  for (LayoutObject* descendant = text_root.FirstChild(); descendant;
       descendant = descendant->NextInPreOrder(&text_root)) {
    if (!descendant->IsSVGInlineText())
      continue;
    auto& text = To<LayoutSVGInlineText>(*descendant);
    text.UpdateScaledFont();
    text.UpdateMetricsList(last_character_was_white_space);
  }
}

static inline void CheckDescendantTextNodeConsistency(
    LayoutSVGText& text,
    HeapVector<Member<LayoutSVGInlineText>>& expected_descendant_text_nodes) {
#if DCHECK_IS_ON()
  HeapVector<Member<LayoutSVGInlineText>> new_descendant_text_nodes;
  CollectDescendantTextNodes(text, new_descendant_text_nodes);
  DCHECK(new_descendant_text_nodes == expected_descendant_text_nodes);
#endif
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

void LayoutSVGText::UpdateLayout() {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());
  // This flag is set and reset as needed only within this function.
  DCHECK(!needs_reordering_);

  ClearOffsetMappingIfNeeded();

  // When laying out initially, build the character data map and propagate
  // resulting layout attributes to all LayoutSVGInlineText children in the
  // subtree.
  if (!EverHadLayout()) {
    needs_positioning_values_update_ = true;
    needs_text_metrics_update_ = true;
  }

  bool update_parent_boundaries = false;

  // If the root layout size changed (eg. window size changes), or the screen
  // scale factor has changed, then recompute the on-screen font size. Since
  // the computation of layout attributes uses the text metrics, we need to
  // update them before updating the layout attributes.
  if (needs_text_metrics_update_) {
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

    UpdateFontAndMetrics(*this);
    // Font changes may change the size of the "em" unit, so we need to
    // update positions that might depend on the font size. This is a big
    // hammer but we have no simple way to determine if the positions of
    // children depend on the font size.
    needs_positioning_values_update_ = true;
    needs_text_metrics_update_ = false;
    update_parent_boundaries = true;
  }

  // When the x/y/dx/dy/rotate lists change, we need to recompute the layout
  // attributes.
  if (needs_positioning_values_update_) {
    descendant_text_nodes_.clear();
    CollectDescendantTextNodes(*this, descendant_text_nodes_);

    SVGTextLayoutAttributesBuilder(*this).BuildLayoutAttributes();

    needs_positioning_values_update_ = false;
    needs_reordering_ = true;
    update_parent_boundaries = true;
  }

  CheckDescendantTextNodeConsistency(*this, descendant_text_nodes_);

  // Reduced version of LayoutBlock::layoutBlock(), which only takes care of SVG
  // text. All if branches that could cause early exit in LayoutBlocks
  // layoutBlock() method are turned into assertions.
  DCHECK(!IsInline());
  DCHECK(!SimplifiedLayout());
  DCHECK(!ScrollsOverflow());
  DCHECK(!HasControlClip());
  DCHECK(!PositionedObjects());
  DCHECK(!IsAnonymousBlock());

  if (!FirstChild())
    SetChildrenInline(true);

  // FIXME: We need to find a way to only layout the child boxes, if needed.
  gfx::RectF old_boundaries = ObjectBoundingBox();
  DCHECK(ChildrenInline());

  RebuildFloatsFromIntruding();

  LayoutUnit before_edge =
      BorderBefore() + PaddingBefore() + ComputeLogicalScrollbars().block_start;
  LayoutUnit after_edge =
      BorderAfter() + PaddingAfter() + ComputeLogicalScrollbars().block_end;
  SetLogicalHeight(before_edge);

  LayoutState state(*this);
  LayoutInlineChildren(true, after_edge);

  needs_reordering_ = false;

  const bool bounds_changed = old_boundaries != ObjectBoundingBox();
  if (bounds_changed) {
    // Invalidate all resources of this client if our reference box changed.
    SVGResourceInvalidator resource_invalidator(*this);
    resource_invalidator.InvalidateEffects();
    resource_invalidator.InvalidatePaints();
    update_parent_boundaries = true;
  }

  if (UpdateTransformAfterLayout(bounds_changed))
    update_parent_boundaries = true;

  ClearLayoutOverflow();

  // If our bounds changed, notify the parents.
  if (update_parent_boundaries)
    LayoutSVGBlock::SetNeedsBoundariesUpdate();

  UpdateTransformAffectsVectorEffect();

  DCHECK(!needs_reordering_);
  DCHECK(!needs_transform_update_);
  DCHECK(!needs_text_metrics_update_);
  DCHECK(!needs_positioning_values_update_);
  ClearSelfNeedsLayoutOverflowRecalc();
  ClearNeedsLayout();
}

void LayoutSVGText::RecalcVisualOverflow() {
  NOT_DESTROYED();
  ClearVisualOverflow();
  LayoutObject::RecalcVisualOverflow();
  AddSelfVisualOverflow(LayoutRect(ObjectBoundingBox()));
  AddVisualEffectOverflow();
}

RootInlineBox* LayoutSVGText::CreateRootInlineBox() {
  NOT_DESTROYED();
  RootInlineBox* box =
      MakeGarbageCollected<SVGRootInlineBox>(LineLayoutItem(this));
  box->SetHasVirtualLogicalHeight();
  return box;
}

bool LayoutSVGText::NodeAtPoint(HitTestResult& result,
                                const HitTestLocation& hit_test_location,
                                const PhysicalOffset& accumulated_offset,
                                HitTestPhase phase) {
  NOT_DESTROYED();
  DCHECK_EQ(accumulated_offset, PhysicalOffset());
  // We only draw in the foreground phase, so we only hit-test then.
  if (phase != HitTestPhase::kForeground)
    return false;

  TransformedHitTestLocation local_location(hit_test_location,
                                            LocalToSVGParentTransform());
  if (!local_location)
    return false;
  if (!SVGLayoutSupport::IntersectsClipPath(*this, ObjectBoundingBox(),
                                            *local_location))
    return false;

  if (LayoutBlock::NodeAtPoint(result, *local_location, accumulated_offset,
                               phase))
    return true;

  // Consider the bounding box if requested.
  if (StyleRef().UsedPointerEvents() == EPointerEvents::kBoundingBox) {
    if (IsObjectBoundingBoxValid() &&
        local_location->Intersects(ObjectBoundingBox())) {
      UpdateHitTestResult(result, PhysicalOffset::FromPointFRound(
                                      local_location->TransformedPoint()));
      if (result.AddNodeToListBasedTestResult(GetElement(), *local_location) ==
          kStopHitTesting)
        return true;
    }
  }
  return false;
}

PositionWithAffinity LayoutSVGText::PositionForPoint(
    const PhysicalOffset& point_in_contents) const {
  NOT_DESTROYED();
  RootInlineBox* root_box = FirstRootBox();
  if (!root_box)
    return CreatePositionWithAffinity(0);

  PhysicalOffset clipped_point_in_contents(point_in_contents);
  clipped_point_in_contents -= root_box->PhysicalLocation();
  clipped_point_in_contents.ClampNegativeToZero();
  clipped_point_in_contents += root_box->PhysicalLocation();

  DCHECK(!root_box->NextRootBox());
  DCHECK(ChildrenInline());

  auto* closest_box =
      To<SVGRootInlineBox>(root_box)->ClosestLeafChildForPosition(
          clipped_point_in_contents);
  if (!closest_box)
    return CreatePositionWithAffinity(0);

  return closest_box->GetLineLayoutItem().PositionForPoint(
      PhysicalOffset(clipped_point_in_contents.left, closest_box->Y()));
}

void LayoutSVGText::AbsoluteQuads(Vector<gfx::QuadF>& quads,
                                  MapCoordinatesFlags mode) const {
  NOT_DESTROYED();
  quads.push_back(LocalToAbsoluteQuad(gfx::QuadF(StrokeBoundingBox()), mode));
}

void LayoutSVGText::Paint(const PaintInfo& paint_info) const {
  NOT_DESTROYED();
  SVGTextPainter(*this).Paint(paint_info);
}

gfx::RectF LayoutSVGText::ObjectBoundingBox() const {
  NOT_DESTROYED();
  if (const RootInlineBox* box = FirstRootBox())
    return gfx::RectF(box->FrameRect());
  return gfx::RectF();
}

gfx::RectF LayoutSVGText::StrokeBoundingBox() const {
  NOT_DESTROYED();
  if (!FirstRootBox())
    return gfx::RectF();
  return SVGLayoutSupport::ExtendTextBBoxWithStroke(*this, ObjectBoundingBox());
}

gfx::RectF LayoutSVGText::VisualRectInLocalSVGCoordinates() const {
  NOT_DESTROYED();
  if (!FirstRootBox())
    return gfx::RectF();
  return SVGLayoutSupport::ComputeVisualRectForText(*this, ObjectBoundingBox());
}

void LayoutSVGText::AddOutlineRects(Vector<PhysicalRect>& rects,
                                    OutlineInfo* info,
                                    const PhysicalOffset&,
                                    NGOutlineType) const {
  NOT_DESTROYED();
  rects.push_back(PhysicalRect::EnclosingRect(ObjectBoundingBox()));
  if (info)
    *info = OutlineInfo::GetUnzoomedFromStyle(StyleRef());
}

bool LayoutSVGText::IsObjectBoundingBoxValid() const {
  NOT_DESTROYED();
  // If we don't have any line boxes, then consider the bbox invalid.
  return FirstLineBox();
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

}  // namespace blink
