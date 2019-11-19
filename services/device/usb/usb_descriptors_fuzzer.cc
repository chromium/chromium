// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "services/device/usb/usb_descriptors.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  device::UsbDeviceDescriptor desc;
  desc.Parse(std::vector<uint8_t>(data, data + size));
  return 0;
}
