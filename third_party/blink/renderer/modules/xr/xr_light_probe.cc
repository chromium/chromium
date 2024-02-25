// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/xr/xr_light_probe.h"

#include "device/vr/public/mojom/vr_service.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_xr_light_probe_init.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/modules/event_modules.h"
#include "third_party/blink/renderer/modules/event_target_modules.h"
#include "third_party/blink/renderer/modules/xr/xr_cube_map.h"
#include "third_party/blink/renderer/modules/xr/xr_light_estimate.h"
#include "third_party/blink/renderer/modules/xr/xr_object_space.h"
#include "third_party/blink/renderer/modules/xr/xr_session.h"

namespace blink {

namespace {

// Milliseconds to wait between reflection change events.
const double kReflectionChangeDelta = 1000.0;

}  // namespace

XRLightProbe::XRLightProbe(XRSession* session, XRLightProbeInit* options)
    : session_(session) {
  if (options->reflectionFormat() == "rgba16f") {
    reflection_format_ = kReflectionFormatRGBA16F;
  } else {
    reflection_format_ = kReflectionFormatSRGBA8;
  }
}

XRSpace* XRLightProbe::probeSpace() const {
  if (!probe_space_) {
    probe_space_ =
        MakeGarbageCollected<XRObjectSpace<XRLightProbe>>(session_, this);
  }

  return probe_space_.Get();
}

device::mojom::blink::XRNativeOriginInformationPtr XRLightProbe::NativeOrigin()
    const {
  return device::mojom::blink::XRNativeOriginInformation::NewReferenceSpaceType(
      device::mojom::XRReferenceSpaceType::kLocal);
}

std::optional<gfx::Transform> XRLightProbe::MojoFromObject() const {
  // For the moment we're making an assumption that the lighting estimations
  // are always generated from the local space origin. This is the case for
  // ARCore, but will need to be made more flexible as other runtimes or methods
  // of light estimation are added.
  return session_->GetMojoFrom(device::mojom::XRReferenceSpaceType::kLocal);
}

void XRLightProbe::ProcessLightEstimationData(
    const device::mojom::blink::XRLightEstimationData* data,
    double timestamp) {
  bool reflection_changed = false;

  if (data) {
    light_estimate_ = MakeGarbageCollected<XRLightEstimate>(*data->light_probe);

    if (data->reflection_probe) {
      if (!cube_map_) {
        reflection_changed = true;
      }

      const device::mojom::blink::XRReflectionProbe& reflection_probe =
          *data->reflection_probe;
      cube_map_ = std::make_unique<XRCubeMap>(*reflection_probe.cube_map);
    }
  } else {
    if (cube_map_) {
      reflection_changed = true;
    }

    light_estimate_ = nullptr;
    cube_map_ = nullptr;
  }

  // Until we get proper notification of updated reflection data from the
  // runtime we'll limit reflection change events to once per second.
  if (reflection_changed ||
      (cube_map_ &&
       timestamp > last_reflection_change_ + kReflectionChangeDelta)) {
    last_reflection_change_ = timestamp;
    DispatchEvent(*blink::Event::Create(event_type_names::kReflectionchange));
  }
}

ExecutionContext* XRLightProbe::GetExecutionContext() const {
  return session_->GetExecutionContext();
}

const AtomicString& XRLightProbe::InterfaceName() const {
  return event_target_names::kXRLightProbe;
}

void XRLightProbe::Trace(Visitor* visitor) const {
  visitor->Trace(session_);
  visitor->Trace(probe_space_);
  visitor->Trace(light_estimate_);
  EventTarget::Trace(visitor);
}

}  // namespace blink
