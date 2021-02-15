// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_io_thread.h"
#include "build/build_config.h"
#include "device/base/features.h"
#include "services/device/test/usb_test_gadget.h"
#include "services/device/usb/usb_device.h"
#include "services/device/usb/usb_device_handle.h"
#include "services/device/usb/usb_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

class UsbServiceTest : public ::testing::Test {
 public:
  UsbServiceTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI),
        usb_service_(UsbService::Create()),
        io_thread_(base::TestIOThread::kAutoStart) {}

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<UsbService> usb_service_;
  base::TestIOThread io_thread_;
};

void OnGetDevices(base::OnceClosure quit_closure,
                  const std::vector<scoped_refptr<UsbDevice>>& devices) {
  // Since there's no guarantee that any devices are connected at the moment
  // this test doesn't assume anything about the result but it at least verifies
  // that devices can be enumerated without the application crashing.
  std::move(quit_closure).Run();
}

}  // namespace

TEST_F(UsbServiceTest, GetDevices) {
  // The USB service is not available on all platforms.
  if (usb_service_) {
    base::RunLoop loop;
    usb_service_->GetDevices(base::BindOnce(&OnGetDevices, loop.QuitClosure()));
    loop.Run();
  }
}

#if defined(OS_WIN)
TEST_F(UsbServiceTest, GetDevicesNewBackend) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(device::kNewUsbBackend);

  // The USB service is not available on all platforms.
  if (usb_service_) {
    base::RunLoop loop;
    usb_service_->GetDevices(base::BindOnce(&OnGetDevices, loop.QuitClosure()));
    loop.Run();
  }
}
#endif  // defined(OS_WIN)

TEST_F(UsbServiceTest, ClaimGadget) {
  if (!UsbTestGadget::IsTestEnabled() || !usb_service_)
    return;

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(usb_service_.get(), io_thread_.task_runner());
  ASSERT_TRUE(gadget);

  scoped_refptr<UsbDevice> device = gadget->GetDevice();
  ASSERT_EQ("Google Inc.", base::UTF16ToUTF8(device->manufacturer_string()));
  ASSERT_EQ("Test Gadget (default state)",
            base::UTF16ToUTF8(device->product_string()));
}

TEST_F(UsbServiceTest, DisconnectAndReconnect) {
  if (!UsbTestGadget::IsTestEnabled() || !usb_service_)
    return;

  std::unique_ptr<UsbTestGadget> gadget =
      UsbTestGadget::Claim(usb_service_.get(), io_thread_.task_runner());
  ASSERT_TRUE(gadget);

  ASSERT_TRUE(gadget->Disconnect());
  ASSERT_TRUE(gadget->Reconnect());
}

}  // namespace device
