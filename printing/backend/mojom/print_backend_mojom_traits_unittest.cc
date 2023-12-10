// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "build/build_config.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "printing/backend/mojom/print_backend.mojom.h"
#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_test_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

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
  PrinterSemanticCapsAndDefaults::Papers test_papers = kPapers;
  test_papers.push_back(kPaperCustom);

  for (const auto& paper : test_papers) {
    PrinterSemanticCapsAndDefaults::Paper input = paper;
    PrinterSemanticCapsAndDefaults::Paper output;
    EXPECT_TRUE(
        mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));
    EXPECT_EQ(paper, output);
  }
}

TEST(PrintBackendMojomTraitsTest, TestPaperCtors) {
  // All constructors should be able to generate valid papers.
  constexpr gfx::Size kNonEmptySize(100, 200);
  constexpr gfx::Rect kNonEmptyPrintableArea(kNonEmptySize);
  PrinterSemanticCapsAndDefaults::Paper output;

  PrinterSemanticCapsAndDefaults::Paper input;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));

  input = PrinterSemanticCapsAndDefaults::Paper("display_name", "vendor_id",
                                                kNonEmptySize);
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));

  input = PrinterSemanticCapsAndDefaults::Paper(
      "display_name", "vendor_id", kNonEmptySize, kNonEmptyPrintableArea);
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));

  input = PrinterSemanticCapsAndDefaults::Paper(
      "display_name", "vendor_id", kNonEmptySize, kNonEmptyPrintableArea,
      /*max_height_um=*/200);
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));

  input = PrinterSemanticCapsAndDefaults::Paper(
      "display_name", "vendor_id", kNonEmptySize, kNonEmptyPrintableArea,
      /*max_height_um=*/200, /*has_borderless_variant=*/true);
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));
}

TEST(PrintBackendMojomTraitsTest, TestPaperEmpty) {
  // Empty Papers should be valid.
  PrinterSemanticCapsAndDefaults::Paper input;
  PrinterSemanticCapsAndDefaults::Paper output;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));
  EXPECT_EQ(input, output);
}

TEST(PrintBackendMojomTraitsTest, TestPaperInvalidCustomSize) {
  // The min height is larger than the max height, so it should be invalid.
  PrinterSemanticCapsAndDefaults::Paper input{
      /*display_name=*/"display_name",
      /*vendor_id=*/"vendor_id",
      /*size_um=*/gfx::Size(4000, 7000),
      /*printable_area_um=*/gfx::Rect(0, 0, 4000, 7000),
      /*max_height_um=*/6000,
      /*has_borderless_variant=*/true,
  };
  PrinterSemanticCapsAndDefaults::Paper output;

  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));
}

TEST(PrintBackendMojomTraitsTest, TestPaperEmptyPrintableArea) {
  // The printable area is empty, but the other fields are not, so it should be
  // invalid.
  PrinterSemanticCapsAndDefaults::Paper input{
      /*display_name=*/"display_name", /*vendor_id=*/"vendor_id",
      /*size_um=*/gfx::Size(4000, 7000),
      /*printable_area_um=*/gfx::Rect(0, 100, 0, 0)};
  PrinterSemanticCapsAndDefaults::Paper output;

  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));
}

TEST(PrintBackendMojomTraitsTest, TestPaperPrintableAreaLargerThanSize) {
  // The printable area is larger than the size, so it should be invalid.
  PrinterSemanticCapsAndDefaults::Paper input{
      /*display_name=*/"display_name", /*vendor_id=*/"vendor_id",
      /*size_um=*/gfx::Size(4000, 7000),
      /*printable_area_um=*/gfx::Rect(0, 100, 4100, 7200)};
  PrinterSemanticCapsAndDefaults::Paper output;

  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));
}

TEST(PrintBackendMojomTraitsTest, TestPaperPrintableAreaLargerThanCustomSize) {
  // The printable area is larger than the custom size, so it should be invalid.
  PrinterSemanticCapsAndDefaults::Paper input{
      /*display_name=*/"display_name",
      /*vendor_id=*/"vendor_id",
      /*size_um=*/gfx::Size(4000, 7000),
      /*printable_area_um=*/gfx::Rect(0, 100, 4100, 7200),
      /*max_height_um=*/8000};
  PrinterSemanticCapsAndDefaults::Paper output;

  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));
}

TEST(PrintBackendMojomTraitsTest, TestPaperPrintableAreaOutOfBounds) {
  // The printable area is out of bounds of the size, so it should be invalid.
  PrinterSemanticCapsAndDefaults::Paper input{
      /*display_name=*/"display_name", /*vendor_id=*/"vendor_id",
      /*size_um=*/gfx::Size(4000, 7000),
      /*printable_area_um=*/gfx::Rect(4050, 6950, 100, 100)};
  PrinterSemanticCapsAndDefaults::Paper output;

  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));
}

TEST(PrintBackendMojomTraitsTest, TestPaperNegativePrintableArea) {
  // The printable area has negative x and y values, so it should be invalid.
  PrinterSemanticCapsAndDefaults::Paper input{
      /*display_name=*/"display_name", /*vendor_id=*/"vendor_id",
      /*size_um=*/gfx::Size(4000, 7000),
      /*printable_area_um=*/gfx::Rect(-10, -10, 2800, 6000)};
  PrinterSemanticCapsAndDefaults::Paper output;

  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::Paper>(input, output));
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
  OptionalSampleCapabilities caps;
#if BUILDFLAG(IS_CHROMEOS)
  caps = SampleWithPinAndAdvancedCapabilities();
#endif  // BUILDFLAG(IS_CHROMEOS)
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults(std::move(caps));
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
  EXPECT_EQ(kMediaTypes, output.media_types);
  EXPECT_EQ(kDefaultMediaType, output.default_media_type);
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(kPinSupported, output.pin_supported);
  EXPECT_EQ(kAdvancedCapabilities, output.advanced_capabilities);
#endif  // BUILDFLAG(IS_CHROMEOS)
}

TEST(PrintBackendMojomTraitsTest,
     TestSerializeAndDeserializePrinterSemanticCapsAndDefaultsCopiesMax) {
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults({});
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
      GenerateSamplePrinterSemanticCapsAndDefaults({});
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
      GenerateSamplePrinterSemanticCapsAndDefaults({});
  PrinterSemanticCapsAndDefaults output;

  // Override sample with empty `papers`.  This is known to be possible, seen
  // with Epson PX660 series driver.
  const PrinterSemanticCapsAndDefaults::Papers kEmptyPapers;
  input.papers.clear();

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::PrinterSemanticCapsAndDefaults>(input, output));

  EXPECT_EQ(kEmptyPapers, output.papers);
}

TEST(PrintBackendMojomTraitsTest,
     TestSerializeAndDeserializePrinterSemanticCapsAndDefaultsEmptyMediaTypes) {
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults({});
  PrinterSemanticCapsAndDefaults output;

  // Override sample with empty `media_types`.
  const PrinterSemanticCapsAndDefaults::MediaTypes kEmptyMediaTypes;
  input.media_types.clear();

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::PrinterSemanticCapsAndDefaults>(input, output));

  EXPECT_EQ(kEmptyMediaTypes, output.media_types);
}

TEST(
    PrintBackendMojomTraitsTest,
    TestSerializeAndDeserializePrinterSemanticCapsAndDefaultsNoDuplicatesInArrays) {
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults({});
  PrinterSemanticCapsAndDefaults output;

  // Override sample with arrays containing duplicates, which is not allowed.
  input.duplex_modes = {mojom::DuplexMode::kLongEdge,
                        mojom::DuplexMode::kSimplex,
                        mojom::DuplexMode::kSimplex};

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               mojom::PrinterSemanticCapsAndDefaults>(input, output));

  input = GenerateSamplePrinterSemanticCapsAndDefaults({});
  input.user_defined_papers = {kPaperLetter, kPaperLetter};

  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               mojom::PrinterSemanticCapsAndDefaults>(input, output));

#if BUILDFLAG(IS_CHROMEOS)
  // Use an advanced capability with same name but different other fields.
  AdvancedCapability advanced_capability1_prime = kAdvancedCapability1;
  advanced_capability1_prime.type = AdvancedCapability::Type::kInteger;
  advanced_capability1_prime.default_value = "42";
  input = GenerateSamplePrinterSemanticCapsAndDefaults({});
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
      GenerateSamplePrinterSemanticCapsAndDefaults({});
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
  PrinterSemanticCapsAndDefaults::Paper paper_a4_prime(
      kPaperA4.display_name(), kPaperA4.vendor_id(), kPaperLetter.size_um(),
      kPaperA4.printable_area_um());
  input = GenerateSamplePrinterSemanticCapsAndDefaults({});
  const PrinterSemanticCapsAndDefaults::Papers kDuplicatePapers{
      kPaperA4, kPaperLetter, kPaperLedger, paper_a4_prime};
  input.papers = kDuplicatePapers;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::PrinterSemanticCapsAndDefaults>(input, output));

  EXPECT_EQ(kDuplicatePapers, output.papers);
}

#if BUILDFLAG(IS_WIN)
TEST(PrintBackendMojomTraitsTest,
     TestSerializeAndDeserializePageOutputQualityAttribute) {
  PageOutputQualityAttribute input = kPageOutputQualityAttribute1;
  PageOutputQualityAttribute output;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageOutputQualityAttribute>(
          input, output));
  EXPECT_EQ(kPageOutputQualityAttribute1.display_name, output.display_name);
  EXPECT_EQ(kPageOutputQualityAttribute1.name, output.name);
}

TEST(PrintBackendMojomTraitsTest,
     TestSerializeAndDeserializePageOutputQuality) {
  PageOutputQuality input = kPageOutputQuality;
  PageOutputQuality output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::PageOutputQuality>(
      input, output));
  EXPECT_EQ(kPageOutputQuality.qualities, output.qualities);
  EXPECT_EQ(kPageOutputQuality.default_quality, output.default_quality);
}

TEST(PrintBackendMojomTraitsTest,
     TestSerializeAndDeserializePrinterSemanticCapsAndDefaultsXpsCapabilities) {
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults(
          SampleWithPageOutputQuality());
  PrinterSemanticCapsAndDefaults output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::PrinterSemanticCapsAndDefaults>(input, output));
  ASSERT_TRUE(output.page_output_quality);
  EXPECT_EQ(kPageOutputQuality.qualities,
            output.page_output_quality->qualities);
  EXPECT_EQ(kPageOutputQuality.default_quality,
            output.page_output_quality->default_quality);
}

TEST(
    PrintBackendMojomTraitsTest,
    TestSerializeAndDeserializePrinterSemanticCapsAndDefaultsAllowableEmptyArraysXpsCapabilities) {
  const PageOutputQualityAttributes kEmptyQualities;
  PageOutputQuality quality(kEmptyQualities, /*default_quality=*/std::nullopt);
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults({});
  input.page_output_quality = quality;

  PrinterSemanticCapsAndDefaults output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::PrinterSemanticCapsAndDefaults>(input, output));
  ASSERT_TRUE(output.page_output_quality);
  EXPECT_EQ(kEmptyQualities, output.page_output_quality->qualities);
}

TEST(
    PrintBackendMojomTraitsTest,
    TestSerializeAndDeserializePrinterSemanticCapsAndDefaultsDefaultQualityInArraysXpsCapabilities) {
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults(
          SampleWithPageOutputQuality());
  input.page_output_quality->default_quality = kDefaultQuality;
  PrinterSemanticCapsAndDefaults output;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<
              mojom::PrinterSemanticCapsAndDefaults>(input, output));
  ASSERT_TRUE(output.page_output_quality);
  EXPECT_EQ(kPageOutputQuality.qualities,
            output.page_output_quality->qualities);
  EXPECT_EQ(kDefaultQuality, output.page_output_quality->default_quality);
}

TEST(
    PrintBackendMojomTraitsTest,
    TestSerializeAndDeserializePrinterSemanticCapsAndDefaultsMissingDefaultQualityInArraysXpsCapabilities) {
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults(
          SampleWithPageOutputQuality());

  // Default quality is non-null, but there is no quality with same name as
  // default quality, which is not allowed.
  input.page_output_quality->default_quality = "ns000:MissingDefault";
  PrinterSemanticCapsAndDefaults output;
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               mojom::PrinterSemanticCapsAndDefaults>(input, output));
}

TEST(
    PrintBackendMojomTraitsTest,
    TestSerializeAndDeserializePrinterSemanticCapsAndDefaultsNoDuplicatesInArraysXpsCapabilities) {
  // `kPageOutputQualityAttributePrime` has same display_name and name with
  // `kPageOutputQualityAttribute1`, which is not allowed.
  const PageOutputQualityAttribute kPageOutputQualityAttributePrime(
      /*display_name=*/"Normal",
      /*name=*/"ns000:Normal");
  PageOutputQuality page_output_quality(
      {kPageOutputQualityAttribute1, kPageOutputQualityAttributePrime,
       kPageOutputQualityAttribute2},
      /*default_quality=*/std::nullopt);
  PrinterSemanticCapsAndDefaults input =
      GenerateSamplePrinterSemanticCapsAndDefaults({});
  input.page_output_quality = page_output_quality;
  PrinterSemanticCapsAndDefaults output;
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<
               mojom::PrinterSemanticCapsAndDefaults>(input, output));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace printing
