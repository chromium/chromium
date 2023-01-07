// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_context.h"
#include "base/threading/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libusb/src/libusb/libusb.h"

namespace device {

namespace {

class UsbContextTest : public testing::Test {
 protected:
  class UsbContextForTest : public UsbContext {
   public:
    explicit UsbContextForTest(PlatformUsbContext context)
        : UsbContext(context) {}

    UsbContextForTest(const UsbContextForTest&) = delete;
    UsbContextForTest& operator=(const UsbContextForTest&) = delete;

   private:
    ~UsbContextForTest() override {}
  };
};

}  // namespace

TEST_F(UsbContextTest, GracefulShutdown) {
  base::TimeTicks start = base::TimeTicks::Now();
  {
    PlatformUsbContext platform_context;
    ASSERT_EQ(LIBUSB_SUCCESS, libusb_init(&platform_context));
    scoped_refptr<UsbContextForTest> context(
        new UsbContextForTest(platform_context));
  }
  base::TimeDelta elapse = base::TimeTicks::Now() - start;
  if (elapse > base::Seconds(2)) {
    FAIL();
  }
}

}  // namespace device
