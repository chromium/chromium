// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/android/openxr_anchor_manager_android.h"

#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "base/types/expected.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "device/vr/openxr/openxr_util.h"
#include "device/vr/public/mojom/pose.h"
#include "device/vr/public/mojom/xr_session.mojom-shared.h"
#include "third_party/openxr/dev/xr_android.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

OpenXrAnchorManagerAndroid::OpenXrAnchorManagerAndroid(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space)
    : extension_helper_(extension_helper),
      session_(session),
      mojo_space_(mojo_space) {}

OpenXrAnchorManagerAndroid::~OpenXrAnchorManagerAndroid() = default;

XrSpace OpenXrAnchorManagerAndroid::CreateAnchor(
    XrPosef pose,
    XrSpace space,
    XrTime predicted_display_time) {
  XrAnchorSpaceCreateInfoANDROID anchor_create_info{
      XR_TYPE_ANCHOR_SPACE_CREATE_INFO_ANDROID};
  anchor_create_info.space = space;
  anchor_create_info.time = predicted_display_time;
  anchor_create_info.pose = pose;

  XrSpace anchor_space = XR_NULL_HANDLE;
  XrResult result =
      extension_helper_->ExtensionMethods().xrCreateAnchorSpaceANDROID(
          session_, &anchor_create_info, &anchor_space);
  RETURN_VAL_IF_XR_FAILED(result, XR_NULL_HANDLE);

  return anchor_space;
}

void OpenXrAnchorManagerAndroid::OnDetachAnchor(const XrSpace& anchor_space) {
  // Nothing to do as the base class manages the space, which is all we need.
}

base::expected<device::Pose, OpenXrAnchorManager::AnchorTrackingErrorType>
OpenXrAnchorManagerAndroid::GetAnchorFromMojom(
    XrSpace anchor_space,
    XrTime predicted_display_time) const {
  XrSpaceLocation anchor_from_mojo = {XR_TYPE_SPACE_LOCATION};
  XrAnchorStateANDROID anchor_state{XR_TYPE_ANCHOR_STATE_ANDROID};
  anchor_from_mojo.next = &anchor_state;
  XrResult result = xrLocateSpace(anchor_space, mojo_space_,
                                  predicted_display_time, &anchor_from_mojo);
  if (XR_FAILED(result)) {
    DVLOG(3) << __func__ << " xrLocateSpace returned: " << result;
    return base::unexpected(
        OpenXrAnchorManager::AnchorTrackingErrorType::kTemporary);
  }

  if (anchor_state.trackingState == XR_TRACKING_STATE_STOPPED_ANDROID) {
    DVLOG(3) << __func__ << " Anchor is no longer tracked";
    return base::unexpected(
        OpenXrAnchorManager::AnchorTrackingErrorType::kPermanent);
  }

  if (!IsPoseValid(anchor_from_mojo.locationFlags)) {
    DVLOG(3) << __func__ << " Anchor pose was not valid";
    return base::unexpected(
        OpenXrAnchorManager::AnchorTrackingErrorType::kTemporary);
  }

  return XrPoseToDevicePose(anchor_from_mojo.pose);
}

OpenXrAnchorManagerAndroidFactory::OpenXrAnchorManagerAndroidFactory() =
    default;
OpenXrAnchorManagerAndroidFactory::~OpenXrAnchorManagerAndroidFactory() =
    default;

const base::flat_set<std::string_view>&
OpenXrAnchorManagerAndroidFactory::GetRequestedExtensions() const {
  static base::NoDestructor<base::flat_set<std::string_view>> kExtensions(
      {XR_ANDROID_TRACKABLES_EXTENSION_NAME});
  return *kExtensions;
}

std::set<device::mojom::XRSessionFeature>
OpenXrAnchorManagerAndroidFactory::GetSupportedFeatures(
    const OpenXrExtensionEnumeration* extension_enum) const {
  if (!IsEnabled(extension_enum)) {
    return {};
  }

  return {device::mojom::XRSessionFeature::ANCHORS};
}

std::unique_ptr<OpenXrAnchorManager>
OpenXrAnchorManagerAndroidFactory::CreateAnchorManager(
    const OpenXrExtensionHelper& extension_helper,
    XrSession session,
    XrSpace mojo_space) const {
  bool is_supported = IsEnabled(extension_helper.ExtensionEnumeration());
  DVLOG(2) << __func__ << " is_supported=" << is_supported;
  if (is_supported) {
    return std::make_unique<OpenXrAnchorManagerAndroid>(extension_helper,
                                                        session, mojo_space);
  }

  return nullptr;
}

}  // namespace device
