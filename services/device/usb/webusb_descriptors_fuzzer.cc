// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/at_exit.h"
#include "base/functional/bind.h"
#include "base/i18n/icu_util.h"
#include "base/no_destructor.h"
#include "services/device/usb/fake_usb_device_handle.h"
#include "services/device/usb/webusb_descriptors.h"

struct TestCase {
  TestCase() { CHECK(base::i18n::InitializeICU()); }

  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

namespace device {

void Done(const GURL& landing_page) {}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static const base::NoDestructor<TestCase> test_case;
  scoped_refptr<UsbDeviceHandle> device_handle =
      new FakeUsbDeviceHandle(data, size);
  ReadWebUsbDescriptors(device_handle, base::BindOnce(&Done));
  return 0;
}

}  // namespace device
