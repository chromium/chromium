// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_PRINT_JOB_CONSTANTS_CUPS_H_
#define PRINTING_PRINT_JOB_CONSTANTS_CUPS_H_

#include <string_view>

#include "base/component_export.h"
#include "build/build_config.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(IS_MAC)
#include "base/containers/span.h"

#endif

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
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSCanonCNColorMode[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSCanonCNIJGrayScale[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSEpsonInk[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSHpColorMode[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSHpPjlColorAsGray[];
COMPONENT_EXPORT(PRINTING_BASE)
extern const char kCUPSKonicaMinoltaSelectColor[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSLexmarkBLW[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSOkiControl[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSSharpARCMode[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSXeroxXROutputColor[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kCUPSXeroxXRXColor[];

// Variations of identifier used for specifying printer color model choice.
COMPONENT_EXPORT(PRINTING_BASE) extern const char kAuto[];
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
COMPONENT_EXPORT(PRINTING_BASE) extern const char kHpPjlColorAsGrayNo[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kHpPjlColorAsGrayYes[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kKonicaMinoltaColor[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kKonicaMinoltaGrayscale[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kLexmarkBLWFalse[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kLexmarkBLWTrue[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kMono[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kMonochrome[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kNormal[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kNormalGray[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kOne[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kPrintAsColor[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kPrintAsGrayscale[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kRGB[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kRGBA[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kRGB16[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kSamsungColorFalse[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kSamsungColorTrue[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kSharpCMColor[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kSharpCMBW[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kXeroxAutomatic[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kXeroxBW[];
COMPONENT_EXPORT(PRINTING_BASE) extern const char kZero[];

#if BUILDFLAG(IS_MAC)
// Represents set identifier used to specifier to select the color model for a
// particular printer manufacturer, and the corresponding names used with that
// to choose either black and white or color printing.
struct COMPONENT_EXPORT(PRINTING_BASE) PpdColorSetting {
  constexpr PpdColorSetting(std::string_view name,
                            std::string_view bw,
                            std::string_view color)
      : name(name), bw(bw), color(color) {}

  std::string_view name;
  std::string_view bw;
  std::string_view color;
};

COMPONENT_EXPORT(PRINTING_BASE)
base::span<const PpdColorSetting> GetKnownPpdColorSettings();
#endif

}  // namespace printing

#endif  // PRINTING_PRINT_JOB_CONSTANTS_CUPS_H_
