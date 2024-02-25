// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/vr_service_type_converters.h"

#include "ui/gfx/geometry/point3_f.h"

namespace mojo {

std::optional<blink::XRPlane::Orientation>
TypeConverter<std::optional<blink::XRPlane::Orientation>,
              device::mojom::blink::XRPlaneOrientation>::
    Convert(const device::mojom::blink::XRPlaneOrientation& orientation) {
  switch (orientation) {
    case device::mojom::blink::XRPlaneOrientation::UNKNOWN:
      return std::nullopt;
    case device::mojom::blink::XRPlaneOrientation::HORIZONTAL:
      return blink::XRPlane::Orientation::kHorizontal;
    case device::mojom::blink::XRPlaneOrientation::VERTICAL:
      return blink::XRPlane::Orientation::kVertical;
  }
}

blink::HeapVector<blink::Member<blink::DOMPointReadOnly>>
TypeConverter<blink::HeapVector<blink::Member<blink::DOMPointReadOnly>>,
              Vector<device::mojom::blink::XRPlanePointDataPtr>>::
    Convert(const Vector<device::mojom::blink::XRPlanePointDataPtr>& vertices) {
  blink::HeapVector<blink::Member<blink::DOMPointReadOnly>> result;

  for (const auto& vertex_data : vertices) {
    result.push_back(blink::DOMPointReadOnly::Create(vertex_data->x, 0.0,
                                                     vertex_data->z, 1.0));
  }

  return result;
}

}  // namespace mojo
