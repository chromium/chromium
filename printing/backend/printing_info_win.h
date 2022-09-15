// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINTING_INFO_WIN_H_
#define PRINTING_BACKEND_PRINTING_INFO_WIN_H_

#include <objidl.h>
#include <stdint.h>
#include <winspool.h>

#include <memory>

#include "base/component_export.h"

namespace printing {

namespace internal {

COMPONENT_EXPORT(PRINT_BACKEND)
std::unique_ptr<uint8_t[]> GetDriverInfo(HANDLE printer, int level);
COMPONENT_EXPORT(PRINT_BACKEND)
std::unique_ptr<uint8_t[]> GetPrinterInfo(HANDLE printer, int level);

// This class is designed to work with PRINTER_INFO_X structures
// and calls GetPrinter internally with correctly allocated buffer.
template <typename PrinterInfoType, int level>
class PrinterInfo {
 public:
  bool Init(HANDLE printer) {
    buffer_ = GetPrinterInfo(printer, level);
    return buffer_ != nullptr;
  }

  const PrinterInfoType* get() const {
    return reinterpret_cast<const PrinterInfoType*>(buffer_.get());
  }

 private:
  std::unique_ptr<uint8_t[]> buffer_;
};

// This class is designed to work with DRIVER_INFO_X structures
// and calls GetDriverInfo internally with correctly allocated buffer.
template <typename DriverInfoType, int level>
class DriverInfo {
 public:
  bool Init(HANDLE printer) {
    buffer_ = GetDriverInfo(printer, level);
    return buffer_ != nullptr;
  }

  const DriverInfoType* get() const {
    return reinterpret_cast<const DriverInfoType*>(buffer_.get());
  }

 private:
  std::unique_ptr<uint8_t[]> buffer_;
};

}  // namespace internal

using PrinterInfo2 = internal::PrinterInfo<PRINTER_INFO_2, 2>;
using PrinterInfo5 = internal::PrinterInfo<PRINTER_INFO_5, 5>;

using DriverInfo6 = internal::DriverInfo<DRIVER_INFO_6, 6>;

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINTING_INFO_WIN_H_
