// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/msft/openxr_anchor_manager_msft.h"

#include <optional>

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/pose.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

OpenXrAnchorManagerMsft::OpenXrAnchorManagerMsft(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space)
    : extension_helper_(extension_helper),
      session_(session),
      mojo_space_(mojo_space) {}

OpenXrAnchorManagerMsft::~OpenXrAnchorManagerMsft() {
  // Destruction of the XrSpace will happen in the parent class, but we need to
  // clean up our anchor mapping.
  // Note that we replicate the logic of `OnAnchorDetached` here since that
  // class would modify the map while we are iterating over it.
  for (const auto& [_, anchor] : space_to_anchor_map_) {
    extension_helper_->ExtensionMethods().xrDestroySpatialAnchorMSFT(anchor);
  }

  space_to_anchor_map_.clear();
}

XrSpace OpenXrAnchorManagerMsft::CreateAnchor(XrPosef pose,
                                              XrSpace space,
                                              XrTime predicted_display_time) {
  XrSpatialAnchorMSFT xr_anchor;
  XrSpatialAnchorCreateInfoMSFT anchor_create_info{
      XR_TYPE_SPATIAL_ANCHOR_CREATE_INFO_MSFT};
  anchor_create_info.space = space;
  anchor_create_info.pose = pose;
  anchor_create_info.time = predicted_display_time;

  CHECK(extension_helper_->ExtensionMethods().xrCreateSpatialAnchorMSFT);
  CHECK(extension_helper_->ExtensionMethods().xrCreateSpatialAnchorSpaceMSFT);
  CHECK(extension_helper_->ExtensionMethods().xrDestroySpatialAnchorMSFT);

  if (XR_FAILED(extension_helper_->ExtensionMethods().xrCreateSpatialAnchorMSFT(
          session_, &anchor_create_info, &xr_anchor))) {
    return XR_NULL_HANDLE;
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
    return XR_NULL_HANDLE;
  }

  space_to_anchor_map_.insert({anchor_space, xr_anchor});

  return anchor_space;
}

void OpenXrAnchorManagerMsft::OnDetachAnchor(const XrSpace& anchor_space) {
  auto it = space_to_anchor_map_.find(anchor_space);
  if (it == space_to_anchor_map_.end()) {
    return;
  }

  extension_helper_->ExtensionMethods().xrDestroySpatialAnchorMSFT(it->second);
  space_to_anchor_map_.erase(it);
}

base::expected<device::Pose, OpenXrAnchorManager::AnchorTrackingErrorType>
OpenXrAnchorManagerMsft::GetAnchorFromMojom(
    XrSpace anchor_space,
    XrTime predicted_display_time) const {
  XrSpaceLocation anchor_from_mojo = {XR_TYPE_SPACE_LOCATION};
  if (XR_FAILED(xrLocateSpace(anchor_space, mojo_space_, predicted_display_time,
                              &anchor_from_mojo)) ||
      !IsPoseValid(anchor_from_mojo.locationFlags)) {
    return base::unexpected(
        OpenXrAnchorManager::AnchorTrackingErrorType::kTemporary);
  }

  return XrPoseToDevicePose(anchor_from_mojo.pose);
}

OpenXrAnchorManagerMsftFactory::OpenXrAnchorManagerMsftFactory() = default;
OpenXrAnchorManagerMsftFactory::~OpenXrAnchorManagerMsftFactory() = default;

const base::flat_set<std::string_view>&
OpenXrAnchorManagerMsftFactory::GetRequestedExtensions() const {
  static base::NoDestructor<base::flat_set<std::string_view>> kExtensions(
      {XR_MSFT_SPATIAL_ANCHOR_EXTENSION_NAME});
  return *kExtensions;
}

std::set<device::mojom::XRSessionFeature>
OpenXrAnchorManagerMsftFactory::GetSupportedFeatures(
    const OpenXrExtensionEnumeration* extension_enum) const {
  if (!IsEnabled(extension_enum)) {
    return {};
  }

  return {device::mojom::XRSessionFeature::ANCHORS};
}

std::unique_ptr<OpenXrAnchorManager>
OpenXrAnchorManagerMsftFactory::CreateAnchorManager(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space) const {
  bool is_supported = IsEnabled(extension_helper.ExtensionEnumeration());
  DVLOG(2) << __func__ << " is_supported=" << is_supported;
  if (is_supported) {
    return std::make_unique<OpenXrAnchorManagerMsft>(extension_helper, session,
                                                     mojo_space);
  }

  return nullptr;
}

}  // namespace device
