// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cups/cups.h>

#include "printing/backend/print_backend.h"
#include "printing/backend/print_backend_cups.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

TEST(PrintBackendCupsMacTest, PrinterBasicInfoFromCUPS) {
  const char kName[] = "printer";
  cups_dest_t* printer = nullptr;
  ASSERT_EQ(1, cupsAddDest(kName, nullptr, 0, &printer));

  int num_options = 0;
  cups_option_t* options = nullptr;
  num_options = cupsAddOption("printer-info", "name", num_options, &options);
  num_options = cupsAddOption("printer-make-and-model", "description",
                              num_options, &options);
  printer->num_options = num_options;
  printer->options = options;

  PrinterBasicInfo printer_info;
  PrintBackendCUPS::PrinterBasicInfoFromCUPS(*printer, &printer_info);
  cupsFreeDests(1, printer);

  EXPECT_EQ("printer", printer_info.printer_name);
  EXPECT_EQ("name", printer_info.display_name);
  EXPECT_EQ("description", printer_info.printer_description);
}

}  // namespace printing
