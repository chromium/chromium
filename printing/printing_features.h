// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_FEATURES_H_
#define PRINTING_PRINTING_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "printing/buildflags/buildflags.h"

namespace printing::features {

// The following features are declared alphabetically. The features should be
// documented with descriptions of their behaviors in the .cc file.

#if BUILDFLAG(IS_CHROMEOS)
COMPONENT_EXPORT(PRINTING_BASE)
BASE_DECLARE_FEATURE(kAddPrinterViaPrintscanmgr);

COMPONENT_EXPORT(PRINTING_BASE)
BASE_DECLARE_FEATURE(kApiPrintingMarginsAndScale);
#endif  // BUILDFLAG(IS_CHROMEOS)

COMPONENT_EXPORT(PRINTING_BASE)
BASE_DECLARE_FEATURE(kAlignPdfDefaultPrintSettingsWithHTML);

#if BUILDFLAG(IS_LINUX)
COMPONENT_EXPORT(PRINTING_BASE) BASE_DECLARE_FEATURE(kCupsIppPrintingBackend);
#endif  // BUILDFLAG(IS_LINUX)

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(PRINTING_BASE)
BASE_DECLARE_FEATURE(kPrintWithPostScriptType42Fonts);
COMPONENT_EXPORT(PRINTING_BASE)
BASE_DECLARE_FEATURE(kPrintWithReducedRasterization);
COMPONENT_EXPORT(PRINTING_BASE)
BASE_DECLARE_FEATURE(kReadPrinterCapabilitiesWithXps);
COMPONENT_EXPORT(PRINTING_BASE) BASE_DECLARE_FEATURE(kUseXpsForPrinting);
COMPONENT_EXPORT(PRINTING_BASE) BASE_DECLARE_FEATURE(kUseXpsForPrintingFromPdf);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_OOP_PRINTING)
COMPONENT_EXPORT(PRINTING_BASE) BASE_DECLARE_FEATURE(kEnableOopPrintDrivers);
COMPONENT_EXPORT(PRINTING_BASE)
extern const base::FeatureParam<bool> kEnableOopPrintDriversEarlyStart;
COMPONENT_EXPORT(PRINTING_BASE)
extern const base::FeatureParam<bool> kEnableOopPrintDriversJobPrint;
COMPONENT_EXPORT(PRINTING_BASE)
extern const base::FeatureParam<bool> kEnableOopPrintDriversSandbox;
#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(PRINTING_BASE)
extern const base::FeatureParam<bool> kEnableOopPrintDriversSingleProcess;
#endif
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

}  // namespace printing::features

#endif  // PRINTING_PRINTING_FEATURES_H_
