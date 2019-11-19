// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "device/vr/windows_mixed_reality/wrappers/wmr_input_location.h"

#include <windows.ui.input.spatial.h>
#include <wrl.h>

#include "base/logging.h"

namespace WFN = ABI::Windows::Foundation::Numerics;

using ABI::Windows::Foundation::IReference;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceLocation;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceLocation2;
using ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceLocation3;
using Microsoft::WRL::ComPtr;

namespace {
template <class T>
bool TryGetValue(ComPtr<IReference<T>> val_ref, T* out_val) {
  if (!val_ref)
    return false;

  HRESULT hr = val_ref->get_Value(out_val);
  DCHECK(SUCCEEDED(hr));

  return true;
}
}  // anonymous namespace

namespace device {

WMRInputLocationImpl::WMRInputLocationImpl(
    ComPtr<ISpatialInteractionSourceLocation> location)
    : location_(location) {
  DCHECK(location_);
  location_.As(&location2_);
  location_.As(&location3_);
}

WMRInputLocationImpl::~WMRInputLocationImpl() = default;

bool WMRInputLocationImpl::TryGetPosition(WFN::Vector3* position) const {
  DCHECK(position);
  if (!location_)
    return false;
  ComPtr<IReference<WFN::Vector3>> ref;
  HRESULT hr = location_->get_Position(&ref);
  DCHECK(SUCCEEDED(hr));
  return TryGetValue(ref, position);
}

bool WMRInputLocationImpl::TryGetOrientation(
    WFN::Quaternion* orientation) const {
  DCHECK(orientation);
  if (!location2_)
    return false;
  ComPtr<IReference<WFN::Quaternion>> ref;
  HRESULT hr = location2_->get_Orientation(&ref);
  DCHECK(SUCCEEDED(hr));
  return TryGetValue(ref, orientation);
}

bool WMRInputLocationImpl::TryGetPositionAccuracy(
    ABI::Windows::UI::Input::Spatial::SpatialInteractionSourcePositionAccuracy*
        position_accuracy) const {
  DCHECK(position_accuracy);
  if (!location3_)
    return false;
  HRESULT hr = location3_->get_PositionAccuracy(position_accuracy);
  DCHECK(SUCCEEDED(hr));
  return true;
}

}  // namespace device
