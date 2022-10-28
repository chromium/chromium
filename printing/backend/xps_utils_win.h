// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_XPS_UTILS_WIN_H_
#define PRINTING_BACKEND_XPS_UTILS_WIN_H_

#include "base/component_export.h"
#include "base/types/expected.h"
#include "printing/mojom/print.mojom-forward.h"

namespace base {
class Value;
}  // namespace base

namespace printing {

struct PrinterSemanticCapsAndDefaults;
struct XpsCapabilities;

// Since parsing XML data to XpsCapabilities can not be done in the
// //printing/backend level, parse base::Value into XpsCapabilities data
// structure instead. Returns an XPS capabilities object on success with valid
// relevant features as fields, ignoring invalid relevant features, or
// mojom::ResultCode on failure.
COMPONENT_EXPORT(PRINT_BACKEND)
base::expected<XpsCapabilities, mojom::ResultCode>
ParseValueForXpsPrinterCapabilities(const base::Value& capabilities);

// Moves all data from `xps_capabilities` into `printer_capabilities`.
COMPONENT_EXPORT(PRINT_BACKEND)
void MergeXpsCapabilities(XpsCapabilities xps_capabilities,
                          PrinterSemanticCapsAndDefaults& printer_capabilities);

}  // namespace printing

#endif  // PRINTING_BACKEND_XPS_UTILS_WIN_H_
