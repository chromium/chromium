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

namespace printing {
namespace features {

// The following features are declared alphabetically. The features should be
// documented with descriptions of their behaviors in the .cc file.

#if BUILDFLAG(IS_MAC)
COMPONENT_EXPORT(PRINTING_BASE) BASE_DECLARE_FEATURE(kCupsIppPrintingBackend);
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)
COMPONENT_EXPORT(PRINTING_BASE)
BASE_DECLARE_FEATURE(kPrintWithPostScriptType42Fonts);
COMPONENT_EXPORT(PRINTING_BASE)
BASE_DECLARE_FEATURE(kPrintWithReducedRasterization);
COMPONENT_EXPORT(PRINTING_BASE)
BASE_DECLARE_FEATURE(kReadPrinterCapabilitiesWithXps);
COMPONENT_EXPORT(PRINTING_BASE) BASE_DECLARE_FEATURE(kUseXpsForPrinting);
COMPONENT_EXPORT(PRINTING_BASE) BASE_DECLARE_FEATURE(kUseXpsForPrintingFromPdf);

// Helper function to determine if there is any print path which could require
// the use of XPS print capabilities.
COMPONENT_EXPORT(PRINTING_BASE) bool IsXpsPrintCapabilityRequired();

// Helper function to determine if printing of a document from a particular
// source should be done using XPS printing API instead of with GDI.
COMPONENT_EXPORT(PRINTING_BASE)
bool ShouldPrintUsingXps(bool source_is_pdf);
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_OOP_PRINTING)
COMPONENT_EXPORT(PRINTING_BASE) BASE_DECLARE_FEATURE(kEnableOopPrintDrivers);
COMPONENT_EXPORT(PRINTING_BASE)
extern const base::FeatureParam<bool> kEnableOopPrintDriversJobPrint;
COMPONENT_EXPORT(PRINTING_BASE)
extern const base::FeatureParam<bool> kEnableOopPrintDriversSandbox;
#endif  // BUILDFLAG(ENABLE_OOP_PRINTING)

#if BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)
// Enterprise code gated by the following two features is handled almost
// identically through the print stack, but the underlying enterprise behavior
// changes significantly depending on which service provider is set by the
// OnPrintEnterpriseConnector policy. As such, using two features allows both
// workflows to be tested and ramped up/down independently.
//
// Since the policy can only ever be enabled with a cloud provider or a local
// provider, both flags being enabled at once shouldn't impact either workflows
// in an unexpected way since administrators will select a policy value that
// interacts with at most one of these two features.

// Allows the scanning to happen post-print-preview when
// OnPrintEnterpriseConnector has the "google" service_provider instead of doing
// a pre-print-preview snapshot and sending it to the cloud for analysis.
COMPONENT_EXPORT(PRINTING_BASE)
BASE_DECLARE_FEATURE(kEnableCloudScanAfterPreview);

// Allows the scanning to happen post-print-preview when
// OnPrintEnterpriseConnector has a local agent service_provider instead of
// doing a pre-print-preview snapshot and sending it to a local agent for
// analysis. This applies to the following service_provider values:
//  - local_user_agent
//  - local_system_agent
//  - brcm_chrm_cas
//
// TODO(b/216105729): Remove once the local content scanning post-preview UX is
// officially supported.
COMPONENT_EXPORT(PRINTING_BASE)
BASE_DECLARE_FEATURE(kEnableLocalScanAfterPreview);
#endif  // BUILDFLAG(ENABLE_PRINT_CONTENT_ANALYSIS)

}  // namespace features
}  // namespace printing

#endif  // PRINTING_PRINTING_FEATURES_H_
