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

#if BUILDFLAG(IS_CHROMEOS)
// Add printers via printscanmgr instead of debugd.
BASE_FEATURE(kAddPrinterViaPrintscanmgr,
             "AddPrinterViaPrintscanmgr",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)
// Use the CUPS IPP printing backend instead of the original CUPS backend that
// calls the deprecated PPD API.
BASE_FEATURE(kCupsIppPrintingBackend,
             "CupsIppPrintingBackend",
#if BUILDFLAG(IS_LINUX)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC)

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
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_OOP_PRINTING)
// Enables printing interactions with the operating system to be performed
// out-of-process.
BASE_FEATURE(kEnableOopPrintDrivers,
             "EnableOopPrintDrivers",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<bool> kEnableOopPrintDriversEarlyStart{
    &kEnableOopPrintDrivers, "EarlyStart", false};

const base::FeatureParam<bool> kEnableOopPrintDriversJobPrint{
    &kEnableOopPrintDrivers, "JobPrint", true};

const base::FeatureParam<bool> kEnableOopPrintDriversSandbox{
    &kEnableOopPrintDrivers, "Sandbox", false};

#if BUILDFLAG(IS_WIN)
const base::FeatureParam<bool> kEnableOopPrintDriversSingleProcess{
    &kEnableOopPrintDrivers, "SingleProcess", true};
#endif
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

}  // namespace features
}  // namespace printing
