// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_features.h"

#include "build/build_config.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
#include "base/metrics/field_trial_params.h"
#endif

namespace printing {
namespace features {

#if BUILDFLAG(IS_MAC)
// Use the CUPS IPP printing backend instead of the original CUPS backend that
// calls the deprecated PPD API.
const base::Feature kCupsIppPrintingBackend{"CupsIppPrintingBackend",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
// When using PostScript level 3 printing, render text with Type 42 fonts if
// possible.
const base::Feature kPrintWithPostScriptType42Fonts{
    "PrintWithPostScriptType42Fonts", base::FEATURE_DISABLED_BY_DEFAULT};

// When using GDI printing, avoid rasterization if possible.
const base::Feature kPrintWithReducedRasterization{
    "PrintWithReducedRasterization", base::FEATURE_DISABLED_BY_DEFAULT};

// Read printer capabilities with XPS when use XPS for printing.
const base::Feature kReadPrinterCapabilitiesWithXps{
    "ReadPrinterCapabilitiesWithXps", base::FEATURE_DISABLED_BY_DEFAULT};

// Use XPS for printing instead of GDI.
const base::Feature kUseXpsForPrinting{"UseXpsForPrinting",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

// Use XPS for printing instead of GDI for printing PDF documents. This is
// independent of `kUseXpsForPrinting`; can use XPS for PDFs even if still using
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
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_OOP_PRINTING)
// Enables printing interactions with the operating system to be performed
// out-of-process.
const base::Feature kEnableOopPrintDrivers{"EnableOopPrintDrivers",
                                           base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<bool> kEnableOopPrintDriversJobPrint{
    &kEnableOopPrintDrivers, "JobPrint", false};
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

}  // namespace features
}  // namespace printing
