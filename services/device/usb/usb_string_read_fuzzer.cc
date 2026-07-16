// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "services/device/usb/fake_usb_device_handle.h"
#include "services/device/usb/usb_descriptors.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"

namespace device {

void Done(std::unique_ptr<std::map<uint8_t, std::u16string>> index_map) {}

}  // namespace device

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
  // Uses the first few bytes of the input to decide which strings to request.
  // Stops once it encounters 0 which is not a valid string index.
  std::unique_ptr<std::map<uint8_t, std::u16string>> index_map(
      new std::map<uint8_t, std::u16string>());
  for (size_t i = 0; i < data.size() && data[i] != 0; i++) {
    (*index_map)[data[i]] = std::u16string();
  }
  size_t used = index_map->size() + 1;
  if (used > data.size()) {
    return 0;
  }

  scoped_refptr<device::UsbDeviceHandle> device_handle =
      new device::FakeUsbDeviceHandle(data.subspan(used));
  device::ReadUsbStringDescriptors(device_handle, std::move(index_map),
                                   base::BindOnce(&device::Done));
  return 0;
}
