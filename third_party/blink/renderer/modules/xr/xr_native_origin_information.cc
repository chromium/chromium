// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_native_origin_information.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/modules/xr/xr_anchor.h"
#include "third_party/blink/renderer/modules/xr/xr_input_source.h"
#include "third_party/blink/renderer/modules/xr/xr_light_probe.h"
#include "third_party/blink/renderer/modules/xr/xr_plane.h"
#include "third_party/blink/renderer/modules/xr/xr_reference_space.h"

namespace blink {

namespace XRNativeOriginInformation {

device::mojom::blink::XRNativeOriginInformation Create(const XRAnchor* anchor) {
  DCHECK(anchor);

  device::mojom::blink::XRNativeOriginInformation result;
  result.set_anchor_id(anchor->id());

  return result;
}

device::mojom::blink::XRNativeOriginInformation Create(
    const XRImageTrackingResult* image) {
  DCHECK(image);

  // TODO(https://crbug.com/1143575): We'll want these to correspond to an
  // actual, independent space eventually, but at the moment it's sufficient for
  // the ARCore implementation to have it be equivalent to the local reference
  // space.
  return Create(device::mojom::XRReferenceSpaceType::kLocal);
}

device::mojom::blink::XRNativeOriginInformation Create(
    const XRInputSource* input_source) {
  DCHECK(input_source);

  device::mojom::blink::XRNativeOriginInformation result;
  result.set_input_source_id(input_source->source_id());

  return result;
}

device::mojom::blink::XRNativeOriginInformation Create(const XRPlane* plane) {
  DCHECK(plane);

  device::mojom::blink::XRNativeOriginInformation result;
  result.set_plane_id(plane->id());

  return result;
}

device::mojom::blink::XRNativeOriginInformation Create(
    const XRLightProbe* light_probe) {
  DCHECK(light_probe);

  // TODO: We'll want these to correspond to an actual, independent space
  // eventually, but at the moment it's sufficient for the ARCore implementation
  // to have it be equivalent to the local reference space.
  return Create(device::mojom::XRReferenceSpaceType::kLocal);
}

device::mojom::blink::XRNativeOriginInformation Create(
    const XRReferenceSpace* reference_space) {
  DCHECK(reference_space);

  auto reference_space_type = reference_space->GetType();

  return Create(reference_space_type);
}

device::mojom::blink::XRNativeOriginInformation Create(
    device::mojom::XRReferenceSpaceType reference_space_type) {
  device::mojom::blink::XRNativeOriginInformation result;
  result.set_reference_space_type(reference_space_type);

  return result;
}

}  // namespace XRNativeOriginInformation

}  // namespace blink
