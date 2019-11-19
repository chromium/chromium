// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device_base.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "device/vr/vr_device_provider.h"

namespace device {

VRDeviceBase::VRDeviceBase(mojom::XRDeviceId id) : id_(id) {}

VRDeviceBase::~VRDeviceBase() = default;

mojom::XRDeviceId VRDeviceBase::GetId() const {
  return id_;
}

void VRDeviceBase::PauseTracking() {}

void VRDeviceBase::ResumeTracking() {}

mojom::VRDisplayInfoPtr VRDeviceBase::GetVRDisplayInfo() {
  return display_info_.Clone();
}

void VRDeviceBase::ShutdownSession(base::OnceClosure on_completed) {
  DVLOG(2) << __func__;
  // TODO(https://crbug.com/1015594): The default implementation of running the
  // callback immediately is backwards compatible, but runtimes should be
  // updated to override this, calling the callback at the appropriate time
  // after any necessary cleanup has been completed. Once that's done, make this
  // method abstract.
  std::move(on_completed).Run();
}

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
    mojo::PendingAssociatedRemote<mojom::XRRuntimeEventListener> listener_info,
    mojom::XRRuntime::ListenToDeviceChangesCallback callback) {
  listener_.Bind(std::move(listener_info));
  std::move(callback).Run(display_info_.Clone());
}

void VRDeviceBase::SetVRDisplayInfo(mojom::VRDisplayInfoPtr display_info) {
  DCHECK(display_info);
  DCHECK(display_info->id == id_);
  display_info_ = std::move(display_info);

  if (listener_)
    listener_->OnDisplayInfoChanged(display_info_.Clone());
}

void VRDeviceBase::OnVisibilityStateChanged(
    mojom::XRVisibilityState visibility_state) {
  if (listener_)
    listener_->OnVisibilityStateChanged(visibility_state);
}

mojo::PendingRemote<mojom::XRRuntime> VRDeviceBase::BindXRRuntime() {
  DVLOG(2) << __func__;
  return runtime_receiver_.BindNewPipeAndPassRemote();
}

void VRDeviceBase::SetInlinePosesEnabled(bool enable) {
  inline_poses_enabled_ = enable;
}

void VRDeviceBase::RequestHitTest(
    mojom::XRRayPtr ray,
    mojom::XREnvironmentIntegrationProvider::RequestHitTestCallback callback) {
  NOTREACHED() << "Unexpected call to a device without hit-test support";
  std::move(callback).Run(base::nullopt);
}

void LogViewerType(VrViewerType type) {
  base::UmaHistogramSparse("VRViewerType", static_cast<int>(type));
}

}  // namespace device
