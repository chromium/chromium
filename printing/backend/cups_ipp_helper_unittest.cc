// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_ipp_helper.h"

#include <cups/cups.h>

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "printing/backend/mock_cups_printer.h"
#include "printing/backend/print_backend_utils.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

using ::testing::Pointwise;

// Matches the name field to a string.
MATCHER(AdvancedCapabilityName, "") {
  *result_listener << "Expected: " << std::get<1>(arg)
                   << " vs Actual: " << std::get<0>(arg).name;
  return std::get<0>(arg).name == std::get<1>(arg);
}

class MockCupsPrinterWithMarginsAndAttributes : public MockCupsPrinter {
 public:
  MockCupsPrinterWithMarginsAndAttributes() = default;
  ~MockCupsPrinterWithMarginsAndAttributes() override = default;

  // CupsOptionProvider:
  ipp_attribute_t* GetSupportedOptionValues(
      const char* option_name) const override {
    const auto attr = supported_attributes_.find(option_name);
    return attr != supported_attributes_.end() ? attr->second : nullptr;
  }

  // CupsOptionProvider:
  std::vector<base::StringPiece> GetSupportedOptionValueStrings(
      const char* option_name) const override {
    ipp_attribute_t* attr = GetSupportedOptionValues(option_name);
    if (!attr)
      return std::vector<base::StringPiece>();

    std::vector<base::StringPiece> strings;
    const int size = ippGetCount(attr);
    strings.reserve(size);
    for (int i = 0; i < size; ++i) {
      const char* const value = ippGetString(attr, i, nullptr);
      if (!value) {
        continue;
      }
      strings.push_back(value);
    }

    return strings;
  }

  // CupsOptionProvider:
  ipp_attribute_t* GetDefaultOptionValue(
      const char* option_name) const override {
    const auto attr = default_attributes_.find(option_name);
    return attr != default_attributes_.end() ? attr->second : nullptr;
  }

  // CupsOptionProvider:
  ipp_attribute_t* GetMediaColDatabase() const override {
    return media_col_database_;
  }

  // CupsOptionProvider:
  bool CheckOptionSupported(const char* name,
                            const char* value) const override {
    NOTREACHED();
    return false;
  }

  void SetSupportedOptions(base::StringPiece name, ipp_attribute_t* attribute) {
    supported_attributes_[name] = attribute;
  }

  void SetOptionDefault(base::StringPiece name, ipp_attribute_t* attribute) {
    default_attributes_[name] = attribute;
  }

  void SetMediaColDatabase(ipp_attribute_t* attribute) {
    media_col_database_ = attribute;
  }

 private:
  std::map<base::StringPiece, ipp_attribute_t*> supported_attributes_;
  std::map<base::StringPiece, ipp_attribute_t*> default_attributes_;
  ipp_attribute_t* media_col_database_;
};

class PrintBackendCupsIppHelperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ipp_ = ippNew();
    printer_ = std::make_unique<MockCupsPrinterWithMarginsAndAttributes>();
  }

  void TearDown() override {
    ippDelete(ipp_);
    printer_.reset();
  }

  raw_ptr<ipp_t> ipp_;
  std::unique_ptr<MockCupsPrinterWithMarginsAndAttributes> printer_;
};

ipp_attribute_t* MakeInteger(ipp_t* ipp, int value) {
  return ippAddInteger(ipp, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "TEST_DATA",
                       value);
}

ipp_attribute_t* MakeIntCollection(ipp_t* ipp, const std::vector<int>& values) {
  return ippAddIntegers(ipp, IPP_TAG_PRINTER, IPP_TAG_INTEGER, "TEST_DATA",
                        values.size(), values.data());
}

ipp_attribute_t* MakeRange(ipp_t* ipp, int lower_bound, int upper_bound) {
  return ippAddRange(ipp, IPP_TAG_PRINTER, "TEST_DATA", lower_bound,
                     upper_bound);
}

ipp_attribute_t* MakeString(ipp_t* ipp, const char* value) {
  return ippAddString(ipp, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "TEST_DATA",
                      nullptr, value);
}

ipp_attribute_t* MakeStringCollection(ipp_t* ipp,
                                      const std::vector<const char*>& strings) {
  return ippAddStrings(ipp, IPP_TAG_PRINTER, IPP_TAG_KEYWORD, "TEST_DATA",
                       strings.size(), nullptr, strings.data());
}

struct media_info {
  int width;
  int height;
  int bottom_margin;
  int left_margin;
  int right_margin;
  int top_margin;
  std::map<const char*, const char*> keyword_attrs;
  bool is_range;
  int width_max;
  int height_max;
};

ScopedIppPtr MakeMediaCol(const media_info& info) {
  ScopedIppPtr media_col = WrapIpp(ippNew());
  ScopedIppPtr media_size = WrapIpp(ippNew());

  if (info.is_range) {
    ippAddRange(media_size.get(), IPP_TAG_ZERO, "x-dimension", info.width,
                info.width_max);
    ippAddRange(media_size.get(), IPP_TAG_ZERO, "y-dimension", info.height,
                info.height_max);
  } else {
    ippAddInteger(media_size.get(), IPP_TAG_ZERO, IPP_TAG_INTEGER,
                  "x-dimension", info.width);
    ippAddInteger(media_size.get(), IPP_TAG_ZERO, IPP_TAG_INTEGER,
                  "y-dimension", info.height);
  }

  ippAddCollection(media_col.get(), IPP_TAG_ZERO, "media-size",
                   media_size.get());

  ippAddInteger(media_col.get(), IPP_TAG_ZERO, IPP_TAG_INTEGER,
                "media-bottom-margin", info.bottom_margin);
  ippAddInteger(media_col.get(), IPP_TAG_ZERO, IPP_TAG_INTEGER,
                "media-left-margin", info.left_margin);
  ippAddInteger(media_col.get(), IPP_TAG_ZERO, IPP_TAG_INTEGER,
                "media-right-margin", info.right_margin);
  ippAddInteger(media_col.get(), IPP_TAG_ZERO, IPP_TAG_INTEGER,
                "media-top-margin", info.top_margin);

  for (auto& it : info.keyword_attrs) {
    ippAddString(media_col.get(), IPP_TAG_ZERO, IPP_TAG_KEYWORD, it.first,
                 nullptr, it.second);
  }

  return media_col;
}

ipp_attribute_t* MakeMediaColDefault(ipp_t* ipp, const media_info& info) {
  ScopedIppPtr media_col = MakeMediaCol(info);
  return ippAddCollection(ipp, IPP_TAG_ZERO, "TEST_DATA", media_col.get());
}

ipp_attribute_t* MakeMediaColDatabase(ipp_t* ipp,
                                      const std::vector<media_info>& media) {
  std::vector<ScopedIppPtr> collections;
  std::vector<const ipp_t*> raw_collections;

  for (auto info : media) {
    ScopedIppPtr entry = MakeMediaCol(info);
    raw_collections.emplace_back(entry.get());
    collections.emplace_back(std::move(entry));
  }

  return ippAddCollections(ipp, IPP_TAG_PRINTER, "TEST_DATA",
                           raw_collections.size(), raw_collections.data());
}

TEST_F(PrintBackendCupsIppHelperTest, DefaultPaper) {
  EXPECT_EQ(PrinterSemanticCapsAndDefaults::Paper(), DefaultPaper(*printer_));
  printer_->SetOptionDefault(
      "media-col",
      MakeMediaColDefault(ipp_, {21000, 29700, 10, 10, 10, 10, {}}));
  PrinterSemanticCapsAndDefaults::Paper default_paper = DefaultPaper(*printer_);
  EXPECT_EQ(default_paper.size_um.width(), 210000);
  EXPECT_EQ(default_paper.size_um.height(), 297000);
}

TEST_F(PrintBackendCupsIppHelperTest, CopiesCapable) {
  printer_->SetSupportedOptions("copies", MakeRange(ipp_, 1, 2));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_EQ(2, caps.copies_max);
}

TEST_F(PrintBackendCupsIppHelperTest, CopiesNotCapable) {
  // copies missing, no setup
  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_EQ(1, caps.copies_max);
}

TEST_F(PrintBackendCupsIppHelperTest, ColorPrinter) {
  printer_->SetSupportedOptions(
      "print-color-mode", MakeStringCollection(ipp_, {"color", "monochrome"}));
  printer_->SetOptionDefault("print-color-mode", MakeString(ipp_, "color"));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_TRUE(caps.color_changeable);
  EXPECT_TRUE(caps.color_default);
}

TEST_F(PrintBackendCupsIppHelperTest, BWPrinter) {
  printer_->SetSupportedOptions("print-color-mode",
                                MakeStringCollection(ipp_, {"monochrome"}));
  printer_->SetOptionDefault("print-color-mode",
                             MakeString(ipp_, "monochrome"));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_FALSE(caps.color_changeable);
  EXPECT_FALSE(caps.color_default);
}

TEST_F(PrintBackendCupsIppHelperTest, DuplexSupported) {
  printer_->SetSupportedOptions(
      "sides",
      MakeStringCollection(ipp_, {"two-sided-long-edge", "one-sided"}));
  printer_->SetOptionDefault("sides", MakeString(ipp_, "one-sided"));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_THAT(caps.duplex_modes,
              testing::UnorderedElementsAre(mojom::DuplexMode::kSimplex,
                                            mojom::DuplexMode::kLongEdge));
  EXPECT_EQ(mojom::DuplexMode::kSimplex, caps.duplex_default);
}

TEST_F(PrintBackendCupsIppHelperTest, DuplexNotSupported) {
  printer_->SetSupportedOptions("sides",
                                MakeStringCollection(ipp_, {"one-sided"}));
  printer_->SetOptionDefault("sides", MakeString(ipp_, "one-sided"));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_THAT(caps.duplex_modes,
              testing::UnorderedElementsAre(mojom::DuplexMode::kSimplex));
  EXPECT_EQ(mojom::DuplexMode::kSimplex, caps.duplex_default);
}

TEST_F(PrintBackendCupsIppHelperTest, A4PaperSupported) {
  printer_->SetMediaColDatabase(
      MakeMediaColDatabase(ipp_, {{21000, 29700, 10, 10, 10, 10, {}}}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  PrinterSemanticCapsAndDefaults::Paper paper = caps.papers[0];
  EXPECT_EQ(210000, paper.size_um.width());
  EXPECT_EQ(297000, paper.size_um.height());
}

TEST_F(PrintBackendCupsIppHelperTest, LegalPaperDefault) {
  // na_legal_8.5x14in
  printer_->SetOptionDefault(
      "media-col",
      MakeMediaColDefault(ipp_, {21590, 35560, 10, 10, 10, 10, {}}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);
  EXPECT_EQ(215900, caps.default_paper.size_um.width());
  EXPECT_EQ(355600, caps.default_paper.size_um.height());
}

// Tests that CapsAndDefaultsFromPrinter() does not propagate papers with
// invalid sizes or margins to the Chromium print backend.
TEST_F(PrintBackendCupsIppHelperTest, OmitPapersWithInvalidSizes) {
  printer_->SetMediaColDatabase(
      MakeMediaColDatabase(ipp_, {
                                     {18200, 25700, 100, 100, 100, 100, {}},
                                     {0, 29700, 100, 100, 100, 100, {}},
                                     {-1, 29700, 100, 100, 100, 100, {}},
                                     {21000, 0, 100, 100, 100, 100, {}},
                                     {21000, -1, 100, 100, 100, 100, {}},
                                     {21000, 29700, -1, 100, 100, 100, {}},
                                     {21000, 29700, 100, -1, 100, 100, {}},
                                     {21000, 29700, 100, 100, -1, 100, {}},
                                     {21000, 29700, 100, 100, 100, -1, {}},
                                     {21000, 29700, 100, 10500, 10500, 100, {}},
                                     {21000, 29700, 14850, 100, 100, 14850, {}},
                                     {17600, 25000, 100, 100, 100, 100, {}},
                                 }));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  // The printer reports that it supports four media sizes, two of which
  // are invalid (``invalidsize'' and the empty vendor ID). The
  // preceding call to CapsAndDefaultsFromPrinter() will have dropped
  // these invalid sizes.
  ASSERT_EQ(2U, caps.papers.size());
  for (const auto& paper : caps.papers) {
    EXPECT_NE(21000, paper.size_um.width());
    EXPECT_NE(29700, paper.size_um.height());
  }
}

// Tests that CapsAndDefaultsFromPrinter() does not propagate custom size ranges
// from the media-col-database to the Chromium print backend.
TEST_F(PrintBackendCupsIppHelperTest, OmitPapersWithSizeRanges) {
  printer_->SetMediaColDatabase(MakeMediaColDatabase(
      ipp_, {
                {11430, 26352, 100, 100, 100, 100, {}},
                {0, 0, 100, 100, 100, 100, {}, true, 2540000, 2540000},
                {20320, 25400, 100, 100, 100, 100, {}},
                {100000, 141400, 100, 100, 100, 100, {}},
            }));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  // The printer reports that it supports four media sizes, one of which is not
  // meant for users' eyes (the size range). The preceding call to
  // CapsAndDefaultsFromPrinter() will have dropped these sizes, refusing to
  // propagate them out of the backend.
  ASSERT_EQ(3U, caps.papers.size());
}

// Tests that when the media-col-database contains both bordered and borderless
// versions of a size, CapsAndDefaultsFromPrinter() takes the bordered version
// and drops the borderless version.
TEST_F(PrintBackendCupsIppHelperTest, PreferBorderedSizes) {
  PrinterSemanticCapsAndDefaults caps;

  printer_->SetMediaColDatabase(
      MakeMediaColDatabase(ipp_, {
                                     {21000, 29700, 100, 100, 100, 100, {}},
                                     {21000, 29700, 0, 0, 0, 0, {}},
                                 }));
  CapsAndDefaultsFromPrinter(*printer_, &caps);
  ASSERT_EQ(1U, caps.papers.size());
  EXPECT_NE(gfx::Rect(0, 0, 210000, 297000), caps.papers[0].printable_area_um);

  printer_->SetMediaColDatabase(
      MakeMediaColDatabase(ipp_, {
                                     {21000, 29700, 0, 0, 0, 0, {}},
                                     {21000, 29700, 100, 100, 100, 100, {}},
                                 }));
  CapsAndDefaultsFromPrinter(*printer_, &caps);
  ASSERT_EQ(1U, caps.papers.size());
  EXPECT_NE(gfx::Rect(0, 0, 210000, 297000), caps.papers[0].printable_area_um);

  // If the only available version of a size is borderless, go ahead and use it.
  // Not sure if any actual printers do this, but it's allowed by the IPP spec.
  printer_->SetMediaColDatabase(
      MakeMediaColDatabase(ipp_, {
                                     {21000, 29700, 0, 0, 0, 0, {}},
                                 }));
  CapsAndDefaultsFromPrinter(*printer_, &caps);
  ASSERT_EQ(1U, caps.papers.size());
  EXPECT_EQ(gfx::Rect(0, 0, 210000, 297000), caps.papers[0].printable_area_um);
}

// At the time of this writing, there are no media-source or media-type
// attributes in the media-col-database that cupsd gives us. However, according
// to the IPP spec, each paper size *should* have a separate variant for each
// supported combination of size and type. So make sure behavior doesn't change
// and we don't create duplicate paper sizes when/if CUPS improves in the
// future.
TEST_F(PrintBackendCupsIppHelperTest, NoDuplicateSizes) {
  printer_->SetMediaColDatabase(MakeMediaColDatabase(
      ipp_,
      {
          {21000,
           29700,
           300,
           300,
           300,
           300,
           {{"media-type", "stationery"}, {"media-source", "main"}}},
          {21000,
           29700,
           300,
           300,
           300,
           300,
           {{"media-type", "stationery"}, {"media-source", "main"}}},
          {21000,
           29700,
           500,
           500,
           500,
           500,
           {{"media-type", "stationery"}, {"media-source", "main"}}},
          {21000,
           29700,
           300,
           300,
           300,
           300,
           {{"media-type", "photographic"}, {"media-source", "main"}}},
          {21000,
           29700,
           0,
           0,
           0,
           0,
           {{"media-type", "photographic"}, {"media-source", "main"}}},
          {21000,
           29700,
           300,
           300,
           300,
           300,
           {{"media-type", "photographic-high-gloss"},
            {"media-source", "main"}}},
          {21000,
           29700,
           0,
           0,
           0,
           0,
           {{"media-type", "photographic-high-gloss"},
            {"media-source", "main"}}},
          {21000,
           29700,
           300,
           300,
           300,
           300,
           {{"media-type", "photographic-glossy"}, {"media-source", "main"}}},
          {21000,
           29700,
           0,
           0,
           0,
           0,
           {{"media-type", "photographic-glossy"}, {"media-source", "main"}}},
          {21000,
           29700,
           300,
           300,
           300,
           300,
           {{"media-type", "photographic-semi-gloss"},
            {"media-source", "main"}}},
          {21000,
           29700,
           0,
           0,
           0,
           0,
           {{"media-type", "photographic-semi-gloss"},
            {"media-source", "main"}}},
          {21000,
           29700,
           300,
           300,
           300,
           300,
           {{"media-type", "photographic-matte"}, {"media-source", "main"}}},
          {21000,
           29700,
           0,
           0,
           0,
           0,
           {{"media-type", "photographic-matte"}, {"media-source", "main"}}},
      }));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  ASSERT_EQ(1U, caps.papers.size());
}

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(PrintBackendCupsIppHelperTest, PinSupported) {
  printer_->SetSupportedOptions("job-password", MakeInteger(ipp_, 4));
  printer_->SetSupportedOptions("job-password-encryption",
                                MakeStringCollection(ipp_, {"none"}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_TRUE(caps.pin_supported);
}

TEST_F(PrintBackendCupsIppHelperTest, PinNotSupported) {
  // Pin support missing, no setup.
  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_FALSE(caps.pin_supported);
}

TEST_F(PrintBackendCupsIppHelperTest, PinEncryptionNotSupported) {
  printer_->SetSupportedOptions("job-password", MakeInteger(ipp_, 4));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_FALSE(caps.pin_supported);
}

TEST_F(PrintBackendCupsIppHelperTest, PinTooShort) {
  printer_->SetSupportedOptions("job-password", MakeInteger(ipp_, 3));
  printer_->SetSupportedOptions("job-password-encryption",
                                MakeStringCollection(ipp_, {"none"}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_FALSE(caps.pin_supported);
}

TEST_F(PrintBackendCupsIppHelperTest, AdvancedCaps) {
  base::HistogramTester histograms;

  printer_->SetSupportedOptions(
      "job-creation-attributes",
      MakeStringCollection(
          ipp_, {"copies", "confirmation-sheet-print", "finishings",
                 "job-message-to-operator", "output-bin", "print-quality"}));
  printer_->SetSupportedOptions("finishings",
                                MakeIntCollection(ipp_, {3, 7, 10}));
  printer_->SetSupportedOptions(
      "output-bin", MakeStringCollection(ipp_, {"face-down", "face-up"}));
  printer_->SetSupportedOptions("print-quality",
                                MakeIntCollection(ipp_, {3, 4, 5}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  ASSERT_EQ(6u, caps.advanced_capabilities.size());
  EXPECT_EQ("confirmation-sheet-print", caps.advanced_capabilities[0].name);
  EXPECT_EQ(AdvancedCapability::Type::kBoolean,
            caps.advanced_capabilities[0].type);
  EXPECT_EQ("finishings/7", caps.advanced_capabilities[1].name);
  EXPECT_EQ(AdvancedCapability::Type::kBoolean,
            caps.advanced_capabilities[1].type);
  EXPECT_EQ("finishings/10", caps.advanced_capabilities[2].name);
  EXPECT_EQ(AdvancedCapability::Type::kBoolean,
            caps.advanced_capabilities[2].type);
  EXPECT_EQ("job-message-to-operator", caps.advanced_capabilities[3].name);
  EXPECT_EQ(AdvancedCapability::Type::kString,
            caps.advanced_capabilities[3].type);
  EXPECT_EQ("output-bin", caps.advanced_capabilities[4].name);
  EXPECT_EQ(AdvancedCapability::Type::kString,
            caps.advanced_capabilities[4].type);
  EXPECT_EQ(2u, caps.advanced_capabilities[4].values.size());
  EXPECT_EQ("print-quality", caps.advanced_capabilities[5].name);
  EXPECT_EQ(AdvancedCapability::Type::kString,
            caps.advanced_capabilities[5].type);
  EXPECT_EQ(3u, caps.advanced_capabilities[5].values.size());
  histograms.ExpectUniqueSample("Printing.CUPS.IppAttributesCount", 5, 1);
}

TEST_F(PrintBackendCupsIppHelperTest, MediaSource) {
  printer_->SetSupportedOptions(
      "media-source",
      MakeStringCollection(ipp_, {"top", "main", "auto", "tray-3", "tray-4"}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  ASSERT_EQ(1u, caps.advanced_capabilities.size());
  const AdvancedCapability& cap = caps.advanced_capabilities[0];
  EXPECT_EQ("media-source", cap.name);
  EXPECT_EQ(AdvancedCapability::Type::kString, cap.type);
  EXPECT_THAT(cap.values,
              Pointwise(AdvancedCapabilityName(),
                        {"top", "main", "auto", "tray-3", "tray-4"}));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace printing
