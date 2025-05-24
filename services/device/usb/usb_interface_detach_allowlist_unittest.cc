// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_interface_detach_allowlist.h"

#include "base/memory/raw_ref.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

class UsbInterfaceDetachAllowlistTest : public ::testing::Test {
 protected:
  UsbInterfaceDetachAllowlistTest() {
    interface_info_.class_code = 1;
    interface_info_.subclass_code = 2;
    interface_info_.protocol_code = 3;
  }
  ~UsbInterfaceDetachAllowlistTest() override = default;

  std::string driver_name_ = "the_driver";
  mojom::UsbAlternateInterfaceInfo interface_info_;
};

TEST_F(UsbInterfaceDetachAllowlistTest, DriverName_Match) {
  UsbInterfaceDetachAllowlist allowlist({
      {/*driver_name=*/"another_driver", /*protocols=*/{}},
      {/*driver_name=*/"the_driver", /*protocols=*/{}},
      {/*driver_name=*/"yet_another_driver", /*protocols=*/{}},
  });

  ASSERT_TRUE(allowlist.CanDetach(driver_name_, interface_info_));
}

TEST_F(UsbInterfaceDetachAllowlistTest, DriverName_NoMatch) {
  UsbInterfaceDetachAllowlist allowlist({
      {/*driver_name=*/"another_driver", /*protocols=*/{}},
      {/*driver_name=*/"yet_another_driver", /*protocols=*/{{1, 2, 3}}},
  });

  ASSERT_FALSE(allowlist.CanDetach(driver_name_, interface_info_));
}

TEST_F(UsbInterfaceDetachAllowlistTest, Interface_Match) {
  UsbInterfaceDetachAllowlist allowlist({
      {/*driver_name=*/"the_driver", /*protocols=*/{{1, 2, 3}}},
      {/*driver_name=*/"another_driver", /*protocols=*/{{4, 5, 6}}},
  });

  ASSERT_TRUE(allowlist.CanDetach(driver_name_, interface_info_));
}

TEST_F(UsbInterfaceDetachAllowlistTest, Interface_NoMatch) {
  UsbInterfaceDetachAllowlist allowlist({
      {/*driver_name=*/"the_driver", /*protocols=*/{{0, 2, 3}}},
      {/*driver_name=*/"the_driver", /*protocols=*/{{1, 0, 3}}},
      {/*driver_name=*/"the_driver", /*protocols=*/{{1, 2, 0}}},
      {/*driver_name=*/"another_driver", /*protocols=*/{{1, 2, 3}}},
  });

  ASSERT_FALSE(allowlist.CanDetach(driver_name_, interface_info_));
}

}  // namespace
}  // namespace device
