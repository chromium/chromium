// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_features.h"

namespace printing {
namespace features {

#if defined(OS_CHROMEOS)
// Enables Advanced PPD Attributes.
const base::Feature kAdvancedPpdAttributes{"AdvancedPpdAttributes",
                                           base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // defined(OS_CHROMEOS)

#if defined(OS_MAC)
// Use the CUPS IPP printing backend instead of the original CUPS backend that
// calls the deprecated PPD API.
const base::Feature kCupsIppPrintingBackend{"CupsIppPrintingBackend",
                                            base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_MAC)

#if defined(OS_WIN)
// When using GDI printing, avoid rasterization if possible.
const base::Feature kPrintWithReducedRasterization{
    "PrintWithReducedRasterization", base::FEATURE_DISABLED_BY_DEFAULT};

// Use XPS for printing instead of GDI.
const base::Feature kUseXpsForPrinting{"UseXpsForPrinting",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Use XPS for printing instead of GDI for printing PDF documents. This is
// independent of |kUseXpsForPrinting|; can use XPS for PDFs even if still using
// GDI for modifiable content.
const base::Feature kUseXpsForPrintingFromPdf{
    "UseXpsForPrintingFromPdf", base::FEATURE_DISABLED_BY_DEFAULT};

bool IsXpsPrintCapabilityRequired() {
  return base::FeatureList::IsEnabled(features::kUseXpsForPrinting) ||
         base::FeatureList::IsEnabled(features::kUseXpsForPrintingFromPdf);
}

bool ShouldPrintUsingXps(bool source_is_pdf) {
  return base::FeatureList::IsEnabled(source_is_pdf
                                          ? features::kUseXpsForPrintingFromPdf
                                          : features::kUseXpsForPrinting);
}
#endif  // defined(OS_WIN)

#if defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
// Enables printing interactions with the operating system to be performed
// out-of-process.
const base::Feature kEnableOopPrintDrivers{"EnableOopPrintDrivers",
                                           base::FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(OS_WIN) || defined(OS_MAC) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

}  // namespace features
}  // namespace printing
