// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/windows_mixed_reality/wrappers/wmr_origins.h"

#include <windows.perception.spatial.h>
#include <wrl.h>
#include <wrl/event.h>

#include <cstdint>
#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_hstring.h"
#include "device/vr/windows_mixed_reality/mixed_reality_statics.h"
#include "device/vr/windows_mixed_reality/wrappers/test/mock_wmr_origins.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_logging.h"
#include "device/vr/windows_mixed_reality/wrappers/wmr_timestamp.h"

namespace WFN = ABI::Windows::Foundation::Numerics;
using SpatialMovementRange =
    ABI::Windows::Perception::Spatial::SpatialMovementRange;
using ABI::Windows::Foundation::IEventHandler;
using ABI::Windows::Foundation::IReference;
using ABI::Windows::Perception::Spatial::ISpatialCoordinateSystem;
using ABI::Windows::Perception::Spatial::ISpatialLocator;
using ABI::Windows::Perception::Spatial::
    ISpatialLocatorAttachedFrameOfReference;
using ABI::Windows::Perception::Spatial::ISpatialLocatorStatics;
using ABI::Windows::Perception::Spatial::ISpatialStageFrameOfReference;
using ABI::Windows::Perception::Spatial::ISpatialStageFrameOfReferenceStatics;
using ABI::Windows::Perception::Spatial::ISpatialStationaryFrameOfReference;
using Microsoft::WRL::ComPtr;

namespace device {
// WMRCoordinateSystem
ISpatialCoordinateSystem* WMRCoordinateSystem::GetRawPtr() const {
  // This should only ever be used by the real implementation, so by default
  // make sure it's not called.
  NOTREACHED();
  return nullptr;
}

WMRCoordinateSystemImpl::WMRCoordinateSystemImpl(
    ComPtr<ISpatialCoordinateSystem> coordinates)
    : coordinates_(coordinates) {
  DCHECK(coordinates_);
}

WMRCoordinateSystemImpl::~WMRCoordinateSystemImpl() = default;

bool WMRCoordinateSystemImpl::TryGetTransformTo(
    const WMRCoordinateSystem* other,
    WFN::Matrix4x4* this_to_other) {
  DCHECK(this_to_other);
  DCHECK(other);
  ComPtr<IReference<WFN::Matrix4x4>> this_to_other_ref;
  HRESULT hr =
      coordinates_->TryGetTransformTo(other->GetRawPtr(), &this_to_other_ref);
  if (FAILED(hr) || !this_to_other_ref) {
    WMRLogging::TraceError(WMRErrorLocation::kGetTransformBetweenOrigins, hr);
    return false;
  }

  hr = this_to_other_ref->get_Value(this_to_other);
  DCHECK(SUCCEEDED(hr));
  return true;
}

ISpatialCoordinateSystem* WMRCoordinateSystemImpl::GetRawPtr() const {
  return coordinates_.Get();
}

// WMRStationaryOrigin
WMRStationaryOriginImpl::WMRStationaryOriginImpl(
    ComPtr<ISpatialStationaryFrameOfReference> stationary_origin)
    : stationary_origin_(stationary_origin) {
  DCHECK(stationary_origin_);
}

WMRStationaryOriginImpl::~WMRStationaryOriginImpl() = default;

std::unique_ptr<WMRCoordinateSystem>
WMRStationaryOriginImpl::CoordinateSystem() {
  ComPtr<ISpatialCoordinateSystem> coordinates;
  HRESULT hr = stationary_origin_->get_CoordinateSystem(&coordinates);
  DCHECK(SUCCEEDED(hr));
  return std::make_unique<WMRCoordinateSystemImpl>(coordinates);
}

// WMRAttachedOrigin
WMRAttachedOriginImpl::WMRAttachedOriginImpl(
    ComPtr<ISpatialLocatorAttachedFrameOfReference> attached_origin)
    : attached_origin_(attached_origin) {
  DCHECK(attached_origin_);
}

WMRAttachedOriginImpl::~WMRAttachedOriginImpl() = default;

std::unique_ptr<WMRCoordinateSystem>
WMRAttachedOriginImpl::TryGetCoordinatesAtTimestamp(
    const WMRTimestamp* timestamp) {
  ComPtr<ISpatialCoordinateSystem> coordinates;
  HRESULT hr = attached_origin_->GetStationaryCoordinateSystemAtTimestamp(
      timestamp->GetRawPtr(), &coordinates);
  if (FAILED(hr))
    return nullptr;

  return std::make_unique<WMRCoordinateSystemImpl>(coordinates);
}

// WMRStageOrigin
WMRStageOriginImpl::WMRStageOriginImpl(
    ComPtr<ISpatialStageFrameOfReference> stage_origin)
    : stage_origin_(stage_origin) {
  DCHECK(stage_origin_);
}

WMRStageOriginImpl::~WMRStageOriginImpl() = default;

std::unique_ptr<WMRCoordinateSystem> WMRStageOriginImpl::CoordinateSystem() {
  ComPtr<ISpatialCoordinateSystem> coordinates;
  HRESULT hr = stage_origin_->get_CoordinateSystem(&coordinates);
  DCHECK(SUCCEEDED(hr));

  return std::make_unique<WMRCoordinateSystemImpl>(coordinates);
}

SpatialMovementRange WMRStageOriginImpl::MovementRange() {
  SpatialMovementRange movement_range;
  HRESULT hr = stage_origin_->get_MovementRange(&movement_range);
  DCHECK(SUCCEEDED(hr));

  return movement_range;
}

std::vector<WFN::Vector3> WMRStageOriginImpl::GetMovementBounds(
    const WMRCoordinateSystem* coordinates) {
  DCHECK(coordinates);

  std::vector<WFN::Vector3> ret_val;
  uint32_t size;
  base::win::ScopedCoMem<WFN::Vector3> bounds;
  HRESULT hr = stage_origin_->TryGetMovementBounds(coordinates->GetRawPtr(),
                                                   &size, &bounds);
  if (FAILED(hr))
    return ret_val;

  for (uint32_t i = 0; i < size; i++) {
    ret_val.push_back(bounds[i]);
  }

  return ret_val;
}

// WMRStageStatics
WMRStageStaticsImpl::WMRStageStaticsImpl(
    ComPtr<ISpatialStageFrameOfReferenceStatics> stage_statics)
    : stage_statics_(stage_statics) {
  DCHECK(stage_statics_);
  auto callback = Microsoft::WRL::Callback<IEventHandler<IInspectable*>>(
      this, &WMRStageStaticsImpl::OnCurrentChanged);
  HRESULT hr =
      stage_statics_->add_CurrentChanged(callback.Get(), &stage_changed_token_);
  DCHECK(SUCCEEDED(hr));
}

WMRStageStaticsImpl::~WMRStageStaticsImpl() {
  if (stage_changed_token_.value != 0) {
    HRESULT hr = stage_statics_->remove_CurrentChanged(stage_changed_token_);
    stage_changed_token_.value = 0;
    DCHECK(SUCCEEDED(hr));
  }
}

std::unique_ptr<WMRStageOrigin> WMRStageStaticsImpl::CurrentStage() {
  ComPtr<ISpatialStageFrameOfReference> stage_origin;
  HRESULT hr = stage_statics_->get_Current(&stage_origin);
  if (FAILED(hr) || !stage_origin) {
    WMRLogging::TraceError(WMRErrorLocation::kAcquireCurrentStage, hr);
    return nullptr;
  }

  return std::make_unique<WMRStageOriginImpl>(stage_origin);
}

std::unique_ptr<base::CallbackList<void()>::Subscription>
WMRStageStaticsImpl::AddStageChangedCallback(
    const base::RepeatingCallback<void()>& cb) {
  return callback_list_.Add(cb);
}

HRESULT WMRStageStaticsImpl::OnCurrentChanged(IInspectable*, IInspectable*) {
  callback_list_.Notify();
  return S_OK;
}
}  // namespace device
