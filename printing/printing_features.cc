// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printing_features.h"

#include "build/build_config.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_OOP_PRINTING)
#include "base/metrics/field_trial_params.h"
#endif

namespace printing {
namespace features {

#if BUILDFLAG(IS_MAC)
// Use the CUPS IPP printing backend instead of the original CUPS backend that
// calls the deprecated PPD API.
BASE_FEATURE(kCupsIppPrintingBackend,
             "CupsIppPrintingBackend",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
// When using PostScript level 3 printing, render text with Type 42 fonts if
// possible.
BASE_FEATURE(kPrintWithPostScriptType42Fonts,
             "PrintWithPostScriptType42Fonts",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When using GDI printing, avoid rasterization if possible.
BASE_FEATURE(kPrintWithReducedRasterization,
             "PrintWithReducedRasterization",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Read printer capabilities with XPS when use XPS for printing.
BASE_FEATURE(kReadPrinterCapabilitiesWithXps,
             "ReadPrinterCapabilitiesWithXps",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use XPS for printing instead of GDI.
BASE_FEATURE(kUseXpsForPrinting,
             "UseXpsForPrinting",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Use XPS for printing instead of GDI for printing PDF documents. This is
// independent of `kUseXpsForPrinting`; can use XPS for PDFs even if still using
// GDI for modifiable content.
BASE_FEATURE(kUseXpsForPrintingFromPdf,
             "UseXpsForPrintingFromPdf",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsXpsPrintCapabilityRequired() {
  // Require XPS printing to be used out-of-process.
#if BUILDFLAG(ENABLE_OOP_PRINTING)
  return features::kEnableOopPrintDriversJobPrint.Get() &&
         (base::FeatureList::IsEnabled(features::kUseXpsForPrinting) ||
          base::FeatureList::IsEnabled(features::kUseXpsForPrintingFromPdf));
#else
  return false;
#endif
}

bool ShouldPrintUsingXps(bool source_is_pdf) {
  // Require XPS to be used out-of-process.
  return features::kEnableOopPrintDriversJobPrint.Get() &&
         base::FeatureList::IsEnabled(source_is_pdf
                                          ? features::kUseXpsForPrintingFromPdf
                                          : features::kUseXpsForPrinting);
}
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_OOP_PRINTING)
// Enables printing interactions with the operating system to be performed
// out-of-process.
BASE_FEATURE(kEnableOopPrintDrivers,
             "EnableOopPrintDrivers",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kEnableOopPrintDriversJobPrint{
    &kEnableOopPrintDrivers, "JobPrint", false};

const base::FeatureParam<bool> kEnableOopPrintDriversSandbox{
    &kEnableOopPrintDrivers, "Sandbox", false};
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
// Enables scanning of to-be-printed pages and documents for sensitive data if
// the OnPrintEnterpriseConnector policy is enabled.
BASE_FEATURE(kEnablePrintContentAnalysis,
             "EnablePrintContentAnalysis",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)

}  // namespace features
}  // namespace printing
