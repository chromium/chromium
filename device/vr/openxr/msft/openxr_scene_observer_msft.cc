// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/openxr/msft/openxr_scene_observer_msft.h"

#include "base/check.h"
#include "device/vr/openxr/openxr_extension_helper.h"
#include "third_party/openxr/src/include/openxr/openxr.h"

namespace device {

OpenXrSceneObserverMsft::OpenXrSceneObserverMsft(
    const device::OpenXrExtensionHelper& extensions,
    XrSession session)
    : extensions_(extensions),
      scene_observer_(
          XR_NULL_HANDLE,
          OpenXrExtensionHandleTraits<XrSceneObserverMSFT>(
              extensions.ExtensionMethods().xrDestroySceneObserverMSFT)) {
  XrSceneObserverCreateInfoMSFT create_info{
      XR_TYPE_SCENE_OBSERVER_CREATE_INFO_MSFT};
  XrResult create_scene_observer_result =
      extensions.ExtensionMethods().xrCreateSceneObserverMSFT(
          session, &create_info,
          OpenXrExtensionHandle<XrSceneObserverMSFT>::Receiver(scene_observer_)
              .get());
  DCHECK(XR_SUCCEEDED(create_scene_observer_result));
}
OpenXrSceneObserverMsft::~OpenXrSceneObserverMsft() = default;

XrResult OpenXrSceneObserverMsft::ComputeNewScene(
    base::span<const XrSceneComputeFeatureMSFT> requested_features,
    const OpenXrSceneBoundsMsft& bounds) {
  XrNewSceneComputeInfoMSFT compute_info{XR_TYPE_NEW_SCENE_COMPUTE_INFO_MSFT};
  compute_info.requestedFeatureCount =
      static_cast<uint32_t>(requested_features.size());
  compute_info.requestedFeatures = requested_features.data();
  compute_info.bounds.space = bounds.space_;
  compute_info.bounds.time = bounds.time_;
  compute_info.bounds.boxCount =
      static_cast<uint32_t>(bounds.box_bounds_.size());
  compute_info.bounds.boxes = bounds.box_bounds_.data();
  compute_info.bounds.frustumCount =
      static_cast<uint32_t>(bounds.frustum_bounds_.size());
  compute_info.bounds.frustums = bounds.frustum_bounds_.data();
  compute_info.bounds.sphereCount =
      static_cast<uint32_t>(bounds.sphere_bounds_.size());
  compute_info.bounds.spheres = bounds.sphere_bounds_.data();
  compute_info.consistency =
      XR_SCENE_COMPUTE_CONSISTENCY_SNAPSHOT_COMPLETE_MSFT;

  return extensions_->ExtensionMethods().xrComputeNewSceneMSFT(
      scene_observer_.get(), &compute_info);
}

XrSceneComputeStateMSFT OpenXrSceneObserverMsft::GetSceneComputeState() const {
  XrSceneComputeStateMSFT state{XR_SCENE_COMPUTE_STATE_NONE_MSFT};
  XrResult get_compute_state_result =
      extensions_->ExtensionMethods().xrGetSceneComputeStateMSFT(
          scene_observer_.get(), &state);
  DCHECK(XR_SUCCEEDED(get_compute_state_result));
  return state;
}

bool OpenXrSceneObserverMsft::IsSceneComputeCompleted() const {
  const XrSceneComputeStateMSFT state = GetSceneComputeState();
  return state == XR_SCENE_COMPUTE_STATE_COMPLETED_MSFT ||
         state == XR_SCENE_COMPUTE_STATE_COMPLETED_WITH_ERROR_MSFT;
}

std::unique_ptr<OpenXrSceneMsft> OpenXrSceneObserverMsft::CreateScene() const {
  return std::make_unique<OpenXrSceneMsft>(*extensions_, scene_observer_.get());
}

}  // namespace device
