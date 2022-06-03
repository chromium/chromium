// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/printing_conversions.h"

#include <stdint.h>

#include <vector>

#include "base/cxx17_backports.h"
#include "ppapi/c/dev/ppp_printing_dev.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;

}  // namespace

TEST(PrintingConversionsTest, SingleOnePageRange) {
  static constexpr PP_PrintPageNumberRange_Dev kRanges[] = {{1, 1}};
  EXPECT_THAT(
      PageNumbersFromPPPrintPageNumberRange(kRanges, base::size(kRanges)),
      ElementsAre(1));
}

TEST(PrintingConversionsTest, SingleMultiPageRange) {
  static constexpr PP_PrintPageNumberRange_Dev kRanges[] = {{1, 3}};
  EXPECT_THAT(
      PageNumbersFromPPPrintPageNumberRange(kRanges, base::size(kRanges)),
      ElementsAre(1, 2, 3));
}

TEST(PrintingConversionsTest, MultipleOnePageRange) {
  static constexpr PP_PrintPageNumberRange_Dev kRanges[] = {{1, 1}, {3, 3}};
  EXPECT_THAT(
      PageNumbersFromPPPrintPageNumberRange(kRanges, base::size(kRanges)),
      ElementsAre(1, 3));
}

TEST(PrintingConversionsTest, MultipleMultiPageRange) {
  static constexpr PP_PrintPageNumberRange_Dev kRanges[] = {{1, 3}, {5, 6}};
  EXPECT_THAT(
      PageNumbersFromPPPrintPageNumberRange(kRanges, base::size(kRanges)),
      ElementsAre(1, 2, 3, 5, 6));
}

TEST(PrintingConversionsTest, OverlappingPageRange) {
  static constexpr PP_PrintPageNumberRange_Dev kRanges[] = {{1, 3}, {2, 4}};
  EXPECT_THAT(
      PageNumbersFromPPPrintPageNumberRange(kRanges, base::size(kRanges)),
      ElementsAre(1, 2, 3, 2, 3, 4));
}

}  // namespace chrome_pdf
