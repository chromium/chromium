/*
 * Copyright (C) 2004, 2005, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Google, Inc.  All rights reserved.
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_container.h"

#include "third_party/blink/renderer/core/layout/hit_test_result.h"
#include "third_party/blink/renderer/core/layout/layout_analyzer.h"
#include "third_party/blink/renderer/core/layout/svg/svg_layout_support.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources.h"
#include "third_party/blink/renderer/core/layout/svg/svg_resources_cache.h"
#include "third_party/blink/renderer/core/layout/svg/transform_helper.h"
#include "third_party/blink/renderer/core/layout/svg/transformed_hit_test_location.h"
#include "third_party/blink/renderer/core/paint/svg_container_painter.h"

namespace blink {

LayoutSVGContainer::LayoutSVGContainer(SVGElement* node)
    : LayoutSVGModelObject(node),
      object_bounding_box_valid_(false),
      needs_boundaries_update_(true),
      did_screen_scale_factor_change_(false),
      has_non_isolated_blending_descendants_(false),
      has_non_isolated_blending_descendants_dirty_(false) {}

LayoutSVGContainer::~LayoutSVGContainer() = default;

void LayoutSVGContainer::UpdateLayout() {
  DCHECK(NeedsLayout());
  LayoutAnalyzer::Scope analyzer(*this);

  // Update the local transform in subclasses.
  SVGTransformChange transform_change = CalculateLocalTransform();
  did_screen_scale_factor_change_ =
      transform_change == SVGTransformChange::kFull ||
      SVGLayoutSupport::ScreenScaleFactorChanged(Parent());

  // When hasRelativeLengths() is false, no descendants have relative lengths
  // (hence no one is interested in viewport size changes).
  bool layout_size_changed =
      GetElement()->HasRelativeLengths() &&
      SVGLayoutSupport::LayoutSizeOfNearestViewportChanged(this);

  SVGLayoutSupport::LayoutChildren(FirstChild(), false,
                                   did_screen_scale_factor_change_,
                                   layout_size_changed);

  // Invalidate all resources of this client if our layout changed.
  if (EverHadLayout() && NeedsLayout())
    SVGResourcesCache::ClientLayoutChanged(*this);

  if (needs_boundaries_update_ ||
      transform_change != SVGTransformChange::kNone) {
    UpdateCachedBoundaries();
    needs_boundaries_update_ = false;

    // If our bounds changed, notify the parents.
    LayoutSVGModelObject::SetNeedsBoundariesUpdate();
  }

  DCHECK(!needs_boundaries_update_);
  ClearNeedsLayout();
}

void LayoutSVGContainer::AddChild(LayoutObject* child,
                                  LayoutObject* before_child) {
  LayoutSVGModelObject::AddChild(child, before_child);
  SVGResourcesCache::ClientWasAddedToTree(*child, child->StyleRef());

  bool should_isolate_descendants =
      (child->IsBlendingAllowed() && child->StyleRef().HasBlendMode()) ||
      child->HasNonIsolatedBlendingDescendants();
  if (should_isolate_descendants)
    DescendantIsolationRequirementsChanged(kDescendantIsolationRequired);
}

void LayoutSVGContainer::RemoveChild(LayoutObject* child) {
  SVGResourcesCache::ClientWillBeRemovedFromTree(*child);
  LayoutSVGModelObject::RemoveChild(child);

  bool had_non_isolated_descendants =
      (child->IsBlendingAllowed() && child->StyleRef().HasBlendMode()) ||
      child->HasNonIsolatedBlendingDescendants();
  if (had_non_isolated_descendants)
    DescendantIsolationRequirementsChanged(kDescendantIsolationNeedsUpdate);
}

bool LayoutSVGContainer::SelfWillPaint() const {
  return SVGLayoutSupport::HasFilterResource(*this);
}

void LayoutSVGContainer::StyleDidChange(StyleDifference diff,
                                        const ComputedStyle* old_style) {
  LayoutSVGModelObject::StyleDidChange(diff, old_style);

  bool had_isolation =
      old_style && !IsSVGHiddenContainer() &&
      SVGLayoutSupport::WillIsolateBlendingDescendantsForStyle(*old_style);

  bool will_isolate_blending_descendants =
      SVGLayoutSupport::WillIsolateBlendingDescendantsForObject(this);

  bool isolation_changed = had_isolation != will_isolate_blending_descendants;

  if (isolation_changed)
    SetNeedsPaintPropertyUpdate();

  if (!Parent() || !isolation_changed)
    return;

  if (HasNonIsolatedBlendingDescendants()) {
    Parent()->DescendantIsolationRequirementsChanged(
        will_isolate_blending_descendants ? kDescendantIsolationNeedsUpdate
                                          : kDescendantIsolationRequired);
  }
}

bool LayoutSVGContainer::HasNonIsolatedBlendingDescendants() const {
  if (has_non_isolated_blending_descendants_dirty_) {
    has_non_isolated_blending_descendants_ =
        SVGLayoutSupport::ComputeHasNonIsolatedBlendingDescendants(this);
    has_non_isolated_blending_descendants_dirty_ = false;
  }
  return has_non_isolated_blending_descendants_;
}

void LayoutSVGContainer::DescendantIsolationRequirementsChanged(
    DescendantIsolationState state) {
  switch (state) {
    case kDescendantIsolationRequired:
      has_non_isolated_blending_descendants_ = true;
      has_non_isolated_blending_descendants_dirty_ = false;
      break;
    case kDescendantIsolationNeedsUpdate:
      if (has_non_isolated_blending_descendants_dirty_)
        return;
      has_non_isolated_blending_descendants_dirty_ = true;
      break;
  }
  if (SVGLayoutSupport::WillIsolateBlendingDescendantsForObject(this)) {
    SetNeedsPaintPropertyUpdate();
    return;
  }
  if (Parent())
    Parent()->DescendantIsolationRequirementsChanged(state);
}

void LayoutSVGContainer::Paint(const PaintInfo& paint_info) const {
  SVGContainerPainter(*this).Paint(paint_info);
}

void LayoutSVGContainer::UpdateCachedBoundaries() {
  SVGLayoutSupport::ComputeContainerBoundingBoxes(
      this, object_bounding_box_, object_bounding_box_valid_,
      stroke_bounding_box_, local_visual_rect_);
  GetElement()->SetNeedsResizeObserverUpdate();
}

bool LayoutSVGContainer::NodeAtPoint(HitTestResult& result,
                                     const HitTestLocation& hit_test_location,
                                     const PhysicalOffset& accumulated_offset,
                                     HitTestAction hit_test_action) {
  DCHECK_EQ(accumulated_offset, PhysicalOffset());
  TransformedHitTestLocation local_location(hit_test_location,
                                            LocalToSVGParentTransform());
  if (!local_location)
    return false;
  if (!SVGLayoutSupport::IntersectsClipPath(*this, object_bounding_box_,
                                            *local_location))
    return false;

  if (!PaintBlockedByDisplayLock(DisplayLockLifecycleTarget::kChildren) &&
      SVGLayoutSupport::HitTestChildren(LastChild(), result, *local_location,
                                        accumulated_offset, hit_test_action))
    return true;

  // pointer-events: bounding-box makes it possible for containers to be direct
  // targets.
  if (StyleRef().PointerEvents() == EPointerEvents::kBoundingBox) {
    // Check for a valid bounding box because it will be invalid for empty
    // containers.
    if (IsObjectBoundingBoxValid() &&
        local_location->Intersects(ObjectBoundingBox())) {
      UpdateHitTestResult(result, PhysicalOffset::FromFloatPointRound(
                                      local_location->TransformedPoint()));
      if (result.AddNodeToListBasedTestResult(GetElement(), *local_location) ==
          kStopHitTesting)
        return true;
    }
  }
  // 16.4: "If there are no graphics elements whose relevant graphics content is
  // under the pointer (i.e., there is no target element), the event is not
  // dispatched."
  return false;
}

SVGTransformChange LayoutSVGContainer::CalculateLocalTransform() {
  return SVGTransformChange::kNone;
}

}  // namespace blink
