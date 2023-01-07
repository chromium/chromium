// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_XPS_UTILS_WIN_H_
#define PRINTING_BACKEND_XPS_UTILS_WIN_H_

#include "base/component_export.h"
#include "printing/mojom/print.mojom-forward.h"

namespace base {
class Value;
}  // namespace base

namespace printing {

struct PrinterSemanticCapsAndDefaults;

// Since parsing XML data to `PrinterSemanticCapsAndDefaults` can not be done
// in the print_backend level, parse base::Value into
// `PrinterSemanticCapsAndDefaults` data structure instead. Parsing XML data
// to base::Value will be processed by data_decoder service.
COMPONENT_EXPORT(PRINT_BACKEND)
mojom::ResultCode ParseValueForXpsPrinterCapabilities(
    const base::Value& capabilities,
    PrinterSemanticCapsAndDefaults* printer_info);

}  // namespace printing

#endif  // PRINTING_BACKEND_XPS_UTILS_WIN_H_
