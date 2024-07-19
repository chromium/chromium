// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "services/device/usb/fake_usb_device_handle.h"
#include "services/device/usb/usb_descriptors.h"

namespace device {

void Done(std::unique_ptr<std::map<uint8_t, std::u16string>> index_map) {}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Uses the first few bytes of the input to decide which strings to request.
  // Stops once it encounters 0 which is not a valid string index.
  std::unique_ptr<std::map<uint8_t, std::u16string>> index_map(
      new std::map<uint8_t, std::u16string>());
  for (size_t i = 0; i < size && data[i] != 0; i++)
    (*index_map)[data[i]] = std::u16string();
  size_t used = index_map->size() + 1;
  if (used > size)
    return 0;

  scoped_refptr<UsbDeviceHandle> device_handle =
      new FakeUsbDeviceHandle(data + used, size - used);
  ReadUsbStringDescriptors(device_handle, std::move(index_map),
                           base::BindOnce(&Done));
  return 0;
}

}  // namespace device
