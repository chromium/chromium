// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_TYPE_CONVERTERS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_TYPE_CONVERTERS_H_

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/modules/xr/xr_plane.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"

namespace mojo {

template <>
struct TypeConverter<base::Optional<blink::XRPlane::Orientation>,
                     device::mojom::blink::XRPlaneOrientation> {
  static base::Optional<blink::XRPlane::Orientation> Convert(
      const device::mojom::blink::XRPlaneOrientation& orientation);
};

template <>
struct TypeConverter<blink::TransformationMatrix,
                     device::mojom::blink::VRPosePtr> {
  static blink::TransformationMatrix Convert(
      const device::mojom::blink::VRPosePtr& pose);
};

template <>
struct TypeConverter<blink::TransformationMatrix,
                     device::mojom::blink::PosePtr> {
  static blink::TransformationMatrix Convert(
      const device::mojom::blink::PosePtr& pose);
};

template <>
struct TypeConverter<blink::HeapVector<blink::Member<blink::DOMPointReadOnly>>,
                     WTF::Vector<device::mojom::blink::XRPlanePointDataPtr>> {
  static blink::HeapVector<blink::Member<blink::DOMPointReadOnly>> Convert(
      const WTF::Vector<device::mojom::blink::XRPlanePointDataPtr>& vertices);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_TYPE_CONVERTERS_H_
