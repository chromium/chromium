/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_gradient.h"

#include <memory>

namespace blink {

LayoutSVGResourceGradient::LayoutSVGResourceGradient(SVGGradientElement* node)
    : LayoutSVGResourcePaintServer(node),
      should_collect_gradient_attributes_(true),
      gradient_map_(MakeGarbageCollected<GradientMap>()) {}

void LayoutSVGResourceGradient::RemoveAllClientsFromCache(
    bool mark_for_invalidation) {
  gradient_map_->clear();
  should_collect_gradient_attributes_ = true;
  To<SVGGradientElement>(*GetElement()).InvalidateDependentGradients();
  MarkAllClientsForInvalidation(
      mark_for_invalidation ? SVGResourceClient::kPaintInvalidation
                            : SVGResourceClient::kParentOnlyInvalidation);
}

bool LayoutSVGResourceGradient::RemoveClientFromCache(
    SVGResourceClient& client) {
  auto entry = gradient_map_->find(&client);
  if (entry == gradient_map_->end())
    return false;
  gradient_map_->erase(entry);
  return true;
}

SVGPaintServer LayoutSVGResourceGradient::PreparePaintServer(
    const SVGResourceClient& client,
    const FloatRect& object_bounding_box) {
  ClearInvalidationMask();

  // Validate gradient DOM state before building the actual
  // gradient. This should avoid tearing down the gradient we're
  // currently working on. Preferably the state validation should have
  // no side-effects though.
  if (should_collect_gradient_attributes_) {
    if (!CollectGradientAttributes())
      return SVGPaintServer::Invalid();
    should_collect_gradient_attributes_ = false;
  }

  // Spec: When the geometry of the applicable element has no width or height
  // and objectBoundingBox is specified, then the given effect (e.g. a gradient
  // or a filter) will be ignored.
  if (GradientUnits() == SVGUnitTypes::kSvgUnitTypeObjectboundingbox &&
      object_bounding_box.IsEmpty())
    return SVGPaintServer::Invalid();

  std::unique_ptr<GradientData>& gradient_data =
      gradient_map_->insert(&client, nullptr).stored_value->value;
  if (!gradient_data)
    gradient_data = std::make_unique<GradientData>();

  // Create gradient object
  if (!gradient_data->gradient) {
    gradient_data->gradient = BuildGradient();

    // We want the text bounding box applied to the gradient space transform
    // now, so the gradient shader can use it.
    if (GradientUnits() == SVGUnitTypes::kSvgUnitTypeObjectboundingbox &&
        !object_bounding_box.IsEmpty()) {
      gradient_data->userspace_transform.Translate(object_bounding_box.X(),
                                                   object_bounding_box.Y());
      gradient_data->userspace_transform.ScaleNonUniform(
          object_bounding_box.Width(), object_bounding_box.Height());
    }

    AffineTransform gradient_transform = CalculateGradientTransform();
    gradient_data->userspace_transform *= gradient_transform;
  }

  if (!gradient_data->gradient)
    return SVGPaintServer::Invalid();

  return SVGPaintServer(gradient_data->gradient,
                        gradient_data->userspace_transform);
}

bool LayoutSVGResourceGradient::IsChildAllowed(LayoutObject* child,
                                               const ComputedStyle&) const {
  if (!child->IsSVGResourceContainer())
    return false;

  return ToLayoutSVGResourceContainer(child)->IsSVGPaintServer();
}

GradientSpreadMethod LayoutSVGResourceGradient::PlatformSpreadMethodFromSVGType(
    SVGSpreadMethodType method) {
  switch (method) {
    case kSVGSpreadMethodUnknown:
    case kSVGSpreadMethodPad:
      return kSpreadMethodPad;
    case kSVGSpreadMethodReflect:
      return kSpreadMethodReflect;
    case kSVGSpreadMethodRepeat:
      return kSpreadMethodRepeat;
  }

  NOTREACHED();
  return kSpreadMethodPad;
}

}  // namespace blink
