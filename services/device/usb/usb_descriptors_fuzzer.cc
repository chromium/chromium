// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_descriptors.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/compiler_specific.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  device::UsbDeviceDescriptor desc;
  desc.Parse(std::vector<uint8_t>(data, UNSAFE_TODO(data + size)));
  return 0;
}
