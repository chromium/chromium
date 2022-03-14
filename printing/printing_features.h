// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINTING_FEATURES_H_
#define PRINTING_PRINTING_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "printing/buildflags/buildflags.h"

namespace printing {
namespace features {

// The following features are declared alphabetically. The features should be
// documented with descriptions of their behaviors in the .cc file.

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(PRINTING_BASE)
extern const base::Feature kCupsIppPrintingBackend;
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(PRINTING_BASE)
extern const base::Feature kPrintWithPostScriptType42Fonts;
COMPONENT_EXPORT(PRINTING_BASE)
extern const base::Feature kPrintWithReducedRasterization;
COMPONENT_EXPORT(PRINTING_BASE)
extern const base::Feature kReadPrinterCapabilitiesWithXps;
COMPONENT_EXPORT(PRINTING_BASE)
extern const base::Feature kUseXpsForPrinting;
COMPONENT_EXPORT(PRINTING_BASE)
extern const base::Feature kUseXpsForPrintingFromPdf;

// Helper function to determine if there is any print path which could require
// the use of XPS print capabilities.
COMPONENT_EXPORT(PRINTING_BASE) bool IsXpsPrintCapabilityRequired();

// Helper function to determine if printing of a document from a particular
// source should be done using XPS printing API instead of with GDI.
COMPONENT_EXPORT(PRINTING_BASE)
bool ShouldPrintUsingXps(bool source_is_pdf);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_OOP_PRINTING)
COMPONENT_EXPORT(PRINTING_BASE)
extern const base::Feature kEnableOopPrintDrivers;
COMPONENT_EXPORT(PRINTING_BASE)
extern const base::FeatureParam<bool> kEnableOopPrintDriversJobPrint;
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

}  // namespace features
}  // namespace printing

#endif  // PRINTING_PRINTING_FEATURES_H_
