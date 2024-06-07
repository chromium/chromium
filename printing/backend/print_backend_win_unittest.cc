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
  ASSERT_EQ(mojom::ResultCode::kSuccess, result);

  PrinterSemanticCapsAndDefaults caps;
  EXPECT_EQ(mojom::ResultCode::kSuccess,
            backend->GetPrinterSemanticCapsAndDefaults(printer_name, &caps));
  EXPECT_EQ(1u, load_printable_area_call_count);

  // Request printable area for the same default paper size.  This should result
  // in another call to `LoadPaperPrintableAreaUm()`.
  std::optional<gfx::Rect> printable_area_um = backend->GetPaperPrintableArea(
      printer_name, caps.default_paper.vendor_id(),
      caps.default_paper.size_um());
  ASSERT_TRUE(printable_area_um.has_value());
  EXPECT_EQ(2u, load_printable_area_call_count);
}

}  // namespace printing
