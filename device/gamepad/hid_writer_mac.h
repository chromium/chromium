// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_HID_WRITER_MAC_H_
#define DEVICE_GAMEPAD_HID_WRITER_MAC_H_

#include <stddef.h>
#include <stdint.h>

#include <IOKit/hid/IOHIDManager.h>

#include "base/containers/span.h"
#include "device/gamepad/hid_writer.h"

namespace device {

class HidWriterMac final : public HidWriter {
 public:
  explicit HidWriterMac(IOHIDDeviceRef device_ref);
  ~HidWriterMac() override;

  // HidWriter implementation.
  size_t WriteOutputReport(base::span<const uint8_t> report) override;

 private:
  IOHIDDeviceRef device_ref_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_HID_WRITER_MAC_H_
