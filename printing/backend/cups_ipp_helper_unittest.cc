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
#include "printing/backend/cups_printer.h"
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

class MockCupsOptionProvider : public CupsOptionProvider {
 public:
  ~MockCupsOptionProvider() override = default;

  ipp_attribute_t* GetSupportedOptionValues(
      const char* option_name) const override {
    const auto attr = supported_attributes_.find(option_name);
    return attr != supported_attributes_.end() ? attr->second : nullptr;
  }

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

  ipp_attribute_t* GetDefaultOptionValue(
      const char* option_name) const override {
    const auto attr = default_attributes_.find(option_name);
    return attr != default_attributes_.end() ? attr->second : nullptr;
  }

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

 private:
  std::map<base::StringPiece, ipp_attribute_t*> supported_attributes_;
  std::map<base::StringPiece, ipp_attribute_t*> default_attributes_;
};

class PrintBackendCupsIppHelperTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ipp_ = ippNew();
    printer_ = std::make_unique<MockCupsOptionProvider>();
  }

  void TearDown() override {
    ippDelete(ipp_);
    printer_.reset();
  }

  raw_ptr<ipp_t> ipp_;
  std::unique_ptr<MockCupsOptionProvider> printer_;
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

TEST_F(PrintBackendCupsIppHelperTest, DefaultPaper) {
  EXPECT_EQ(ParsePaper(""), DefaultPaper(*printer_));
  printer_->SetOptionDefault("media", MakeString(ipp_, "iso_a4_210x297mm"));
  EXPECT_EQ(ParsePaper("iso_a4_210x297mm"), DefaultPaper(*printer_));
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
  printer_->SetSupportedOptions(
      "media", MakeStringCollection(ipp_, {"iso_a4_210x297mm"}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  PrinterSemanticCapsAndDefaults::Paper paper = caps.papers[0];
  // media display name localization is handled more fully in
  // AssemblePrinterSettings().
  EXPECT_EQ("iso a4", paper.display_name);
  EXPECT_EQ("iso_a4_210x297mm", paper.vendor_id);
  EXPECT_EQ(210000, paper.size_um.width());
  EXPECT_EQ(297000, paper.size_um.height());
}

TEST_F(PrintBackendCupsIppHelperTest, LegalPaperDefault) {
  printer_->SetOptionDefault("media", MakeString(ipp_, "na_legal_8.5x14in"));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);
  // media display name localization is handled more fully in
  // AssemblePrinterSettings().
  EXPECT_EQ("na legal", caps.default_paper.display_name);
  EXPECT_EQ("na_legal_8.5x14in", caps.default_paper.vendor_id);
  EXPECT_EQ(215900, caps.default_paper.size_um.width());
  EXPECT_EQ(355600, caps.default_paper.size_um.height());
}

// Tests that CapsAndDefaultsFromPrinter() does not propagate papers
// with badly formatted vendor IDs - such papers will not transform into
// meaningful ParsedPaper instances and are sometimes inimical to
// ARC++.
TEST_F(PrintBackendCupsIppHelperTest, OmitPapersWithoutVendorIds) {
  printer_->SetSupportedOptions(
      "media", MakeStringCollection(ipp_, {"jis_b5_182x257mm", "invalidsize",
                                           "", "iso_b5_176x250mm"}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  // The printer reports that it supports four media sizes, two of which
  // are invalid (``invalidsize'' and the empty vendor ID). The
  // preceding call to CapsAndDefaultsFromPrinter() will have dropped
  // these invalid sizes.
  ASSERT_EQ(2U, caps.papers.size());

  // While not directly pertinent to this test, we expect a certain
  // format for the other supported papers.
  EXPECT_THAT(
      caps.papers,
      testing::UnorderedElementsAre(
          testing::Field(&PrinterSemanticCapsAndDefaults::Paper::display_name,
                         "jis b5"),
          testing::Field(&PrinterSemanticCapsAndDefaults::Paper::display_name,
                         "iso b5")));
}

// Tests that CapsAndDefaultsFromPrinter() does not propagate the
// special IPP values that CUPS happens to expose to the Chromium print
// backend.
TEST_F(PrintBackendCupsIppHelperTest, OmitPapersWithSpecialVendorIds) {
  // Maintainer's note: there's no reason why a printer would deliver
  // two discrete sizes for custom_min* and custom_max*; in practice,
  // we always see the fully qualified custom_m(in|ax)_<DIMENSIONS>
  // delivered to the Chromium print backend.
  printer_->SetSupportedOptions(
      "media",
      MakeStringCollection(
          ipp_, {"na_number-11_4.5x10.375in", "custom_max", "custom_min_0x0in",
                 "na_govt-letter_8x10in", "custom_min",
                 "custom_max_1000x1000in", "iso_b0_1000x1414mm"}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  // The printer reports that it supports seven media sizes, four of
  // which are not meant for users' eyes (``custom_min*'' and
  // ``custom_max*''). The preceding call to
  // CapsAndDefaultsFromPrinter() will have dropped these sizes,
  // refusing to propagate them out of the backend.
  ASSERT_EQ(3U, caps.papers.size());

  // While not directly pertinent to this test, we expect a certain
  // format for the other supported papers.
  EXPECT_THAT(
      caps.papers,
      testing::UnorderedElementsAre(
          testing::Field(&PrinterSemanticCapsAndDefaults::Paper::display_name,
                         "na number-11"),
          testing::Field(&PrinterSemanticCapsAndDefaults::Paper::display_name,
                         "na govt-letter"),
          testing::Field(&PrinterSemanticCapsAndDefaults::Paper::display_name,
                         "iso b0")));
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
