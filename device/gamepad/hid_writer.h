// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_HID_WRITER_H_
#define DEVICE_GAMEPAD_HID_WRITER_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"

namespace device {

// HidWriter defines an interface for writing output reports to a single HID
// device.
class HidWriter {
 public:
  HidWriter() = default;
  virtual ~HidWriter() = default;

  // Platform implementation for writing an output report. |report| contains
  // the data to be written, with the report ID (if present) as the first byte.
  // Returns the number of bytes written, or zero on failure.
  virtual size_t WriteOutputReport(base::span<const uint8_t> report) = 0;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_HID_WRITER_H_
