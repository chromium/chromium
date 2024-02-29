// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend_test_constants.h"

#include <optional>

#include "printing/backend/print_backend.h"

namespace printing {

OptionalSampleCapabilities::OptionalSampleCapabilities() = default;

OptionalSampleCapabilities::OptionalSampleCapabilities(
    OptionalSampleCapabilities&& other) noexcept = default;

OptionalSampleCapabilities& OptionalSampleCapabilities::operator=(
    OptionalSampleCapabilities&& other) noexcept = default;

OptionalSampleCapabilities::~OptionalSampleCapabilities() = default;

#if BUILDFLAG(IS_CHROMEOS)
OptionalSampleCapabilities SampleWithPinAndAdvancedCapabilities() {
  OptionalSampleCapabilities caps;
  caps.pin_supported = kPinSupported;
  caps.advanced_capabilities = kAdvancedCapabilities;
  return caps;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
OptionalSampleCapabilities SampleWithPageOutputQuality() {
  OptionalSampleCapabilities caps;
  caps.page_output_quality = kPageOutputQuality;
  return caps;
}
#endif  // BUILDFLAG(IS_WIN)

PrinterSemanticCapsAndDefaults GenerateSamplePrinterSemanticCapsAndDefaults(
    OptionalSampleCapabilities sample_capabilities) {
  PrinterSemanticCapsAndDefaults caps;

  caps.collate_capable = kCollateCapable;
  caps.collate_default = kCollateDefault;
  caps.copies_max = kCopiesMax;
  caps.duplex_modes = kDuplexModes;
  caps.duplex_default = kDuplexDefault;
  caps.color_changeable = kColorChangeable;
  caps.color_default = kColorDefault;
  caps.color_model = kColorModel;
  caps.bw_model = kBwModel;
  caps.papers = kPapers;
  caps.user_defined_papers = kUserDefinedPapers;
  caps.default_paper = kPaperLetter;
  caps.dpis = kDpis;
  caps.default_dpi = kDefaultDpi;
  caps.media_types = kMediaTypes;
  caps.default_media_type = kDefaultMediaType;
#if BUILDFLAG(IS_CHROMEOS)
  caps.pin_supported = sample_capabilities.pin_supported;
  caps.advanced_capabilities = sample_capabilities.advanced_capabilities;
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_WIN)
  caps.page_output_quality = sample_capabilities.page_output_quality;
#endif  // BUILDFLAG(IS_WIN)

  return caps;
}

}  // namespace printing
