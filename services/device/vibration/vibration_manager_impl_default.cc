// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/vibration/vibration_manager_impl.h"

#include <stdint.h>
#include <utility>

#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace device {

int64_t VibrationManagerImpl::milli_seconds_for_testing_ = -1;
bool VibrationManagerImpl::cancelled_for_testing_ = false;

namespace {

class VibrationManagerEmptyImpl : public mojom::VibrationManager {
 public:
  VibrationManagerEmptyImpl() {}
  ~VibrationManagerEmptyImpl() override {}

  void Vibrate(int64_t milliseconds, VibrateCallback callback) override {
    VibrationManagerImpl::milli_seconds_for_testing_ = milliseconds;
    std::move(callback).Run();
  }

  void Cancel(CancelCallback callback) override {
    VibrationManagerImpl::cancelled_for_testing_ = true;
    std::move(callback).Run();
  }
};

}  // namespace

// static
void VibrationManagerImpl::Create(
    mojo::PendingReceiver<mojom::VibrationManager> receiver) {
  mojo::MakeSelfOwnedReceiver(std::make_unique<VibrationManagerEmptyImpl>(),
                              std::move(receiver));
}

}  // namespace device
