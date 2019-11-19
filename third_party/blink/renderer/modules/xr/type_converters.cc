// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/type_converters.h"

#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"

namespace mojo {

base::Optional<blink::XRPlane::Orientation>
TypeConverter<base::Optional<blink::XRPlane::Orientation>,
              device::mojom::blink::XRPlaneOrientation>::
    Convert(const device::mojom::blink::XRPlaneOrientation& orientation) {
  switch (orientation) {
    case device::mojom::blink::XRPlaneOrientation::UNKNOWN:
      return base::nullopt;
    case device::mojom::blink::XRPlaneOrientation::HORIZONTAL:
      return blink::XRPlane::Orientation::kHorizontal;
    case device::mojom::blink::XRPlaneOrientation::VERTICAL:
      return blink::XRPlane::Orientation::kVertical;
  }
}

blink::TransformationMatrix
TypeConverter<blink::TransformationMatrix, device::mojom::blink::VRPosePtr>::
    Convert(const device::mojom::blink::VRPosePtr& pose) {
  DCHECK(pose);

  blink::TransformationMatrix result;
  blink::TransformationMatrix::DecomposedType decomp = {};

  decomp.perspective_w = 1;
  decomp.scale_x = 1;
  decomp.scale_y = 1;
  decomp.scale_z = 1;

  if (pose->orientation) {
    // TODO(https://crbug.com/929841): Remove negation once the bug is fixed.
    gfx::Quaternion quat = pose->orientation->inverse();
    decomp.quaternion_x = quat.x();
    decomp.quaternion_y = quat.y();
    decomp.quaternion_z = quat.z();
    decomp.quaternion_w = quat.w();
  } else {
    decomp.quaternion_w = 1.0;
  }

  if (pose->position) {
    decomp.translate_x = pose->position->X();
    decomp.translate_y = pose->position->Y();
    decomp.translate_z = pose->position->Z();
  }

  result.Recompose(decomp);

  return result;
}

blink::TransformationMatrix
TypeConverter<blink::TransformationMatrix, device::mojom::blink::PosePtr>::
    Convert(const device::mojom::blink::PosePtr& pose) {
  DCHECK(pose);

  blink::TransformationMatrix result;
  blink::TransformationMatrix::DecomposedType decomp = {};

  decomp.perspective_w = 1;
  decomp.scale_x = 1;
  decomp.scale_y = 1;
  decomp.scale_z = 1;

  // TODO(https://crbug.com/929841): Remove negation once the bug is fixed.
  gfx::Quaternion quat = pose->orientation.inverse();
  decomp.quaternion_x = quat.x();
  decomp.quaternion_y = quat.y();
  decomp.quaternion_z = quat.z();
  decomp.quaternion_w = quat.w();

  decomp.translate_x = pose->position.X();
  decomp.translate_y = pose->position.Y();
  decomp.translate_z = pose->position.Z();

  result.Recompose(decomp);

  return result;
}

blink::HeapVector<blink::Member<blink::DOMPointReadOnly>>
TypeConverter<blink::HeapVector<blink::Member<blink::DOMPointReadOnly>>,
              WTF::Vector<device::mojom::blink::XRPlanePointDataPtr>>::
    Convert(const WTF::Vector<device::mojom::blink::XRPlanePointDataPtr>&
                vertices) {
  blink::HeapVector<blink::Member<blink::DOMPointReadOnly>> result;

  for (const auto& vertex_data : vertices) {
    result.push_back(blink::DOMPointReadOnly::Create(vertex_data->x, 0.0,
                                                     vertex_data->z, 1.0));
  }

  return result;
}

}  // namespace mojo
