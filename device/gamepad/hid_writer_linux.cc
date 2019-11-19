// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/hid_writer_linux.h"

#include "base/posix/eintr_wrapper.h"

namespace device {

HidWriterLinux::HidWriterLinux(const base::ScopedFD& fd) : fd_(fd.get()) {}

HidWriterLinux::~HidWriterLinux() = default;

size_t HidWriterLinux::WriteOutputReport(base::span<const uint8_t> report) {
  ssize_t bytes_written =
      HANDLE_EINTR(write(fd_, report.data(), report.size_bytes()));
  return bytes_written < 0 ? 0 : static_cast<size_t>(bytes_written);
}

}  // namespace device
