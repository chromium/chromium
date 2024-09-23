// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/vibration/vibration_manager_impl.h"

#include <utility>

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace device {

int64_t VibrationManagerImpl::milli_seconds_for_testing_ = -1;
bool VibrationManagerImpl::cancelled_for_testing_ = false;

VibrationManagerImpl::VibrationManagerImpl(
    mojo::PendingRemote<mojom::VibrationManagerListener> listener) {
  CHECK(!listener_);
  listener_.Bind(std::move(listener));
}
VibrationManagerImpl::~VibrationManagerImpl() = default;

void VibrationManagerImpl::Vibrate(int64_t milliseconds,
                                   VibrateCallback callback) {
  PlatformVibrate(milliseconds);
  VibrationManagerImpl::milli_seconds_for_testing_ = milliseconds;
  if (listener_) {
    listener_->OnVibrate();
  }

  std::move(callback).Run();
}

void VibrationManagerImpl::Cancel(CancelCallback callback) {
  VibrationManagerImpl::cancelled_for_testing_ = true;
  std::move(callback).Run();
}

void VibrationManagerImpl::PlatformVibrate(int64_t milliseconds) {}
void VibrationManagerImpl::PlatformCancel() {}

// static

void VibrationManagerImpl::Create(
    mojo::PendingReceiver<mojom::VibrationManager> receiver,
    mojo::PendingRemote<mojom::VibrationManagerListener> listener) {
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<VibrationManagerImpl>(std::move(listener)),
      std::move(receiver));
}

}  // namespace device
