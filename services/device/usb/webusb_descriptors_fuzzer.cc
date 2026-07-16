// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/webusb_descriptors.h"

#include <stddef.h>
#include <stdint.h>

#include "base/at_exit.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/i18n/icu_util.h"
#include "base/no_destructor.h"
#include "services/device/usb/fake_usb_device_handle.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

struct TestCase {
  TestCase() { CHECK(base::i18n::InitializeICU()); }

  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

namespace device {

void Done(const GURL& landing_page) {}

}  // namespace device

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
  static const base::NoDestructor<TestCase> test_case;
  scoped_refptr<device::UsbDeviceHandle> device_handle =
      new device::FakeUsbDeviceHandle(data);
  device::ReadWebUsbDescriptors(device_handle, base::BindOnce(&device::Done));
  return 0;
}
