// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cups/cups.h>

#include <map>
#include <memory>

#include "base/logging.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "printing/backend/cups_ipp_util.h"
#include "printing/backend/cups_printer.h"
#include "printing/printing_features_chromeos.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

class MockCupsOptionProvider : public CupsOptionProvider {
 public:
  ~MockCupsOptionProvider() override {}

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
    int size = ippGetCount(attr);
    for (int i = 0; i < size; ++i) {
      strings.emplace_back(ippGetString(attr, i, nullptr));
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

class PrintBackendCupsIppUtilTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ipp_ = ippNew();
    printer_ = std::make_unique<MockCupsOptionProvider>();
  }

  void TearDown() override {
    ippDelete(ipp_);
    printer_.reset();
  }

  ipp_t* ipp_;
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

TEST_F(PrintBackendCupsIppUtilTest, CopiesCapable) {
  printer_->SetSupportedOptions("copies", MakeRange(ipp_, 1, 2));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_TRUE(caps.copies_capable);
}

TEST_F(PrintBackendCupsIppUtilTest, CopiesNotCapable) {
  // copies missing, no setup
  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_FALSE(caps.copies_capable);
}

TEST_F(PrintBackendCupsIppUtilTest, ColorPrinter) {
  printer_->SetSupportedOptions(
      "print-color-mode", MakeStringCollection(ipp_, {"color", "monochrome"}));
  printer_->SetOptionDefault("print-color-mode", MakeString(ipp_, "color"));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_TRUE(caps.color_changeable);
  EXPECT_TRUE(caps.color_default);
}

TEST_F(PrintBackendCupsIppUtilTest, BWPrinter) {
  printer_->SetSupportedOptions("print-color-mode",
                                MakeStringCollection(ipp_, {"monochrome"}));
  printer_->SetOptionDefault("print-color-mode",
                             MakeString(ipp_, "monochrome"));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_FALSE(caps.color_changeable);
  EXPECT_FALSE(caps.color_default);
}

TEST_F(PrintBackendCupsIppUtilTest, DuplexSupported) {
  printer_->SetSupportedOptions(
      "sides",
      MakeStringCollection(ipp_, {"two-sided-long-edge", "one-sided"}));
  printer_->SetOptionDefault("sides", MakeString(ipp_, "one-sided"));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_THAT(caps.duplex_modes,
              testing::UnorderedElementsAre(SIMPLEX, LONG_EDGE));
  EXPECT_EQ(SIMPLEX, caps.duplex_default);
}

TEST_F(PrintBackendCupsIppUtilTest, DuplexNotSupported) {
  printer_->SetSupportedOptions("sides",
                                MakeStringCollection(ipp_, {"one-sided"}));
  printer_->SetOptionDefault("sides", MakeString(ipp_, "one-sided"));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_THAT(caps.duplex_modes, testing::UnorderedElementsAre(SIMPLEX));
  EXPECT_EQ(SIMPLEX, caps.duplex_default);
}

TEST_F(PrintBackendCupsIppUtilTest, A4PaperSupported) {
  printer_->SetSupportedOptions(
      "media", MakeStringCollection(ipp_, {"iso_a4_210x297mm"}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  PrinterSemanticCapsAndDefaults::Paper paper = caps.papers[0];
  // media display name localization is handled more fully in
  // GetPrinterCapabilitiesOnBlockingTaskRunner().
  EXPECT_EQ("iso a4", paper.display_name);
  EXPECT_EQ("iso_a4_210x297mm", paper.vendor_id);
  EXPECT_EQ(210000, paper.size_um.width());
  EXPECT_EQ(297000, paper.size_um.height());
}

TEST_F(PrintBackendCupsIppUtilTest, LegalPaperDefault) {
  printer_->SetOptionDefault("media", MakeString(ipp_, "na_legal_8.5x14in"));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);
  // media display name localization is handled more fully in
  // GetPrinterCapabilitiesOnBlockingTaskRunner().
  EXPECT_EQ("na legal", caps.default_paper.display_name);
  EXPECT_EQ("na_legal_8.5x14in", caps.default_paper.vendor_id);
  EXPECT_EQ(215900, caps.default_paper.size_um.width());
  EXPECT_EQ(355600, caps.default_paper.size_um.height());
}

TEST_F(PrintBackendCupsIppUtilTest, PinSupported) {
  printer_->SetSupportedOptions("job-password", MakeInteger(ipp_, 4));
  printer_->SetSupportedOptions("job-password-encryption",
                                MakeStringCollection(ipp_, {"none"}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_TRUE(caps.pin_supported);
}

TEST_F(PrintBackendCupsIppUtilTest, PinNotSupported) {
  // Pin support missing, no setup.
  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_FALSE(caps.pin_supported);
}

TEST_F(PrintBackendCupsIppUtilTest, PinEncryptionNotSupported) {
  printer_->SetSupportedOptions("job-password", MakeInteger(ipp_, 4));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_FALSE(caps.pin_supported);
}

TEST_F(PrintBackendCupsIppUtilTest, PinTooShort) {
  printer_->SetSupportedOptions("job-password", MakeInteger(ipp_, 3));
  printer_->SetSupportedOptions("job-password-encryption",
                                MakeStringCollection(ipp_, {"none"}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_FALSE(caps.pin_supported);
}

TEST_F(PrintBackendCupsIppUtilTest, AdvancedCaps) {
  base::HistogramTester histograms;
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(printing::kAdvancedPpdAttributes);

  printer_->SetSupportedOptions(
      "job-creation-attributes",
      MakeStringCollection(
          ipp_, {"copies", "ipp-attribute-fidelity", "finishings", "job-name",
                 "output-bin", "print-quality"}));
  printer_->SetSupportedOptions("finishings",
                                MakeIntCollection(ipp_, {3, 7, 10}));
  printer_->SetSupportedOptions(
      "output-bin", MakeStringCollection(ipp_, {"face-down", "face-up"}));
  printer_->SetSupportedOptions("print-quality",
                                MakeIntCollection(ipp_, {3, 4, 5}));

  PrinterSemanticCapsAndDefaults caps;
  CapsAndDefaultsFromPrinter(*printer_, &caps);

  EXPECT_EQ(6u, caps.advanced_capabilities.size());
  EXPECT_EQ("ipp-attribute-fidelity", caps.advanced_capabilities[0].name);
  EXPECT_EQ(base::Value::Type::BOOLEAN, caps.advanced_capabilities[0].type);
  EXPECT_EQ("finishings/7", caps.advanced_capabilities[1].name);
  EXPECT_EQ(base::Value::Type::BOOLEAN, caps.advanced_capabilities[1].type);
  EXPECT_EQ("finishings/10", caps.advanced_capabilities[2].name);
  EXPECT_EQ(base::Value::Type::BOOLEAN, caps.advanced_capabilities[2].type);
  EXPECT_EQ("job-name", caps.advanced_capabilities[3].name);
  EXPECT_EQ(base::Value::Type::STRING, caps.advanced_capabilities[3].type);
  EXPECT_EQ("output-bin", caps.advanced_capabilities[4].name);
  EXPECT_EQ(2u, caps.advanced_capabilities[4].values.size());
  EXPECT_EQ("print-quality", caps.advanced_capabilities[5].name);
  EXPECT_EQ(3u, caps.advanced_capabilities[5].values.size());
  histograms.ExpectUniqueSample("Printing.CUPS.IppAttributesCount", 5, 1);
}

}  // namespace printing
