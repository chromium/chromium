// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_anchor_manager.h"

#include <set>
#include <utility>

#include "base/trace_event/trace_event.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/pose.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {
OpenXrAnchorManager::OpenXrAnchorManager() = default;

OpenXrAnchorManager::~OpenXrAnchorManager() {
  DisposeActiveAnchorCallbacks();
}

void OpenXrAnchorManager::AddCreateAnchorRequest(
    const mojom::XRNativeOriginInformation& native_origin_information,
    const device::Pose& native_origin_from_anchor,
    const std::optional<PlaneId>& plane_id,
    CreateAnchorCallback callback) {
  create_anchor_requests_.emplace_back(native_origin_information,
                                       native_origin_from_anchor.ToTransform(),
                                       plane_id, std::move(callback));
}

device::mojom::XRAnchorsDataPtr OpenXrAnchorManager::ProcessAnchorsForFrame(
    OpenXrApiWrapper* openxr,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state,
    XrTime predicted_display_time) {
  TRACE_EVENT0("xr", "ProcessAnchorsForFrame");
  ProcessCreateAnchorRequests(openxr, input_state);
  return GetCurrentAnchorsData(predicted_display_time);
}

void OpenXrAnchorManager::ProcessCreateAnchorRequests(
    OpenXrApiWrapper* openxr,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state) {
  XrTime display_time = openxr->GetPredictedDisplayTime();
  for (auto& request : create_anchor_requests_) {
    std::optional<XrLocation> anchor_location =
        GetXrLocationFromNativeOriginInformation(
            openxr, request.GetNativeOriginInformation(),
            request.GetNativeOriginFromAnchor(), input_state);

    if (!anchor_location.has_value()) {
      request.TakeCallback().Run(std::nullopt);
      continue;
    }

    AnchorId anchor_id =
        CreateAnchor(anchor_location->pose, anchor_location->space,
                     display_time, request.GetPlaneId());

    request.TakeCallback().Run(anchor_id);
  }
  create_anchor_requests_.clear();
}

void OpenXrAnchorManager::DisposeActiveAnchorCallbacks() {
  for (auto& create_anchor : create_anchor_requests_) {
    create_anchor.TakeCallback().Run(std::nullopt);
  }
  create_anchor_requests_.clear();
}

std::optional<OpenXrAnchorManager::XrLocation>
OpenXrAnchorManager::GetXrLocationFromNativeOriginInformation(
    OpenXrApiWrapper* openxr,
    const mojom::XRNativeOriginInformation& native_origin_information,
    const gfx::Transform& native_origin_from_anchor,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state) const {
  switch (native_origin_information.which()) {
    case mojom::XRNativeOriginInformation::Tag::kInputSourceSpaceInfo:
      // Currently unimplemented as only anchors are supported and are never
      // created relative to input sources
      return std::nullopt;
    case mojom::XRNativeOriginInformation::Tag::kReferenceSpaceType:
      return GetXrLocationFromReferenceSpace(openxr, native_origin_information,
                                             native_origin_from_anchor);
    case mojom::XRNativeOriginInformation::Tag::kHandJointSpaceInfo:
    case mojom::XRNativeOriginInformation::Tag::kImageIndex:
      // Unsupported for now
      return std::nullopt;
    case mojom::XRNativeOriginInformation::Tag::kAnchorId:
      return GetXrLocationFromAnchor(native_origin_information.get_anchor_id(),
                                     native_origin_from_anchor);
    case mojom::XRNativeOriginInformation::Tag::kPlaneId:
      return GetXrLocationFromPlane(native_origin_information.get_plane_id(),
                                    native_origin_from_anchor);
  }
}

std::optional<OpenXrAnchorManager::XrLocation>
OpenXrAnchorManager::GetXrLocationFromReferenceSpace(
    OpenXrApiWrapper* openxr,
    const mojom::XRNativeOriginInformation& native_origin_information,
    const gfx::Transform& native_origin_from_anchor) const {
  return XrLocation{GfxTransformToXrPose(native_origin_from_anchor),
                    openxr->GetReferenceSpace(
                        native_origin_information.get_reference_space_type())};
}

}  // namespace device
