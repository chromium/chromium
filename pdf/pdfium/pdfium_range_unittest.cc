// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_range.h"

#include <memory>

#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_page.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {

class PDFiumRangeTest : public PDFiumTestBase {
 public:
  void SetUp() override {
    PDFiumTestBase::SetUp();
    engine_ = InitializeEngine(&client_, FILE_PATH_LITERAL("hello_world2.pdf"));
    ASSERT_TRUE(engine_);
  }

  void TearDown() override {
    // Must reset the PDFiumEngine before PDFiumTestBase uninitializes PDFium
    // altogether.
    engine_.reset();
    PDFiumTestBase::TearDown();
  }

  PDFiumEngine* engine() { return engine_.get(); }

 private:
  TestClient client_;
  std::unique_ptr<PDFiumEngine> engine_;
};

TEST_P(PDFiumRangeTest, Empty) {
  PDFiumPage page(engine(), 0);
  page.MarkAvailable();
  {
    PDFiumRange range(&page, /*char_index=*/0, /*char_count=*/0);
    EXPECT_EQ(0, range.char_index());
    EXPECT_EQ(0, range.char_count());
    EXPECT_EQ(u"", range.GetText());
  }
  {
    PDFiumRange range(&page, /*char_index=*/1, /*char_count=*/0);
    EXPECT_EQ(1, range.char_index());
    EXPECT_EQ(0, range.char_count());
    EXPECT_EQ(u"", range.GetText());
  }
}

TEST_P(PDFiumRangeTest, Forward) {
  PDFiumPage page(engine(), 0);
  page.MarkAvailable();
  {
    PDFiumRange range(&page, /*char_index=*/0, /*char_count=*/2);
    EXPECT_EQ(0, range.char_index());
    EXPECT_EQ(2, range.char_count());
    EXPECT_EQ(u"He", range.GetText());
  }
  {
    PDFiumRange range(&page, /*char_index=*/15, /*char_count=*/3);
    EXPECT_EQ(15, range.char_index());
    EXPECT_EQ(3, range.char_count());
    EXPECT_EQ(u"Goo", range.GetText());
  }
  {
    PDFiumRange range(&page, /*char_index=*/28, /*char_count=*/2);
    EXPECT_EQ(28, range.char_index());
    EXPECT_EQ(2, range.char_count());
    EXPECT_EQ(u"d!", range.GetText());
  }
  {
    PDFiumRange range(&page, /*char_index=*/29, /*char_count=*/1);
    EXPECT_EQ(29, range.char_index());
    EXPECT_EQ(1, range.char_count());
    EXPECT_EQ(u"!", range.GetText());
  }
}

TEST_P(PDFiumRangeTest, Backward) {
  PDFiumPage page(engine(), 0);
  page.MarkAvailable();
  {
    PDFiumRange range(&page, /*char_index=*/1, /*char_count=*/-2);
    EXPECT_EQ(1, range.char_index());
    EXPECT_EQ(-2, range.char_count());
    EXPECT_EQ(u"He", range.GetText());
  }
  {
    PDFiumRange range(&page, /*char_index=*/17, /*char_count=*/-3);
    EXPECT_EQ(17, range.char_index());
    EXPECT_EQ(-3, range.char_count());
    EXPECT_EQ(u"Goo", range.GetText());
  }
  {
    PDFiumRange range(&page, /*char_index=*/29, /*char_count=*/-2);
    EXPECT_EQ(29, range.char_index());
    EXPECT_EQ(-2, range.char_count());
    EXPECT_EQ(u"d!", range.GetText());
  }
  {
    PDFiumRange range(&page, /*char_index=*/29, /*char_count=*/-1);
    EXPECT_EQ(29, range.char_index());
    EXPECT_EQ(-1, range.char_count());
    EXPECT_EQ(u"!", range.GetText());
  }
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumRangeTest, testing::Bool());

}  // namespace chrome_pdf
