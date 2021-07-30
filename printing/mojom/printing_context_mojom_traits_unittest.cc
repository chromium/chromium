// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/test_support/test_utils.h"
#include "printing/mojom/printing_context.mojom.h"
#include "printing/page_setup.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

namespace {

// Values for PageMargins, just want non-zero and all different.
const PageMargins kPageMarginNonzero(/*header=*/1,
                                     /*footer=*/2,
                                     /*left=*/3,
                                     /*right=*/4,
                                     /*top=*/5,
                                     /*bottom=*/6);

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

}  // namespace printing
