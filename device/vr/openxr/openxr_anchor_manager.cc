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
    XrTime predicted_display_time) {
  TRACE_EVENT0("xr", "ProcessAnchorsForFrame");
  ProcessCreateAnchorRequests(openxr);
  return GetCurrentAnchorsData(predicted_display_time);
}

void OpenXrAnchorManager::ProcessCreateAnchorRequests(
    OpenXrApiWrapper* openxr) {
  XrTime display_time = openxr->GetPredictedDisplayTime();
  for (auto& request : create_anchor_requests_) {
    if (!IsSupportedOrigin(request.GetNativeOriginInformation())) {
      // Unsupported for now.
      request.TakeCallback().Run(std::nullopt);
      continue;
    }

    std::optional<XrLocation> anchor_location =
        openxr->GetXrLocationFromNativeOriginInformation(
            request.GetNativeOriginInformation(),
            request.GetNativeOriginFromAnchor());

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

bool OpenXrAnchorManager::IsSupportedOrigin(
    const mojom::XRNativeOriginInformation& native_origin_info) const {
  const auto space_type = native_origin_info.which();
  return !(space_type ==
               mojom::XRNativeOriginInformation::Tag::kInputSourceSpaceInfo ||
           space_type ==
               mojom::XRNativeOriginInformation::Tag::kHandJointSpaceInfo ||
           space_type == mojom::XRNativeOriginInformation::Tag::kImageIndex);
}

}  // namespace device
