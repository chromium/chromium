// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/resize_observer/resize_observation.h"

#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/flat_tree_traversal.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_box_options.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_utilities.h"
#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "ui/gfx/geometry/size_f.h"

namespace blink {

namespace {

// Given |box_option|, compute the appropriate size for an SVG element that
// does not have an associated layout box.
gfx::SizeF ComputeZoomAdjustedSVGBox(ResizeObserverBoxOptions box_option,
                                     const LayoutObject& layout_object) {
  DCHECK(layout_object.IsSVGChild());
  auto* svg_graphics_element =
      DynamicTo<SVGGraphicsElement>(layout_object.GetNode());
  if (!svg_graphics_element)
    return gfx::SizeF();
  const gfx::SizeF bounding_box_size = svg_graphics_element->GetBBox().size();
  switch (box_option) {
    case ResizeObserverBoxOptions::kBorderBox:
    case ResizeObserverBoxOptions::kContentBox:
      return bounding_box_size;
    case ResizeObserverBoxOptions::kDevicePixelContentBox: {
      const ComputedStyle& style = layout_object.Style();
      const gfx::SizeF scaled_bounding_box_size(
          gfx::ScaleSize(bounding_box_size, style.EffectiveZoom()));
      return gfx::SizeF(
          ResizeObserverUtilities::ComputeSnappedDevicePixelContentBox(
              scaled_bounding_box_size, layout_object, style));
    }
  }
}

}  // namespace

ResizeObservation::ResizeObservation(Element* target,
                                     ResizeObserver* observer,
                                     ResizeObserverBoxOptions observed_box)
    : target_(target), observer_(observer), observed_box_(observed_box) {
  DCHECK(target_);
  DCHECK(observer_);
}

bool ResizeObservation::ObservationSizeOutOfSync() {
  if (observation_size_ == ComputeTargetSize()) {
    return false;
  }

  const Element* target = target_.Get();
  if (!target) {
    return false;
  }

  // Skip resize observations on locked elements.
  if (DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(*target))
      [[unlikely]] {
    return false;
  }

  // Don't observe non-atomic inlines if requested.
  // This is used by contain-intrinsic-size delegate to implement the following
  // resolution:
  // https://github.com/w3c/csswg-drafts/issues/7606#issuecomment-1240015961
  const LayoutObject* layout_object = target->GetLayoutObject();
  if (observer_->SkipNonAtomicInlineObservations() && layout_object &&
      layout_object->IsNonAtomicInline()) {
    return false;
  }

  return true;
}

void ResizeObservation::SetObservationSize(const gfx::SizeF& observation_size) {
  observation_size_ = observation_size;
}

// https://drafts.csswg.org/resize-observer/#calculate-depth-for-node
// 1. Let p be the parent-traversal path from node to a root Element of this
//    element’s flattened DOM tree.
// 2. Return number of nodes in p.
size_t ResizeObservation::TargetDepth() {
  unsigned depth = 0;
  for (Element* parent = target_; parent;
       parent = FlatTreeTraversal::ParentElement(*parent))
    ++depth;
  return depth;
}

gfx::SizeF ResizeObservation::ComputeTargetSize() const {
  const Element* target = target_.Get();
  if (!target) {
    return gfx::SizeF();
  }
  const LayoutObject* layout_object = target_->GetLayoutObject();
  if (!layout_object) {
    return gfx::SizeF();
  }
  gfx::SizeF size;
  if (layout_object->IsSVGChild()) {
    size = ComputeZoomAdjustedSVGBox(observed_box_, *layout_object);
  } else if (const auto* layout_box = DynamicTo<LayoutBox>(layout_object)) {
    size = ResizeObserverUtilities::ComputeZoomAdjustedBox(
        observed_box_, *layout_box, layout_box->Style());
  }
  if (!RuntimeEnabledFeatures::
          ResizeObserverUnroundedZoomAdjustedSizesEnabled()) {
    return gfx::SizeF(LayoutUnit(size.width()), LayoutUnit(size.height()));
  }
  return size;
}

void ResizeObservation::Trace(Visitor* visitor) const {
  visitor->Trace(target_);
  visitor->Trace(observer_);
}

}  // namespace blink
