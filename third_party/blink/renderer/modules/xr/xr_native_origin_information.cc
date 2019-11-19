// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_native_origin_information.h"

#include "third_party/blink/renderer/modules/xr/type_converters.h"
#include "third_party/blink/renderer/modules/xr/xr_anchor.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_plane.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"

namespace blink {

base::Optional<XRNativeOriginInformation> XRNativeOriginInformation::Create(
    const XRAnchor* anchor) {
  DCHECK(anchor);
  return XRNativeOriginInformation(Type::Anchor, anchor->id());
}

base::Optional<XRNativeOriginInformation> XRNativeOriginInformation::Create(
    const XRInputSource* input_source) {
  DCHECK(input_source);
  return XRNativeOriginInformation(Type::InputSource,
                                   input_source->source_id());
}

base::Optional<XRNativeOriginInformation> XRNativeOriginInformation::Create(
    const XRPlane* plane) {
  DCHECK(plane);
  return XRNativeOriginInformation(Type::Plane, plane->id());
}

base::Optional<XRNativeOriginInformation> XRNativeOriginInformation::Create(
    const XRReferenceSpace* reference_space) {
  DCHECK(reference_space);
  switch (reference_space->GetType()) {
    case XRReferenceSpace::Type::kTypeBoundedFloor:
      return XRNativeOriginInformation(
          Type::ReferenceSpace,
          device::mojom::XRReferenceSpaceCategory::BOUNDED_FLOOR);
    case XRReferenceSpace::Type::kTypeUnbounded:
      return XRNativeOriginInformation(
          Type::ReferenceSpace,
          device::mojom::XRReferenceSpaceCategory::UNBOUNDED);
    case XRReferenceSpace::Type::kTypeLocalFloor:
      return XRNativeOriginInformation(
          Type::ReferenceSpace,
          device::mojom::XRReferenceSpaceCategory::LOCAL_FLOOR);
    case XRReferenceSpace::Type::kTypeLocal:
      return XRNativeOriginInformation(
          Type::ReferenceSpace, device::mojom::XRReferenceSpaceCategory::LOCAL);
    case XRReferenceSpace::Type::kTypeViewer:
      return XRNativeOriginInformation(
          Type::ReferenceSpace,
          device::mojom::XRReferenceSpaceCategory::VIEWER);
  }
}

XRNativeOriginInformation::XRNativeOriginInformation(Type type,
                                                     uint32_t input_source_id)
    : type_(type), input_source_id_(input_source_id) {}

XRNativeOriginInformation::XRNativeOriginInformation(
    Type type,
    uint64_t anchor_or_plane_id)
    : type_(type), anchor_or_plane_id_(anchor_or_plane_id) {}

XRNativeOriginInformation::XRNativeOriginInformation(
    Type type,
    device::mojom::XRReferenceSpaceCategory reference_space_category)
    : type_(type), reference_space_category_(reference_space_category) {}

device::mojom::blink::XRNativeOriginInformationPtr
XRNativeOriginInformation::ToMojo() const {
  switch (type_) {
    case XRNativeOriginInformation::Type::Anchor:
      return device::mojom::blink::XRNativeOriginInformation::NewAnchorId(
          anchor_or_plane_id_);

    case XRNativeOriginInformation::Type::InputSource:
      return device::mojom::blink::XRNativeOriginInformation::NewInputSourceId(
          input_source_id_);

    case XRNativeOriginInformation::Type::Plane:
      return device::mojom::blink::XRNativeOriginInformation::NewPlaneId(
          anchor_or_plane_id_);

    case XRNativeOriginInformation::Type::ReferenceSpace:
      return device::mojom::blink::XRNativeOriginInformation::
          NewReferenceSpaceCategory(reference_space_category_);
  }
}

}  // namespace blink
