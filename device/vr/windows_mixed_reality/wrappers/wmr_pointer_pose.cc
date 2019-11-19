// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/wrappers/wmr_pointer_pose.h"

#include <windows.perception.h>
#include <windows.ui.input.spatial.h>

#include <wrl.h>

#include "base/logging.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_source.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_pointer_source_pose.h"

namespace WFN = ABI::Windows::Foundation::Numerics;
using ABI::Windows::Perception::People::IHeadPose;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSource;
using ABI::Windows::UI::Input::Spatial::ISpatialPointerInteractionSourcePose;
using ABI::Windows::UI::Input::Spatial::ISpatialPointerPose;
using ABI::Windows::UI::Input::Spatial::ISpatialPointerPose2;
using Microsoft::WRL::ComPtr;

namespace device {
WMRPointerPoseImpl::WMRPointerPoseImpl(ComPtr<ISpatialPointerPose> pointer_pose)
    : pointer_pose_(pointer_pose) {
  DCHECK(pointer_pose_);
  pointer_pose_.As(&pointer_pose2_);

  HRESULT hr = pointer_pose_->get_Head(&head_);
  DCHECK(SUCCEEDED(hr));
}

WMRPointerPoseImpl::~WMRPointerPoseImpl() = default;

bool WMRPointerPoseImpl::IsValid() const {
  return pointer_pose_ != nullptr;
}

std::unique_ptr<WMRPointerSourcePose>
WMRPointerPoseImpl::TryGetInteractionSourcePose(
    const WMRInputSource* source) const {
  if (!pointer_pose2_)
    return nullptr;

  ComPtr<ISpatialPointerInteractionSourcePose> psp_wmr;
  HRESULT hr = pointer_pose2_->TryGetInteractionSourcePose(source->GetRawPtr(),
                                                           &psp_wmr);
  if (SUCCEEDED(hr) && psp_wmr)
    return std::make_unique<WMRPointerSourcePoseImpl>(psp_wmr);

  return nullptr;
}

WFN::Vector3 WMRPointerPoseImpl::HeadForward() const {
  DCHECK(IsValid());
  WFN::Vector3 val;
  HRESULT hr = head_->get_ForwardDirection(&val);
  DCHECK(SUCCEEDED(hr));
  return val;
}

}  // namespace device
