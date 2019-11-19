// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_device_enumerator.h"

#include <vector>

#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class SerialDeviceEnumeratorTest : public testing::Test {
 public:
  SerialDeviceEnumeratorTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}
  ~SerialDeviceEnumeratorTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(SerialDeviceEnumeratorTest, GetDevices) {
  // There is no guarantee that a test machine will have a serial device
  // available. The purpose of this test is to ensure that the process of
  // attempting to enumerate devices does not cause a crash.
  auto enumerator = SerialDeviceEnumerator::Create();
  ASSERT_TRUE(enumerator);
  std::vector<mojom::SerialPortInfoPtr> devices = enumerator->GetDevices();
}

}  // namespace

}  // namespace device
