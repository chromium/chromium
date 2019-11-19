// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Methods for parsing IPP Printer attributes.

#ifndef PRINTING_BACKEND_CUPS_IPP_UTIL_H_
#define PRINTING_BACKEND_CUPS_IPP_UTIL_H_

#include <memory>

#include "printing/backend/cups_printer.h"
#include "printing/backend/print_backend.h"
#include "printing/printing_export.h"

namespace printing {

// Smart ptr wrapper for CUPS ipp_t
using ScopedIppPtr = std::unique_ptr<ipp_t, void (*)(ipp_t*)>;

// Returns the default paper setting for |printer|.
PrinterSemanticCapsAndDefaults::Paper DefaultPaper(
    const CupsOptionProvider& printer);

// Populates the |printer_info| object with attributes retrived using IPP from
// |printer|.
PRINTING_EXPORT void CapsAndDefaultsFromPrinter(
    const CupsOptionProvider& printer,
    PrinterSemanticCapsAndDefaults* printer_info);

// Wraps |ipp| in unique_ptr with appropriate deleter
PRINTING_EXPORT ScopedIppPtr WrapIpp(ipp_t* ipp);

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_IPP_UTIL_H_
