// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/vr_device_base.h"

#include "device/vr/vr_device_provider.h"
#include "device/vr/vr_display_impl.h"

namespace device {

VRDeviceBase::VRDeviceBase(mojom::XRDeviceId id)
    : id_(id), runtime_binding_(this) {}

VRDeviceBase::~VRDeviceBase() = default;

mojom::XRDeviceId VRDeviceBase::GetId() const {
  return id_;
}

void VRDeviceBase::PauseTracking() {}

void VRDeviceBase::ResumeTracking() {}

mojom::VRDisplayInfoPtr VRDeviceBase::GetVRDisplayInfo() {
  return display_info_.Clone();
}

void VRDeviceBase::OnExitPresent() {
  if (listener_)
    listener_->OnExitPresent();
  presenting_ = false;
}

void VRDeviceBase::OnStartPresenting() {
  presenting_ = true;
}

bool VRDeviceBase::HasExclusiveSession() {
  return presenting_;
}

void VRDeviceBase::SetMagicWindowEnabled(bool enabled) {
  magic_window_enabled_ = enabled;
}

void VRDeviceBase::ListenToDeviceChanges(
    mojom::XRRuntimeEventListenerAssociatedPtrInfo listener_info,
    mojom::XRRuntime::ListenToDeviceChangesCallback callback) {
  listener_.Bind(std::move(listener_info));
  std::move(callback).Run(display_info_.Clone());
}

void VRDeviceBase::GetFrameData(
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  if (!magic_window_enabled_) {
    std::move(callback).Run(nullptr);
    return;
  }

  OnMagicWindowFrameDataRequest(std::move(callback));
}

void VRDeviceBase::SetVRDisplayInfo(mojom::VRDisplayInfoPtr display_info) {
  DCHECK(display_info);
  DCHECK(display_info->id == id_);
  display_info_ = std::move(display_info);

  if (listener_)
    listener_->OnDisplayInfoChanged(display_info_.Clone());
}

void VRDeviceBase::OnActivate(mojom::VRDisplayEventReason reason,
                              base::Callback<void(bool)> on_handled) {
  if (listener_)
    listener_->OnDeviceActivated(reason, std::move(on_handled));
}

mojom::XRRuntimePtr VRDeviceBase::BindXRRuntimePtr() {
  mojom::XRRuntimePtr runtime;
  runtime_binding_.Bind(mojo::MakeRequest(&runtime));
  return runtime;
}

bool VRDeviceBase::ShouldPauseTrackingWhenFrameDataRestricted() {
  return false;
}

void VRDeviceBase::OnListeningForActivate(bool listening) {}

void VRDeviceBase::OnMagicWindowFrameDataRequest(
    mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  std::move(callback).Run(nullptr);
}

void VRDeviceBase::SetListeningForActivate(bool is_listening) {
  OnListeningForActivate(is_listening);
}

void VRDeviceBase::EnsureInitialized(EnsureInitializedCallback callback) {
  std::move(callback).Run();
}

void VRDeviceBase::RequestHitTest(
    mojom::XRRayPtr ray,
    mojom::XREnvironmentIntegrationProvider::RequestHitTestCallback callback) {
  NOTREACHED() << "Unexpected call to a device without hit-test support";
  std::move(callback).Run(base::nullopt);
}

void VRDeviceBase::ReturnNonImmersiveSession(
    mojom::XRRuntime::RequestSessionCallback callback) {
  mojom::XRFrameDataProviderPtr data_provider;
  mojom::XREnvironmentIntegrationProviderPtr environment_provider;
  mojom::XRSessionControllerPtr controller;
  magic_window_sessions_.push_back(
      std::make_unique<VRDisplayImpl>(this, mojo::MakeRequest(&data_provider),
                                      mojo::MakeRequest(&environment_provider),
                                      mojo::MakeRequest(&controller)));

  auto session = mojom::XRSession::New();
  session->data_provider = data_provider.PassInterface();
  // TODO(http://crbug.com/876135) Not all sessions want the environment
  // provider. This should be refactored to only be passed when requested.
  session->environment_provider = environment_provider.PassInterface();
  if (display_info_) {
    session->display_info = display_info_.Clone();
  }

  std::move(callback).Run(std::move(session), std::move(controller));
}

void VRDeviceBase::EndMagicWindowSession(VRDisplayImpl* session) {
  base::EraseIf(magic_window_sessions_,
                [&](const std::unique_ptr<VRDisplayImpl>& item) {
                  return item.get() == session;
                });
}

}  // namespace device
