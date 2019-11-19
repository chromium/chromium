// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_engine.h"

#include "pdf/document_layout.h"
#include "pdf/pdfium/pdfium_page.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "pdf/test/test_utils.h"
#include "ppapi/cpp/size.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_pdf {
namespace {

using ::testing::InSequence;
using ::testing::NiceMock;
using ::testing::Return;

MATCHER_P2(LayoutWithSize, width, height, "") {
  return arg.size() == pp::Size(width, height);
}

class MockTestClient : public TestClient {
 public:
  MockTestClient() {
    ON_CALL(*this, ProposeDocumentLayout)
        .WillByDefault([this](const DocumentLayout& layout) {
          TestClient::ProposeDocumentLayout(layout);
        });
  }

  // TODO(crbug.com/989095): MOCK_METHOD() triggers static_assert on Windows.
  MOCK_METHOD1(ProposeDocumentLayout, void(const DocumentLayout& layout));
  MOCK_METHOD1(ScrollToPage, void(int page));
};

class PDFiumEngineTest : public PDFiumTestBase {
 protected:
  void ExpectPageRect(PDFiumEngine* engine,
                      size_t page_index,
                      const pp::Rect& expected_rect) {
    PDFiumPage* page = GetPDFiumPageForTest(engine, page_index);
    ASSERT_TRUE(page);
    CompareRect(expected_rect, page->rect());
  }
};

TEST_F(PDFiumEngineTest, InitializeWithRectanglesMultiPagesPdf) {
  NiceMock<MockTestClient> client;

  // ProposeDocumentLayout() gets called twice during loading because
  // PDFiumEngine::ContinueLoadingDocument() calls LoadBody() (which eventually
  // triggers a layout proposal), and then calls FinishLoadingDocument() (since
  // the document is complete), which calls LoadBody() again. Coalescing these
  // proposals is not correct unless we address the issue covered by
  // PDFiumEngineTest.ProposeDocumentLayoutWithOverlap.
  EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(343, 1664)))
      .Times(2);

  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);
  ASSERT_EQ(5, engine->GetNumberOfPages());

  ExpectPageRect(engine.get(), 0, {38, 3, 266, 333});
  ExpectPageRect(engine.get(), 1, {5, 350, 333, 266});
  ExpectPageRect(engine.get(), 2, {38, 630, 266, 333});
  ExpectPageRect(engine.get(), 3, {38, 977, 266, 333});
  ExpectPageRect(engine.get(), 4, {38, 1324, 266, 333});
}

TEST_F(PDFiumEngineTest, AppendBlankPagesWithFewerPages) {
  NiceMock<MockTestClient> client;
  {
    InSequence normal_then_append;
    EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(343, 1664)))
        .Times(2);
    EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(276, 1037)));
  }

  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);

  engine->AppendBlankPages(3);
  ASSERT_EQ(3, engine->GetNumberOfPages());

  ExpectPageRect(engine.get(), 0, {5, 3, 266, 333});
  ExpectPageRect(engine.get(), 1, {5, 350, 266, 333});
  ExpectPageRect(engine.get(), 2, {5, 697, 266, 333});
}

TEST_F(PDFiumEngineTest, AppendBlankPagesWithMorePages) {
  NiceMock<MockTestClient> client;
  {
    InSequence normal_then_append;
    EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(343, 1664)))
        .Times(2);
    EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(276, 2425)));
  }

  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);

  engine->AppendBlankPages(7);
  ASSERT_EQ(7, engine->GetNumberOfPages());

  ExpectPageRect(engine.get(), 0, {5, 3, 266, 333});
  ExpectPageRect(engine.get(), 1, {5, 350, 266, 333});
  ExpectPageRect(engine.get(), 2, {5, 697, 266, 333});
  ExpectPageRect(engine.get(), 3, {5, 1044, 266, 333});
  ExpectPageRect(engine.get(), 4, {5, 1391, 266, 333});
  ExpectPageRect(engine.get(), 5, {5, 1738, 266, 333});
  ExpectPageRect(engine.get(), 6, {5, 2085, 266, 333});
}

TEST_F(PDFiumEngineTest, ProposeDocumentLayoutWithOverlap) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);

  EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(343, 1463)))
      .WillOnce(Return());
  engine->RotateClockwise();

  EXPECT_CALL(client, ProposeDocumentLayout(LayoutWithSize(343, 1664)))
      .WillOnce(Return());
  engine->RotateCounterclockwise();
}

TEST_F(PDFiumEngineTest, ApplyDocumentLayoutAvoidsInfiniteLoop) {
  NiceMock<MockTestClient> client;
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("rectangles_multi_pages.pdf"));
  ASSERT_TRUE(engine);

  DocumentLayout::Options options;
  EXPECT_CALL(client, ScrollToPage(-1)).Times(0);
  CompareSize({343, 1664}, engine->ApplyDocumentLayout(options));

  options.RotatePagesClockwise();
  EXPECT_CALL(client, ScrollToPage(-1)).Times(1);
  CompareSize({343, 1463}, engine->ApplyDocumentLayout(options));
  CompareSize({343, 1463}, engine->ApplyDocumentLayout(options));
}

}  // namespace
}  // namespace chrome_pdf
