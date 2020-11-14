// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/test_print_backend.h"

#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/stl_util.h"
#include "base/test/gtest_util.h"
#include "printing/backend/print_backend.h"
#include "printing/mojom/print.mojom.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

constexpr char kDefaultPrinterName[] = "default-test-printer";
constexpr char kAlternatePrinterName[] = "alternate-test-printer";
constexpr char kNullDataPrinterName[] = "null-data-test-printer";
constexpr char kInvalidPrinterName[] = "invalid-test-printer";

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
  }

  // Get the test print backend.
  TestPrintBackend* GetPrintBackend() const {
    return test_print_backend_.get();
  }

 private:
  scoped_refptr<TestPrintBackend> test_print_backend_;
};

TEST_F(TestPrintBackendTest, EnumeratePrinters) {
  const PrinterList kPrinterList{kAlternatePrinterInfo, kDefaultPrinterInfo};
  PrinterList printer_list;

  // Should return false when there are no printers in the environment.
  EXPECT_FALSE(GetPrintBackend()->EnumeratePrinters(&printer_list));

  AddPrinters();

  EXPECT_TRUE(GetPrintBackend()->EnumeratePrinters(&printer_list));
  EXPECT_THAT(printer_list, testing::ContainerEq(kPrinterList));
}

TEST_F(TestPrintBackendTest, DefaultPrinterName) {
  // If no printers added then no default.
  EXPECT_TRUE(GetPrintBackend()->GetDefaultPrinterName().empty());

  // Once printers are available, should be a default.
  AddPrinters();
  EXPECT_EQ(GetPrintBackend()->GetDefaultPrinterName(), kDefaultPrinterName);

  // Changing default should be reflected on next query.
  GetPrintBackend()->SetDefaultPrinterName(kAlternatePrinterName);
  EXPECT_EQ(GetPrintBackend()->GetDefaultPrinterName(), kAlternatePrinterName);

  // Adding a new printer to environment which is marked as default should
  // automatically make it the new default.
  static constexpr char kNewDefaultPrinterName[] = "new-default-test-printer";
  auto caps = std::make_unique<PrinterSemanticCapsAndDefaults>();
  auto printer_info = std::make_unique<PrinterBasicInfo>();
  printer_info->printer_name = kNewDefaultPrinterName;
  printer_info->is_default = true;
  GetPrintBackend()->AddValidPrinter(kNewDefaultPrinterName, std::move(caps),
                                     std::move(printer_info));
  EXPECT_EQ(GetPrintBackend()->GetDefaultPrinterName(), kNewDefaultPrinterName);

  // Requesting an invalid printer name to be a default should have no effect.
  GetPrintBackend()->SetDefaultPrinterName(kInvalidPrinterName);
  EXPECT_EQ(GetPrintBackend()->GetDefaultPrinterName(), kNewDefaultPrinterName);

  // Verify that re-adding a printer that was previously the default with null
  // basic info results in no default printer anymore.
  GetPrintBackend()->AddValidPrinter(kNewDefaultPrinterName, /*caps=*/nullptr,
                                     /*info=*/nullptr);
  EXPECT_TRUE(GetPrintBackend()->GetDefaultPrinterName().empty());
}

TEST_F(TestPrintBackendTest, PrinterBasicInfo) {
  PrinterBasicInfo printer_info;

  AddPrinters();

  EXPECT_TRUE(GetPrintBackend()->GetPrinterBasicInfo(kDefaultPrinterName,
                                                     &printer_info));
  EXPECT_EQ(printer_info.printer_name, kDefaultPrinterName);
  EXPECT_EQ(printer_info.printer_status, kDefaultPrinterStatus);
  EXPECT_TRUE(printer_info.is_default);

  EXPECT_TRUE(GetPrintBackend()->GetPrinterBasicInfo(kAlternatePrinterName,
                                                     &printer_info));
  EXPECT_EQ(printer_info.printer_name, kAlternatePrinterName);
  EXPECT_EQ(printer_info.printer_status, kAlternatePrinterStatus);
  EXPECT_FALSE(printer_info.is_default);

  EXPECT_FALSE(GetPrintBackend()->GetPrinterBasicInfo(kInvalidPrinterName,
                                                      &printer_info));

  // Changing default should be reflected on next query.
  GetPrintBackend()->SetDefaultPrinterName(kAlternatePrinterName);
  EXPECT_TRUE(GetPrintBackend()->GetPrinterBasicInfo(kAlternatePrinterName,
                                                     &printer_info));
  EXPECT_TRUE(printer_info.is_default);
  EXPECT_TRUE(GetPrintBackend()->GetPrinterBasicInfo(kDefaultPrinterName,
                                                     &printer_info));
  EXPECT_FALSE(printer_info.is_default);

  // Printers added with null basic info fail to get data on a query.
  EXPECT_FALSE(GetPrintBackend()->GetPrinterBasicInfo(kNullDataPrinterName,
                                                      &printer_info));

  // Verify that (re)adding a printer with null basic info results in a failure
  // the next time when trying to get the basic info.
  GetPrintBackend()->AddValidPrinter(kAlternatePrinterName, /*caps=*/nullptr,
                                     /*info=*/nullptr);
  EXPECT_FALSE(GetPrintBackend()->GetPrinterBasicInfo(kAlternatePrinterName,
                                                      &printer_info));
}

TEST_F(TestPrintBackendTest, GetPrinterSemanticCapsAndDefaults) {
  PrinterSemanticCapsAndDefaults caps;

  // Should fail when there are no printers in the environment.
  EXPECT_FALSE(GetPrintBackend()->GetPrinterSemanticCapsAndDefaults(
      kDefaultPrinterName, &caps));

  AddPrinters();

  EXPECT_TRUE(GetPrintBackend()->GetPrinterSemanticCapsAndDefaults(
      kDefaultPrinterName, &caps));
  EXPECT_EQ(caps.copies_max, kDefaultCopiesMax);

  EXPECT_TRUE(GetPrintBackend()->GetPrinterSemanticCapsAndDefaults(
      kAlternatePrinterName, &caps));
  EXPECT_EQ(caps.copies_max, kAlternateCopiesMax);

  EXPECT_FALSE(GetPrintBackend()->GetPrinterSemanticCapsAndDefaults(
      kInvalidPrinterName, &caps));

  // Printers added with null capabilities fail to get data on a query.
  EXPECT_FALSE(GetPrintBackend()->GetPrinterSemanticCapsAndDefaults(
      kNullDataPrinterName, &caps));

  // Verify that (re)adding a printer with null capabilities results in a
  // failure the next time when trying to get capabilities.
  GetPrintBackend()->AddValidPrinter(kAlternatePrinterName, /*caps=*/nullptr,
                                     /*info=*/nullptr);
  EXPECT_FALSE(GetPrintBackend()->GetPrinterSemanticCapsAndDefaults(
      kAlternatePrinterName, &caps));
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

}  // namespace printing
