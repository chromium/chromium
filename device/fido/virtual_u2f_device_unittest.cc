// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/virtual_u2f_device.h"

#include <cstdint>
#include <memory>

#include "base/containers/span.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/virtual_fido_device.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

void SendCommand(VirtualU2fDevice* device, base::span<const uint8_t> command) {
  device->DeviceTransact(fido_parsing_utils::Materialize(command),
                         base::DoNothing());
}

}  // namespace

class VirtualU2fDeviceTest : public ::testing::Test {
 protected:
  void MakeSelfDestructingDevice() {
    auto state = base::MakeRefCounted<VirtualFidoDevice::State>();
    device_ = std::make_unique<VirtualU2fDevice>(state);

    state->simulate_press_callback =
        base::BindLambdaForTesting([&](VirtualFidoDevice* _) {
          device_.reset();
          return true;
        });
  }

  std::unique_ptr<VirtualU2fDevice> device_;

  base::test::TaskEnvironment task_environment_;
};

// Tests that destroying the virtual device from the |simulate_press_callback|
// does not crash.
TEST_F(VirtualU2fDeviceTest, DestroyInsideSimulatePressCallback) {
  MakeSelfDestructingDevice();
  SendCommand(device_.get(), test_data::kU2fRegisterCommandApdu);
  ASSERT_FALSE(device_);

  MakeSelfDestructingDevice();
  SendCommand(device_.get(), test_data::kU2fSignCommandApduWithKeyAlpha);
  ASSERT_FALSE(device_);
}

}  // namespace device
