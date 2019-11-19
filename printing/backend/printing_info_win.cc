// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/printing_info_win.h"

#include <stdint.h>

#include "base/logging.h"

namespace printing {

namespace internal {

uint8_t* GetDriverInfo(HANDLE printer, int level) {
  DWORD size = 0;
  ::GetPrinterDriver(printer, NULL, level, NULL, 0, &size);
  if (size == 0) {
    return NULL;
  }
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[size]);
  memset(buffer.get(), 0, size);
  if (!::GetPrinterDriver(printer, NULL, level, buffer.get(), size, &size)) {
    return NULL;
  }
  return buffer.release();
}

uint8_t* GetPrinterInfo(HANDLE printer, int level) {
  DWORD size = 0;
  ::GetPrinter(printer, level, NULL, 0, &size);
  if (size == 0) {
    LOG(WARNING) << "Failed to get size of PRINTER_INFO_" << level
                 << ", error = " << GetLastError();
    return NULL;
  }
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[size]);
  memset(buffer.get(), 0, size);
  if (!::GetPrinter(printer, level, buffer.get(), size, &size)) {
    LOG(WARNING) << "Failed to get PRINTER_INFO_" << level
                 << ", error = " << GetLastError();
    return NULL;
  }
  return buffer.release();
}

}  // namespace internal

}  // namespace printing
