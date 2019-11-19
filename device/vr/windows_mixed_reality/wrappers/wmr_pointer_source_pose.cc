// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/wmr_pointer_source_pose.h"

#include <windows.ui.input.spatial.h>

#include <wrl.h>

#include "base/logging.h"

namespace WFN = ABI::Windows::Foundation::Numerics;
using ABI::Windows::UI::Input::Spatial::ISpatialPointerInteractionSourcePose;
using ABI::Windows::UI::Input::Spatial::ISpatialPointerInteractionSourcePose2;
using Microsoft::WRL::ComPtr;

namespace device {
WMRPointerSourcePoseImpl::WMRPointerSourcePoseImpl(
    ComPtr<ISpatialPointerInteractionSourcePose> pointer_source_pose)
    : pointer_source_pose_(pointer_source_pose) {
  DCHECK(pointer_source_pose_);
  HRESULT hr = pointer_source_pose_.As(&pointer_source_pose2_);
  DCHECK(SUCCEEDED(hr));
}

WMRPointerSourcePoseImpl::~WMRPointerSourcePoseImpl() = default;

bool WMRPointerSourcePoseImpl::IsValid() const {
  return pointer_source_pose_ != nullptr;
}

WFN::Vector3 WMRPointerSourcePoseImpl::Position() const {
  DCHECK(IsValid());
  WFN::Vector3 val;
  HRESULT hr = pointer_source_pose_->get_Position(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

WFN::Quaternion WMRPointerSourcePoseImpl::Orientation() const {
  DCHECK(IsValid());
  WFN::Quaternion val;
  HRESULT hr = pointer_source_pose2_->get_Orientation(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

}  // namespace device
