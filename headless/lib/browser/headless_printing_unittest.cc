// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/values.h"
#include "headless/lib/browser/headless_devtools_manager_delegate.h"
#include "headless/lib/browser/headless_print_manager.h"
#include "printing/buildflags/buildflags.h"
#include "printing/units.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace headless {

TEST(PageRangeTextToPagesTest, General) {
  using PM = HeadlessPrintManager;
  std::vector<uint32_t> pages;
  std::vector<uint32_t> expected_pages;

  // "-" is full range of pages.
  PM::PageRangeStatus status = PM::PageRangeTextToPages("-", false, 10, &pages);
  expected_pages = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  EXPECT_EQ(expected_pages, pages);
  EXPECT_EQ(PM::PRINT_NO_ERROR, status);

  // If no start page is specified, we start at first page.
  status = PM::PageRangeTextToPages("-5", false, 10, &pages);
  expected_pages = {0, 1, 2, 3, 4};
  EXPECT_EQ(expected_pages, pages);
  EXPECT_EQ(PM::PRINT_NO_ERROR, status);

  // If no end page is specified, we end at last page.
  status = PM::PageRangeTextToPages("5-", false, 10, &pages);
  expected_pages = {4, 5, 6, 7, 8, 9};
  EXPECT_EQ(expected_pages, pages);
  EXPECT_EQ(PM::PRINT_NO_ERROR, status);

  // Multiple ranges are separated by commas.
  status = PM::PageRangeTextToPages("1-3,9-10,4-6", false, 10, &pages);
  expected_pages = {0, 1, 2, 3, 4, 5, 8, 9};
  EXPECT_EQ(expected_pages, pages);
  EXPECT_EQ(PM::PRINT_NO_ERROR, status);

  // White space is ignored.
  status = PM::PageRangeTextToPages("1- 3, 9-10,4 -6", false, 10, &pages);
  expected_pages = {0, 1, 2, 3, 4, 5, 8, 9};
  EXPECT_EQ(expected_pages, pages);
  EXPECT_EQ(PM::PRINT_NO_ERROR, status);

  // End page beyond number of pages is supported and capped to number of pages.
  status = PM::PageRangeTextToPages("1-10", false, 5, &pages);
  expected_pages = {0, 1, 2, 3, 4};
  EXPECT_EQ(expected_pages, pages);
  EXPECT_EQ(PM::PRINT_NO_ERROR, status);

  // Start page beyond number of pages results in error.
  status = PM::PageRangeTextToPages("1-3,9-10,4-6", false, 5, &pages);
  EXPECT_EQ(PM::LIMIT_ERROR, status);

  // Invalid page ranges are ignored if |ignore_invalid_page_ranges| is true.
  status = PM::PageRangeTextToPages("9-10,4-6,3-1", true, 5, &pages);
  expected_pages = {3, 4};
  EXPECT_EQ(expected_pages, pages);
  EXPECT_EQ(PM::PRINT_NO_ERROR, status);

  // Invalid input results in error.
  status = PM::PageRangeTextToPages("abcd", false, 10, &pages);
  EXPECT_EQ(PM::SYNTAX_ERROR, status);

  // Invalid input results in error.
  status = PM::PageRangeTextToPages("1-3,9-a10,4-6", false, 10, &pages);
  EXPECT_EQ(PM::SYNTAX_ERROR, status);
}

}  // namespace headless
