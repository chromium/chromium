// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PRINTING_BACKEND_PRINT_BACKEND_TEST_CONSTANTS_H_
#define PRINTING_BACKEND_PRINT_BACKEND_TEST_CONSTANTS_H_

#include <stdint.h>

#include <vector>

#include "build/build_config.h"
#include "printing/backend/mojom/print_backend.mojom-forward.h"
#include "printing/backend/print_backend.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

// Sample capabilities used to set optional fields in
// GenerateSamplePrinterSemanticCapsAndDefaults().
struct OptionalSampleCapabilities {
  OptionalSampleCapabilities();
  OptionalSampleCapabilities(const OptionalSampleCapabilities&) = delete;
  OptionalSampleCapabilities& operator=(const OptionalSampleCapabilities&) =
      delete;
  OptionalSampleCapabilities(OptionalSampleCapabilities&& other) noexcept;
  OptionalSampleCapabilities& operator=(
      OptionalSampleCapabilities&& other) noexcept;
  ~OptionalSampleCapabilities();

#if BUILDFLAG(IS_CHROMEOS)
  bool pin_supported = false;
  AdvancedCapabilities advanced_capabilities;
#endif  // BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_WIN)
  absl::optional<PageOutputQuality> page_output_quality;
#endif  // BUILDFLAG(IS_WIN)
};

inline const PrinterSemanticCapsAndDefaults::Paper kPaperA3{
    /*display_name=*/"A3", /*vendor_id=*/"67",
    /*size_um=*/gfx::Size(7016, 9921)};
inline const PrinterSemanticCapsAndDefaults::Paper kPaperA4{
    /*display_name=*/"A4", /*vendor_id=*/"12",
    /*size_um=*/gfx::Size(4961, 7016)};
inline const PrinterSemanticCapsAndDefaults::Paper kPaperLetter{
    /*display_name=*/"Letter", /*vendor_id=*/"45",
    /*size_um=*/gfx::Size(5100, 6600)};
inline const PrinterSemanticCapsAndDefaults::Paper kPaperLedger{
    /*display_name=*/"Ledger", /*vendor_id=*/"89",
    /*size_um=*/gfx::Size(6600, 10200)};

#if BUILDFLAG(IS_CHROMEOS)
inline const AdvancedCapability kAdvancedCapability1(
    /*name=*/"advanced_cap_bool",
    /*display_name=*/"Advanced Capability #1 (bool)",
    /*type=*/AdvancedCapability::Type::kBoolean,
    /*default_value=*/"true",
    /*values=*/{});
inline const AdvancedCapability kAdvancedCapability2(
    /*name=*/"advanced_cap_double",
    /*display_name=*/"Advanced Capability #2 (double)",
    /*type=*/AdvancedCapability::Type::kFloat,
    /*default_value=*/"3.14159",
    /*values=*/
    {
        AdvancedCapabilityValue(
            /*name=*/"adv_cap_val_1",
            /*display_name=*/"Advanced Capability #1"),
        AdvancedCapabilityValue(
            /*name=*/"adv_cap_val_2",
            /*display_name=*/"Advanced Capability #2"),
        AdvancedCapabilityValue(
            /*name=*/"adv_cap_val_3",
            /*display_name=*/"Advanced Capability #3"),
    });
inline const AdvancedCapabilities kAdvancedCapabilities{kAdvancedCapability1,
                                                        kAdvancedCapability2};
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
inline const PageOutputQualityAttribute kPageOutputQualityAttribute1(
    /*display_name=*/"Normal",
    /*name=*/"ns000:Normal");
inline const PageOutputQualityAttribute kPageOutputQualityAttribute2(
    /*display_name=*/"Draft",
    /*name=*/"ns000:Draft");
inline const PageOutputQualityAttribute kPageOutputQualityAttribute3(
    /*display_name=*/"Advance",
    /*name=*/"ns000:Advance");
inline const PageOutputQualityAttributes kPageOutputQualityAttributes{
    kPageOutputQualityAttribute1, kPageOutputQualityAttribute2,
    kPageOutputQualityAttribute3};
inline const PageOutputQuality kPageOutputQuality(
    kPageOutputQualityAttributes,
    /*default_quality=*/absl::nullopt);
inline constexpr char kDefaultQuality[] = "ns000:Draft";
#endif  // BUILDFLAG(IS_WIN)

inline constexpr bool kCollateCapable = true;
inline constexpr bool kCollateDefault = true;
inline constexpr int32_t kCopiesMax = 123;
inline const std::vector<mojom::DuplexMode> kDuplexModes{
    mojom::DuplexMode::kSimplex, mojom::DuplexMode::kLongEdge,
    mojom::DuplexMode::kShortEdge};
inline constexpr mojom::DuplexMode kDuplexDefault = mojom::DuplexMode::kSimplex;
inline constexpr bool kColorChangeable = true;
inline constexpr bool kColorDefault = true;
inline constexpr mojom::ColorModel kColorModel = mojom::ColorModel::kRGB;
inline constexpr mojom::ColorModel kBwModel = mojom::ColorModel::kGrayscale;
inline const PrinterSemanticCapsAndDefaults::Papers kPapers{kPaperA4,
                                                            kPaperLetter};
inline const PrinterSemanticCapsAndDefaults::Papers kUserDefinedPapers{
    kPaperA3, kPaperLedger};
inline const PrinterSemanticCapsAndDefaults::Paper kDefaultPaper = kPaperLetter;
inline constexpr gfx::Size kDpi600(600, 600);
inline constexpr gfx::Size kDpi1200(1200, 1200);
inline constexpr gfx::Size kDpi1200x600(1200, 600);
inline const std::vector<gfx::Size> kDpis{kDpi600, kDpi1200, kDpi1200x600};
inline constexpr gfx::Size kDefaultDpi = kDpi600;
#if BUILDFLAG(IS_CHROMEOS)
inline constexpr bool kPinSupported = true;
#endif

#if BUILDFLAG(IS_CHROMEOS)
OptionalSampleCapabilities SampleWithPinAndAdvancedCapabilities();
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN)
OptionalSampleCapabilities SampleWithPageOutputQuality();
#endif  // BUILDFLAG(IS_WIN)

PrinterSemanticCapsAndDefaults GenerateSamplePrinterSemanticCapsAndDefaults(
    OptionalSampleCapabilities sample_capabilities);

}  // namespace printing

#endif  // PRINTING_BACKEND_PRINT_BACKEND_TEST_CONSTANTS_H_
