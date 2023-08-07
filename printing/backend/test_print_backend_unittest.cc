// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/test_print_backend.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/test/gtest_util.h"
#include "build/build_config.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "printing/backend/mojom/print_backend.mojom.h"
#include "printing/backend/print_backend.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_WIN)
#include "base/test/gmock_expected_support.h"
#endif  // BUILDFLAG(IS_WIN)

namespace printing {

namespace {

constexpr char kDefaultPrinterName[] = "default-test-printer";
constexpr char kAlternatePrinterName[] = "alternate-test-printer";
constexpr char kNullDataPrinterName[] = "null-data-test-printer";
constexpr char kAccessDeniedPrinterName[] = "access-denied-test-printer";
constexpr char kInvalidPrinterName[] = "invalid-test-printer";
constexpr char kInvalidDataPrinterName[] = "invalid-data-test-printer";

constexpr int kDefaultPrinterStatus = 0;
constexpr int kAlternatePrinterStatus = 1;

const PrinterBasicInfo kDefaultPrinterInfo(
    /*printer_name=*/kDefaultPrinterName,
    /*display_name=*/"default test printer",
    /*printer_description=*/"Default printer for testing.",
    /*printer_status=*/kDefaultPrinterStatus,
    /*is_default=*/true,
    /*options=*/PrinterBasicInfoOptions{});
const PrinterBasicInfo kAlternatePrinterInfo(
    /*printer_name=*/kAlternatePrinterName,
    /*display_name=*/"alternate test printer",
    /*printer_description=*/"Alternate printer for testing.",
    /*printer_status=*/kAlternatePrinterStatus,
    /*is_default=*/false,
    /*options=*/PrinterBasicInfoOptions{});

constexpr int32_t kDefaultCopiesMax = 123;
constexpr int32_t kAlternateCopiesMax = 456;

}  // namespace

class TestPrintBackendTest : public testing::Test {
 public:
  void SetUp() override {
    test_print_backend_ = base::MakeRefCounted<TestPrintBackend>();
  }

  void AddPrinters() {
    // Add some printers; only bother to set one capabilities field that will
    // be paid attention to in the tests as way of knowing it has provided the
    // real capabilities.
    auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
    caps->copies_max = kDefaultCopiesMax;
    test_print_backend_->AddValidPrinter(
        kDefaultPrinterName, std::move(caps),
        std::make_unique<PrinterBasicInfo>(kDefaultPrinterInfo));

    caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
    caps->copies_max = kAlternateCopiesMax;
    test_print_backend_->AddValidPrinter(
        kAlternatePrinterName, std::move(caps),
        std::make_unique<PrinterBasicInfo>(kAlternatePrinterInfo));

    test_print_backend_->AddValidPrinter(kNullDataPrinterName, /*caps=*/nullptr,
                                         /*info=*/nullptr);
#if BUILDFLAG(IS_WIN)
    test_print_backend_->SetXmlCapabilitiesForPrinter(kNullDataPrinterName, "");
#endif  // BUILDFLAG(IS_WIN)
  }

  void AddInvalidDataPrinter() {
    test_print_backend_->AddInvalidDataPrinter(kInvalidDataPrinterName);
  }

  void AddAccessDeniedPrinter() {
    test_print_backend_->AddAccessDeniedPrinter(kAccessDeniedPrinterName);
  }

  // Get the test print backend.
  TestPrintBackend* GetPrintBackend() { return test_print_backend_.get(); }

 private:
  scoped_refptr<TestPrintBackend> test_print_backend_;
};

TEST_F(TestPrintBackendTest, EnumeratePrinters) {
  const PrinterList kPrinterList{kAlternatePrinterInfo, kDefaultPrinterInfo};
  PrinterList printer_list;

  AddPrinters();

  EXPECT_EQ(GetPrintBackend()->EnumeratePrinters(printer_list),
            mojom::ResultCode::kSuccess);
  EXPECT_THAT(printer_list, testing::ContainerEq(kPrinterList));
}

TEST_F(TestPrintBackendTest, EnumeratePrintersNoneFound) {
  const PrinterList kPrinterList{kAlternatePrinterInfo, kDefaultPrinterInfo};
  PrinterList printer_list;

  // Should return true even when there are no printers in the environment.
  EXPECT_EQ(GetPrintBackend()->EnumeratePrinters(printer_list),
            mojom::ResultCode::kSuccess);
  EXPECT_TRUE(printer_list.empty());
}

TEST_F(TestPrintBackendTest, DefaultPrinterName) {
  std::string default_printer;

  // If no printers added then no default.
  ASSERT_EQ(GetPrintBackend()->GetDefaultPrinterName(default_printer),
            mojom::ResultCode::kSuccess);
  EXPECT_TRUE(default_printer.empty());

  // Once printers are available, should be a default.
  AddPrinters();
  ASSERT_EQ(GetPrintBackend()->GetDefaultPrinterName(default_printer),
            mojom::ResultCode::kSuccess);
  EXPECT_EQ(default_printer, kDefaultPrinterName);

  // Changing default should be reflected on next query.
  GetPrintBackend()->SetDefaultPrinterName(kAlternatePrinterName);
  ASSERT_EQ(GetPrintBackend()->GetDefaultPrinterName(default_printer),
            mojom::ResultCode::kSuccess);
  EXPECT_EQ(default_printer, kAlternatePrinterName);

  // Adding a new printer to environment which is marked as default should
  // automatically make it the new default.
  static constexpr char kNewDefaultPrinterName[] = "new-default-test-printer";
  auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
  auto printer_info = std::make_unique<PrinterBasicInfo>();
  printer_info->printer_name = kNewDefaultPrinterName;
  printer_info->is_default = true;
  GetPrintBackend()->AddValidPrinter(kNewDefaultPrinterName, std::move(caps),
                                     std::move(printer_info));
  ASSERT_EQ(GetPrintBackend()->GetDefaultPrinterName(default_printer),
            mojom::ResultCode::kSuccess);
  EXPECT_EQ(default_printer, kNewDefaultPrinterName);

  // Requesting an invalid printer name to be a default should have no effect.
  GetPrintBackend()->SetDefaultPrinterName(kInvalidPrinterName);
  ASSERT_EQ(GetPrintBackend()->GetDefaultPrinterName(default_printer),
            mojom::ResultCode::kSuccess);
  EXPECT_EQ(default_printer, kNewDefaultPrinterName);

  // Verify that re-adding a printer that was previously the default with null
  // basic info results in no default printer anymore.
  GetPrintBackend()->AddValidPrinter(kNewDefaultPrinterName, /*caps=*/nullptr,
                                     /*info=*/nullptr);
  ASSERT_EQ(GetPrintBackend()->GetDefaultPrinterName(default_printer),
            mojom::ResultCode::kSuccess);
  EXPECT_TRUE(default_printer.empty());
}

TEST_F(TestPrintBackendTest, PrinterBasicInfo) {
  PrinterBasicInfo printer_info;

  AddPrinters();

  EXPECT_EQ(GetPrintBackend()->GetPrinterBasicInfo(kDefaultPrinterName,
                                                   &printer_info),
            mojom::ResultCode::kSuccess);
  EXPECT_EQ(printer_info.printer_name, kDefaultPrinterName);
  EXPECT_EQ(printer_info.printer_status, kDefaultPrinterStatus);
  EXPECT_TRUE(printer_info.is_default);

  EXPECT_EQ(GetPrintBackend()->GetPrinterBasicInfo(kAlternatePrinterName,
                                                   &printer_info),
            mojom::ResultCode::kSuccess);
  EXPECT_EQ(printer_info.printer_name, kAlternatePrinterName);
  EXPECT_EQ(printer_info.printer_status, kAlternatePrinterStatus);
  EXPECT_FALSE(printer_info.is_default);

  EXPECT_EQ(GetPrintBackend()->GetPrinterBasicInfo(kInvalidPrinterName,
                                                   &printer_info),
            mojom::ResultCode::kFailed);

  // Changing default should be reflected on next query.
  GetPrintBackend()->SetDefaultPrinterName(kAlternatePrinterName);
  EXPECT_EQ(GetPrintBackend()->GetPrinterBasicInfo(kAlternatePrinterName,
                                                   &printer_info),
            mojom::ResultCode::kSuccess);
  EXPECT_TRUE(printer_info.is_default);
  EXPECT_EQ(GetPrintBackend()->GetPrinterBasicInfo(kDefaultPrinterName,
                                                   &printer_info),
            mojom::ResultCode::kSuccess);
  EXPECT_FALSE(printer_info.is_default);

  // Printers added with null basic info fail to get data on a query.
  EXPECT_EQ(GetPrintBackend()->GetPrinterBasicInfo(kNullDataPrinterName,
                                                   &printer_info),
            mojom::ResultCode::kFailed);

  // Verify that (re)adding a printer with null basic info results in a failure
  // the next time when trying to get the basic info.
  GetPrintBackend()->AddValidPrinter(kAlternatePrinterName, /*caps=*/nullptr,
                                     /*info=*/nullptr);
  EXPECT_EQ(GetPrintBackend()->GetPrinterBasicInfo(kAlternatePrinterName,
                                                   &printer_info),
            mojom::ResultCode::kFailed);
}

TEST_F(TestPrintBackendTest, PrinterBasicInfoAccessDenied) {
  PrinterBasicInfo printer_info;

  AddAccessDeniedPrinter();

  EXPECT_EQ(GetPrintBackend()->GetPrinterBasicInfo(kAccessDeniedPrinterName,
                                                   &printer_info),
            mojom::ResultCode::kAccessDenied);
}

// Demonstrate that a printer might be able to present data considered to be
// invalid, which becomes detectable when it undergoes Mojom message
// validation.
TEST_F(TestPrintBackendTest, PrinterBasicInfoInvalidData) {
  PrinterBasicInfo printer_info;

  AddInvalidDataPrinter();

  EXPECT_EQ(GetPrintBackend()->GetPrinterBasicInfo(kInvalidDataPrinterName,
                                                   &printer_info),
            mojom::ResultCode::kSuccess);

  PrinterBasicInfo output;
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<mojom::PrinterBasicInfo>(
      printer_info, output));
}

TEST_F(TestPrintBackendTest, GetPrinterSemanticCapsAndDefaults) {
  PrinterSemanticCapsAndDefaults caps;

  // Should fail when there are no printers in the environment.
  EXPECT_EQ(GetPrintBackend()->GetPrinterSemanticCapsAndDefaults(
                kDefaultPrinterName, &caps),
            mojom::ResultCode::kFailed);

  AddPrinters();

  EXPECT_EQ(GetPrintBackend()->GetPrinterSemanticCapsAndDefaults(
                kDefaultPrinterName, &caps),
            mojom::ResultCode::kSuccess);
  EXPECT_EQ(caps.copies_max, kDefaultCopiesMax);

  EXPECT_EQ(GetPrintBackend()->GetPrinterSemanticCapsAndDefaults(
                kAlternatePrinterName, &caps),
            mojom::ResultCode::kSuccess);
  EXPECT_EQ(caps.copies_max, kAlternateCopiesMax);

  EXPECT_EQ(GetPrintBackend()->GetPrinterSemanticCapsAndDefaults(
                kInvalidPrinterName, &caps),
            mojom::ResultCode::kFailed);

  // Printers added with null capabilities fail to get data on a query.
  EXPECT_EQ(GetPrintBackend()->GetPrinterSemanticCapsAndDefaults(
                kNullDataPrinterName, &caps),
            mojom::ResultCode::kFailed);

  // Verify that (re)adding a printer with null capabilities results in a
  // failure the next time when trying to get capabilities.
  GetPrintBackend()->AddValidPrinter(kAlternatePrinterName, /*caps=*/nullptr,
                                     /*info=*/nullptr);
  EXPECT_EQ(GetPrintBackend()->GetPrinterSemanticCapsAndDefaults(
                kAlternatePrinterName, &caps),
            mojom::ResultCode::kFailed);
}

TEST_F(TestPrintBackendTest, GetPrinterSemanticCapsAndDefaultsAccessDenied) {
  PrinterSemanticCapsAndDefaults caps;

  AddAccessDeniedPrinter();

  EXPECT_EQ(GetPrintBackend()->GetPrinterSemanticCapsAndDefaults(
                kAccessDeniedPrinterName, &caps),
            mojom::ResultCode::kAccessDenied);
}

TEST_F(TestPrintBackendTest, IsValidPrinter) {
  PrinterSemanticCapsAndDefaults caps;

  // Should fail when there are no printers in the environment.
  EXPECT_FALSE(GetPrintBackend()->IsValidPrinter(kDefaultPrinterName));

  AddPrinters();

  EXPECT_TRUE(GetPrintBackend()->IsValidPrinter(kDefaultPrinterName));
  EXPECT_TRUE(GetPrintBackend()->IsValidPrinter(kAlternatePrinterName));
  EXPECT_FALSE(GetPrintBackend()->IsValidPrinter(kInvalidPrinterName));

  // Verify that still shows as valid printer even if basic info and
  // capabilities were originally null.
  EXPECT_TRUE(GetPrintBackend()->IsValidPrinter(kNullDataPrinterName));

  // Verify that (re)adding a printer with null info and capabilities still
  // shows as valid.
  GetPrintBackend()->AddValidPrinter(kAlternatePrinterName, /*caps=*/nullptr,
                                     /*info=*/nullptr);
  EXPECT_TRUE(GetPrintBackend()->IsValidPrinter(kAlternatePrinterName));
}

#if BUILDFLAG(IS_WIN)
TEST_F(TestPrintBackendTest, GetXmlPrinterCapabilitiesForXpsDriver) {
  // Should fail when there are no printers in the environment.
  EXPECT_THAT(GetPrintBackend()->GetXmlPrinterCapabilitiesForXpsDriver(
                  kDefaultPrinterName),
              base::test::ErrorIs(mojom::ResultCode::kFailed));

  AddPrinters();

  // The default XML string set for valid printers should be valid, so verify
  // that we receive an XML string.
  ASSERT_TRUE(GetPrintBackend()
                  ->GetXmlPrinterCapabilitiesForXpsDriver(kDefaultPrinterName)
                  .has_value());

  EXPECT_THAT(GetPrintBackend()->GetXmlPrinterCapabilitiesForXpsDriver(
                  kInvalidPrinterName),
              base::test::ErrorIs(mojom::ResultCode::kFailed));

  // Printers set with invalid XML should return failure. Invalid XML is
  // considered an empty string for these tests.
  EXPECT_THAT(GetPrintBackend()->GetXmlPrinterCapabilitiesForXpsDriver(
                  kNullDataPrinterName),
              base::test::ErrorIs(mojom::ResultCode::kFailed));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace printing
