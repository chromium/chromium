// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/cups_ipp_helper.h"

#include <cups/cups.h>

#include <map>
#include <memory>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "printing/backend/cups_ipp_constants.h"
#include "printing/backend/mock_cups_printer.h"
#include "printing/backend/print_backend_utils.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

using ::testing::Pointwise;
using ::testing::UnorderedPointwise;

// Matches the name field to a string.
MATCHER(AdvancedCapabilityName, "") {
  *result_listener << "Expected: " << std::get<1>(arg)
                   << " vs Actual: " << std::get<0>(arg).name;
  return std::get<0>(arg).name == std::get<1>(arg);
}

// Compares an ipp_t* db entry from a media-col-database to a media_info
// object.
MATCHER(EqualsMediaColEntry, "") {
  return MediaColDbEntryEquals(std::get<0>(arg), std::get<1>(arg));
}

class MockCupsPrinterWithMarginsAndAttributes : public MockCupsPrinter {
 public:
  // name and value of IPP attribute; needed to fetch localized display name
  using LocalizationKey = std::pair<std::string_view, std::string_view>;

  MockCupsPrinterWithMarginsAndAttributes() = default;
  ~MockCupsPrinterWithMarginsAndAttributes() override = default;

  // CupsOptionProvider:
  ipp_attribute_t* GetSupportedOptionValues(
      const char* option_name) const override {
    const auto attr = supported_attributes_.find(option_name);
    return attr != supported_attributes_.end() ? attr->second : nullptr;
  }

  // CupsOptionProvider:
  std::vector<std::string_view> GetSupportedOptionValueStrings(
      const char* option_name) const override {
    ipp_attribute_t* attr = GetSupportedOptionValues(option_name);
    if (!attr)
      return std::vector<std::string_view>();

    std::vector<std::string_view> strings;
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
  }

  const char* GetLocalizedOptionValueName(const char* option_name,
                                          const char* value) const override {
    LocalizationKey key = {option_name, value};
    auto localized_name = localized_strings_.find(key);
    if (localized_name == localized_strings_.end()) {
      return nullptr;
    }
    return localized_name->second.c_str();
  }

  void SetSupportedOptions(std::string_view name, ipp_attribute_t* attribute) {
    supported_attributes_[name] = attribute;
  }

  void SetOptionDefault(std::string_view name, ipp_attribute_t* attribute) {
    default_attributes_[name] = attribute;
  }

  void SetLocalizedOptionValueNames(
      std::map<LocalizationKey, std::string> strings) {
    localized_strings_ = std::move(strings);
  }

  void SetMediaColDatabase(ipp_attribute_t* attribute) {
    media_col_database_ = attribute;
  }

 private:
  std::map<std::string_view, raw_ptr<ipp_attribute_t, CtnExperimental>>
      supported_attributes_;
  std::map<std::string_view, raw_ptr<ipp_attribute_t, CtnExperimental>>
      default_attributes_;
  std::map<LocalizationKey, std::string> localized_strings_;
  raw_ptr<ipp_attribute_t, DanglingUntriaged> media_col_database_;
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

  raw_ptr<ipp_t, DanglingUntriaged> ipp_;
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
  bool is_width_range;
  int width_max;
  bool is_height_range;
  int height_max;
};

// Return true if `db_entry` matches the data specified in `info`
// (`keyword_attrs` are not checked).
bool MediaColDbEntryEquals(ipp_t* db_entry, media_info info) {
  if (!db_entry) {
    return false;
  }
  ipp_t* media_size = ippGetCollection(
      ippFindAttribute(db_entry, kIppMediaSize, IPP_TAG_BEGIN_COLLECTION), 0);
  if (!media_size) {
    return false;
  }

  ipp_attribute_t* bottom_attr =
      ippFindAttribute(db_entry, kIppMediaBottomMargin, IPP_TAG_INTEGER);
  ipp_attribute_t* left_attr =
      ippFindAttribute(db_entry, kIppMediaLeftMargin, IPP_TAG_INTEGER);
  ipp_attribute_t* right_attr =
      ippFindAttribute(db_entry, kIppMediaRightMargin, IPP_TAG_INTEGER);
  ipp_attribute_t* top_attr =
      ippFindAttribute(db_entry, kIppMediaTopMargin, IPP_TAG_INTEGER);
  if (!bottom_attr || !left_attr || !right_attr || !top_attr) {
    return false;
  }
  if (ippGetInteger(bottom_attr, 0) != info.bottom_margin ||
      ippGetInteger(left_attr, 0) != info.left_margin ||
      ippGetInteger(right_attr, 0) != info.right_margin ||
      ippGetInteger(top_attr, 0) != info.top_margin) {
    return false;
  }

  if (info.is_width_range) {
    ipp_attribute_t* width_range_attr =
        ippFindAttribute(media_size, kIppXDimension, IPP_TAG_RANGE);
    if (!width_range_attr) {
      return false;
    }
    int max_width = 0;
    int width = ippGetRange(width_range_attr, 0, &max_width);
    if (width != info.width || max_width != info.width_max) {
      return false;
    }
  } else {
    ipp_attribute_t* width_attr =
        ippFindAttribute(media_size, kIppXDimension, IPP_TAG_INTEGER);
    if (!width_attr) {
      return false;
    }
    int width = ippGetInteger(width_attr, 0);
    if (width != info.width) {
      return false;
    }
  }

  if (info.is_height_range) {
    ipp_attribute_t* height_range_attr =
        ippFindAttribute(media_size, kIppYDimension, IPP_TAG_RANGE);
    if (!height_range_attr) {
      return false;
    }
    int max_height = 0;
    int height = ippGetRange(height_range_attr, 0, &max_height);
    if (height != info.height || max_height != info.height_max) {
      return false;
    }
  } else {
    ipp_attribute_t* height_attr =
        ippFindAttribute(media_size, kIppYDimension, IPP_TAG_INTEGER);
    if (!height_attr) {
      return false;
    }
    int height = ippGetInteger(height_attr, 0);
    if (height != info.height) {
      return false;
    }
  }

  return true;
}

// Returns a vector with pointers to all the entries in `media_col_db`.
// `media_col_db` maintains ownership of the returned values.
std::vector<ipp_t*> GetMediaColEntries(ipp_attribute_t* media_col_db) {
  if (!media_col_db) {
    return std::vector<ipp_t*>();
  }

  std::vector<ipp_t*> retval;
  for (int i = 0; i < ippGetCount(media_col_db); i++) {
    retval.push_back(ippGetCollection(media_col_db, i));
  }

  return retval;
}

ScopedIppPtr MakeMediaCol(const media_info& info) {
  ScopedIppPtr media_col = WrapIpp(ippNew());
  ScopedIppPtr media_size = WrapIpp(ippNew());

  if (info.is_width_range) {
    ippAddRange(media_size.get(), IPP_TAG_ZERO, "x-dimension", info.width,
                info.width_max);
  } else {
    ippAddInteger(media_size.get(), IPP_TAG_ZERO, IPP_TAG_INTEGER,
                  "x-dimension", info.width);
  }
  if (info.is_height_range) {
    ippAddRange(media_size.get(), IPP_TAG_ZERO, "y-dimension", info.height,
                info.height_max);
  } else {
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

  return ippAddCollections(ipp, IPP_TAG_PRINTER, kIppMediaColDatabase,
                           raw_collections.size(), raw_collections.data());
}

TEST_F(PrintBackendCupsIppHelperTest, DefaultPaper) {
  EXPECT_EQ(PrinterSemanticCapsAndDefaults::Paper(), DefaultPaper(*printer_));
  printer_->SetOptionDefault(
      "media-col",
      MakeMediaColDefault(ipp_, {21000, 29700, 10, 10, 10, 10, {}}));
  PrinterSemanticCapsAndDefaults::Paper default_paper = DefaultPaper(*printer_);
  EXPECT_EQ(default_paper.size_um().width(), 210000);
  EXPECT_EQ(default_paper.size_um().height(), 297000);
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

TEST_F(PrintBackendCupsIppHelperTest, MediaTypes) {
  printer_->SetSupportedOptions(
      "media-type",
      MakeStringCollection(
          ipp_, {"stationery", "custom-1", "photographic-glossy", "custom-2"}));
  printer_->SetOptionDefault("media-type", MakeString(ipp_, "stationery"));

  // set mock display names that would be read from the printer's strings file
  // (printer-strings-uri)
  printer_->SetLocalizedOptionValueNames({
      {{"media-type", "stationery"}, "Plain Paper"},
      {{"media-type", "custom-2"}, "Custom Two"},
  });

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_EQ(caps.default_media_type.vendor_id, "stationery");
  ASSERT_EQ(caps.media_types.size(), 4U);
  EXPECT_EQ(caps.media_types[0].vendor_id, "stationery");
  EXPECT_EQ(caps.media_types[0].display_name, "Plain Paper");
  EXPECT_EQ(caps.media_types[1].vendor_id, "custom-1");
  EXPECT_EQ(caps.media_types[1].display_name, "custom-1");
  EXPECT_EQ(caps.media_types[2].vendor_id, "photographic-glossy");
  EXPECT_EQ(caps.media_types[2].display_name, "photographic-glossy");
  EXPECT_EQ(caps.media_types[3].vendor_id, "custom-2");
  EXPECT_EQ(caps.media_types[3].display_name, "Custom Two");
}

TEST_F(PrintBackendCupsIppHelperTest, DefaultMediaTypeNotSupported) {
  printer_->SetSupportedOptions(
      "media-type",
      MakeStringCollection(ipp_, {"stationery", "photographic-glossy"}));
  printer_->SetOptionDefault("media-type",
                             MakeString(ipp_, "not-actually-supported"));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_EQ(caps.default_media_type.vendor_id, "stationery");
  ASSERT_EQ(caps.media_types.size(), 2U);
  EXPECT_EQ(caps.media_types[0].vendor_id, "stationery");
  EXPECT_EQ(caps.media_types[1].vendor_id, "photographic-glossy");
}

TEST_F(PrintBackendCupsIppHelperTest, A4PaperSupported) {
  printer_->SetMediaColDatabase(
      MakeMediaColDatabase(ipp_, {{21000, 29700, 10, 10, 10, 10, {}}}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  ASSERT_EQ(1U, caps.papers.size());

  PrinterSemanticCapsAndDefaults::Paper paper = caps.papers[0];
  EXPECT_EQ(210000, paper.size_um().width());
  EXPECT_EQ(297000, paper.size_um().height());
}

#if BUILDFLAG(IS_MAC)
TEST_F(PrintBackendCupsIppHelperTest, NearA4PaperDetected) {
  printer_->SetMediaColDatabase(
      MakeMediaColDatabase(ipp_, {{20990, 29704, 10, 10, 10, 10, {}}}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  ASSERT_EQ(1U, caps.papers.size());

  PrinterSemanticCapsAndDefaults::Paper paper = caps.papers[0];
  EXPECT_EQ(210000, paper.size_um().width());
  EXPECT_EQ(297000, paper.size_um().height());
}

TEST_F(PrintBackendCupsIppHelperTest, NonStandardPaperUnchanged) {
  printer_->SetMediaColDatabase(
      MakeMediaColDatabase(ipp_, {{20800, 29500, 10, 10, 10, 10, {}}}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  ASSERT_EQ(1U, caps.papers.size());

  PrinterSemanticCapsAndDefaults::Paper paper = caps.papers[0];
  EXPECT_EQ(208000, paper.size_um().width());
  EXPECT_EQ(295000, paper.size_um().height());
}
#endif  // BUILDFLAG(IS_MAC)

TEST_F(PrintBackendCupsIppHelperTest, CustomPaperSupported) {
  printer_->SetMediaColDatabase(MakeMediaColDatabase(
      ipp_, {{8000, 2540, 10, 10, 10, 10, {}, false, 0, true, 254000}}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  ASSERT_EQ(1U, caps.papers.size());

  PrinterSemanticCapsAndDefaults::Paper paper = caps.papers[0];
  EXPECT_EQ(80000, paper.size_um().width());
  EXPECT_EQ(25400, paper.size_um().height());
  EXPECT_EQ(2540000, paper.max_height_um());
}

TEST_F(PrintBackendCupsIppHelperTest, CustomPaperWithZeroMinHeight) {
  printer_->SetMediaColDatabase(MakeMediaColDatabase(
      ipp_, {{8000, 0, 0, 0, 0, 0, {}, false, 8000, true, 254000}}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  ASSERT_EQ(1U, caps.papers.size());

  // Zero-height paper is not allowed.  However, this paper will get used but
  // the height will get changed into some small, non-zero value.
  PrinterSemanticCapsAndDefaults::Paper paper = caps.papers[0];
  EXPECT_EQ(80000, paper.size_um().width());
  EXPECT_EQ(2540000, paper.max_height_um());
  EXPECT_TRUE(paper.size_um().height() > 0);
}

TEST_F(PrintBackendCupsIppHelperTest, CustomPaperWithInvalidHeight) {
  // Max height is less than min height, which is not allowed.
  printer_->SetMediaColDatabase(MakeMediaColDatabase(
      ipp_, {{2000, 2540, 0, 0, 0, 0, {}, true, 8000, true, 1000}}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  ASSERT_EQ(0U, caps.papers.size());
}

TEST_F(PrintBackendCupsIppHelperTest, CustomPaperWithInvalidWidth) {
  // Varible-width pages are not supported.
  printer_->SetMediaColDatabase(MakeMediaColDatabase(
      ipp_, {{2000, 2540, 0, 0, 0, 0, {}, true, 8000, true, 254000}}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  ASSERT_EQ(0U, caps.papers.size());
}

TEST_F(PrintBackendCupsIppHelperTest, LegalPaperDefault) {
  // na_legal_8.5x14in
  printer_->SetOptionDefault(
      "media-col",
      MakeMediaColDefault(ipp_, {21590, 35560, 10, 10, 10, 10, {}}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);
  EXPECT_EQ(215900, caps.default_paper.size_um().width());
  EXPECT_EQ(355600, caps.default_paper.size_um().height());
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
    EXPECT_NE(21000, paper.size_um().width());
    EXPECT_NE(29700, paper.size_um().height());
  }
}

// Tests that CapsAndDefaultsFromPrinter() will propagate custom size ranges
// from the  the media-col-database to the Chromium print backend.
TEST_F(PrintBackendCupsIppHelperTest, IncludePapersWithSizeRanges) {
  printer_->SetMediaColDatabase(MakeMediaColDatabase(
      ipp_, {
                {11430, 26352, 100, 100, 100, 100, {}},
                {8000, 2540, 100, 100, 100, 100, {}, false, 0, true, 2540000},
                {20320, 25400, 100, 100, 100, 100, {}},
                {100000, 141400, 100, 100, 100, 100, {}},
            }));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  // The printer reports that it supports four media sizes, one of which
  // contains a custom range.  All four of these should be supported.
  ASSERT_EQ(4U, caps.papers.size());
}

// Tests that when the media-col-database contains both bordered and borderless
// versions of a size, CapsAndDefaultsFromPrinter() takes the bordered version
// and marks it as having a borderless variant.
TEST_F(PrintBackendCupsIppHelperTest, HandleBorderlessVariants) {
  PrinterSemanticCapsAndDefaults caps;

  printer_->SetMediaColDatabase(
      MakeMediaColDatabase(ipp_, {
                                     {21000, 29700, 100, 100, 100, 100, {}},
                                     {21000, 29700, 0, 0, 0, 0, {}},
                                 }));
  CapsAndDefaultsFromPrinter(*printer_, &caps);
  ASSERT_EQ(1U, caps.papers.size());
  EXPECT_NE(gfx::Rect(0, 0, 210000, 297000),
            caps.papers[0].printable_area_um());
  EXPECT_TRUE(caps.papers[0].has_borderless_variant());

  printer_->SetMediaColDatabase(
      MakeMediaColDatabase(ipp_, {
                                     {21000, 29700, 0, 0, 0, 0, {}},
                                     {21000, 29700, 100, 100, 100, 100, {}},
                                 }));
  CapsAndDefaultsFromPrinter(*printer_, &caps);
  ASSERT_EQ(1U, caps.papers.size());
  EXPECT_NE(gfx::Rect(0, 0, 210000, 297000),
            caps.papers[0].printable_area_um());
  EXPECT_TRUE(caps.papers[0].has_borderless_variant());

  // If the only available version of a size is borderless, go ahead and use it.
  // Not sure if any actual printers do this, but it's allowed by the IPP spec.
  printer_->SetMediaColDatabase(
      MakeMediaColDatabase(ipp_, {
                                     {21000, 29700, 0, 0, 0, 0, {}},
                                 }));
  CapsAndDefaultsFromPrinter(*printer_, &caps);
  ASSERT_EQ(1U, caps.papers.size());
  EXPECT_EQ(gfx::Rect(0, 0, 210000, 297000),
            caps.papers[0].printable_area_um());
  EXPECT_FALSE(caps.papers[0].has_borderless_variant());

  // If the only available versions of a size are bordered, there shouldn't be
  // a borderless variant.
  printer_->SetMediaColDatabase(
      MakeMediaColDatabase(ipp_, {
                                     {21000, 29700, 100, 100, 100, 100, {}},
                                     {21000, 29700, 200, 200, 200, 200, {}},
                                 }));
  CapsAndDefaultsFromPrinter(*printer_, &caps);
  ASSERT_EQ(1U, caps.papers.size());
  EXPECT_EQ(gfx::Rect(1000, 1000, 210000 - 1000 - 1000, 297000 - 1000 - 1000),
            caps.papers[0].printable_area_um());
  EXPECT_FALSE(caps.papers[0].has_borderless_variant());
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

TEST_F(PrintBackendCupsIppHelperTest, FilterMediaColSizesNominal) {
  // Create db with no variable-size entries.  All 4 should be retained after
  // filtering.
  ScopedIppPtr ipp = WrapIpp(ippNew());
  MakeMediaColDatabase(ipp.get(), {
                                      {5800, 20000},
                                      {5800, 200000},
                                      {8000, 20000},
                                      {8000, 200000},
                                  });

  FilterMediaColSizes(ipp);

  ipp_attribute_t* media_col_db = ippFindAttribute(
      ipp.get(), kIppMediaColDatabase, IPP_TAG_BEGIN_COLLECTION);

  ASSERT_TRUE(media_col_db);
  EXPECT_THAT(
      GetMediaColEntries(media_col_db),
      UnorderedPointwise(EqualsMediaColEntry(),
                         {media_info{5800, 20000}, media_info{5800, 200000},
                          media_info{8000, 20000}, media_info{8000, 200000}}));
}

TEST_F(PrintBackendCupsIppHelperTest,
       FilterMediaColSizesVariableWidthAndHeight) {
  // This db has one variable-sized entry and 2 fixed widths, so the
  // variable-sized entry should get replaced by two variable-sized height
  // entries with the fixed widths.
  ScopedIppPtr ipp = WrapIpp(ippNew());
  MakeMediaColDatabase(
      ipp.get(), {
                     {5800, 20000},
                     {5800, 200000},
                     {8000, 20000},
                     {8000, 200000},
                     {2540, 2540, 0, 0, 0, 0, {}, true, 8000, true, 200000},
                 });

  FilterMediaColSizes(ipp);
  ipp_attribute_t* media_col_db = ippFindAttribute(
      ipp.get(), kIppMediaColDatabase, IPP_TAG_BEGIN_COLLECTION);
  ASSERT_TRUE(media_col_db);
  EXPECT_THAT(
      GetMediaColEntries(media_col_db),
      UnorderedPointwise(
          EqualsMediaColEntry(),
          {media_info{5800, 20000}, media_info{5800, 200000},
           media_info{8000, 20000}, media_info{8000, 200000},
           media_info{5800, 2540, 0, 0, 0, 0, {}, false, 0, true, 200000},
           media_info{8000, 2540, 0, 0, 0, 0, {}, false, 0, true, 200000}}));
}

TEST_F(PrintBackendCupsIppHelperTest, FilterMediaColSizesVariableWidth) {
  // Variable width entries (with fixed height) will get filtered out.
  ScopedIppPtr ipp = WrapIpp(ippNew());
  MakeMediaColDatabase(ipp.get(),
                       {
                           {5800, 20000},
                           {5800, 200000},
                           {8000, 20000},
                           {8000, 200000},
                           {7000, 20000, 0, 0, 0, 0, {}, true, 8000, false, 0},
                       });

  FilterMediaColSizes(ipp);
  ipp_attribute_t* media_col_db = ippFindAttribute(
      ipp.get(), kIppMediaColDatabase, IPP_TAG_BEGIN_COLLECTION);
  ASSERT_TRUE(media_col_db);
  EXPECT_THAT(
      GetMediaColEntries(media_col_db),
      UnorderedPointwise(EqualsMediaColEntry(),
                         {media_info{5800, 20000}, media_info{5800, 200000},
                          media_info{8000, 20000}, media_info{8000, 200000}}));
}

TEST_F(PrintBackendCupsIppHelperTest, FilterMediaColSizesVariableHeight) {
  // Entry with fixed width and variable height should get retained after
  // filtering.
  ScopedIppPtr ipp = WrapIpp(ippNew());
  MakeMediaColDatabase(ipp.get(),
                       {
                           {5800, 20000},
                           {5800, 200000},
                           {8000, 20000},
                           {8000, 200000},
                           {8000, 2540, 0, 0, 0, 0, {}, false, 0, true, 200000},
                       });

  FilterMediaColSizes(ipp);
  ipp_attribute_t* media_col_db = ippFindAttribute(
      ipp.get(), kIppMediaColDatabase, IPP_TAG_BEGIN_COLLECTION);
  ASSERT_TRUE(media_col_db);
  EXPECT_THAT(
      GetMediaColEntries(media_col_db),
      UnorderedPointwise(
          EqualsMediaColEntry(),
          {media_info{5800, 20000}, media_info{5800, 200000},
           media_info{8000, 20000}, media_info{8000, 200000},
           media_info{8000, 2540, 0, 0, 0, 0, {}, false, 0, true, 200000}}));
}

TEST_F(PrintBackendCupsIppHelperTest, FilterMediaColSizesSameVariableHeight) {
  // Entry with a variable height with the min and max equal to each other just
  // ends up being a fixed height entry.
  ScopedIppPtr ipp = WrapIpp(ippNew());
  MakeMediaColDatabase(ipp.get(),
                       {
                           {8000, 20000, 0, 0, 0, 0, {}, false, 0, true, 20000},
                       });

  FilterMediaColSizes(ipp);
  ipp_attribute_t* media_col_db = ippFindAttribute(
      ipp.get(), kIppMediaColDatabase, IPP_TAG_BEGIN_COLLECTION);
  ASSERT_TRUE(media_col_db);
  EXPECT_THAT(
      GetMediaColEntries(media_col_db),
      UnorderedPointwise(EqualsMediaColEntry(), {media_info{8000, 20000}}));
}

TEST_F(PrintBackendCupsIppHelperTest, FilterMediaColSizesSingleVariableEntry) {
  // Since there are no fixed widths, this will get filtered out.  Consequently,
  // the media-col-database entry won't even exist.
  ScopedIppPtr ipp = WrapIpp(ippNew());
  MakeMediaColDatabase(
      ipp.get(), {
                     {2540, 2540, 0, 0, 0, 0, {}, true, 8000, true, 200000},
                 });

  FilterMediaColSizes(ipp);
  ipp_attribute_t* media_col_db = ippFindAttribute(
      ipp.get(), kIppMediaColDatabase, IPP_TAG_BEGIN_COLLECTION);
  ASSERT_FALSE(media_col_db);
}

TEST_F(PrintBackendCupsIppHelperTest, FilterMediaColSizesInvalidWidth) {
  // This db has one variable-sized entry which has a minimum width that only
  // matches one of the fixed width sizes, so only one variable entry will be
  // retained.
  ScopedIppPtr ipp = WrapIpp(ippNew());
  MakeMediaColDatabase(
      ipp.get(), {
                     {5800, 20000},
                     {5800, 200000},
                     {8000, 20000},
                     {8000, 200000},
                     {7000, 2540, 0, 0, 0, 0, {}, true, 8000, true, 200000},
                 });

  FilterMediaColSizes(ipp);
  ipp_attribute_t* media_col_db = ippFindAttribute(
      ipp.get(), kIppMediaColDatabase, IPP_TAG_BEGIN_COLLECTION);
  ASSERT_TRUE(media_col_db);
  EXPECT_THAT(
      GetMediaColEntries(media_col_db),
      UnorderedPointwise(
          EqualsMediaColEntry(),
          {media_info{5800, 20000}, media_info{5800, 200000},
           media_info{8000, 20000}, media_info{8000, 200000},
           media_info{8000, 2540, 0, 0, 0, 0, {}, false, 0, true, 200000}}));
}

TEST_F(PrintBackendCupsIppHelperTest,
       FilterMediaColSizesMultipleVariableEntries) {
  // Multiple variable entries - non-overlapping.
  ScopedIppPtr ipp = WrapIpp(ippNew());
  MakeMediaColDatabase(
      ipp.get(), {
                     {5800, 20000},
                     {5800, 200000},
                     {8000, 20000},
                     {8000, 200000},
                     {5000, 2540, 0, 0, 0, 0, {}, true, 6000, true, 100000},
                     {7000, 2540, 0, 0, 0, 0, {}, true, 8000, true, 200000},
                 });

  FilterMediaColSizes(ipp);
  ipp_attribute_t* media_col_db = ippFindAttribute(
      ipp.get(), kIppMediaColDatabase, IPP_TAG_BEGIN_COLLECTION);
  ASSERT_TRUE(media_col_db);
  EXPECT_THAT(
      GetMediaColEntries(media_col_db),
      UnorderedPointwise(
          EqualsMediaColEntry(),
          {media_info{5800, 20000}, media_info{5800, 200000},
           media_info{8000, 20000}, media_info{8000, 200000},
           media_info{5800, 2540, 0, 0, 0, 0, {}, false, 0, true, 100000},
           media_info{8000, 2540, 0, 0, 0, 0, {}, false, 0, true, 200000}}));
}

TEST_F(PrintBackendCupsIppHelperTest,
       FilterMediaColSizesMultipleVariableEntriesOverlap) {
  // Multiple variable entries - overlapping.
  ScopedIppPtr ipp = WrapIpp(ippNew());
  MakeMediaColDatabase(
      ipp.get(), {
                     {5800, 20000},
                     {5800, 200000},
                     {8000, 20000},
                     {8000, 200000},
                     {2540, 4000, 0, 0, 0, 0, {}, true, 8000, true, 100000},
                     {2540, 5000, 0, 0, 0, 0, {}, true, 8000, true, 200000},
                 });

  FilterMediaColSizes(ipp);
  ipp_attribute_t* media_col_db = ippFindAttribute(
      ipp.get(), kIppMediaColDatabase, IPP_TAG_BEGIN_COLLECTION);
  ASSERT_TRUE(media_col_db);
  EXPECT_THAT(
      GetMediaColEntries(media_col_db),
      UnorderedPointwise(
          EqualsMediaColEntry(),
          {media_info{5800, 20000}, media_info{5800, 200000},
           media_info{8000, 20000}, media_info{8000, 200000},
           media_info{5800, 4000, 0, 0, 0, 0, {}, false, 0, true, 100000},
           media_info{8000, 4000, 0, 0, 0, 0, {}, false, 0, true, 100000},
           media_info{5800, 5000, 0, 0, 0, 0, {}, false, 0, true, 200000},
           media_info{8000, 5000, 0, 0, 0, 0, {}, false, 0, true, 200000}}));
}

TEST_F(PrintBackendCupsIppHelperTest,
       FilterMediaColSizesMultipleVariableEntriesOverlapOutsideRange) {
  // Multiple variable entries - overlapping.  The second variable-length entry
  // will get rejected because there is no fixed width in bounds.
  ScopedIppPtr ipp = WrapIpp(ippNew());
  MakeMediaColDatabase(
      ipp.get(), {
                     {5800, 20000},
                     {2540, 4000, 0, 0, 0, 0, {}, true, 8000, true, 100000},
                     {2540, 5000, 0, 0, 0, 0, {}, true, 3000, true, 200000},
                 });

  FilterMediaColSizes(ipp);
  ipp_attribute_t* media_col_db = ippFindAttribute(
      ipp.get(), kIppMediaColDatabase, IPP_TAG_BEGIN_COLLECTION);
  ASSERT_TRUE(media_col_db);
  EXPECT_THAT(
      GetMediaColEntries(media_col_db),
      UnorderedPointwise(
          EqualsMediaColEntry(),
          {media_info{5800, 20000},
           media_info{5800, 4000, 0, 0, 0, 0, {}, false, 0, true, 100000}}));
}

TEST_F(PrintBackendCupsIppHelperTest,
       OverrideUnavailableCanonDefaultMediaType) {
  printer_->SetSupportedOptions(
      "media-type",
      MakeStringCollection(ipp_, {"com.canon.unavailable", "stationery"}));
  printer_->SetOptionDefault("media-type",
                             MakeString(ipp_, "com.canon.unavailable"));

  printer_->SetLocalizedOptionValueNames({
      {{"media-type", "com.canon.unavailable"}, "Unavailable Media"},
      {{"media-type", "stationery"}, "Plain Paper"},
  });

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);
  EXPECT_EQ(caps.default_media_type.vendor_id, "stationery");
}

TEST_F(PrintBackendCupsIppHelperTest,
       OverrideUnavailableCanonDefaultMediaTypeStationeryUnavailable) {
  printer_->SetSupportedOptions(
      "media-type",
      MakeStringCollection(ipp_, {"com.canon.unavailable", "not.stationery"}));
  printer_->SetOptionDefault("media-type",
                             MakeString(ipp_, "com.canon.unavailable"));

  printer_->SetLocalizedOptionValueNames({
      {{"media-type", "com.canon.unavailable"}, "Unavailable Media"},
      {{"media-type", "not.stationery"}, "Not Plain Paper"},
  });

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);
  EXPECT_EQ(caps.default_media_type.vendor_id, "com.canon.unavailable");
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
