// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/printing_info_win.h"

#include <stdint.h>

#include "base/logging.h"

namespace printing {

namespace internal {

std::unique_ptr<uint8_t[]> GetDriverInfo(HANDLE printer, int level) {
  DWORD size = 0;
  // ::GetPrinterDriver() will always fail on this check for the required size
  // because the provided buffer is intentionally insufficient.
  ::GetPrinterDriver(printer, nullptr, level, nullptr, 0, &size);
  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || size == 0)
    return nullptr;

  auto buffer = std::make_unique<uint8_t[]>(size);
  if (!::GetPrinterDriver(printer, nullptr, level, buffer.get(), size, &size))
    return nullptr;

  return buffer;
}

std::unique_ptr<uint8_t[]> GetPrinterInfo(HANDLE printer, int level) {
  DWORD size = 0;
  // ::GetPrinter() will always fail on this check for the required size
  // because the provided buffer is intentionally insufficient.
  ::GetPrinter(printer, level, nullptr, 0, &size);
  DWORD last_err = GetLastError();
  if (last_err != ERROR_INSUFFICIENT_BUFFER || size == 0) {
    LOG(WARNING) << "Failed to get size of PRINTER_INFO_" << level
                 << ", error = " << last_err;
    return nullptr;
  }

  auto buffer = std::make_unique<uint8_t[]>(size);
  if (!::GetPrinter(printer, level, buffer.get(), size, &size)) {
    LOG(WARNING) << "Failed to get PRINTER_INFO_" << level
                 << ", error = " << GetLastError();
    return nullptr;
  }
  return buffer;
}

}  // namespace internal

}  // namespace printing
