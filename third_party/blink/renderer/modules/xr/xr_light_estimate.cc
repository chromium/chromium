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
  // We must send 9 coefficients worth of data to the page.
  CHECK_EQ(spherical_harmonics.coefficients.size(), 9u);
  // We need to send a red, blue, and green channel for each coefficient.
  constexpr size_t kSphericalHarmonicsChannels = 3u;

  sh_coefficients_ = NotShared(DOMFloat32Array::Create(
      spherical_harmonics.coefficients.size() * kSphericalHarmonicsChannels));

  auto sh_span = sh_coefficients_->AsSpan();
  for (const auto& coefficient : spherical_harmonics.coefficients) {
    sh_span[0] = coefficient.red();
    sh_span[1] = coefficient.green();
    sh_span[2] = coefficient.blue();
    sh_span = sh_span.subspan(kSphericalHarmonicsChannels);
  }

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
