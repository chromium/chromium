// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_document.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

TEST(PrintedDocumentTest, GetCenteredPageContentRect) {
  scoped_refptr<PrintedDocument> document;
  gfx::Rect page_content;
  const base::string16 name(base::ASCIIToUTF16("name"));

  // No centering.
  document = base::MakeRefCounted<PrintedDocument>(
      std::make_unique<PrintSettings>(), name, 1);
  gfx::Size page_size = gfx::Size(1200, 1200);
  gfx::Rect page_content_rect = gfx::Rect(0, 0, 400, 1100);
  page_content = document->GetCenteredPageContentRect(
      gfx::Size(1000, 1000), page_size, page_content_rect);
  EXPECT_EQ(0, page_content.x());
  EXPECT_EQ(0, page_content.y());
  EXPECT_EQ(400, page_content.width());
  EXPECT_EQ(1100, page_content.height());

  // X centered.
  document = base::MakeRefCounted<PrintedDocument>(
      std::make_unique<PrintSettings>(), name, 1);
  page_size = gfx::Size(500, 1200);
  page_content = document->GetCenteredPageContentRect(
      gfx::Size(1000, 1000), page_size, page_content_rect);
  EXPECT_EQ(250, page_content.x());
  EXPECT_EQ(0, page_content.y());
  EXPECT_EQ(400, page_content.width());
  EXPECT_EQ(1100, page_content.height());

  // Y centered.
  document = base::MakeRefCounted<PrintedDocument>(
      std::make_unique<PrintSettings>(), name, 1);
  page_size = gfx::Size(1200, 500);
  page_content = document->GetCenteredPageContentRect(
      gfx::Size(1000, 1000), page_size, page_content_rect);
  EXPECT_EQ(0, page_content.x());
  EXPECT_EQ(250, page_content.y());
  EXPECT_EQ(400, page_content.width());
  EXPECT_EQ(1100, page_content.height());

  // Both X and Y centered.
  document = base::MakeRefCounted<PrintedDocument>(
      std::make_unique<PrintSettings>(), name, 1);
  page_size = gfx::Size(500, 500),
  page_content = document->GetCenteredPageContentRect(
      gfx::Size(1000, 1000), page_size, page_content_rect);
  EXPECT_EQ(250, page_content.x());
  EXPECT_EQ(250, page_content.y());
  EXPECT_EQ(400, page_content.width());
  EXPECT_EQ(1100, page_content.height());
}

}  // namespace printing
