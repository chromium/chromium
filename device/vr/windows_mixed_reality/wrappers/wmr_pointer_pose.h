// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_POINTER_POSE_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_POINTER_POSE_H_

#include <windows.perception.spatial.h>
#include <windows.ui.input.spatial.h>
#include <wrl.h>
#include <memory>

#include "base/macros.h"

namespace device {
class WMRInputSource;
class WMRPointerSourcePose;
class WMRPointerPose {
 public:
  virtual ~WMRPointerPose() = default;

  virtual bool IsValid() const = 0;
  virtual std::unique_ptr<WMRPointerSourcePose> TryGetInteractionSourcePose(
      const WMRInputSource* source) const = 0;
  virtual ABI::Windows::Foundation::Numerics::Vector3 HeadForward() const = 0;
};

class WMRPointerPoseImpl : public WMRPointerPose {
 public:
  explicit WMRPointerPoseImpl(
      Microsoft::WRL::ComPtr<
          ABI::Windows::UI::Input::Spatial::ISpatialPointerPose> pointer_pose);
  ~WMRPointerPoseImpl() override;

  bool IsValid() const override;

  // Uses ISpatialPointerPose2.
  std::unique_ptr<WMRPointerSourcePose> TryGetInteractionSourcePose(
      const WMRInputSource* source) const override;

  // Uses IHeadPose.
  ABI::Windows::Foundation::Numerics::Vector3 HeadForward() const override;

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::UI::Input::Spatial::ISpatialPointerPose>
      pointer_pose_;
  Microsoft::WRL::ComPtr<ABI::Windows::UI::Input::Spatial::ISpatialPointerPose2>
      pointer_pose2_;

  // This is a simple interface, so expose it directly rather than create
  // a new wrapper class.
  Microsoft::WRL::ComPtr<ABI::Windows::Perception::People::IHeadPose> head_;
  DISALLOW_COPY(WMRPointerPoseImpl);
};

}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_POINTER_POSE_H_
