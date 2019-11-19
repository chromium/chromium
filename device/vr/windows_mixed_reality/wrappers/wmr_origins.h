// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_ORIGINS_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_ORIGINS_H_

#include <windows.perception.spatial.h>
#include <wrl.h>

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/macros.h"

namespace device {
class WMRTimestamp;

class WMRCoordinateSystem {
 public:
  virtual ~WMRCoordinateSystem() = default;
  virtual bool TryGetTransformTo(
      const WMRCoordinateSystem* other,
      ABI::Windows::Foundation::Numerics::Matrix4x4* this_to_other) = 0;
  virtual ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem*
  GetRawPtr() const;
};

class WMRCoordinateSystemImpl : public WMRCoordinateSystem {
 public:
  explicit WMRCoordinateSystemImpl(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem>
          coordinates);
  ~WMRCoordinateSystemImpl() override;

  bool TryGetTransformTo(
      const WMRCoordinateSystem* other,
      ABI::Windows::Foundation::Numerics::Matrix4x4* this_to_other) override;

  ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem* GetRawPtr()
      const override;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem>
      coordinates_;

  DISALLOW_COPY_AND_ASSIGN(WMRCoordinateSystemImpl);
};

class WMRStationaryOrigin {
 public:
  virtual ~WMRStationaryOrigin() = default;

  virtual std::unique_ptr<WMRCoordinateSystem> CoordinateSystem() = 0;
};

class WMRStationaryOriginImpl : public WMRStationaryOrigin {
 public:
  explicit WMRStationaryOriginImpl(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Perception::Spatial::ISpatialStationaryFrameOfReference>
          stationary_origin);
  ~WMRStationaryOriginImpl() override;

  std::unique_ptr<WMRCoordinateSystem> CoordinateSystem() override;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::Perception::Spatial::ISpatialStationaryFrameOfReference>
      stationary_origin_;

  DISALLOW_COPY_AND_ASSIGN(WMRStationaryOriginImpl);
};

class WMRAttachedOrigin {
 public:
  virtual ~WMRAttachedOrigin() = default;

  virtual std::unique_ptr<WMRCoordinateSystem> TryGetCoordinatesAtTimestamp(
      const WMRTimestamp* timestamp) = 0;
};

class WMRAttachedOriginImpl : public WMRAttachedOrigin {
 public:
  explicit WMRAttachedOriginImpl(
      Microsoft::WRL::ComPtr<ABI::Windows::Perception::Spatial::
                                 ISpatialLocatorAttachedFrameOfReference>
          attached_origin);
  ~WMRAttachedOriginImpl() override;

  std::unique_ptr<WMRCoordinateSystem> TryGetCoordinatesAtTimestamp(
      const WMRTimestamp* timestamp) override;

 private:
  Microsoft::WRL::ComPtr<ABI::Windows::Perception::Spatial::
                             ISpatialLocatorAttachedFrameOfReference>
      attached_origin_;

  DISALLOW_COPY_AND_ASSIGN(WMRAttachedOriginImpl);
};

class WMRStageOrigin {
 public:
  virtual ~WMRStageOrigin() = default;

  virtual std::unique_ptr<WMRCoordinateSystem> CoordinateSystem() = 0;
  virtual ABI::Windows::Perception::Spatial::SpatialMovementRange
  MovementRange() = 0;

  // This will return an empty array if no bounds are set.
  virtual std::vector<ABI::Windows::Foundation::Numerics::Vector3>
  GetMovementBounds(const WMRCoordinateSystem* coordinates) = 0;
};

class WMRStageOriginImpl : public WMRStageOrigin {
 public:
  explicit WMRStageOriginImpl(
      Microsoft::WRL::ComPtr<
          ABI::Windows::Perception::Spatial::ISpatialStageFrameOfReference>
          stage_origin);
  ~WMRStageOriginImpl() override;

  std::unique_ptr<WMRCoordinateSystem> CoordinateSystem() override;
  ABI::Windows::Perception::Spatial::SpatialMovementRange MovementRange()
      override;
  std::vector<ABI::Windows::Foundation::Numerics::Vector3> GetMovementBounds(
      const WMRCoordinateSystem* coordinates) override;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::Perception::Spatial::ISpatialStageFrameOfReference>
      stage_origin_;

  DISALLOW_COPY_AND_ASSIGN(WMRStageOriginImpl);
};

class WMRStageStatics {
 public:
  virtual ~WMRStageStatics() = default;

  virtual std::unique_ptr<WMRStageOrigin> CurrentStage() = 0;

  virtual std::unique_ptr<base::CallbackList<void()>::Subscription>
  AddStageChangedCallback(const base::RepeatingCallback<void()>& cb) = 0;
};

class WMRStageStaticsImpl : public WMRStageStatics {
 public:
  explicit WMRStageStaticsImpl(
      Microsoft::WRL::ComPtr<ABI::Windows::Perception::Spatial::
                                 ISpatialStageFrameOfReferenceStatics>
          stage_statics);
  ~WMRStageStaticsImpl() override;

  std::unique_ptr<WMRStageOrigin> CurrentStage() override;

  std::unique_ptr<base::CallbackList<void()>::Subscription>
  AddStageChangedCallback(const base::RepeatingCallback<void()>& cb) override;

 private:
  HRESULT OnCurrentChanged(IInspectable* sender, IInspectable* args);
  Microsoft::WRL::ComPtr<
      ABI::Windows::Perception::Spatial::ISpatialStageFrameOfReferenceStatics>
      stage_statics_;

  EventRegistrationToken stage_changed_token_;
  base::CallbackList<void()> callback_list_;

  DISALLOW_COPY_AND_ASSIGN(WMRStageStaticsImpl);
};
}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_ORIGINS_H_
