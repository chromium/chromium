// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_anchor_manager.h"

#include <tuple>

#include "device/vr/openxr/openxr_api_wrapper.h"
#include "device/vr/openxr/openxr_util.h"

namespace device {

namespace {
device::Pose XrPoseToDevicePose(const XrPosef& pose) {
  gfx::Quaternion orientation{pose.orientation.x, pose.orientation.y,
                              pose.orientation.z, pose.orientation.w};
  gfx::Point3F position{pose.position.x, pose.position.y, pose.position.z};
  return device::Pose{position, orientation};
}
}  // namespace

OpenXrAnchorManager::~OpenXrAnchorManager() {
  DisposeActiveAnchorCallbacks();
  for (const auto& it : openxr_anchors_) {
    DestroyAnchorData(it.second);
  }
}

OpenXrAnchorManager::OpenXrAnchorManager(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space)
    : extension_helper_(extension_helper),
      session_(session),
      mojo_space_(mojo_space) {}

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
    absl::optional<XrLocation> anchor_location =
        GetXrLocationFromNativeOriginInformation(
            openxr, current_stage_parameters,
            request.GetNativeOriginInformation(),
            request.GetNativeOriginFromAnchor(), input_state);
    if (!anchor_location.has_value()) {
      request.TakeCallback().Run(device::mojom::CreateAnchorResult::FAILURE, 0);
      continue;
    }

    AnchorId anchor_id = kInvalidAnchorId;
    XrTime display_time = openxr->GetPredictedDisplayTime();
    anchor_id = CreateAnchor(anchor_location->pose, anchor_location->space,
                             display_time);

    if (anchor_id.is_null()) {
      request.TakeCallback().Run(device::mojom::CreateAnchorResult::FAILURE, 0);
    } else {
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

AnchorId OpenXrAnchorManager::CreateAnchor(XrPosef pose,
                                           XrSpace space,
                                           XrTime predicted_display_time) {
  XrSpatialAnchorMSFT xr_anchor;
  XrSpatialAnchorCreateInfoMSFT anchor_create_info{
      XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_MSFT};
  anchor_create_info.space = space;
  anchor_create_info.pose = pose;
  anchor_create_info.time = predicted_display_time;

  DCHECK(extension_helper_->ExtensionMethods().xrCreateSpatialAnchorMSFT);
  DCHECK(extension_helper_->ExtensionMethods().xrCreateSpatialAnchorSpaceMSFT);
  DCHECK(extension_helper_->ExtensionMethods().xrDestroySpatialAnchorMSFT);

  if (XR_FAILED(extension_helper_->ExtensionMethods().xrCreateSpatialAnchorMSFT(
          session_, &anchor_create_info, &xr_anchor))) {
    return kInvalidAnchorId;
  }

  XrSpace anchor_space;
  XrSpatialAnchorSpaceCreateInfoMSFT space_create_info{
      XR_TYPE_SPATIAL_ANCHOR_SPACE_CREATE_INFO_MSFT};
  space_create_info.anchor = xr_anchor;
  space_create_info.poseInAnchorSpace = PoseIdentity();
  if (XR_FAILED(
          extension_helper_->ExtensionMethods().xrCreateSpatialAnchorSpaceMSFT(
              session_, &space_create_info, &anchor_space))) {
    std::ignore =
        extension_helper_->ExtensionMethods().xrDestroySpatialAnchorMSFT(
            xr_anchor);
    return kInvalidAnchorId;
  }

  AnchorId anchor_id = anchor_id_generator_.GenerateNextId();
  DCHECK(anchor_id);
  openxr_anchors_.insert({anchor_id, AnchorData{xr_anchor, anchor_space}});
  return anchor_id;
}

XrSpace OpenXrAnchorManager::GetAnchorSpace(AnchorId anchor_id) const {
  DCHECK(anchor_id);
  auto it = openxr_anchors_.find(anchor_id);
  if (it == openxr_anchors_.end()) {
    return XR_NULL_HANDLE;
  }
  return it->second.space;
}

void OpenXrAnchorManager::DestroyAnchorData(
    const AnchorData& anchor_data) const {
  std::ignore = xrDestroySpace(anchor_data.space);
  std::ignore =
      extension_helper_->ExtensionMethods().xrDestroySpatialAnchorMSFT(
          anchor_data.anchor);
}

void OpenXrAnchorManager::DetachAnchor(AnchorId anchor_id) {
  DCHECK(anchor_id);
  auto it = openxr_anchors_.find(anchor_id);
  if (it == openxr_anchors_.end()) {
    return;
  }

  DestroyAnchorData(it->second);
  openxr_anchors_.erase(it);
}

mojom::XRAnchorsDataPtr OpenXrAnchorManager::GetCurrentAnchorsData(
    XrTime predicted_display_time) const {
  std::vector<uint64_t> all_anchors_ids(openxr_anchors_.size());
  std::vector<mojom::XRAnchorDataPtr> updated_anchors(openxr_anchors_.size());

  uint32_t index = 0;
  for (const auto& map_entry : openxr_anchors_) {
    const AnchorId anchor_id = map_entry.first;
    const XrSpace anchor_space = map_entry.second.space;
    all_anchors_ids[index] = anchor_id.GetUnsafeValue();

    XrSpaceLocation anchor_from_mojo = {XR_TYPE_SPACE_LOCATION};
    if (XR_FAILED(xrLocateSpace(anchor_space, mojo_space_,
                                predicted_display_time, &anchor_from_mojo)) ||
        !IsPoseValid(anchor_from_mojo.locationFlags)) {
      updated_anchors[index] =
          mojom::XRAnchorData::New(anchor_id.GetUnsafeValue(), absl::nullopt);
    } else {
      updated_anchors[index] =
          mojom::XRAnchorData::New(anchor_id.GetUnsafeValue(),
                                   XrPoseToDevicePose(anchor_from_mojo.pose));
    }
    index++;
  }

  return mojom::XRAnchorsData::New(std::move(all_anchors_ids),
                                   std::move(updated_anchors));
}

absl::optional<OpenXrAnchorManager::XrLocation>
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
      return absl::nullopt;
    case mojom::XRNativeOriginInformation::Tag::kReferenceSpaceType:
      return GetXrLocationFromReferenceSpace(openxr, current_stage_parameters,
                                             native_origin_information,
                                             native_origin_from_anchor);
    case mojom::XRNativeOriginInformation::Tag::kPlaneId:
    case mojom::XRNativeOriginInformation::Tag::kHandJointSpaceInfo:
    case mojom::XRNativeOriginInformation::Tag::kImageIndex:
      // Unsupported for now
      return absl::nullopt;
    case mojom::XRNativeOriginInformation::Tag::kAnchorId:
      return XrLocation{
          GfxTransformToXrPose(native_origin_from_anchor),
          GetAnchorSpace(AnchorId(native_origin_information.get_anchor_id()))};
  }
}

absl::optional<OpenXrAnchorManager::XrLocation>
OpenXrAnchorManager::GetXrLocationFromReferenceSpace(
    OpenXrApiWrapper* openxr,
    const mojom::VRStageParametersPtr& current_stage_parameters,
    const mojom::XRNativeOriginInformation& native_origin_information,
    const gfx::Transform& native_origin_from_anchor) const {
  // Floor corresponds to offset_from_local * local, so we must apply the
  // offset to get the correct pose in the local space.
  auto type = native_origin_information.get_reference_space_type();
  if (type == device::mojom::XRReferenceSpaceType::kLocalFloor) {
    if (!current_stage_parameters) {
      return absl::nullopt;
    }
    // The offset from the floor to mojo (aka local) is encoded in
    // current_stage_parameters->mojo_from_floor. mojo_from_floor *
    // native_origin_from_anchor gives us the correct location of the anchor
    // relative to the local floor reference space.
    return XrLocation{
        GfxTransformToXrPose(current_stage_parameters->mojo_from_floor *
                             native_origin_from_anchor),
        openxr->GetReferenceSpace(device::mojom::XRReferenceSpaceType::kLocal)};
  }

  return XrLocation{GfxTransformToXrPose(native_origin_from_anchor),
                    openxr->GetReferenceSpace(type)};
}

}  // namespace device
