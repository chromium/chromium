// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_POINTER_SOURCE_POSE_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_POINTER_SOURCE_POSE_H_

#include <windows.ui.input.spatial.h>
#include <wrl.h>

#include "base/macros.h"

namespace device {
class WMRPointerSourcePose {
 public:
  virtual ~WMRPointerSourcePose() = default;

  virtual bool IsValid() const = 0;
  virtual ABI::Windows::Foundation::Numerics::Vector3 Position() const = 0;
  virtual ABI::Windows::Foundation::Numerics::Quaternion Orientation()
      const = 0;
};

class WMRPointerSourcePoseImpl : public WMRPointerSourcePose {
 public:
  explicit WMRPointerSourcePoseImpl(
      Microsoft::WRL::ComPtr<ABI::Windows::UI::Input::Spatial::
                                 ISpatialPointerInteractionSourcePose>
          pointer_pose);
  ~WMRPointerSourcePoseImpl() override;

  bool IsValid() const override;

  // Uses ISpatialPointerInteractionSourcePose.
  ABI::Windows::Foundation::Numerics::Vector3 Position() const override;

  // Uses ISpatialPointerInteractionSourcePose2.
  ABI::Windows::Foundation::Numerics::Quaternion Orientation() const override;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialPointerInteractionSourcePose>
      pointer_source_pose_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialPointerInteractionSourcePose2>
      pointer_source_pose2_;

  DISALLOW_COPY(WMRPointerSourcePoseImpl);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_POINTER_SOURCE_POSE_H_
