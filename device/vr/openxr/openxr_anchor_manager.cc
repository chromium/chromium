// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/openxr_anchor_manager.h"
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

AnchorId OpenXrAnchorManager::CreateAnchor(XrPosef pose,
                                           XrSpace space,
                                           XrTime predicted_display_time) {
  XrSpatialAnchorMSFT xr_anchor;
  XrSpatialAnchorCreateInfoMSFT anchor_create_info{
      XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_MSFT};
  anchor_create_info.space = space;
  anchor_create_info.pose = pose;
  anchor_create_info.time = predicted_display_time;

  DCHECK(extension_helper_.ExtensionMethods().xrCreateSpatialAnchorMSFT !=
         nullptr);
  DCHECK(extension_helper_.ExtensionMethods().xrCreateSpatialAnchorSpaceMSFT !=
         nullptr);
  DCHECK(extension_helper_.ExtensionMethods().xrDestroySpatialAnchorMSFT !=
         nullptr);

  if (XR_FAILED(extension_helper_.ExtensionMethods().xrCreateSpatialAnchorMSFT(
          session_, &anchor_create_info, &xr_anchor))) {
    return kInvalidAnchorId;
  }

  XrSpace anchor_space;
  XrSpatialAnchorSpaceCreateInfoMSFT space_create_info{
      XR_TYPE_SPATIAL_ANCHOR_SPACE_CREATE_INFO_MSFT};
  space_create_info.anchor = xr_anchor;
  space_create_info.poseInAnchorSpace = PoseIdentity();
  if (FAILED(
          extension_helper_.ExtensionMethods().xrCreateSpatialAnchorSpaceMSFT(
              session_, &space_create_info, &anchor_space))) {
    (void)extension_helper_.ExtensionMethods().xrDestroySpatialAnchorMSFT(
        xr_anchor);
    return kInvalidAnchorId;
  }

  AnchorId anchor_id = anchor_id_generator_.GenerateNextId();
  openxr_anchors_.insert({anchor_id, AnchorData{xr_anchor, anchor_space}});
  return anchor_id;
}

XrSpace OpenXrAnchorManager::GetAnchorSpace(AnchorId anchor_id) const {
  auto it = openxr_anchors_.find(anchor_id);
  if (it == openxr_anchors_.end()) {
    return XR_NULL_HANDLE;
  }
  return it->second.space;
}

void OpenXrAnchorManager::DestroyAnchorData(const AnchorData& anchor_data) {
  (void)xrDestroySpace(anchor_data.space);
  (void)extension_helper_.ExtensionMethods().xrDestroySpatialAnchorMSFT(
      anchor_data.anchor);
}

void OpenXrAnchorManager::DetachAnchor(AnchorId anchor_id) {
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
    if (FAILED(xrLocateSpace(anchor_space, mojo_space_, predicted_display_time,
                             &anchor_from_mojo)) ||
        !(anchor_from_mojo.locationFlags &
          XR_SPACE_LOCATION_POSITION_VALID_BIT) ||
        !(anchor_from_mojo.locationFlags &
          XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
      updated_anchors[index] =
          mojom::XRAnchorData::New(anchor_id.GetUnsafeValue(), base::nullopt);
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
}  // namespace device
