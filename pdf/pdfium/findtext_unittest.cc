// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "pdf/buildflags.h"
#include "pdf/document_layout.h"
#include "pdf/pdfium/pdfium_draw_selection_test_base.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "pdf/text_search.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::InSequence;
using testing::IsEmpty;
using testing::NiceMock;
using testing::Return;

namespace chrome_pdf {

namespace {

MATCHER_P4(PDFiumRangeEq, page_index, char_index, char_count, text, "") {
  return arg.page_index() == page_index && arg.char_index() == char_index &&
         arg.char_count() == char_count && arg.GetText() == text;
}

class FindTextTestClient : public TestClient {
 public:
  FindTextTestClient(bool expected_case_sensitive, bool use_skia_renderer)
      : TestClient(use_skia_renderer),
        expected_case_sensitive_(expected_case_sensitive) {}
  FindTextTestClient(const FindTextTestClient&) = delete;
  FindTextTestClient& operator=(const FindTextTestClient&) = delete;
  ~FindTextTestClient() override = default;

  // PDFiumEngineClient:
  MOCK_METHOD(void, NotifyNumberOfFindResultsChanged, (int, bool), (override));
  MOCK_METHOD(void, NotifySelectedFindResultChanged, (int, bool), (override));
#if BUILDFLAG(ENABLE_PDF_INK2)
  MOCK_METHOD(bool, IsInAnnotationMode, (), (const override));
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

  std::vector<SearchStringResult> SearchString(const std::u16string& needle,
                                               const std::u16string& haystack,
                                               bool case_sensitive) override {
    EXPECT_FALSE(needle.empty());
    EXPECT_FALSE(haystack.empty());
    EXPECT_EQ(case_sensitive, expected_case_sensitive_);
    return TextSearch(/*needle=*/needle, /*haystack=*/haystack, case_sensitive);
  }

 private:
  const bool expected_case_sensitive_;
};

void ExpectInitialSearchResults(FindTextTestClient& client, int count) {
  DCHECK_GE(count, 0);

  if (count == 0) {
    EXPECT_CALL(client,
                NotifyNumberOfFindResultsChanged(0, /*final_result=*/true));
    return;
  }

  InSequence sequence;

  EXPECT_CALL(client,
              NotifyNumberOfFindResultsChanged(1, /*final_result=*/false));
  for (int i = 2; i < count + 1; ++i) {
    EXPECT_CALL(client,
                NotifyNumberOfFindResultsChanged(i, /*final_result=*/false));
  }
  EXPECT_CALL(client,
              NotifyNumberOfFindResultsChanged(count, /*final_result=*/true));
}

}  // namespace

using FindTextTest = PDFiumTestBase;

TEST_P(FindTextTest, FindText) {
  FindTextTestClient client(/*expected_case_sensitive=*/true,
                            /*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  ExpectInitialSearchResults(client, 10);
  engine->StartFind(u"o", /*case_sensitive=*/true);
  const auto kExpected = {PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/4,
                                        /*char_count=*/1, u"o"),
                          PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/8,
                                        /*char_count=*/1, u"o"),
                          PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/16,
                                        /*char_count=*/1, u"o"),
                          PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/17,
                                        /*char_count=*/1, u"o"),
                          PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/25,
                                        /*char_count=*/1, u"o"),
                          PDFiumRangeEq(/*page_index=*/1u, /*char_index=*/4,
                                        /*char_count=*/1, u"o"),
                          PDFiumRangeEq(/*page_index=*/1u, /*char_index=*/8,
                                        /*char_count=*/1, u"o"),
                          PDFiumRangeEq(/*page_index=*/1u, /*char_index=*/16,
                                        /*char_count=*/1, u"o"),
                          PDFiumRangeEq(/*page_index=*/1u, /*char_index=*/17,
                                        /*char_count=*/1, u"o"),
                          PDFiumRangeEq(/*page_index=*/1u, /*char_index=*/25,
                                        /*char_count=*/1, u"o")};
  EXPECT_THAT(engine->find_results_for_testing(), ElementsAreArray(kExpected));
}

TEST_P(FindTextTest, FindHyphenatedText) {
  FindTextTestClient client(/*expected_case_sensitive=*/true,
                            /*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("spanner.pdf"));
  ASSERT_TRUE(engine);

  ExpectInitialSearchResults(client, 6);
  engine->StartFind(u"application", /*case_sensitive=*/true);
  const auto kExpected = {PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/2170,
                                        /*char_count=*/11, u"application"),
                          PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/2334,
                                        /*char_count=*/11, u"application"),
                          PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/2848,
                                        /*char_count=*/12, u"application"),
                          PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/3049,
                                        /*char_count=*/11, u"application"),
                          PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/3591,
                                        /*char_count=*/11, u"application"),
                          PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/3910,
                                        /*char_count=*/11, u"application")};
  EXPECT_THAT(engine->find_results_for_testing(), ElementsAreArray(kExpected));
}

TEST_P(FindTextTest, FindLineBreakText) {
  FindTextTestClient client(/*expected_case_sensitive=*/true,
                            /*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("spanner.pdf"));
  ASSERT_TRUE(engine);

  ExpectInitialSearchResults(client, 1);
  engine->StartFind(u"is the first system", /*case_sensitive=*/true);
  EXPECT_THAT(
      engine->find_results_for_testing(),
      ElementsAre(PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/587,
                                /*char_count=*/20, u"is\r\nthe first system")));
}

TEST_P(FindTextTest, FindSimpleQuotationMarkText) {
  FindTextTestClient client(/*expected_case_sensitive=*/true,
                            /*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("bug_142627.pdf"));
  ASSERT_TRUE(engine);

  ExpectInitialSearchResults(client, 2);
  engine->StartFind(u"don't", /*case_sensitive=*/true);
  const auto kExpected = {PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/0,
                                        /*char_count=*/6, u"don't\r"),
                          PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/7,
                                        /*char_count=*/5, u"don\u2019t")};
  EXPECT_THAT(engine->find_results_for_testing(), ElementsAreArray(kExpected));
}

TEST_P(FindTextTest, FindFancyQuotationMarkText) {
  FindTextTestClient client(/*expected_case_sensitive=*/true,
                            /*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("bug_142627.pdf"));
  ASSERT_TRUE(engine);

  ExpectInitialSearchResults(client, 2);

  // don't, using right apostrophe instead of a single quotation mark
  engine->StartFind(u"don\u2019t", /*case_sensitive=*/true);
  const auto kExpected = {PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/0,
                                        /*char_count=*/6, u"don't\r"),
                          PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/7,
                                        /*char_count=*/5, u"don\u2019t")};
  EXPECT_THAT(engine->find_results_for_testing(), ElementsAreArray(kExpected));
}

TEST_P(FindTextTest, FindHiddenCroppedText) {
  FindTextTestClient client(/*expected_case_sensitive=*/true,
                            /*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world_cropped.pdf"));
  ASSERT_TRUE(engine);

  // The word "Hello" is cropped out.
  ExpectInitialSearchResults(client, 0);
  engine->StartFind(u"Hello", /*case_sensitive=*/true);
  EXPECT_THAT(engine->find_results_for_testing(), IsEmpty());
}

TEST_P(FindTextTest, FindVisibleCroppedText) {
  FindTextTestClient client(/*expected_case_sensitive=*/true,
                            /*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world_cropped.pdf"));
  ASSERT_TRUE(engine);

  // Only one instance of the word "world" is visible. The other is cropped out.
  ExpectInitialSearchResults(client, 1);
  engine->StartFind(u"world", /*case_sensitive=*/true);
  EXPECT_THAT(engine->find_results_for_testing(),
              ElementsAre(PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/7,
                                        /*char_count=*/5, u"world")));
}

TEST_P(FindTextTest, FindVisibleCroppedTextRepeatedly) {
  FindTextTestClient client(/*expected_case_sensitive=*/true,
                            /*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world_cropped.pdf"));
  ASSERT_TRUE(engine);

  // Only one instance of the word "world" is visible. The other is cropped out.
  // These 2 find operations should not trigger https://crbug.com/1344057.
  ExpectInitialSearchResults(client, 1);
  engine->StartFind(u"worl", /*case_sensitive=*/true);
  EXPECT_THAT(engine->find_results_for_testing(),
              ElementsAre(PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/7,
                                        /*char_count=*/4, u"worl")));
  ExpectInitialSearchResults(client, 1);
  engine->StartFind(u"world", /*case_sensitive=*/true);
  EXPECT_THAT(engine->find_results_for_testing(),
              ElementsAre(PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/7,
                                        /*char_count=*/5, u"world")));
}

TEST_P(FindTextTest, FindFirstLetterInPdfWithUnprintableChar) {
  FindTextTestClient client(/*expected_case_sensitive=*/true,
                            /*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("hello_world_unprintable_char.pdf"));
  ASSERT_TRUE(engine);

  ExpectInitialSearchResults(client, 1);
  engine->StartFind(u"H", /*case_sensitive=*/true);
  EXPECT_THAT(engine->find_results_for_testing(),
              ElementsAre(PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/0,
                                        /*char_count=*/1, u"H")));
}

TEST_P(FindTextTest, FindNormalWordInPdfWithUnprintableChar) {
  FindTextTestClient client(/*expected_case_sensitive=*/true,
                            /*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("hello_world_unprintable_char.pdf"));
  ASSERT_TRUE(engine);

  ExpectInitialSearchResults(client, 1);
  engine->StartFind(u"Goodbye", /*case_sensitive=*/true);
  EXPECT_THAT(engine->find_results_for_testing(),
              ElementsAre(PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/17,
                                        /*char_count=*/7, u"Goodbye")));
}

TEST_P(FindTextTest, FindWordAtTheEndInPdfWithUnprintableChar) {
  FindTextTestClient client(/*expected_case_sensitive=*/true,
                            /*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("hello_world_unprintable_char.pdf"));
  ASSERT_TRUE(engine);

  ExpectInitialSearchResults(client, 2);
  engine->StartFind(u"world!", /*case_sensitive=*/true);
  const auto kExpected = {PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/9,
                                        /*char_count=*/7, u"world!\r"),
                          PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/26,
                                        /*char_count=*/6, u"world!")};
  EXPECT_THAT(engine->find_results_for_testing(), ElementsAreArray(kExpected));
}

TEST_P(FindTextTest, FindWordWithUnprintableCharInPdfWithUnprintableChar) {
  FindTextTestClient client(/*expected_case_sensitive=*/true,
                            /*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine = InitializeEngine(
      &client, FILE_PATH_LITERAL("hello_world_unprintable_char.pdf"));
  ASSERT_TRUE(engine);

  ExpectInitialSearchResults(client, 1);
  engine->StartFind(u"Hello", /*case_sensitive=*/true);
  EXPECT_THAT(engine->find_results_for_testing(),
              ElementsAre(PDFiumRangeEq(/*page_index=*/0u, /*char_index=*/0,
                                        /*char_count=*/7, u"Hello")));
}

TEST_P(FindTextTest, SelectFindResult) {
  NiceMock<FindTextTestClient> client(/*expected_case_sensitive=*/true,
                                      /*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  ExpectInitialSearchResults(client, 4);
  engine->StartFind(u"world", /*case_sensitive=*/true);
  ASSERT_TRUE(engine->SelectFindResult(/*forward=*/true));

  EXPECT_CALL(client, NotifyNumberOfFindResultsChanged(_, _)).Times(0);
  EXPECT_CALL(client,
              NotifySelectedFindResultChanged(1, /*final_result=*/true));

  ASSERT_TRUE(engine->SelectFindResult(/*forward=*/true));

  EXPECT_CALL(client,
              NotifySelectedFindResultChanged(2, /*final_result=*/true));
  ASSERT_TRUE(engine->SelectFindResult(/*forward=*/true));

  EXPECT_CALL(client,
              NotifySelectedFindResultChanged(1, /*final_result=*/true));
  ASSERT_TRUE(engine->SelectFindResult(/*forward=*/false));
}

TEST_P(FindTextTest, SelectFindResultAndSwitchToTwoUpView) {
  FindTextTestClient client(/*expected_case_sensitive=*/false,
                            /*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  ExpectInitialSearchResults(client, 4);
  engine->StartFind(u"world", /*case_sensitive=*/false);

  {
    InSequence sequence;

    EXPECT_CALL(client,
                NotifySelectedFindResultChanged(0, /*final_result=*/true));
    EXPECT_CALL(client,
                NotifySelectedFindResultChanged(1, /*final_result=*/true));
  }
  ASSERT_TRUE(engine->SelectFindResult(/*forward=*/true));
  ASSERT_TRUE(engine->SelectFindResult(/*forward=*/true));

  {
    InSequence sequence;

    for (int i = 0; i < 5; ++i) {
      EXPECT_CALL(client,
                  NotifyNumberOfFindResultsChanged(i, /*final_result=*/false));
    }
    EXPECT_CALL(client,
                NotifyNumberOfFindResultsChanged(4, /*final_result=*/true));
  }
  engine->SetDocumentLayout(DocumentLayout::PageSpread::kTwoUpOdd);

  {
    InSequence sequence;

    EXPECT_CALL(client,
                NotifySelectedFindResultChanged(1, /*final_result=*/true));
  }
  ASSERT_TRUE(engine->SelectFindResult(/*forward=*/true));
}

using FindTextDrawSelectionTest = PDFiumDrawSelectionTestBase;

TEST_P(FindTextDrawSelectionTest, DrawFindResult) {
  NiceMock<FindTextTestClient> client(/*expected_case_sensitive=*/false,
                                      /*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  // Update the plugin size so that all the text is visible.
  engine->PluginSizeUpdated({500, 500});

  constexpr int kPageIndex = 0;
  DrawAndExpectBlank(*engine, kPageIndex,
                     /*expected_visible_page_size=*/gfx::Size(266, 266));

  engine->StartFind(u"o", /*case_sensitive=*/false);
  EXPECT_TRUE(engine->SelectFindResult(/*forward=*/true));

  EXPECT_THAT(engine->GetSelectedText(), testing::IsEmpty());
  DrawSelectionAndCompareWithPlatformExpectations(
      *engine, kPageIndex, "hello_world_draw_find_result_0.png");

  EXPECT_TRUE(engine->SelectFindResult(/*forward=*/true));

  EXPECT_THAT(engine->GetSelectedText(), testing::IsEmpty());
  DrawSelectionAndCompareWithPlatformExpectations(
      *engine, kPageIndex, "hello_world_draw_find_result_1.png");

  SetSelection(*engine, /*start_page_index=*/kPageIndex, /*start_char_index=*/1,
               /*end_page_index=*/kPageIndex, /*end_char_index=*/2);

  EXPECT_EQ("e", engine->GetSelectedText());
  DrawSelectionAndCompareWithPlatformExpectations(
      *engine, kPageIndex, "hello_world_draw_find_result_2.png");
}

#if BUILDFLAG(ENABLE_PDF_INK2)
TEST_P(FindTextDrawSelectionTest, DrawFindResultInAnnotationMode) {
  NiceMock<FindTextTestClient> client(/*expected_case_sensitive=*/false,
                                      /*use_skia_renderer=*/GetParam());
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  EXPECT_CALL(client, IsInAnnotationMode()).WillRepeatedly(Return(true));

  // Update the plugin size so that all the text is visible.
  engine->PluginSizeUpdated({500, 500});

  constexpr int kPageIndex = 0;
  DrawAndExpectBlank(*engine, kPageIndex,
                     /*expected_visible_page_size=*/gfx::Size(266, 266));

  engine->StartFind(u"o", /*case_sensitive=*/false);
  EXPECT_TRUE(engine->SelectFindResult(/*forward=*/true));

  EXPECT_THAT(engine->GetSelectedText(), testing::IsEmpty());
  DrawSelectionAndCompareWithPlatformExpectations(
      *engine, kPageIndex, "hello_world_draw_find_result_0.png");

  // Set selected text. It should not be highlighted while in annotation mode.
  SetSelection(*engine, /*start_page_index=*/kPageIndex, /*start_char_index=*/1,
               /*end_page_index=*/kPageIndex, /*end_char_index=*/2);

  EXPECT_EQ("e", engine->GetSelectedText());
  DrawSelectionAndCompareWithPlatformExpectations(
      *engine, kPageIndex, "hello_world_draw_find_result_0.png");
}
#endif  // BUILDFLAG(ENABLE_PDF_INK2)

INSTANTIATE_TEST_SUITE_P(All, FindTextTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(All, FindTextDrawSelectionTest, testing::Bool());

}  // namespace chrome_pdf
