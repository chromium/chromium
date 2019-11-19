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
#include "third_party/blink/renderer/core/layout/api/line_layout_item.h"
#include "third_party/blink/renderer/core/layout/hit_test_request.h"
#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/layout_state.h"
#include "third_party/blink/renderer/core/layout/pointer_events_hit_rules.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_inline_text.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_root.h"
#include "third_party/blink/renderer/core/layout/svg/line/svg_root_inline_box.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/layout/svg/svg_text_layout_attributes_builder.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/paint/svg_text_painter.h"
#include "third_party/blink/renderer/core/style/shadow_list.h"
#include "third_party/blink/renderer/core/svg/svg_text_element.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"

namespace blink {

namespace {

const LayoutSVGText* FindTextRoot(const LayoutObject* start) {
  DCHECK(start);
  for (; start; start = start->Parent()) {
    if (start->IsSVGText())
      return ToLayoutSVGText(start);
  }
  return nullptr;
}

}  // namespace

LayoutSVGText::LayoutSVGText(SVGTextElement* node)
    : LayoutSVGBlock(node),
      needs_reordering_(false),
      needs_positioning_values_update_(false),
      needs_transform_update_(true),
      needs_text_metrics_update_(false) {}

LayoutSVGText::~LayoutSVGText() {
  DCHECK(descendant_text_nodes_.IsEmpty());
}

void LayoutSVGText::StyleDidChange(StyleDifference diff,
                                   const ComputedStyle* old_style) {
  LayoutSVGBlock::StyleDidChange(diff, old_style);
  SVGResources::UpdatePaints(*GetElement(), old_style, StyleRef());
}

void LayoutSVGText::WillBeDestroyed() {
  descendant_text_nodes_.clear();
  SVGResources::ClearPaints(*GetElement(), Style());
  LayoutSVGBlock::WillBeDestroyed();
}

bool LayoutSVGText::IsChildAllowed(LayoutObject* child,
                                   const ComputedStyle&) const {
  return child->IsSVGInline() ||
         (child->IsText() && SVGLayoutSupport::IsLayoutableTextNode(child));
}

LayoutSVGText* LayoutSVGText::LocateLayoutSVGTextAncestor(LayoutObject* start) {
  return const_cast<LayoutSVGText*>(FindTextRoot(start));
}

const LayoutSVGText* LayoutSVGText::LocateLayoutSVGTextAncestor(
    const LayoutObject* start) {
  return FindTextRoot(start);
}

static inline void CollectDescendantTextNodes(
    LayoutSVGText& text_root,
    Vector<LayoutSVGInlineText*>& descendant_text_nodes) {
  for (LayoutObject* descendant = text_root.FirstChild(); descendant;
       descendant = descendant->NextInPreOrder(&text_root)) {
    if (descendant->IsSVGInlineText())
      descendant_text_nodes.push_back(ToLayoutSVGInlineText(descendant));
  }
}

void LayoutSVGText::InvalidatePositioningValues(
    LayoutInvalidationReasonForTracing reason) {
  descendant_text_nodes_.clear();
  SetNeedsPositioningValuesUpdate();
  // TODO(fs): Restore the passing of |reason| here.
  LayoutSVGResourceContainer::MarkForLayoutAndParentResourceInvalidation(*this);
}

void LayoutSVGText::SubtreeChildWasAdded() {
  if (BeingDestroyed() || !EverHadLayout()) {
    DCHECK(descendant_text_nodes_.IsEmpty());
    return;
  }
  if (DocumentBeingDestroyed())
    return;

  // The positioning elements cache depends on the size of each text
  // layoutObject in the subtree. If this changes, clear the cache. It will be
  // rebuilt on the next layout.
  InvalidatePositioningValues(layout_invalidation_reason::kChildChanged);
  SetNeedsTextMetricsUpdate();
}

void LayoutSVGText::SubtreeChildWillBeRemoved() {
  if (BeingDestroyed() || !EverHadLayout()) {
    DCHECK(descendant_text_nodes_.IsEmpty());
    return;
  }

  // The positioning elements cache depends on the size of each text
  // layoutObject in the subtree. If this changes, clear the cache. It will be
  // rebuilt on the next layout.
  InvalidatePositioningValues(layout_invalidation_reason::kChildChanged);
  SetNeedsTextMetricsUpdate();
}

void LayoutSVGText::SubtreeTextDidChange() {
  DCHECK(!BeingDestroyed());
  if (!EverHadLayout()) {
    DCHECK(descendant_text_nodes_.IsEmpty());
    return;
  }

  // The positioning elements cache depends on the size of each text object in
  // the subtree. If this changes, clear the cache and mark it for rebuilding
  // in the next layout.
  InvalidatePositioningValues(layout_invalidation_reason::kTextChanged);
  SetNeedsTextMetricsUpdate();
}

static inline void UpdateFontAndMetrics(LayoutSVGText& text_root) {
  bool last_character_was_white_space = true;
  for (LayoutObject* descendant = text_root.FirstChild(); descendant;
       descendant = descendant->NextInPreOrder(&text_root)) {
    if (!descendant->IsSVGInlineText())
      continue;
    LayoutSVGInlineText& text = ToLayoutSVGInlineText(*descendant);
    text.UpdateScaledFont();
    text.UpdateMetricsList(last_character_was_white_space);
  }
}

static inline void CheckDescendantTextNodeConsistency(
    LayoutSVGText& text,
    Vector<LayoutSVGInlineText*>& expected_descendant_text_nodes) {
#if DCHECK_IS_ON()
  Vector<LayoutSVGInlineText*> new_descendant_text_nodes;
  CollectDescendantTextNodes(text, new_descendant_text_nodes);
  DCHECK(new_descendant_text_nodes == expected_descendant_text_nodes);
#endif
}

void LayoutSVGText::UpdateLayout() {
  DCHECK(NeedsLayout());
  // This flag is set and reset as needed only within this function.
  DCHECK(!needs_reordering_);
  LayoutAnalyzer::Scope analyzer(*this);

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
  FloatRect old_boundaries = ObjectBoundingBox();
  DCHECK(ChildrenInline());

  RebuildFloatsFromIntruding();

  LayoutUnit before_edge = BorderBefore() + PaddingBefore();
  LayoutUnit after_edge =
      BorderAfter() + PaddingAfter() + ScrollbarLogicalHeight();
  SetLogicalHeight(before_edge);

  LayoutState state(*this);
  LayoutInlineChildren(true, after_edge);

  needs_reordering_ = false;

  FloatRect new_boundaries = ObjectBoundingBox();
  bool bounds_changed = old_boundaries != new_boundaries;

  // Update the transform after laying out. Update if the bounds
  // changed too, since the transform could depend on the bounding
  // box.
  if (bounds_changed || needs_transform_update_) {
    local_transform_ =
        GetElement()->CalculateTransform(SVGElement::kIncludeMotionTransform);
    needs_transform_update_ = false;
    update_parent_boundaries = true;
  }

  ClearLayoutOverflow();

  // Invalidate all resources of this client if our layout changed.
  if (EverHadLayout() && SelfNeedsLayout())
    SVGResourcesCache::ClientLayoutChanged(*this);

  // If our bounds changed, notify the parents.
  if (update_parent_boundaries)
    LayoutSVGBlock::SetNeedsBoundariesUpdate();

  DCHECK(!needs_reordering_);
  DCHECK(!needs_transform_update_);
  DCHECK(!needs_text_metrics_update_);
  DCHECK(!needs_positioning_values_update_);
  ClearSelfNeedsLayoutOverflowRecalc();
  ClearNeedsLayout();
}

void LayoutSVGText::RecalcVisualOverflow() {
  ClearVisualOverflow();
  LayoutObject::RecalcVisualOverflow();
  AddSelfVisualOverflow(LayoutRect(ObjectBoundingBox()));
  AddVisualEffectOverflow();
}

RootInlineBox* LayoutSVGText::CreateRootInlineBox() {
  RootInlineBox* box = new SVGRootInlineBox(LineLayoutItem(this));
  box->SetHasVirtualLogicalHeight();
  return box;
}

bool LayoutSVGText::NodeAtPoint(HitTestResult& result,
                                const HitTestLocation& hit_test_location,
                                const PhysicalOffset& accumulated_offset,
                                HitTestAction hit_test_action) {
  DCHECK_EQ(accumulated_offset, PhysicalOffset());
  // We only draw in the foreground phase, so we only hit-test then.
  if (hit_test_action != kHitTestForeground)
    return false;

  TransformedHitTestLocation local_location(hit_test_location,
                                            LocalToSVGParentTransform());
  if (!local_location)
    return false;
  if (!SVGLayoutSupport::IntersectsClipPath(*this, ObjectBoundingBox(),
                                            *local_location))
    return false;

  if (LayoutBlock::NodeAtPoint(result, *local_location, accumulated_offset,
                               hit_test_action))
    return true;

  // Consider the bounding box if requested.
  if (StyleRef().PointerEvents() == EPointerEvents::kBoundingBox) {
    if (IsObjectBoundingBoxValid() &&
        local_location->Intersects(ObjectBoundingBox())) {
      UpdateHitTestResult(result, PhysicalOffset::FromFloatPointRound(
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
  RootInlineBox* root_box = FirstRootBox();
  if (!root_box)
    return CreatePositionWithAffinity(0);

  PhysicalOffset clipped_point_in_contents(point_in_contents);
  clipped_point_in_contents -= root_box->PhysicalLocation();
  clipped_point_in_contents.ClampNegativeToZero();
  clipped_point_in_contents += root_box->PhysicalLocation();

  DCHECK(!root_box->NextRootBox());
  DCHECK(ChildrenInline());

  InlineBox* closest_box =
      ToSVGRootInlineBox(root_box)->ClosestLeafChildForPosition(
          clipped_point_in_contents);
  if (!closest_box)
    return CreatePositionWithAffinity(0);

  return closest_box->GetLineLayoutItem().PositionForPoint(
      PhysicalOffset(clipped_point_in_contents.left, closest_box->Y()));
}

void LayoutSVGText::AbsoluteQuads(Vector<FloatQuad>& quads,
                                  MapCoordinatesFlags mode) const {
  quads.push_back(LocalToAbsoluteQuad(StrokeBoundingBox(), mode));
}

void LayoutSVGText::Paint(const PaintInfo& paint_info) const {
  SVGTextPainter(*this).Paint(paint_info);
}

FloatRect LayoutSVGText::ObjectBoundingBox() const {
  if (const RootInlineBox* box = FirstRootBox())
    return FloatRect(box->FrameRect());
  return FloatRect();
}

FloatRect LayoutSVGText::StrokeBoundingBox() const {
  FloatRect stroke_boundaries = ObjectBoundingBox();
  const SVGComputedStyle& svg_style = StyleRef().SvgStyle();
  if (!svg_style.HasStroke())
    return stroke_boundaries;

  DCHECK(GetElement());
  SVGLengthContext length_context(GetElement());
  stroke_boundaries.Inflate(
      length_context.ValueForLength(svg_style.StrokeWidth()));
  return stroke_boundaries;
}

FloatRect LayoutSVGText::VisualRectInLocalSVGCoordinates() const {
  FloatRect visual_rect = StrokeBoundingBox();
  SVGLayoutSupport::AdjustVisualRectWithResources(*this, ObjectBoundingBox(),
                                                  visual_rect);

  if (const ShadowList* text_shadow = StyleRef().TextShadow())
    text_shadow->AdjustRectForShadow(visual_rect);

  return visual_rect;
}

void LayoutSVGText::AddOutlineRects(Vector<PhysicalRect>& rects,
                                    const PhysicalOffset&,
                                    NGOutlineType) const {
  rects.push_back(PhysicalRect::EnclosingRect(ObjectBoundingBox()));
}

bool LayoutSVGText::IsObjectBoundingBoxValid() const {
  // If we don't have any line boxes, then consider the bbox invalid.
  return FirstLineBox();
}

void LayoutSVGText::AddChild(LayoutObject* child, LayoutObject* before_child) {
  LayoutSVGBlock::AddChild(child, before_child);

  SVGResourcesCache::ClientWasAddedToTree(*child, child->StyleRef());
  SubtreeChildWasAdded();
}

void LayoutSVGText::RemoveChild(LayoutObject* child) {
  SVGResourcesCache::ClientWillBeRemovedFromTree(*child);
  SubtreeChildWillBeRemoved();

  LayoutSVGBlock::RemoveChild(child);
}

}  // namespace blink
