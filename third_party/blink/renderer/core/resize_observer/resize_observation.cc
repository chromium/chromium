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
      const ComputedStyle& style = layout_object.StyleRef();
      const gfx::SizeF scaled_bounding_box_size(
          gfx::ScaleSize(bounding_box_size, style.EffectiveZoom()));
      return ResizeObserverUtilities::ComputeSnappedDevicePixelContentBox(
          scaled_bounding_box_size, layout_object, style);
    }
  }
}

// Set the initial observation size to something impossible so that the first
// gather observation step always will pick up a new observation.
constexpr LogicalSize kInitialObservationSize(kIndefiniteSize, kIndefiniteSize);

}  // namespace

ResizeObservation::ResizeObservation(Element* target,
                                     ResizeObserver* observer,
                                     ResizeObserverBoxOptions observed_box)
    : target_(target),
      observer_(observer),
      observation_size_(kInitialObservationSize),
      observed_box_(observed_box) {
  DCHECK(target_);
  DCHECK(observer_);
}

bool ResizeObservation::ObservationSizeOutOfSync() {
  if (observation_size_ == ComputeTargetSize())
    return false;

  // Skip resize observations on locked elements.
  if (target_ && DisplayLockUtilities::IsInLockedSubtreeCrossingFrames(
                     *target_)) [[unlikely]] {
    return false;
  }

  // Don't observe non-atomic inlines if requested.
  // This is used by contain-intrinsic-size delegate to implement the following
  // resolution:
  // https://github.com/w3c/csswg-drafts/issues/7606#issuecomment-1240015961
  if (observer_->SkipNonAtomicInlineObservations() &&
      target_->GetLayoutObject() && target_->GetLayoutObject()->IsInline() &&
      !target_->GetLayoutObject()->IsAtomicInlineLevel()) {
    return false;
  }

  return true;
}

void ResizeObservation::SetObservationSize(
    const LogicalSize& observation_size) {
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

LogicalSize ResizeObservation::ComputeTargetSize() const {
  if (!target_ || !target_->GetLayoutObject())
    return LogicalSize();
  const LayoutObject& layout_object = *target_->GetLayoutObject();
  if (layout_object.IsSVGChild()) {
    gfx::SizeF size = ComputeZoomAdjustedSVGBox(observed_box_, layout_object);
    return LogicalSize(LayoutUnit(size.width()), LayoutUnit(size.height()));
  }
  if (const auto* layout_box = DynamicTo<LayoutBox>(layout_object)) {
    gfx::SizeF size = ResizeObserverUtilities::ComputeZoomAdjustedBox(
        observed_box_, *layout_box, layout_box->StyleRef());
    return LogicalSize(LayoutUnit(size.width()), LayoutUnit(size.height()));
  }
  return LogicalSize();
}

void ResizeObservation::Trace(Visitor* visitor) const {
  visitor->Trace(target_);
  visitor->Trace(observer_);
}

}  // namespace blink
