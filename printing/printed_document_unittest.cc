// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

TEST(PrintedDocumentTest, GetCenteredPageContentRect) {
  gfx::Rect page_content;

  // No centering.
  gfx::Size page_size = gfx::Size(1200, 1200);
  gfx::Rect page_content_rect = gfx::Rect(0, 0, 400, 1100);
  page_content = PrintedDocument::GetCenteredPageContentRect(
      gfx::Size(1000, 1000), page_size, page_content_rect);
  EXPECT_EQ(0, page_content.x());
  EXPECT_EQ(0, page_content.y());
  EXPECT_EQ(400, page_content.width());
  EXPECT_EQ(1100, page_content.height());

  // X centered.
  page_size = gfx::Size(500, 1200);
  page_content = PrintedDocument::GetCenteredPageContentRect(
      gfx::Size(1000, 1000), page_size, page_content_rect);
  EXPECT_EQ(250, page_content.x());
  EXPECT_EQ(0, page_content.y());
  EXPECT_EQ(400, page_content.width());
  EXPECT_EQ(1100, page_content.height());

  // Y centered.
  page_size = gfx::Size(1200, 500);
  page_content = PrintedDocument::GetCenteredPageContentRect(
      gfx::Size(1000, 1000), page_size, page_content_rect);
  EXPECT_EQ(0, page_content.x());
  EXPECT_EQ(250, page_content.y());
  EXPECT_EQ(400, page_content.width());
  EXPECT_EQ(1100, page_content.height());

  // Both X and Y centered.
  page_size = gfx::Size(500, 500),
  page_content = PrintedDocument::GetCenteredPageContentRect(
      gfx::Size(1000, 1000), page_size, page_content_rect);
  EXPECT_EQ(250, page_content.x());
  EXPECT_EQ(250, page_content.y());
  EXPECT_EQ(400, page_content.width());
  EXPECT_EQ(1100, page_content.height());
}

}  // namespace printing
