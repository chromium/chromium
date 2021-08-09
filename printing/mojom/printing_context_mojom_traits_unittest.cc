// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/test_support/test_utils.h"
#include "printing/mojom/printing_context.mojom.h"
#include "printing/page_range.h"
#include "printing/page_setup.h"
#include "printing/print_settings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace printing {

namespace {

// Values for PageMargins, just want non-zero and all different.
const PageMargins kPageMarginNonzero(/*header=*/1,
                                     /*footer=*/2,
                                     /*left=*/3,
                                     /*right=*/4,
                                     /*top=*/5,
                                     /*bottom=*/6);

constexpr int kPageSetupTextHeight = 1;
constexpr gfx::Size kPageSetupPhysicalSize =
    gfx::Size(/*width=*/100, /*height=*/200);
constexpr gfx::Rect kPageSetupPrintableArea =
    gfx::Rect(/*x=*/5, /*y=*/10, /*width=*/80, /*height=*/170);
const PageMargins kPageSetupRequestedMargins(
    /*header=*/10,
    /*footer=*/20,
    /*left=*/5,
    /*right=*/15,
    /*top=*/10 + kPageSetupTextHeight,
    /*bottom=*/20 + kPageSetupTextHeight);

const PageSetup kPageSetupAsymmetricalMargins(kPageSetupPhysicalSize,
                                              kPageSetupPrintableArea,
                                              kPageSetupRequestedMargins,
                                              /*forced_margins=*/false,
                                              kPageSetupTextHeight);
const PageSetup kPageSetupForcedMargins(kPageSetupPhysicalSize,
                                        kPageSetupPrintableArea,
                                        kPageSetupRequestedMargins,
                                        /*forced_margins=*/true,
                                        kPageSetupTextHeight);

constexpr gfx::Size kRequestedMediaSize =
    gfx::Size(/*width=*/25, /*height=*/75);
const char kRequestedMediaVendorId[] = "iso-foo";

PrintSettings::RequestedMedia GenerateSampleRequestedMedia() {
  PrintSettings::RequestedMedia media;
  media.size_microns = kRequestedMediaSize;
  media.vendor_id = kRequestedMediaVendorId;
  return media;
}

}  // namespace

TEST(PrintingContextMojomTraitsTest, TestSerializeAndDeserializePageMargins) {
  PageMargins input = kPageMarginNonzero;
  PageMargins output;

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageMargins>(input, output));

  EXPECT_EQ(kPageMarginNonzero.header, output.header);
  EXPECT_EQ(kPageMarginNonzero.footer, output.footer);
  EXPECT_EQ(kPageMarginNonzero.left, output.left);
  EXPECT_EQ(kPageMarginNonzero.right, output.right);
  EXPECT_EQ(kPageMarginNonzero.top, output.top);
  EXPECT_EQ(kPageMarginNonzero.bottom, output.bottom);
}

// Test that no margins is valid.
TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePageMarginsNoMargins) {
  // Default constructor is all members zero.
  PageMargins input;
  // Ensure `output` doesn't start out with all zeroes.
  PageMargins output = kPageMarginNonzero;

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageMargins>(input, output));

  EXPECT_EQ(0, output.header);
  EXPECT_EQ(0, output.footer);
  EXPECT_EQ(0, output.left);
  EXPECT_EQ(0, output.right);
  EXPECT_EQ(0, output.top);
  EXPECT_EQ(0, output.bottom);
}

// Test that negative margin values are allowed.
TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePageMarginsNegativeValues) {
  PageMargins input = kPageMarginNonzero;
  PageMargins output;

  input.header = -1;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageMargins>(input, output));
  EXPECT_EQ(-1, output.header);

  input = kPageMarginNonzero;
  input.footer = -1;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageMargins>(input, output));
  EXPECT_EQ(-1, output.footer);

  input = kPageMarginNonzero;
  input.left = -1;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageMargins>(input, output));
  EXPECT_EQ(-1, output.left);

  input = kPageMarginNonzero;
  input.right = -1;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageMargins>(input, output));
  EXPECT_EQ(-1, output.right);

  input = kPageMarginNonzero;
  input.top = -1;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageMargins>(input, output));
  EXPECT_EQ(-1, output.top);

  input = kPageMarginNonzero;
  input.bottom = -1;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageMargins>(input, output));
  EXPECT_EQ(-1, output.bottom);
}

TEST(PrintingContextMojomTraitsTest, TestSerializeAndDeserializePageSetup) {
  PageSetup input = kPageSetupAsymmetricalMargins;
  PageSetup output;

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageSetup>(input, output));

  EXPECT_EQ(kPageSetupAsymmetricalMargins.physical_size(),
            output.physical_size());
  EXPECT_EQ(kPageSetupAsymmetricalMargins.printable_area(),
            output.printable_area());
  EXPECT_EQ(kPageSetupAsymmetricalMargins.overlay_area(),
            output.overlay_area());
  EXPECT_EQ(kPageSetupAsymmetricalMargins.content_area(),
            output.content_area());
  EXPECT_TRUE(kPageSetupAsymmetricalMargins.effective_margins().Equals(
      output.effective_margins()));
  EXPECT_TRUE(kPageSetupAsymmetricalMargins.requested_margins().Equals(
      output.requested_margins()));
  EXPECT_EQ(kPageSetupAsymmetricalMargins.forced_margins(),
            output.forced_margins());
  EXPECT_EQ(kPageSetupAsymmetricalMargins.text_height(), output.text_height());
}

TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePageSetupForcedMargins) {
  PageSetup input = kPageSetupForcedMargins;
  PageSetup output;

  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageSetup>(input, output));

  EXPECT_EQ(kPageSetupForcedMargins.physical_size(), output.physical_size());
  EXPECT_EQ(kPageSetupForcedMargins.printable_area(), output.printable_area());
  EXPECT_EQ(kPageSetupForcedMargins.overlay_area(), output.overlay_area());
  EXPECT_EQ(kPageSetupForcedMargins.content_area(), output.content_area());
  EXPECT_TRUE(kPageSetupForcedMargins.effective_margins().Equals(
      output.effective_margins()));
  EXPECT_TRUE(kPageSetupForcedMargins.requested_margins().Equals(
      output.requested_margins()));
  EXPECT_EQ(kPageSetupForcedMargins.forced_margins(), output.forced_margins());
  EXPECT_EQ(kPageSetupForcedMargins.text_height(), output.text_height());
}

TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePageRangeMultiPage) {
  PageRange input;
  PageRange output;

  input.from = 0u;
  input.to = 5u;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageRange>(input, output));

  EXPECT_EQ(0u, output.from);
  EXPECT_EQ(5u, output.to);
}

TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePageRangeSinglePage) {
  PageRange input;
  PageRange output;

  input.from = 1u;
  input.to = 1u;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::PageRange>(input, output));

  EXPECT_EQ(1u, output.from);
  EXPECT_EQ(1u, output.to);
}

TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializePageRangeReverseRange) {
  PageRange input;
  PageRange output;

  // Verify that reverse ranges are not allowed (e.g., not a mechanism to print
  // the range backwards).
  input.from = 5u;
  input.to = 1u;
  EXPECT_FALSE(
      mojo::test::SerializeAndDeserialize<mojom::PageRange>(input, output));
}

TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializeRequestedMedia) {
  PrintSettings::RequestedMedia input = GenerateSampleRequestedMedia();
  PrintSettings::RequestedMedia output;

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::RequestedMedia>(
      input, output));

  EXPECT_EQ(kRequestedMediaSize, output.size_microns);
  EXPECT_EQ(kRequestedMediaVendorId, output.vendor_id);
}

TEST(PrintingContextMojomTraitsTest,
     TestSerializeAndDeserializeRequestedMediaEmpty) {
  PrintSettings::RequestedMedia input;
  PrintSettings::RequestedMedia output;

  // The default is empty.
  EXPECT_TRUE(input.IsDefault());

  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::RequestedMedia>(
      input, output));

  EXPECT_TRUE(output.IsDefault());
}

}  // namespace printing
