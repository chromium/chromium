// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>

#include "base/check_op.h"
#include "base/strings/utf_string_conversions.h"
#include "pdf/document_layout.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;
using testing::InSequence;

namespace chrome_pdf {

namespace {

class FindTextTestClient : public TestClient {
 public:
  explicit FindTextTestClient(bool expected_case_sensitive)
      : expected_case_sensitive_(expected_case_sensitive) {}
  FindTextTestClient(const FindTextTestClient&) = delete;
  FindTextTestClient& operator=(const FindTextTestClient&) = delete;
  ~FindTextTestClient() override = default;

  // PDFiumEngineClient:
  MOCK_METHOD(void, NotifyNumberOfFindResultsChanged, (int, bool), (override));
  MOCK_METHOD(void, NotifySelectedFindResultChanged, (int, bool), (override));

  std::vector<SearchStringResult> SearchString(const char16_t* string,
                                               const char16_t* term,
                                               bool case_sensitive) override {
    EXPECT_EQ(case_sensitive, expected_case_sensitive_);
    std::u16string haystack = std::u16string(string);
    std::u16string needle = std::u16string(term);
    EXPECT_FALSE(haystack.empty());
    EXPECT_FALSE(needle.empty());

    std::vector<SearchStringResult> results;

    size_t pos = 0;
    while (true) {
      pos = haystack.find(needle, pos);
      if (pos == std::u16string::npos)
        break;

      SearchStringResult result;
      result.length = needle.size();
      result.start_index = pos;
      results.push_back(result);
      pos += needle.size();
    }
    return results;
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
  FindTextTestClient client(/*expected_case_sensitive=*/true);
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  ExpectInitialSearchResults(client, 10);
  engine->StartFind(u"o", /*case_sensitive=*/true);
}

TEST_P(FindTextTest, FindHyphenatedText) {
  FindTextTestClient client(/*expected_case_sensitive=*/true);
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("spanner.pdf"));
  ASSERT_TRUE(engine);

  ExpectInitialSearchResults(client, 6);
  engine->StartFind(u"application", /*case_sensitive=*/true);
}

TEST_P(FindTextTest, FindLineBreakText) {
  FindTextTestClient client(/*expected_case_sensitive=*/true);
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("spanner.pdf"));
  ASSERT_TRUE(engine);

  ExpectInitialSearchResults(client, 1);
  engine->StartFind(u"is the first system", /*case_sensitive=*/true);
}

TEST_P(FindTextTest, FindSimpleQuotationMarkText) {
  FindTextTestClient client(/*expected_case_sensitive=*/true);
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("bug_142627.pdf"));
  ASSERT_TRUE(engine);

  ExpectInitialSearchResults(client, 2);
  engine->StartFind(u"don't", /*case_sensitive=*/true);
}

TEST_P(FindTextTest, FindFancyQuotationMarkText) {
  FindTextTestClient client(/*expected_case_sensitive=*/true);
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("bug_142627.pdf"));
  ASSERT_TRUE(engine);

  ExpectInitialSearchResults(client, 2);

  // don't, using right apostrophe instead of a single quotation mark
  engine->StartFind(u"don\u2019t", /*case_sensitive=*/true);
}

TEST_P(FindTextTest, FindHiddenCroppedText) {
  FindTextTestClient client(/*expected_case_sensitive=*/true);
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world_cropped.pdf"));
  ASSERT_TRUE(engine);

  // The word "Hello" is cropped out.
  ExpectInitialSearchResults(client, 0);
  engine->StartFind(u"Hello", /*case_sensitive=*/true);
}

TEST_P(FindTextTest, FindVisibleCroppedText) {
  FindTextTestClient client(/*expected_case_sensitive=*/true);
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world_cropped.pdf"));
  ASSERT_TRUE(engine);

  // Only one instance of the word "world" is visible. The other is cropped out.
  ExpectInitialSearchResults(client, 1);
  engine->StartFind(u"world", /*case_sensitive=*/true);
}

TEST_P(FindTextTest, FindVisibleCroppedTextRepeatedly) {
  FindTextTestClient client(/*expected_case_sensitive=*/true);
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world_cropped.pdf"));
  ASSERT_TRUE(engine);

  // Only one instance of the word "world" is visible. The other is cropped out.
  // These 2 find operations should not trigger https://crbug.com/1344057.
  ExpectInitialSearchResults(client, 1);
  engine->StartFind(u"worl", /*case_sensitive=*/true);
  ExpectInitialSearchResults(client, 1);
  engine->StartFind(u"world", /*case_sensitive=*/true);
}

TEST_P(FindTextTest, SelectFindResult) {
  FindTextTestClient client(/*expected_case_sensitive=*/true);
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
  FindTextTestClient client(/*expected_case_sensitive=*/false);
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

INSTANTIATE_TEST_SUITE_P(All, FindTextTest, testing::Bool());

}  // namespace chrome_pdf
