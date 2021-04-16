// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/backend/print_backend_utils.h"

#include "printing/backend/print_backend.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

TEST(PrintBackendUtilsTest, ParsePaper) {
  PrinterSemanticCapsAndDefaults::Paper paper_mm =
      ParsePaper("iso_a4_210x297mm");
  EXPECT_EQ(gfx::Size(210000, 297000), paper_mm.size_um);
  EXPECT_EQ("iso_a4_210x297mm", paper_mm.vendor_id);
  EXPECT_EQ("iso a4", paper_mm.display_name);

  PrinterSemanticCapsAndDefaults::Paper paper_in =
      ParsePaper("na_letter_8.5x11in");
  EXPECT_EQ(gfx::Size(215900, 279400), paper_in.size_um);
  EXPECT_EQ("na_letter_8.5x11in", paper_in.vendor_id);
  EXPECT_EQ("na letter", paper_in.display_name);
}

}  // namespace printing
