// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_CUPS_IPP_ADVANCED_CAPS_H_
#define PRINTING_BACKEND_CUPS_IPP_ADVANCED_CAPS_H_

namespace printing {

class CupsOptionProvider;
struct PrinterSemanticCapsAndDefaults;

void ExtractAdvancedCapabilities(const CupsOptionProvider& printer,
                                 PrinterSemanticCapsAndDefaults* printer_info);

}  // namespace printing

#endif  // PRINTING_BACKEND_CUPS_IPP_ADVANCED_CAPS_H_
