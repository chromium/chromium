// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using testing::_;
using testing::InSequence;

namespace chrome_pdf {

namespace {

class FindTextTestClient : public TestClient {
 public:
  FindTextTestClient() = default;
  FindTextTestClient(const FindTextTestClient&) = delete;
  FindTextTestClient& operator=(const FindTextTestClient&) = delete;
  ~FindTextTestClient() override = default;

  // PDFEngine::Client:
  MOCK_METHOD(void, NotifyNumberOfFindResultsChanged, (int, bool), (override));
  MOCK_METHOD(void, NotifySelectedFindResultChanged, (int), (override));

  std::vector<SearchStringResult> SearchString(const char16_t* string,
                                               const char16_t* term,
                                               bool case_sensitive) override {
    EXPECT_TRUE(case_sensitive);
    std::u16string haystack = std::u16string(string);
    std::u16string needle = std::u16string(term);

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
};

}  // namespace

using FindTextTest = PDFiumTestBase;

TEST_F(FindTextTest, FindText) {
  FindTextTestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("hello_world2.pdf"));
  ASSERT_TRUE(engine);

  {
    InSequence sequence;

    EXPECT_CALL(client, NotifyNumberOfFindResultsChanged(1, false));
    EXPECT_CALL(client, NotifySelectedFindResultChanged(0));
    for (int i = 1; i < 10; ++i)
      EXPECT_CALL(client, NotifyNumberOfFindResultsChanged(i + 1, false));
    EXPECT_CALL(client, NotifyNumberOfFindResultsChanged(10, true));
  }

  engine->StartFind("o", /*case_sensitive=*/true);
}

TEST_F(FindTextTest, FindHyphenatedText) {
  FindTextTestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("spanner.pdf"));
  ASSERT_TRUE(engine);

  {
    InSequence sequence;

    EXPECT_CALL(client, NotifyNumberOfFindResultsChanged(1, false));
    EXPECT_CALL(client, NotifySelectedFindResultChanged(0));
    for (int i = 1; i < 6; ++i)
      EXPECT_CALL(client, NotifyNumberOfFindResultsChanged(i + 1, false));
    EXPECT_CALL(client, NotifyNumberOfFindResultsChanged(6, true));
  }

  engine->StartFind("application", /*case_sensitive=*/true);
}

TEST_F(FindTextTest, FindLineBreakText) {
  FindTextTestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("spanner.pdf"));
  ASSERT_TRUE(engine);

  {
    InSequence sequence;

    EXPECT_CALL(client, NotifyNumberOfFindResultsChanged(1, false));
    EXPECT_CALL(client, NotifySelectedFindResultChanged(0));
    EXPECT_CALL(client, NotifyNumberOfFindResultsChanged(1, true));
  }

  engine->StartFind("is the first system", /*case_sensitive=*/true);
}

TEST_F(FindTextTest, FindSimpleQuotationMarkText) {
  FindTextTestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("bug_142627.pdf"));
  ASSERT_TRUE(engine);

  {
    InSequence sequence;

    EXPECT_CALL(client, NotifyNumberOfFindResultsChanged(1, false));
    EXPECT_CALL(client, NotifySelectedFindResultChanged(0));
    EXPECT_CALL(client, NotifyNumberOfFindResultsChanged(2, false));
    EXPECT_CALL(client, NotifyNumberOfFindResultsChanged(2, true));
  }

  engine->StartFind("don't", /*case_sensitive=*/true);
}

TEST_F(FindTextTest, FindFancyQuotationMarkText) {
  FindTextTestClient client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("bug_142627.pdf"));
  ASSERT_TRUE(engine);

  {
    InSequence sequence;

    EXPECT_CALL(client, NotifyNumberOfFindResultsChanged(1, false));
    EXPECT_CALL(client, NotifySelectedFindResultChanged(0));
    EXPECT_CALL(client, NotifyNumberOfFindResultsChanged(2, false));
    EXPECT_CALL(client, NotifyNumberOfFindResultsChanged(2, true));
  }

  // don't, using right apostrophe instead of a single quotation mark
  std::u16string term = {'d', 'o', 'n', 0x2019, 't'};
  engine->StartFind(base::UTF16ToUTF8(term), /*case_sensitive=*/true);
}

}  // namespace chrome_pdf
