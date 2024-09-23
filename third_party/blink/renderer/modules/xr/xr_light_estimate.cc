// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_light_estimate.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"

namespace blink {

XRLightEstimate::XRLightEstimate(
    const device::mojom::blink::XRLightProbe& light_probe) {
  const device::mojom::blink::XRSphericalHarmonics& spherical_harmonics =
      *light_probe.spherical_harmonics;
  DCHECK_EQ(spherical_harmonics.coefficients.size(), 9u);

  sh_coefficients_ = DOMFloat32Array::Create(UNSAFE_TODO(
      base::span(spherical_harmonics.coefficients.data()->components,
                 spherical_harmonics.coefficients.size() *
                     device::RgbTupleF32::kNumComponents)));

  primary_light_direction_ =
      DOMPointReadOnly::Create(light_probe.main_light_direction.x(),
                               light_probe.main_light_direction.y(),
                               light_probe.main_light_direction.z(), 0);
  primary_light_intensity_ =
      DOMPointReadOnly::Create(light_probe.main_light_intensity.red(),
                               light_probe.main_light_intensity.green(),
                               light_probe.main_light_intensity.blue(), 1);
}

void XRLightEstimate::Trace(Visitor* visitor) const {
  visitor->Trace(sh_coefficients_);
  visitor->Trace(primary_light_direction_);
  visitor->Trace(primary_light_intensity_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
