// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/test_support/test_utils.h"
#include "printing/backend/mojom/print_backend.mojom.h"
#include "printing/backend/print_backend.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

const printing::PrinterSemanticCapsAndDefaults::Paper kPaperA3{
    /*display_name=*/"A3", /*vendor_id=*/"67",
    /*size_um=*/gfx::Size(7016, 9921)};
const printing::PrinterSemanticCapsAndDefaults::Paper kPaperA4{
    /*display_name=*/"A4", /*vendor_id=*/"12",
    /*size_um=*/gfx::Size(4961, 7016)};
const printing::PrinterSemanticCapsAndDefaults::Paper kPaperLetter{
    /*display_name=*/"Letter", /*vendor_id=*/"45",
    /*size_um=*/gfx::Size(5100, 6600)};
const printing::PrinterSemanticCapsAndDefaults::Paper kPaperLedger{
    /*display_name=*/"Ledger", /*vendor_id=*/"89",
    /*size_um=*/gfx::Size(6600, 10200)};

}  // namespace

TEST(PrintBackendMojomTraitsTest, TestSerializeAndDeserializePaper) {
  const printing::PrinterSemanticCapsAndDefaults::Papers kPapers{
      kPaperA3, kPaperA4, kPaperLetter, kPaperLedger};

  for (const auto& paper : kPapers) {
    printing::PrinterSemanticCapsAndDefaults::Paper input = paper;
    printing::PrinterSemanticCapsAndDefaults::Paper output;
    EXPECT_TRUE(mojo::test::SerializeAndDeserialize<printing::mojom::Paper>(
        &input, &output));
    EXPECT_EQ(paper, output);
  }
}

}  // namespace printing
