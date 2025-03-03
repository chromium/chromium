// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend_win.h"

#include <stddef.h>

#include <optional>

#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "printing/backend/spooler_win.h"
#include "printing/backend/win_helper.h"
#include "printing/printing_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

void DidLoadPrintableArea(size_t& counter) {
  ++counter;
}

}  // namespace

// This test makes use of a real print backend instance, and thus will
// interact with printer drivers installed on a system.  This can be useful on
// machines which a developer has control over the driver installations, but is
// less useful on bots which are managed by the infra team.  This test is
// intended to be run manually by developers using the --run-manual flag.
// Using the --v=1 flag will also display logging of all the printers in the
// enumeration list.
TEST(PrintBackendWinTest, MANUAL_ValidateFastEnumeratePrinters) {
  // Disable the feature, which forces `EnumeratePrinters()` to use the legacy
  // method to get the PrinterList result.
  base::test::ScopedFeatureList scoped_disable;
  scoped_disable.InitAndDisableFeature(features::kFastEnumeratePrinters);

  // Get list of printer info using only the Print Spooler API.
  const bool expect_some_printers =
      internal::IsSpoolerRunning() == internal::SpoolerServiceStatus::kRunning;
  VLOG(1) << "Print Spooler service is running: "
          << base::ToString(expect_some_printers);
  const mojom::ResultCode expect_result = expect_some_printers
                                              ? mojom::ResultCode::kSuccess
                                              : mojom::ResultCode::kFailed;
  base::HistogramTester histogram_tester;
  PrinterList printer_list;
  auto backend = base::MakeRefCounted<PrintBackendWin>();
  EXPECT_EQ(backend->EnumeratePrinters(printer_list), expect_result);
  EXPECT_EQ(expect_some_printers, !printer_list.empty());

  // For each printer, validate that faster method to get basic information
  // using the mixed method of Print Spooler and the registry.
  VLOG(1) << "Number of printers found: " << printer_list.size();
  for (const auto& info_spooler_only : printer_list) {
    VLOG(1) << "Found printer: `" << info_spooler_only.printer_name << "`";

    ScopedPrinterHandle printer;
    ASSERT_TRUE(printer.OpenPrinterWithName(
        base::UTF8ToWide(info_spooler_only.printer_name).c_str()));

    std::optional<PrinterBasicInfo> info_using_registry =
        GetBasicPrinterInfoMixedMethodForTesting(printer.Get());

    EXPECT_THAT(info_using_registry, testing::Optional(info_spooler_only));
  }

  EXPECT_THAT(
      histogram_tester.GetAllSamples(
          "Printing.EnumeratePrinters.BasicInfo.Registry"),
      BucketsAre(base::Bucket(false, 0),
                 base::Bucket(true, static_cast<int>(printer_list.size()))));
}

TEST(PrintBackendWinTest, GetPrintableAreaSamePaper) {
  auto backend = base::MakeRefCounted<PrintBackendWin>();
  size_t load_printable_area_call_count = 0;

  backend->SetPrintableAreaLoadedCallbackForTesting(base::BindRepeating(
      DidLoadPrintableArea, std::ref(load_printable_area_call_count)));

  std::string printer_name;
  mojom::ResultCode result = backend->GetDefaultPrinterName(printer_name);
  if (result != mojom::ResultCode::kSuccess || printer_name.empty()) {
    GTEST_SKIP() << "Printing with real drivers not available";
  }

  PrinterSemanticCapsAndDefaults caps;
  result = backend->GetPrinterSemanticCapsAndDefaults(printer_name, &caps);
  if (result != mojom::ResultCode::kSuccess) {
    GTEST_SKIP()
        << "Invalid real print driver configuration, printer is not available";
  }
  EXPECT_EQ(1u, load_printable_area_call_count);

  // Request printable area for the same default paper size.  This should use
  // the cached printable area instead of making another call to
  // `LoadPaperPrintableAreaUm()`.
  std::optional<gfx::Rect> printable_area_um = backend->GetPaperPrintableArea(
      printer_name, caps.default_paper.vendor_id(),
      caps.default_paper.size_um());
  ASSERT_TRUE(printable_area_um.has_value());
  EXPECT_EQ(1u, load_printable_area_call_count);
}

TEST(PrintBackendWinTest, GetPrintableAreaDifferentPaper) {
  auto backend = base::MakeRefCounted<PrintBackendWin>();
  size_t load_printable_area_call_count = 0;

  backend->SetPrintableAreaLoadedCallbackForTesting(base::BindRepeating(
      DidLoadPrintableArea, std::ref(load_printable_area_call_count)));

  std::string printer_name;
  mojom::ResultCode result = backend->GetDefaultPrinterName(printer_name);
  if (result != mojom::ResultCode::kSuccess || printer_name.empty()) {
    GTEST_SKIP() << "Printing with real drivers not available";
  }

  PrinterSemanticCapsAndDefaults caps;
  EXPECT_EQ(mojom::ResultCode::kSuccess,
            backend->GetPrinterSemanticCapsAndDefaults(printer_name, &caps));
  EXPECT_EQ(1u, load_printable_area_call_count);

  // Request printable area for a different paper size.
  ASSERT_GT(caps.papers.size(), 1u);
  std::optional<gfx::Rect> other_printable_area_um;
  for (const auto& paper : caps.papers) {
    if (paper == caps.default_paper) {
      continue;
    }

    // Another call to `LoadPaperPrintableAreaUm()` should happen since the
    // paper is different.
    other_printable_area_um = backend->GetPaperPrintableArea(
        printer_name, paper.vendor_id(), paper.size_um());
    break;
  }
  EXPECT_EQ(2u, load_printable_area_call_count);
}

}  // namespace printing
