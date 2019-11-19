// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_HID_WRITER_WIN_H_
#define DEVICE_GAMEPAD_HID_WRITER_WIN_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_types.h"
#include "device/gamepad/hid_writer.h"

namespace device {

class HidWriterWin final : public HidWriter {
 public:
  explicit HidWriterWin(HANDLE device);
  ~HidWriterWin() override;

  // HidWriter implementation.
  size_t WriteOutputReport(base::span<const uint8_t> report) override;

 private:
  base::win::ScopedHandle hid_handle_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_HID_WRITER_WIN_H_
