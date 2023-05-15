// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device_base.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "device/vr/public/cpp/vr_device_provider.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_types.h"
#endif

namespace device {

VRDeviceBase::VRDeviceBase(mojom::XRDeviceId id) : id_(id) {
  device_data_.is_ar_blend_mode_supported = false;
}

VRDeviceBase::~VRDeviceBase() = default;

mojom::XRDeviceId VRDeviceBase::GetId() const {
  return id_;
}

mojom::XRDeviceDataPtr VRDeviceBase::GetDeviceData() const {
  return device_data_.Clone();
}

void VRDeviceBase::PauseTracking() {}

void VRDeviceBase::ResumeTracking() {}

void VRDeviceBase::OnExitPresent() {
  DVLOG(2) << __func__ << ": !!listener_=" << !!listener_;
  if (listener_)
    listener_->OnExitPresent();
  presenting_ = false;
}

void VRDeviceBase::OnStartPresenting() {
  DVLOG(2) << __func__;
  presenting_ = true;
}

bool VRDeviceBase::HasExclusiveSession() {
  return presenting_;
}

void VRDeviceBase::ListenToDeviceChanges(
    mojo::PendingAssociatedRemote<mojom::XRRuntimeEventListener>
        listener_info) {
  listener_.Bind(std::move(listener_info));
}

void VRDeviceBase::OnVisibilityStateChanged(
    mojom::XRVisibilityState visibility_state) {
  if (listener_)
    listener_->OnVisibilityStateChanged(visibility_state);
}

void VRDeviceBase::SetArBlendModeSupported(bool is_ar_blend_mode_supported) {
  device_data_.is_ar_blend_mode_supported = is_ar_blend_mode_supported;
}

#if BUILDFLAG(IS_WIN)
void VRDeviceBase::SetLuid(const CHROME_LUID& luid) {
  if (luid.HighPart != 0 || luid.LowPart != 0) {
    // Only set the LUID if it exists and is nonzero.
    device_data_.luid = luid;
  }
}
#endif

mojo::PendingRemote<mojom::XRRuntime> VRDeviceBase::BindXRRuntime() {
  DVLOG(2) << __func__;
  return runtime_receiver_.BindNewPipeAndPassRemote();
}

void LogViewerType(VrViewerType type) {
  base::UmaHistogramSparse("VRViewerType", static_cast<int>(type));
}

void VRDeviceBase::SetSupportedFeatures(
        const std::vector<mojom::XRSessionFeature>& features) {
  device_data_.supported_features = features;
}

void VRDeviceBase::SetDeviceData(device::mojom::XRDeviceData&& device_data) {
  device_data_ = std::move(device_data);
}

}  // namespace device
