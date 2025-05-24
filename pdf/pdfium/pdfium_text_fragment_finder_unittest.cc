// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_text_fragment_finder.h"

#include <string>

#include "base/containers/span.h"
#include "pdf/pdfium/pdfium_engine.h"
#include "pdf/pdfium/pdfium_test_base.h"
#include "pdf/test/test_client.h"
#include "pdf/text_search.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chrome_pdf {

namespace {

using ::testing::NiceMock;

class SearchStringTestClient : public TestClient {
 public:
  std::vector<SearchStringResult> SearchString(const std::u16string& needle,
                                               const std::u16string& haystack,
                                               bool case_sensitive) override {
    EXPECT_FALSE(needle.empty());
    EXPECT_FALSE(haystack.empty());
    return TextSearch(/*needle=*/needle, /*haystack=*/haystack, case_sensitive);
  }
};

}  // namespace

using PDFiumTextFragmentFinderTest = PDFiumTestBase;

TEST_P(PDFiumTextFragmentFinderTest, OnlyTextStart) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("spanner.pdf"));
  ASSERT_TRUE(engine);

  PDFiumTextFragmentFinder finder(engine.get());
  const auto highlights = finder.FindTextFragments({"Google"});
  ASSERT_EQ(highlights.size(), 1u);

  static constexpr std::u16string_view kExpectedString = u"Google";
  const auto& range = highlights[0];
  EXPECT_EQ(range.GetText(), kExpectedString);
  EXPECT_EQ(range.char_count(), static_cast<int>(kExpectedString.size()));
  EXPECT_EQ(range.char_index(), 9);
  EXPECT_EQ(range.page_index(), 0);
}

TEST_P(PDFiumTextFragmentFinderTest, TextStartAndEnd) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("spanner.pdf"));
  ASSERT_TRUE(engine);

  PDFiumTextFragmentFinder finder(engine.get());
  const auto highlights = finder.FindTextFragments({"spanner,database"});
  ASSERT_EQ(highlights.size(), 1u);

  static constexpr std::u16string_view kExpectedString =
      u"Spanner: Google\x2019s Globally-Distributed Database\r";
  const auto& range = highlights[0];
  EXPECT_EQ(range.GetText(), kExpectedString);
  EXPECT_EQ(range.char_count(), static_cast<int>(kExpectedString.size()));
  EXPECT_EQ(range.char_index(), 0);
  EXPECT_EQ(range.page_index(), 0);
}

TEST_P(PDFiumTextFragmentFinderTest, TextStartAndTextSuffix) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("spanner.pdf"));
  ASSERT_TRUE(engine);

  PDFiumTextFragmentFinder finder(engine.get());
  const auto highlights = finder.FindTextFragments({"how,-many"});
  ASSERT_EQ(highlights.size(), 1u);

  static constexpr std::u16string_view kExpectedString = u"how";
  const auto& range = highlights[0];
  EXPECT_EQ(range.GetText(), kExpectedString);
  EXPECT_EQ(range.char_count(), static_cast<int>(kExpectedString.size()));
  EXPECT_EQ(range.char_index(), 4141);
  EXPECT_EQ(range.page_index(), 0);
}

TEST_P(PDFiumTextFragmentFinderTest, TextStartEndAndSuffix) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("spanner.pdf"));
  ASSERT_TRUE(engine);

  PDFiumTextFragmentFinder finder(engine.get());
  const auto highlights = finder.FindTextFragments({"this,api,-and"});
  ASSERT_EQ(highlights.size(), 1u);
  const auto& range = highlights[0];

  static constexpr std::u16string_view kExpectedString =
      u"This\r\npaper describes how Spanner is structured, its feature "
      u"set,\r\nthe rationale underlying various design decisions, and "
      u"a\r\nnovel time API that exposes clock uncertainty. This API\r";
  EXPECT_EQ(range.GetText(), kExpectedString);
  EXPECT_EQ(range.char_count(), static_cast<int>(kExpectedString.size()));
  EXPECT_EQ(range.char_index(), 704);
  EXPECT_EQ(range.page_index(), 0);
}

TEST_P(PDFiumTextFragmentFinderTest, TextPrefixAndTextStart) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("spanner.pdf"));
  ASSERT_TRUE(engine);

  PDFiumTextFragmentFinder finder(engine.get());
  const auto highlights = finder.FindTextFragments({"is-,Google"});
  ASSERT_EQ(highlights.size(), 1u);

  static constexpr std::u16string_view kExpectedString = u"Google";
  const auto& range = highlights[0];
  EXPECT_EQ(range.GetText(), kExpectedString);
  EXPECT_EQ(range.char_count(), static_cast<int>(kExpectedString.size()));
  EXPECT_EQ(range.char_index(), 489);
  EXPECT_EQ(range.page_index(), 0);
}

TEST_P(PDFiumTextFragmentFinderTest, TextPrefixStartAndSuffix) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("spanner.pdf"));
  ASSERT_TRUE(engine);

  PDFiumTextFragmentFinder finder(engine.get());
  const auto highlights = finder.FindTextFragments({"of-,Google,-'s"});
  ASSERT_EQ(highlights.size(), 1u);

  static constexpr std::u16string_view kExpectedString = u"Google";
  const auto& range = highlights[0];
  EXPECT_EQ(range.GetText(), kExpectedString);
  EXPECT_EQ(range.char_count(), static_cast<int>(kExpectedString.size()));
  EXPECT_EQ(range.char_index(), 2072);
  EXPECT_EQ(range.page_index(), 0);
}

TEST_P(PDFiumTextFragmentFinderTest, TextPrefixStartAndEnd) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("spanner.pdf"));
  ASSERT_TRUE(engine);

  PDFiumTextFragmentFinder finder(engine.get());
  const auto highlights =
      finder.FindTextFragments({"time-,api,implementation"});
  ASSERT_EQ(highlights.size(), 1u);
  const auto& range = highlights[0];

  static constexpr std::u16string_view kExpectedString =
      u"API that exposes clock uncertainty. This API\r\nand its "
      u"implementation";
  EXPECT_EQ(range.GetText(), kExpectedString);
  EXPECT_EQ(range.char_count(), static_cast<int>(kExpectedString.size()));
  EXPECT_EQ(range.char_index(), 840);
  EXPECT_EQ(range.page_index(), 0);
}

TEST_P(PDFiumTextFragmentFinderTest, TextPrefixStartEndAndSuffix) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("spanner.pdf"));
  ASSERT_TRUE(engine);

  PDFiumTextFragmentFinder finder(engine.get());
  const auto highlights =
      finder.FindTextFragments({"and-,applications,old,-timestamps"});
  ASSERT_EQ(highlights.size(), 1u);

  static constexpr std::u16string_view kExpectedString =
      u"applications can read data at old";
  const auto& range = highlights[0];
  EXPECT_EQ(range.GetText(), kExpectedString);
  EXPECT_EQ(range.char_count(), static_cast<int>(kExpectedString.size()));
  EXPECT_EQ(range.char_index(), 3591);
  EXPECT_EQ(range.page_index(), 0);
}

TEST_P(PDFiumTextFragmentFinderTest, MultipleTextFragments) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("spanner.pdf"));
  ASSERT_TRUE(engine);

  PDFiumTextFragmentFinder finder(engine.get());
  const auto highlights =
      finder.FindTextFragments({"Google", "is-,Google", "of-,Google,-'s",
                                "and-,applications,old,-timestamps"});
  ASSERT_EQ(highlights.size(), 4u);

  static constexpr std::u16string_view kExpectedString1 = u"Google";
  static constexpr std::u16string_view kExpectedString2 =
      u"applications can read data at old";
  auto range = highlights[0];
  EXPECT_EQ(range.GetText(), kExpectedString1);
  EXPECT_EQ(range.char_count(), static_cast<int>(kExpectedString1.size()));
  EXPECT_EQ(range.char_index(), 9);
  EXPECT_EQ(range.page_index(), 0);

  range = highlights[1];
  EXPECT_EQ(range.GetText(), kExpectedString1);
  EXPECT_EQ(range.char_count(), static_cast<int>(kExpectedString1.size()));
  EXPECT_EQ(range.char_index(), 489);
  EXPECT_EQ(range.page_index(), 0);

  range = highlights[2];
  EXPECT_EQ(range.GetText(), kExpectedString1);
  EXPECT_EQ(range.char_count(), static_cast<int>(kExpectedString1.size()));
  EXPECT_EQ(range.char_index(), 2072);
  EXPECT_EQ(range.page_index(), 0);

  range = highlights[3];
  EXPECT_EQ(range.GetText(), kExpectedString2);
  EXPECT_EQ(range.char_count(), static_cast<int>(kExpectedString2.size()));
  EXPECT_EQ(range.char_index(), 3591);
  EXPECT_EQ(range.page_index(), 0);
}

TEST_P(PDFiumTextFragmentFinderTest, MultiPage) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("link_annots.pdf"));
  ASSERT_TRUE(engine);

  PDFiumTextFragmentFinder finder(engine.get());
  const auto highlights =
      finder.FindTextFragments({"link", "1-,link,-with", "page,-in",
                                "second-,page", "second-,page,in,-document"});
  ASSERT_EQ(highlights.size(), 5u);

  static constexpr std::u16string_view kExpectedString1 = u"Link";
  static constexpr std::u16string_view kExpectedString2 = u"Page";
  static constexpr std::u16string_view kExpectedString3 = u"page\r";
  static constexpr std::u16string_view kExpectedString4 = u"Page in";
  auto range = highlights[0];
  EXPECT_EQ(range.GetText(), kExpectedString1);
  EXPECT_EQ(range.char_count(), static_cast<int>(kExpectedString1.size()));
  EXPECT_EQ(range.char_index(), 0);
  EXPECT_EQ(range.page_index(), 0);

  range = highlights[1];
  EXPECT_EQ(range.GetText(), kExpectedString1);
  EXPECT_EQ(range.char_count(), static_cast<int>(kExpectedString1.size()));
  EXPECT_EQ(range.char_index(), 27);
  EXPECT_EQ(range.page_index(), 0);

  range = highlights[2];
  EXPECT_EQ(range.GetText(), kExpectedString2);
  EXPECT_EQ(range.char_count(), static_cast<int>(kExpectedString2.size()));
  EXPECT_EQ(range.char_index(), 7);
  EXPECT_EQ(range.page_index(), 1);

  range = highlights[3];
  EXPECT_EQ(range.GetText(), kExpectedString3);
  EXPECT_EQ(range.char_count(), static_cast<int>(kExpectedString3.size()));
  EXPECT_EQ(range.char_index(), 59);
  EXPECT_EQ(range.page_index(), 0);

  range = highlights[4];
  EXPECT_EQ(range.GetText(), kExpectedString4);
  EXPECT_EQ(range.char_count(), static_cast<int>(kExpectedString4.size()));
  EXPECT_EQ(range.char_index(), 7);
  EXPECT_EQ(range.page_index(), 1);
}

TEST_P(PDFiumTextFragmentFinderTest, FragmentNotInPDF) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("spanner.pdf"));
  ASSERT_TRUE(engine);
  PDFiumTextFragmentFinder finder(engine.get());

  // Start is not present in PDF.
  auto highlights = finder.FindTextFragments({"apples"});
  EXPECT_TRUE(highlights.empty());

  // Prefix + start present, but suffix is not.
  highlights = finder.FindTextFragments({"of-,Google,-random"});
  EXPECT_TRUE(highlights.empty());

  // Prefix + start present, but text end is not.
  highlights = finder.FindTextFragments({"of-,Google,random"});
  EXPECT_TRUE(highlights.empty());

  // Prefix + start + end present, but suffix is not.
  highlights = finder.FindTextFragments({"and-,applications,old,-random"});
  EXPECT_TRUE(highlights.empty());

  // Start is present, but prefix is not.
  highlights = finder.FindTextFragments({"apples-,Google"});
  EXPECT_TRUE(highlights.empty());

  // Start is present, but suffix is not.
  highlights = finder.FindTextFragments({"Google,-random"});
  EXPECT_TRUE(highlights.empty());

  // Start is present but end is not.
  highlights = finder.FindTextFragments({"applications,random"});
  EXPECT_TRUE(highlights.empty());

  // Start and end are present, but suffix is not.
  highlights = finder.FindTextFragments({"applications,old,-random"});
  EXPECT_TRUE(highlights.empty());
}

TEST_P(PDFiumTextFragmentFinderTest, EmptyList) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("spanner.pdf"));
  ASSERT_TRUE(engine);

  PDFiumTextFragmentFinder finder(engine.get());
  const auto highlights = finder.FindTextFragments({});
  EXPECT_TRUE(highlights.empty());
}

TEST_P(PDFiumTextFragmentFinderTest,
       TextStartAndEnd_FindsCorrectInstanceOfStart) {
  NiceMock<SearchStringTestClient> client;
  std::unique_ptr<PDFiumEngine> engine =
      InitializeEngine(&client, FILE_PATH_LITERAL("link_annots.pdf"));
  ASSERT_TRUE(engine);

  PDFiumTextFragmentFinder finder(engine.get());
  // "second" appears on both pages of the PDF.
  const auto highlights = finder.FindTextFragments({"second,document"});
  ASSERT_EQ(highlights.size(), 1u);

  static constexpr std::u16string_view kExpectedString =
      u"Second Page in Document";
  const auto& range = highlights[0];
  EXPECT_EQ(range.GetText(), kExpectedString);
  EXPECT_EQ(range.char_count(), static_cast<int>(kExpectedString.size()));
  EXPECT_EQ(range.char_index(), 0);
  EXPECT_EQ(range.page_index(), 1);
}

INSTANTIATE_TEST_SUITE_P(All, PDFiumTextFragmentFinderTest, testing::Bool());

}  // namespace chrome_pdf
