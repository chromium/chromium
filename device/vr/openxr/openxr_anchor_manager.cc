// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_anchor_manager.h"

#include <tuple>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/types/expected.h"
#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/pose.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {
OpenXrAnchorManager::OpenXrAnchorManager() = default;

OpenXrAnchorManager::~OpenXrAnchorManager() {
  DisposeActiveAnchorCallbacks();
  for (const auto& [_, anchor_space] : openxr_anchors_) {
    xrDestroySpace(anchor_space);
  }
}

void OpenXrAnchorManager::AddCreateAnchorRequest(
    const mojom::XRNativeOriginInformation& native_origin_information,
    const device::Pose& native_origin_from_anchor,
    CreateAnchorCallback callback) {
  create_anchor_requests_.emplace_back(native_origin_information,
                                       native_origin_from_anchor.ToTransform(),
                                       std::move(callback));
}

device::mojom::XRAnchorsDataPtr OpenXrAnchorManager::ProcessAnchorsForFrame(
    OpenXrApiWrapper* openxr,
    const mojom::VRStageParametersPtr& current_stage_parameters,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state,
    XrTime predicted_display_time) {
  ProcessCreateAnchorRequests(openxr, current_stage_parameters, input_state);
  return GetCurrentAnchorsData(predicted_display_time);
}

void OpenXrAnchorManager::ProcessCreateAnchorRequests(
    OpenXrApiWrapper* openxr,
    const mojom::VRStageParametersPtr& current_stage_parameters,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state) {
  for (auto& request : create_anchor_requests_) {
    std::optional<XrLocation> anchor_location =
        GetXrLocationFromNativeOriginInformation(
            openxr, current_stage_parameters,
            request.GetNativeOriginInformation(),
            request.GetNativeOriginFromAnchor(), input_state);
    if (!anchor_location.has_value()) {
      request.TakeCallback().Run(device::mojom::CreateAnchorResult::FAILURE, 0);
      continue;
    }

    XrTime display_time = openxr->GetPredictedDisplayTime();
    XrSpace anchor = CreateAnchor(anchor_location->pose, anchor_location->space,
                                  display_time);

    if (anchor == XR_NULL_HANDLE) {
      request.TakeCallback().Run(device::mojom::CreateAnchorResult::FAILURE, 0);
    } else {
      AnchorId anchor_id = anchor_id_generator_.GenerateNextId();
      CHECK(anchor_id);
      openxr_anchors_.insert({anchor_id, anchor});
      request.TakeCallback().Run(device::mojom::CreateAnchorResult::SUCCESS,
                                 anchor_id.GetUnsafeValue());
    }
  }
  create_anchor_requests_.clear();
}

void OpenXrAnchorManager::DisposeActiveAnchorCallbacks() {
  for (auto& create_anchor : create_anchor_requests_) {
    create_anchor.TakeCallback().Run(mojom::CreateAnchorResult::FAILURE, 0);
  }
  create_anchor_requests_.clear();
}

XrSpace OpenXrAnchorManager::GetAnchorSpace(AnchorId anchor_id) const {
  DCHECK(anchor_id);
  auto it = openxr_anchors_.find(anchor_id);
  if (it == openxr_anchors_.end()) {
    return XR_NULL_HANDLE;
  }
  return it->second;
}

void OpenXrAnchorManager::DetachAnchor(AnchorId anchor_id) {
  DCHECK(anchor_id);
  auto it = openxr_anchors_.find(anchor_id);
  if (it == openxr_anchors_.end()) {
    return;
  }

  XrSpace anchor_space = it->second;
  OnDetachAnchor(anchor_space);
  xrDestroySpace(anchor_space);
  openxr_anchors_.erase(it);
}

mojom::XRAnchorsDataPtr OpenXrAnchorManager::GetCurrentAnchorsData(
    XrTime predicted_display_time) {
  std::vector<uint64_t> all_anchors_ids(openxr_anchors_.size());
  std::vector<mojom::XRAnchorDataPtr> updated_anchors(openxr_anchors_.size());

  uint32_t index = 0;
  std::set<AnchorId> deleted_ids;
  for (const auto& [anchor_id, anchor_space] : openxr_anchors_) {
    all_anchors_ids[index] = anchor_id.GetUnsafeValue();
    auto maybe_pose = GetAnchorFromMojom(anchor_space, predicted_display_time);
    if (maybe_pose.has_value()) {
      updated_anchors[index] = mojom::XRAnchorData::New(
          anchor_id.GetUnsafeValue(), maybe_pose.value());
    } else {
      // Regardless of why it failed, if we still have it tracked, send it up
      // this frame, but remove it for future frames.
      updated_anchors[index] =
          mojom::XRAnchorData::New(anchor_id.GetUnsafeValue(), std::nullopt);
      if (maybe_pose.error() == AnchorTrackingErrorType::kPermanent) {
        deleted_ids.insert(anchor_id);
      }
    }
    index++;
  }

  for (const auto& id : deleted_ids) {
    DetachAnchor(id);
  }

  DVLOG(3) << __func__ << " all_anchor_ids size: " << all_anchors_ids.size();
  return mojom::XRAnchorsData::New(std::move(all_anchors_ids),
                                   std::move(updated_anchors));
}

std::optional<OpenXrAnchorManager::XrLocation>
OpenXrAnchorManager::GetXrLocationFromNativeOriginInformation(
    OpenXrApiWrapper* openxr,
    const mojom::VRStageParametersPtr& current_stage_parameters,
    const mojom::XRNativeOriginInformation& native_origin_information,
    const gfx::Transform& native_origin_from_anchor,
    const std::vector<mojom::XRInputSourceStatePtr>& input_state) const {
  switch (native_origin_information.which()) {
    case mojom::XRNativeOriginInformation::Tag::kInputSourceSpaceInfo:
      // Currently unimplemented as only anchors are supported and are never
      // created relative to input sources
      return std::nullopt;
    case mojom::XRNativeOriginInformation::Tag::kReferenceSpaceType:
      return GetXrLocationFromReferenceSpace(openxr, current_stage_parameters,
                                             native_origin_information,
                                             native_origin_from_anchor);
    // TODO: Look into plane data
    case mojom::XRNativeOriginInformation::Tag::kPlaneId:
    case mojom::XRNativeOriginInformation::Tag::kHandJointSpaceInfo:
    case mojom::XRNativeOriginInformation::Tag::kImageIndex:
      // Unsupported for now
      return std::nullopt;
    case mojom::XRNativeOriginInformation::Tag::kAnchorId:
      return XrLocation{
          GfxTransformToXrPose(native_origin_from_anchor),
          GetAnchorSpace(AnchorId(native_origin_information.get_anchor_id()))};
  }
}

std::optional<OpenXrAnchorManager::XrLocation>
OpenXrAnchorManager::GetXrLocationFromReferenceSpace(
    OpenXrApiWrapper* openxr,
    const mojom::VRStageParametersPtr& current_stage_parameters,
    const mojom::XRNativeOriginInformation& native_origin_information,
    const gfx::Transform& native_origin_from_anchor) const {
  // LocalFloor doesn't have a direct backing XrSpace, it's synthesized based on
  // the local XrSpace and some computed information about "stage space", so we
  // have to adjust the supplied native_origin_from_anchor to local_from_anchor.
  auto type = native_origin_information.get_reference_space_type();
  if (type == device::mojom::XRReferenceSpaceType::kLocalFloor) {
    if (!current_stage_parameters) {
      return std::nullopt;
    }
    // local_from_mojo is currently identity.
    gfx::Transform local_from_mojo;
    // Because we're told that the native origin is local_floor,
    // native_origin_from_anchor is floor_from_anchor.
    const auto& floor_from_anchor = native_origin_from_anchor;
    // We need to generate local_from_anchor to use the local reference space.
    gfx::Transform local_from_anchor =
        local_from_mojo * current_stage_parameters->mojo_from_floor *
        floor_from_anchor;
    return XrLocation{
        GfxTransformToXrPose(local_from_anchor),
        openxr->GetReferenceSpace(device::mojom::XRReferenceSpaceType::kLocal)};
  }

  return XrLocation{GfxTransformToXrPose(native_origin_from_anchor),
                    openxr->GetReferenceSpace(type)};
}

}  // namespace device
