// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_JOB_CONSTANTS_CUPS_H_
#define PRINTING_PRINT_JOB_CONSTANTS_CUPS_H_

#include "base/component_export.h"
#include "build/build_config.h"
#include "printing/buildflags/buildflags.h"

#if !BUILDFLAG(USE_CUPS)
#error "CUPS must be enabled."
#endif

namespace printing {

// Variations of identifier used for specifying printer color model.
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSColorMode[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSColorModel[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSPrintoutMode[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSProcessColorModel[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSBrotherMonoColor[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSBrotherPrintQuality[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSEpsonInk[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSHpColorMode[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSSharpARCMode[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSXeroxXRXColor[];

// Variations of identifier used for specifying printer color model choice.
COMPONENT_EXPORT(PRINTING_BASE) extern const char kBlack[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCMYK[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kKCMY[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCMY_K[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCMY[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kColor[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kDraftGray[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kEpsonColor[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kEpsonMono[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kFullColor[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kGray[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kGrayscale[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kGreyscale[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kHighGray[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kHpColorPrint[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kHpGrayscalePrint[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kMono[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kMonochrome[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kNormal[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kNormalGray[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kRGB[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kRGBA[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kRGB16[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kSamsungColorFalse[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kSamsungColorTrue[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kSharpCMColor[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kSharpCMBW[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kXeroxAutomatic[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kXeroxBW[];

}  // namespace printing

#endif  // PRINTING_PRINT_JOB_CONSTANTS_CUPS_H_
