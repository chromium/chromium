// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Methods for parsing IPP Printer attributes.

#ifndef PRINTING_BACKEND_CUPS_IPP_HELPER_H_
#define PRINTING_BACKEND_CUPS_IPP_HELPER_H_

#include <memory>

#include "base/component_export.h"
#include "printing/backend/cups_printer.h"
#include "printing/backend/print_backend.h"

namespace printing {

// Smart ptr wrapper for CUPS ipp_t
using ScopedIppPtr = std::unique_ptr<ipp_t, void (*)(ipp_t*)>;

// Returns the default paper setting for `printer`.
COMPONENT_EXPORT(PRINT_BACKEND)
PrinterSemanticCapsAndDefaults::Paper DefaultPaper(
    const CupsOptionProvider& printer);

// Populates the `printer_info` object with attributes retrieved using IPP from
// `printer`.
COMPONENT_EXPORT(PRINT_BACKEND)
void CapsAndDefaultsFromPrinter(const CupsOptionProvider& printer,
                                PrinterSemanticCapsAndDefaults* printer_info);

// Wraps `ipp` in unique_ptr with appropriate deleter
COMPONENT_EXPORT(PRINT_BACKEND) ScopedIppPtr WrapIpp(ipp_t* ipp);

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_IPP_HELPER_H_
