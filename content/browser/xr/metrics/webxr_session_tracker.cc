// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/xr/metrics/webxr_session_tracker.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/public/mojom/xr_session.mojom.h"

#include <utility>

namespace content {

WebXRSessionTracker::WebXRSessionTracker(
    std::unique_ptr<ukm::builders::XR_WebXR_Session> entry)
    : SessionTracker<ukm::builders::XR_WebXR_Session>(std::move(entry)),
      receiver_(this) {}

WebXRSessionTracker::~WebXRSessionTracker() = default;

void WebXRSessionTracker::ReportRequestedFeatures(
    const device::mojom::XRSessionOptions& session_options,
    const std::unordered_set<device::mojom::XRSessionFeature>&
        enabled_features) {
  using device::mojom::XRSessionFeature;
  using device::mojom::XRSessionFeatureRequestStatus;

  // Set all features as 'not requested', to begin
  SetFeatureRequest(XRSessionFeature::REF_SPACE_VIEWER,
                    XRSessionFeatureRequestStatus::kNotRequested);
  SetFeatureRequest(XRSessionFeature::REF_SPACE_LOCAL,
                    XRSessionFeatureRequestStatus::kNotRequested);
  SetFeatureRequest(XRSessionFeature::REF_SPACE_LOCAL_FLOOR,
                    XRSessionFeatureRequestStatus::kNotRequested);
  SetFeatureRequest(XRSessionFeature::REF_SPACE_BOUNDED_FLOOR,
                    XRSessionFeatureRequestStatus::kNotRequested);
  SetFeatureRequest(XRSessionFeature::REF_SPACE_UNBOUNDED,
                    XRSessionFeatureRequestStatus::kNotRequested);
  // Not currently recording metrics for XRSessionFeature::DOM_OVERLAY

  // Record required feature requests
  for (auto feature : session_options.required_features) {
    DCHECK(enabled_features.find(feature) != enabled_features.end())
        << ": could not find feature " << feature
        << " in the collection of required features!";
    SetFeatureRequest(feature, XRSessionFeatureRequestStatus::kRequired);
  }

  // Record optional feature requests
  for (auto feature : session_options.optional_features) {
    bool enabled = enabled_features.find(feature) != enabled_features.end();
    SetFeatureRequest(
        feature, enabled ? XRSessionFeatureRequestStatus::kOptionalAccepted
                         : XRSessionFeatureRequestStatus::kOptionalRejected);
  }
}

void WebXRSessionTracker::ReportFeatureUsed(
    device::mojom::XRSessionFeature feature) {
  using device::mojom::XRSessionFeature;

  switch (feature) {
    case XRSessionFeature::REF_SPACE_VIEWER:
      ukm_entry_->SetFeatureUse_Viewer(true);
      break;
    case XRSessionFeature::REF_SPACE_LOCAL:
      ukm_entry_->SetFeatureUse_Local(true);
      break;
    case XRSessionFeature::REF_SPACE_LOCAL_FLOOR:
      ukm_entry_->SetFeatureUse_LocalFloor(true);
      break;
    case XRSessionFeature::REF_SPACE_BOUNDED_FLOOR:
      ukm_entry_->SetFeatureUse_BoundedFloor(true);
      break;
    case XRSessionFeature::REF_SPACE_UNBOUNDED:
      ukm_entry_->SetFeatureUse_Unbounded(true);
      break;
    case XRSessionFeature::DOM_OVERLAY:
    case XRSessionFeature::HIT_TEST:
    case XRSessionFeature::LIGHT_ESTIMATION:
    case XRSessionFeature::ANCHORS:
    case XRSessionFeature::CAMERA_ACCESS:
    case XRSessionFeature::PLANE_DETECTION:
    case XRSessionFeature::DEPTH:
    case XRSessionFeature::IMAGE_TRACKING:
    case XRSessionFeature::HAND_INPUT:
    case XRSessionFeature::SECONDARY_VIEWS:
    case XRSessionFeature::LAYERS:
    case XRSessionFeature::FRONT_FACING:
    case XRSessionFeature::WEBGPU:
      // Not recording metrics for these features currently.
      // TODO(crbug.com/41460317): Add metrics for the AR-related features
      // that are enabled by default.
      break;
  }
}

mojo::PendingRemote<device::mojom::XRSessionMetricsRecorder>
WebXRSessionTracker::BindMetricsRecorderPipe() {
  return receiver_.BindNewPipeAndPassRemote();
}

void WebXRSessionTracker::SetFeatureRequest(
    device::mojom::XRSessionFeature feature,
    device::mojom::XRSessionFeatureRequestStatus status) {
  using device::mojom::XRSessionFeature;

  switch (feature) {
    case XRSessionFeature::REF_SPACE_VIEWER:
      ukm_entry_->SetFeatureRequest_Viewer(static_cast<int64_t>(status));
      break;
    case XRSessionFeature::REF_SPACE_LOCAL:
      ukm_entry_->SetFeatureRequest_Local(static_cast<int64_t>(status));
      break;
    case XRSessionFeature::REF_SPACE_LOCAL_FLOOR:
      ukm_entry_->SetFeatureRequest_Local(static_cast<int64_t>(status));
      break;
    case XRSessionFeature::REF_SPACE_BOUNDED_FLOOR:
      ukm_entry_->SetFeatureRequest_Local(static_cast<int64_t>(status));
      break;
    case XRSessionFeature::REF_SPACE_UNBOUNDED:
      ukm_entry_->SetFeatureRequest_Local(static_cast<int64_t>(status));
      break;
    case XRSessionFeature::DOM_OVERLAY:
    case XRSessionFeature::HIT_TEST:
    case XRSessionFeature::LIGHT_ESTIMATION:
    case XRSessionFeature::ANCHORS:
    case XRSessionFeature::CAMERA_ACCESS:
    case XRSessionFeature::PLANE_DETECTION:
    case XRSessionFeature::DEPTH:
    case XRSessionFeature::IMAGE_TRACKING:
    case XRSessionFeature::HAND_INPUT:
    case XRSessionFeature::SECONDARY_VIEWS:
    case XRSessionFeature::LAYERS:
    case XRSessionFeature::FRONT_FACING:
    case XRSessionFeature::WEBGPU:
      // Not recording metrics for these features currently.
      // TODO(crbug.com/41460317): Add metrics for the AR-related features
      // that are enabled by default.
      break;
  }
}
}  // namespace content
