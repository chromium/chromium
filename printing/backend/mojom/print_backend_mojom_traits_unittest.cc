// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <vector>

#include "build/build_config.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "printing/backend/mojom/print_backend.mojom.h"
#include "printing/backend/print_backend.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

const PrinterSemanticCapsAndDefaults::Paper kPaperA3{
    /*display_name=*/"A3", /*vendor_id=*/"67",
    /*size_um=*/gfx::Size(7016, 9921)};
const PrinterSemanticCapsAndDefaults::Paper kPaperA4{
    /*display_name=*/"A4", /*vendor_id=*/"12",
    /*size_um=*/gfx::Size(4961, 7016)};
const PrinterSemanticCapsAndDefaults::Paper kPaperLetter{
    /*display_name=*/"Letter", /*vendor_id=*/"45",
    /*size_um=*/gfx::Size(5100, 6600)};
const PrinterSemanticCapsAndDefaults::Paper kPaperLedger{
    /*display_name=*/"Ledger", /*vendor_id=*/"89",
    /*size_um=*/gfx::Size(6600, 10200)};

#if BUILDFLAG(IS_CHROMEOS)
const AdvancedCapability kAdvancedCapability1(
    /*name=*/"advanced_cap_bool",
    /*display_name=*/"Advanced Capability #1 (bool)",
    /*type=*/AdvancedCapability::Type::kBoolean,
    /*default_value=*/"true",
    /*values=*/{});
const AdvancedCapability kAdvancedCapability2(
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
const AdvancedCapabilities kAdvancedCapabilities{kAdvancedCapability1,
                                                 kAdvancedCapability2};
#endif  // BUILDFLAG(IS_CHROMEOS)

constexpr bool kCollateCapable = true;
constexpr bool kCollateDefault = true;
constexpr int kCopiesMax = 123;
const std::vector<mojom::DuplexMode> kDuplexModes{
    mojom::DuplexMode::kSimplex, mojom::DuplexMode::kLongEdge,
    mojom::DuplexMode::kShortEdge};
constexpr mojom::DuplexMode kDuplexDefault = mojom::DuplexMode::kSimplex;
constexpr bool kColorChangeable = true;
constexpr bool kColorDefault = true;
constexpr mojom::ColorModel kColorModel = mojom::ColorModel::kRGB;
constexpr mojom::ColorModel kBwModel = mojom::ColorModel::kGrayscale;
const PrinterSemanticCapsAndDefaults::Papers kPapers{kPaperA4, kPaperLetter};
const PrinterSemanticCapsAndDefaults::Papers kUserDefinedPapers{kPaperA3,
                                                                kPaperLedger};
const PrinterSemanticCapsAndDefaults::Paper kDefaultPaper = kPaperLetter;
constexpr gfx::Size kDpi600(600, 600);
constexpr gfx::Size kDpi1200(1200, 1200);
constexpr gfx::Size kDpi1200x600(1200, 600);
const std::vector<gfx::Size> kDpis{kDpi600, kDpi1200, kDpi1200x600};
constexpr gfx::Size kDefaultDpi = kDpi600;
#if BUILDFLAG(IS_CHROMEOS)
constexpr bool kPinSupported = true;
#endif

PrinterSemanticCapsAndDefaults GenerateSamplePrinterSemanticCapsAndDefaults() {
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
#if BUILDFLAG(IS_CHROMEOS)
  caps.pin_supported = kPinSupported;
  caps.advanced_capabilities = kAdvancedCapabilities;
#endif  // BUILDFLAG(IS_CHROMEOS)

  return caps;
}

}  // namespace

TEST(PrintBackendMojomTraitsTest, TestSerializeAndDeserializePrinterBasicInfo) {
  static const PrinterBasicInfo kPrinterBasicInfo1(
      /*printer_name=*/"test printer name 1",
      /*display_name=*/"test display name 1",
      /*printer_description=*/"This is printer #1 for unit testing.",
      /*printer_status=*/0,
      /*is_default=*/true,
      /*options=*/{{"opt1", "123"}, {"opt2", "456"}});
  static const PrinterBasicInfo kPrinterBasicInfo2(
      /*printer_name=*/"test printer name 2",
      /*display_name=*/"test display name 2",
      /*printer_description=*/"This is printer #2 for unit testing.",
      /*printer_status=*/1,
      /*is_default=*/false,
      /*options=*/{});
  static const PrinterBasicInfo kPrinterBasicInfo3(
      /*printer_name=*/"test printer name 2",
      /*display_name=*/"test display name 2",
      /*printer_description=*/"",
      /*printer_status=*/9,
      /*is_default=*/false,
      /*options=*/{});
  static const PrinterList kPrinterList{kPrinterBasicInfo1, kPrinterBasicInfo2,
                                        kPrinterBasicInfo3};

  for (const auto& info : kPrinterList) {
    PrinterBasicInfo input = info;
    PrinterBasicInfo output;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::PrinterBasicInfo>(
        input, output));
    EXPECT_EQ(info, output);
  }
}

TEST(PrintBackendMojomTraitsTest,
     TestSerializeAndDeserializePrinterBasicInfoEmptyNames) {
  static const PrinterBasicInfo kPrinterBasicInfoEmptyPrinterName(
      /*printer_name=*/"",
      /*display_name=*/"test display name",
      /*printer_description=*/"",
      /*printer_status=*/0,
      /*is_default=*/true,
      /*options=*/{});
  static const PrinterBasicInfo kPrinterBasicInfoEmptyDisplayName(
      /*printer_name=*/"test printer name",
      /*display_name=*/"",
      /*printer_description=*/"",
      /*printer_status=*/0,
      /*is_default=*/true,
      /*options=*/{});
  static const PrinterList kPrinterList{kPrinterBasicInfoEmptyPrinterName,
                                        kPrinterBasicInfoEmptyDisplayName};

  for (const auto& info : kPrinterList) {
    PrinterBasicInfo input = info;
    PrinterBasicInfo output;
    EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::PrinterBasicInfo>(
        input, output));
  }
}

TEST(PrintBackendMojomTraitsTest, TestSerializeAndDeserializePaper) {
  for (const auto& paper : kPapers) {
    PrinterSemanticCapsAndDefaults::Paper input = paper;
    PrinterSemanticCapsAndDefaults::Paper output;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));
    EXPECT_EQ(paper, output);
  }
}

#if BUILDFLAG(IS_CHROMEOS)
TEST(PrintBackendMojomTraitsTest,
     TestSerializeAndDeserializeAdvancedCapability) {
  for (const auto& advanced_capability : kAdvancedCapabilities) {
    AdvancedCapability input = advanced_capability;
    AdvancedCapability output;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::AdvancedCapability>(
        input, output));
    EXPECT_EQ(advanced_capability, output);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST(PrintBackendMojomTraitsTest,
     TestSerializeAndDeserializePrinterSemanticCapsAndDefaults) {
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults();
  PrinterSemanticCapsAndDefaults output;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::PrinterSemanticCapsAndDefaults>(input, output));

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
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(kPinSupported, output.pin_supported);
  EXPECT_EQ(kAdvancedCapabilities, output.advanced_capabilities);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST(PrintBackendMojomTraitsTest,
     TestSerializeAndDeserializePrinterSemanticCapsAndDefaultsCopiesMax) {
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults();
  PrinterSemanticCapsAndDefaults output;

  // Override sample with no copies.
  input.copies_max = 0;

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               mojom::PrinterSemanticCapsAndDefaults>(input, output));
}

TEST(
    PrintBackendMojomTraitsTest,
    TestSerializeAndDeserializePrinterSemanticCapsAndDefaultsAllowableEmptyArrays) {
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults();
  PrinterSemanticCapsAndDefaults output;

  // Override sample with arrays which are allowed to be empty:
  // `duplex_modes`, `user_defined_papers`, `dpis`, `advanced_capabilities`.
  const std::vector<mojom::DuplexMode> kEmptyDuplexModes;
  const PrinterSemanticCapsAndDefaults::Papers kEmptyUserDefinedPapers;
  const std::vector<gfx::Size> kEmptyDpis;
#if BUILDFLAG(IS_CHROMEOS)
  const AdvancedCapabilities kEmptyAdvancedCapabilities;
#endif

  input.duplex_modes = kEmptyDuplexModes;
  input.user_defined_papers = kEmptyUserDefinedPapers;
  input.dpis = kEmptyDpis;
#if BUILDFLAG(IS_CHROMEOS)
  input.advanced_capabilities = kEmptyAdvancedCapabilities;
#endif

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::PrinterSemanticCapsAndDefaults>(input, output));

  EXPECT_EQ(kEmptyDuplexModes, output.duplex_modes);
  EXPECT_EQ(kEmptyUserDefinedPapers, output.user_defined_papers);
  EXPECT_EQ(kEmptyDpis, output.dpis);
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(kEmptyAdvancedCapabilities, output.advanced_capabilities);
#endif
}

TEST(PrintBackendMojomTraitsTest,
     TestSerializeAndDeserializePrinterSemanticCapsAndDefaultsEmptyPapers) {
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults();
  PrinterSemanticCapsAndDefaults output;

  // Override sample with empty `papers`.  This is known to be possible, seen
  // with Epson PX660 series driver.
  const PrinterSemanticCapsAndDefaults::Papers kEmptyPapers;
  input.papers.clear();

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::PrinterSemanticCapsAndDefaults>(input, output));

  EXPECT_EQ(kEmptyPapers, output.papers);
}

TEST(
    PrintBackendMojomTraitsTest,
    TestSerializeAndDeserializePrinterSemanticCapsAndDefaultsNoDuplicatesInArrays) {
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults();
  PrinterSemanticCapsAndDefaults output;

  // Override sample with arrays containing duplicates, which is not allowed.
  input.duplex_modes = {mojom::DuplexMode::kLongEdge,
                        mojom::DuplexMode::kSimplex,
                        mojom::DuplexMode::kSimplex};

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               mojom::PrinterSemanticCapsAndDefaults>(input, output));

  input = GenerateSamplePrinterSemanticCapsAndDefaults();
  input.user_defined_papers = {kPaperLetter, kPaperLetter};

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               mojom::PrinterSemanticCapsAndDefaults>(input, output));

#if BUILDFLAG(IS_CHROMEOS)
  // Use an advanced capability with same name but different other fields.
  AdvancedCapability advanced_capability1_prime = kAdvancedCapability1;
  advanced_capability1_prime.type = AdvancedCapability::Type::kInteger;
  advanced_capability1_prime.default_value = "42";
  input = GenerateSamplePrinterSemanticCapsAndDefaults();
  input.advanced_capabilities = {kAdvancedCapability1,
                                 advanced_capability1_prime};

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               mojom::PrinterSemanticCapsAndDefaults>(input, output));
#endif
}

TEST(
    PrintBackendMojomTraitsTest,
    TestSerializeAndDeserializePrinterSemanticCapsAndDefaultsAllowedDuplicatesInArrays) {
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults();
  PrinterSemanticCapsAndDefaults output;

  // Override sample with arrays containing duplicates where it is allowed.
  // Duplicate DPIs are known to be possible, seen with the Kyocera KX driver.
  const std::vector<gfx::Size> kDuplicateDpis{kDpi600, kDpi600, kDpi1200};
  input.dpis = kDuplicateDpis;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::PrinterSemanticCapsAndDefaults>(input, output));

  EXPECT_EQ(kDuplicateDpis, output.dpis);

  // Duplicate papers are known to be possible, seen with the Konica Minolta
  // 4750 Series PS driver.
  // Use a paper with same name but different size.
  PrinterSemanticCapsAndDefaults::Paper paper_a4_prime = kPaperA4;
  paper_a4_prime.size_um = kPaperLetter.size_um;
  input = GenerateSamplePrinterSemanticCapsAndDefaults();
  const PrinterSemanticCapsAndDefaults::Papers kDuplicatePapers{
      kPaperA4, kPaperLetter, kPaperLedger, paper_a4_prime};
  input.papers = kDuplicatePapers;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::PrinterSemanticCapsAndDefaults>(input, output));

  EXPECT_EQ(kDuplicatePapers, output.papers);
}

}  // namespace printing
