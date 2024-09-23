// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend_win.h"

#include <stddef.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

void DidLoadPrintableArea(size_t& counter) {
  ++counter;
}

}  // namespace

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
