// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/printed_page_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

TEST(PrintedPageTest, Shrink) {
  scoped_refptr<PrintedPage> page = base::MakeRefCounted<PrintedPage>(
      1, std::unique_ptr<MetafilePlayer>(), gfx::Size(1200, 1200),
      gfx::Rect(0, 0, 400, 1100));
  EXPECT_EQ(0.0f, page->shrink_factor());
  page->set_shrink_factor(0.2f);
  EXPECT_EQ(0.2f, page->shrink_factor());
  page->set_shrink_factor(0.7f);
  EXPECT_EQ(0.7f, page->shrink_factor());
}

}  // namespace printing
