// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "build/chromeos_buildflags.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "printing/backend/mojom/print_backend.mojom.h"
#include "printing/backend/print_backend.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

const printing::PrinterSemanticCapsAndDefaults::Paper kPaperA3{
    /*display_name=*/"A3", /*vendor_id=*/"67",
    /*size_um=*/gfx::Size(7016, 9921)};
const printing::PrinterSemanticCapsAndDefaults::Paper kPaperA4{
    /*display_name=*/"A4", /*vendor_id=*/"12",
    /*size_um=*/gfx::Size(4961, 7016)};
const printing::PrinterSemanticCapsAndDefaults::Paper kPaperLetter{
    /*display_name=*/"Letter", /*vendor_id=*/"45",
    /*size_um=*/gfx::Size(5100, 6600)};
const printing::PrinterSemanticCapsAndDefaults::Paper kPaperLedger{
    /*display_name=*/"Ledger", /*vendor_id=*/"89",
    /*size_um=*/gfx::Size(6600, 10200)};

#if BUILDFLAG(IS_CHROMEOS_ASH)
const printing::AdvancedCapability kAdvancedCapability1(
    /*name=*/"advanced_cap_bool",
    /*display_name=*/"Advanced Capability #1 (bool)",
    /*type=*/printing::AdvancedCapability::Type::kBoolean,
    /*default_value=*/"true",
    /*values=*/std::vector<printing::AdvancedCapabilityValue>());
const printing::AdvancedCapability kAdvancedCapability2(
    /*name=*/"advanced_cap_double",
    /*display_name=*/"Advanced Capability #2 (double)",
    /*type=*/printing::AdvancedCapability::Type::kFloat,
    /*default_value=*/"3.14159",
    /*values=*/
    std::vector<printing::AdvancedCapabilityValue>{
        printing::AdvancedCapabilityValue(
            /*name=*/"adv_cap_val_1",
            /*display_name=*/"Advanced Capability #1"),
        printing::AdvancedCapabilityValue(
            /*name=*/"adv_cap_val_2",
            /*display_name=*/"Advanced Capability #2"),
        printing::AdvancedCapabilityValue(
            /*name=*/"adv_cap_val_3",
            /*display_name=*/"Advanced Capability #3"),
    });
const printing::AdvancedCapabilities kAdvancedCapabilities{
    kAdvancedCapability1, kAdvancedCapability2};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

static constexpr bool kCollateCapable = true;
static constexpr bool kCollateDefault = true;
static constexpr int kCopiesMax = 123;
const std::vector<printing::mojom::DuplexMode> kDuplexModes{
    printing::mojom::DuplexMode::kSimplex,
    printing::mojom::DuplexMode::kLongEdge,
    printing::mojom::DuplexMode::kShortEdge};
static constexpr printing::mojom::DuplexMode kDuplexDefault =
    printing::mojom::DuplexMode::kSimplex;
static constexpr bool kColorChangeable = true;
static constexpr bool kColorDefault = true;
static constexpr printing::mojom::ColorModel kColorModel =
    printing::mojom::ColorModel::kRGB;
static constexpr printing::mojom::ColorModel kBwModel =
    printing::mojom::ColorModel::kGrayscale;
const printing::PrinterSemanticCapsAndDefaults::Papers kPapers{kPaperA4,
                                                               kPaperLetter};
const printing::PrinterSemanticCapsAndDefaults::Papers kUserDefinedPapers{
    kPaperA3, kPaperLedger};
const printing::PrinterSemanticCapsAndDefaults::Paper kDefaultPaper =
    kPaperLetter;
static constexpr gfx::Size kDpi600(600, 600);
static constexpr gfx::Size kDpi1200(1200, 1200);
static constexpr gfx::Size kDpi1200x600(1200, 600);
const std::vector<gfx::Size> kDpis{kDpi600, kDpi1200, kDpi1200x600};
static constexpr gfx::Size kDefaultDpi = kDpi600;
#if BUILDFLAG(IS_CHROMEOS_ASH)
static constexpr bool kPinSupported = true;
#endif

printing::PrinterSemanticCapsAndDefaults
GenerateSamplePrinterSemanticCapsAndDefaults() {
  printing::PrinterSemanticCapsAndDefaults caps;

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
#if BUILDFLAG(IS_CHROMEOS_ASH)
  caps.pin_supported = kPinSupported;
  caps.advanced_capabilities = kAdvancedCapabilities;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return caps;
}

}  // namespace

TEST(PrintBackendMojomTraitsTest, TestSerializeAndDeserializePrinterBasicInfo) {
  static const printing::PrinterBasicInfo kPrinterBasicInfo1(
      /*printer_name=*/"test printer name 1",
      /*display_name=*/"test display name 1",
      /*printer_description=*/"This is printer #1 for unit testing.",
      /*printer_status=*/0,
      /*is_default=*/true,
      /*options=*/
      std::map<std::string, std::string>{{"opt1", "123"}, {"opt2", "456"}});
  static const printing::PrinterBasicInfo kPrinterBasicInfo2(
      /*printer_name=*/"test printer name 2",
      /*display_name=*/"test display name 2",
      /*printer_description=*/"This is printer #2 for unit testing.",
      /*printer_status=*/1,
      /*is_default=*/false,
      /*options=*/std::map<std::string, std::string>{});
  static const printing::PrinterBasicInfo kPrinterBasicInfo3(
      /*printer_name=*/"test printer name 2",
      /*display_name=*/"test display name 2",
      /*printer_description=*/"",
      /*printer_status=*/9,
      /*is_default=*/false,
      /*options=*/std::map<std::string, std::string>{});
  static const PrinterList kPrinterList{kPrinterBasicInfo1, kPrinterBasicInfo2,
                                        kPrinterBasicInfo3};

  for (auto info : kPrinterList) {
    printing::PrinterBasicInfo input = info;
    printing::PrinterBasicInfo output;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<printing::mojom::PrinterBasicInfo>(
            input, output));
    EXPECT_EQ(info, output);
  }
}

TEST(PrintBackendMojomTraitsTest,
     TestSerializeAndDeserializePrinterBasicInfoEmptyNames) {
  static const printing::PrinterBasicInfo kPrinterBasicInfoEmptyPrinterName(
      /*printer_name=*/"",
      /*display_name=*/"test display name",
      /*printer_description=*/"",
      /*printer_status=*/0,
      /*is_default=*/true,
      /*options=*/std::map<std::string, std::string>{});
  static const printing::PrinterBasicInfo kPrinterBasicInfoEmptyDisplayName(
      /*printer_name=*/"test printer name",
      /*display_name=*/"",
      /*printer_description=*/"",
      /*printer_status=*/0,
      /*is_default=*/true,
      /*options=*/std::map<std::string, std::string>{});
  static const PrinterList kPrinterList{kPrinterBasicInfoEmptyPrinterName,
                                        kPrinterBasicInfoEmptyDisplayName};

  for (auto info : kPrinterList) {
    printing::PrinterBasicInfo input = info;
    printing::PrinterBasicInfo output;
    EXPECT_FALSE(
        mojo::test::SerializeAndDeserialize<printing::mojom::PrinterBasicInfo>(
            input, output));
  }
}

TEST(PrintBackendMojomTraitsTest, TestSerializeAndDeserializePaper) {
  for (const auto& paper : kPapers) {
    printing::PrinterSemanticCapsAndDefaults::Paper input = paper;
    printing::PrinterSemanticCapsAndDefaults::Paper output;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<printing::mojom::Paper>(
        input, output));
    EXPECT_EQ(paper, output);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST(PrintBackendMojomTraitsTest,
     TestSerializeAndDeserializeAdvancedCapability) {
  for (const auto& advanced_capability : kAdvancedCapabilities) {
    printing::AdvancedCapability input = advanced_capability;
    printing::AdvancedCapability output;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
                printing::mojom::AdvancedCapability>(input, output));
    EXPECT_EQ(advanced_capability, output);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST(PrintBackendMojomTraitsTest,
     TestSerializeAndDeserializePrinterSemanticCapsAndDefaults) {
  printing::PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults();
  printing::PrinterSemanticCapsAndDefaults output;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              printing::mojom::PrinterSemanticCapsAndDefaults>(input, output));

  EXPECT_EQ(kCollateCapable, output.collate_capable);
  EXPECT_EQ(kCollateDefault, output.collate_default);
  EXPECT_EQ(kCopiesMax, output.copies_max);
  EXPECT_EQ(kDuplexModes, output.duplex_modes);
  EXPECT_EQ(kDuplexDefault, output.duplex_default);
  EXPECT_EQ(kColorChangeable, output.color_changeable);
  EXPECT_EQ(kColorDefault, output.color_default);
  EXPECT_EQ(kColorModel, output.color_model);
  EXPECT_EQ(kBwModel, output.bw_model);
  EXPECT_EQ(kPapers, output.papers);
  EXPECT_EQ(kUserDefinedPapers, output.user_defined_papers);
  EXPECT_TRUE(kDefaultPaper == output.default_paper);
  EXPECT_EQ(kDpis, output.dpis);
  EXPECT_EQ(kDefaultDpi, output.default_dpi);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(kPinSupported, output.pin_supported);
  EXPECT_EQ(kAdvancedCapabilities, output.advanced_capabilities);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

TEST(PrintBackendMojomTraitsTest,
     TestSerializeAndDeserializePrinterSemanticCapsAndDefaultsCopiesMax) {
  printing::PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults();
  printing::PrinterSemanticCapsAndDefaults output;

  // Override sample with no copies.
  input.copies_max = 0;

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               printing::mojom::PrinterSemanticCapsAndDefaults>(input, output));
}

TEST(
    PrintBackendMojomTraitsTest,
    TestSerializeAndDeserializePrinterSemanticCapsAndDefaultsAllowableEmptyArrays) {
  printing::PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults();
  printing::PrinterSemanticCapsAndDefaults output;

  // Override sample with arrays which are allowed to be empty:
  // `duplex_modes`, `user_defined_papers`, `dpis`, `advanced_capabilities`.
  const std::vector<printing::mojom::DuplexMode> kEmptyDuplexModes{};
  const printing::PrinterSemanticCapsAndDefaults::Papers
      kEmptyUserDefinedPapers{};
  const std::vector<gfx::Size> kEmptyDpis{};
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const printing::AdvancedCapabilities kEmptyAdvancedCapabilities{};
#endif

  input.duplex_modes = kEmptyDuplexModes;
  input.user_defined_papers = kEmptyUserDefinedPapers;
  input.dpis = kEmptyDpis;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  input.advanced_capabilities = kEmptyAdvancedCapabilities;
#endif

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              printing::mojom::PrinterSemanticCapsAndDefaults>(input, output));

  EXPECT_EQ(kEmptyDuplexModes, output.duplex_modes);
  EXPECT_EQ(kEmptyUserDefinedPapers, output.user_defined_papers);
  EXPECT_EQ(kEmptyDpis, output.dpis);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_EQ(kEmptyAdvancedCapabilities, output.advanced_capabilities);
#endif
}

TEST(PrintBackendMojomTraitsTest,
     TestSerializeAndDeserializePrinterSemanticCapsAndDefaultsEmptyPapers) {
  printing::PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults();
  printing::PrinterSemanticCapsAndDefaults output;

  // Override sample with empty `papers`, which is not allowed.
  input.papers = printing::PrinterSemanticCapsAndDefaults::Papers{};

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               printing::mojom::PrinterSemanticCapsAndDefaults>(input, output));
}

TEST(
    PrintBackendMojomTraitsTest,
    TestSerializeAndDeserializePrinterSemanticCapsAndDefaultsNoDuplicatesInArrays) {
  printing::PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults();
  printing::PrinterSemanticCapsAndDefaults output;

  // Override sample with arrays containing duplicates, which is not allowed.
  input.duplex_modes = std::vector<printing::mojom::DuplexMode>{
      printing::mojom::DuplexMode::kLongEdge,
      printing::mojom::DuplexMode::kSimplex,
      printing::mojom::DuplexMode::kSimplex};

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               printing::mojom::PrinterSemanticCapsAndDefaults>(input, output));

  // Use a paper with same name but different size.
  printing::PrinterSemanticCapsAndDefaults::Paper paperA4Prime = kPaperA4;
  paperA4Prime.size_um = kPaperLetter.size_um;
  input = GenerateSamplePrinterSemanticCapsAndDefaults();
  input.papers = printing::PrinterSemanticCapsAndDefaults::Papers{
      kPaperA4, kPaperLetter, kPaperLedger, paperA4Prime};

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               printing::mojom::PrinterSemanticCapsAndDefaults>(input, output));

  input = GenerateSamplePrinterSemanticCapsAndDefaults();
  input.user_defined_papers = printing::PrinterSemanticCapsAndDefaults::Papers{
      kPaperLetter, kPaperLetter};

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               printing::mojom::PrinterSemanticCapsAndDefaults>(input, output));

  input = GenerateSamplePrinterSemanticCapsAndDefaults();
  input.dpis = std::vector<gfx::Size>{kDpi600, kDpi600, kDpi1200};

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               printing::mojom::PrinterSemanticCapsAndDefaults>(input, output));

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Use an advanced capability with same name but different other fields.
  printing::AdvancedCapability advancedCapability1Prime = kAdvancedCapability1;
  advancedCapability1Prime.type = printing::AdvancedCapability::Type::kInteger;
  advancedCapability1Prime.default_value = "42";
  input = GenerateSamplePrinterSemanticCapsAndDefaults();
  input.advanced_capabilities = printing::AdvancedCapabilities{
      kAdvancedCapability1, advancedCapability1Prime};

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               printing::mojom::PrinterSemanticCapsAndDefaults>(input, output));
#endif
}

}  // namespace printing
