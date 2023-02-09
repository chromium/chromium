// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/link_header.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

TEST(LinkHeaderTest, Empty) {
  String null_string;
  LinkHeaderSet null_header_set(null_string);
  ASSERT_EQ(null_header_set.size(), unsigned(0));
  String empty_string("");
  LinkHeaderSet empty_header_set(empty_string);
  ASSERT_EQ(empty_header_set.size(), unsigned(0));
}

struct SingleTestCase {
  const char* header_value;
  bool valid;
  const char* url;
  const char* rel;
  const char* as;
  const char* media;
  const char* fetch_priority;
} g_single_test_cases[] = {
    {"</images/cat.jpg>; rel=prefetch", true, "/images/cat.jpg", "prefetch", "",
     "", ""},
    {"</images/cat.jpg>;rel=prefetch", true, "/images/cat.jpg", "prefetch", "",
     "", ""},
    {"</images/cat.jpg>   ;rel=prefetch", true, "/images/cat.jpg", "prefetch",
     "", "", ""},
    {"</images/cat.jpg>   ;   rel=prefetch", true, "/images/cat.jpg",
     "prefetch", "", "", ""},
    {"< /images/cat.jpg>   ;   rel=prefetch", true, "/images/cat.jpg",
     "prefetch", "", "", ""},
    {"</images/cat.jpg >   ;   rel=prefetch", true, "/images/cat.jpg",
     "prefetch", "", "", ""},
    {"</images/cat.jpg wutwut>   ;   rel=prefetch", true,
     "/images/cat.jpg wutwut", "prefetch", "", "", ""},
    {"</images/cat.jpg wutwut  \t >   ;   rel=prefetch", true,
     "/images/cat.jpg wutwut", "prefetch", "", "", ""},
    {"</images/cat.jpg>; rel=prefetch   ", true, "/images/cat.jpg", "prefetch",
     "", "", ""},
    {"</images/cat.jpg>; Rel=prefetch   ", true, "/images/cat.jpg", "prefetch",
     "", "", ""},
    {"</images/cat.jpg>; Rel=PReFetCh   ", true, "/images/cat.jpg", "prefetch",
     "", "", ""},
    {"</images/cat.jpg>; rel=prefetch; rel=somethingelse", true,
     "/images/cat.jpg", "prefetch", "", "", ""},
    {"  </images/cat.jpg>; rel=prefetch   ", true, "/images/cat.jpg",
     "prefetch", "", "", ""},
    {"\t  </images/cat.jpg>; rel=prefetch   ", true, "/images/cat.jpg",
     "prefetch", "", "", ""},
    {"</images/cat.jpg>\t\t ; \trel=prefetch \t  ", true, "/images/cat.jpg",
     "prefetch", "", "", ""},
    {"\f</images/cat.jpg>\t\t ; \trel=prefetch \t  ", false},
    {"</images/cat.jpg>; rel= prefetch", true, "/images/cat.jpg", "prefetch",
     "", "", ""},
    {"<../images/cat.jpg?dog>; rel= prefetch", true, "../images/cat.jpg?dog",
     "prefetch", "", "", ""},
    {"</images/cat.jpg>; rel =prefetch", true, "/images/cat.jpg", "prefetch",
     "", "", ""},
    {"</images/cat.jpg>; rel pel=prefetch", false},
    {"< /images/cat.jpg>", true, "/images/cat.jpg", "", "", "", ""},
    {"</images/cat.jpg>; rel =", false},
    {"</images/cat.jpg>; wut=sup; rel =prefetch", true, "/images/cat.jpg",
     "prefetch", "", "", ""},
    {"</images/cat.jpg>; wut=sup ; rel =prefetch", true, "/images/cat.jpg",
     "prefetch", "", "", ""},
    {"</images/cat.jpg>; wut=sup ; rel =prefetch  \t  ;", true,
     "/images/cat.jpg", "prefetch", "", "", ""},
    {"</images/cat.jpg> wut=sup ; rel =prefetch  \t  ;", false},
    {"<   /images/cat.jpg", false},
    {"<   http://wut.com/  sdfsdf ?sd>; rel=dns-prefetch", true,
     "http://wut.com/  sdfsdf ?sd", "dns-prefetch", "", "", ""},
    {"<   http://wut.com/%20%20%3dsdfsdf?sd>; rel=dns-prefetch", true,
     "http://wut.com/%20%20%3dsdfsdf?sd", "dns-prefetch", "", "", ""},
    {"<   http://wut.com/dfsdf?sdf=ghj&wer=rty>; rel=prefetch", true,
     "http://wut.com/dfsdf?sdf=ghj&wer=rty", "prefetch", "", "", ""},
    {"<   http://wut.com/dfsdf?sdf=ghj&wer=rty>;;;;; rel=prefetch", true,
     "http://wut.com/dfsdf?sdf=ghj&wer=rty", "prefetch", "", "", ""},
    {"<   http://wut.com/%20%20%3dsdfsdf?sd>; rel=preload;as=image", true,
     "http://wut.com/%20%20%3dsdfsdf?sd", "preload", "image", "", ""},
    {"<   http://wut.com/%20%20%3dsdfsdf?sd>; rel=preload;as=whatever", true,
     "http://wut.com/%20%20%3dsdfsdf?sd", "preload", "whatever", "", ""},
    {"</images/cat.jpg>; anchor=foo; rel=prefetch;", false},
    {"</images/cat.jpg>; rel=prefetch;anchor=foo ", false},
    {"</images/cat.jpg>; anchor='foo'; rel=prefetch;", false},
    {"</images/cat.jpg>; rel=prefetch;anchor='foo' ", false},
    {"</images/cat.jpg>; rel=prefetch;anchor='' ", false},
    {"</images/cat.jpg>; rel=prefetch;", true, "/images/cat.jpg", "prefetch",
     "", "", ""},
    {"</images/cat.jpg>; rel=prefetch    ;", true, "/images/cat.jpg",
     "prefetch", "", "", ""},
    {"</images/ca,t.jpg>; rel=prefetch    ;", true, "/images/ca,t.jpg",
     "prefetch", "", "", ""},
    {"<simple.css>; rel=stylesheet; title=\"title with a DQUOTE and "
     "backslash\"",
     true, "simple.css", "stylesheet", "", "", ""},
    {"<simple.css>; rel=stylesheet; title=\"title with a DQUOTE \\\" and "
     "backslash: \\\"",
     false},
    {"<simple.css>; title=\"title with a DQUOTE \\\" and backslash: \"; "
     "rel=stylesheet; ",
     true, "simple.css", "stylesheet", "", "", ""},
    {"<simple.css>; title=\'title with a DQUOTE \\\' and backslash: \'; "
     "rel=stylesheet; ",
     true, "simple.css", "stylesheet", "", "", ""},
    {"<simple.css>; title=\"title with a DQUOTE \\\" and ;backslash,: \"; "
     "rel=stylesheet; ",
     true, "simple.css", "stylesheet", "", "", ""},
    {"<simple.css>; title=\"title with a DQUOTE \' and ;backslash,: \"; "
     "rel=stylesheet; ",
     true, "simple.css", "stylesheet", "", "", ""},
    {"<simple.css>; title=\"\"; rel=stylesheet; ", true, "simple.css",
     "stylesheet", "", "", ""},
    {"<simple.css>; title=\"\"; rel=\"stylesheet\"; ", true, "simple.css",
     "stylesheet", "", "", ""},
    {"<simple.css>; rel=stylesheet; title=\"", false},
    {"<simple.css>; rel=stylesheet; title=\"\"", true, "simple.css",
     "stylesheet", "", "", ""},
    {"<simple.css>; rel=\"stylesheet\"; title=\"", false},
    {"<simple.css>; rel=\";style,sheet\"; title=\"", false},
    {"<simple.css>; rel=\"bla'sdf\"; title=\"", false},
    {"<simple.css>; rel=\"\"; title=\"\"", true, "simple.css", "", "", "", ""},
    {"<simple.css>; rel=''; title=\"\"", true, "simple.css", "''", "", "", ""},
    {"<simple.css>; rel=''; title=", false},
    {"<simple.css>; rel=''; title", false},
    {"<simple.css>; rel=''; media", false},
    {"<simple.css>; rel=''; hreflang", false},
    {"<simple.css>; rel=''; type", false},
    {"<simple.css>; rel=''; rev", false},
    {"<simple.css>; rel=''; bla", true, "simple.css", "''", "", "", ""},
    {"<simple.css>; rel='prefetch", true, "simple.css", "'prefetch", "", "",
     ""},
    {"<simple.css>; rel=\"prefetch", false},
    {"<simple.css>; rel=\"", false},
    {"<http://whatever.com>; rel=preconnect; valid!", true,
     "http://whatever.com", "preconnect", "", "", ""},
    {"<http://whatever.com>; rel=preconnect; valid$", true,
     "http://whatever.com", "preconnect", "", "", ""},
    {"<http://whatever.com>; rel=preconnect; invalid@", false},
    {"<http://whatever.com>; rel=preconnect; invalid*", false},
    {"</images/cat.jpg>; rel=prefetch;media='(max-width: 5000px)'", true,
     "/images/cat.jpg", "prefetch", "", "'(max-width: 5000px)'", ""},
    {"</images/cat.jpg>; rel=prefetch;media=\"(max-width: 5000px)\"", true,
     "/images/cat.jpg", "prefetch", "", "(max-width: 5000px)", ""},
    {"</images/cat.jpg>; rel=prefetch;media=(max-width:5000px)", true,
     "/images/cat.jpg", "prefetch", "", "(max-width:5000px)", ""},
    {"<simple.css>; rel=preload; fetchpriority=auto", true, "simple.css",
     "preload", "", "", "auto"},
    {"<simple.css>; rel=preload; fetchpriority=low", true, "simple.css",
     "preload", "", "", "low"},
    {"<simple.css>; rel=preload; fetchpriority=high", true, "simple.css",
     "preload", "", "", "high"},
};

void PrintTo(const SingleTestCase& test, std::ostream* os) {
  *os << testing::PrintToString(test.header_value);
}

class SingleLinkHeaderTest : public testing::TestWithParam<SingleTestCase> {};

// Test the cases with a single header
TEST_P(SingleLinkHeaderTest, Single) {
  const SingleTestCase test_case = GetParam();
  LinkHeaderSet header_set(test_case.header_value);
  ASSERT_EQ(1u, header_set.size());
  LinkHeader& header = header_set[0];
  EXPECT_EQ(test_case.valid, header.Valid());
  if (test_case.valid) {
    EXPECT_EQ(test_case.url, header.Url().Ascii());
    EXPECT_EQ(test_case.rel, header.Rel().Ascii());
    EXPECT_EQ(test_case.as, header.As().Ascii());
    EXPECT_EQ(test_case.media, header.Media().Ascii());
    EXPECT_EQ(test_case.fetch_priority, header.FetchPriority().Ascii());
  }
}

INSTANTIATE_TEST_SUITE_P(LinkHeaderTest,
                         SingleLinkHeaderTest,
                         testing::ValuesIn(g_single_test_cases));

struct DoubleTestCase {
  const char* header_value;
  const char* url;
  const char* rel;
  bool valid;
  const char* url2;
  const char* rel2;
  bool valid2;
} g_double_test_cases[] = {
    {"<ybg.css>; rel=stylesheet, <simple.css>; rel=stylesheet", "ybg.css",
     "stylesheet", true, "simple.css", "stylesheet", true},
    {"<ybg.css>; rel=stylesheet,<simple.css>; rel=stylesheet", "ybg.css",
     "stylesheet", true, "simple.css", "stylesheet", true},
    {"<ybg.css>; rel=stylesheet;crossorigin,<simple.css>; rel=stylesheet",
     "ybg.css", "stylesheet", true, "simple.css", "stylesheet", true},
    {"<hel,lo.css>; rel=stylesheet; title=\"foo,bar\", <simple.css>; "
     "rel=stylesheet; title=\"foo;bar\"",
     "hel,lo.css", "stylesheet", true, "simple.css", "stylesheet", true},
};

void PrintTo(const DoubleTestCase& test, std::ostream* os) {
  *os << testing::PrintToString(test.header_value);
}

class DoubleLinkHeaderTest : public testing::TestWithParam<DoubleTestCase> {};

TEST_P(DoubleLinkHeaderTest, Double) {
  const DoubleTestCase test_case = GetParam();
  LinkHeaderSet header_set(test_case.header_value);
  ASSERT_EQ(2u, header_set.size());
  LinkHeader& header1 = header_set[0];
  LinkHeader& header2 = header_set[1];
  EXPECT_EQ(test_case.url, header1.Url());
  EXPECT_EQ(test_case.rel, header1.Rel());
  EXPECT_EQ(test_case.valid, header1.Valid());
  EXPECT_EQ(test_case.url2, header2.Url());
  EXPECT_EQ(test_case.rel2, header2.Rel());
  EXPECT_EQ(test_case.valid2, header2.Valid());
}

INSTANTIATE_TEST_SUITE_P(LinkHeaderTest,
                         DoubleLinkHeaderTest,
                         testing::ValuesIn(g_double_test_cases));

struct CrossOriginTestCase {
  const char* header_value;
  const char* url;
  const char* rel;
  const char* crossorigin;
  bool valid;
} g_cross_origin_test_cases[] = {
    {"<http://whatever.com>; rel=preconnect", "http://whatever.com",
     "preconnect", nullptr, true},
    {"<http://whatever.com>; rel=preconnect; crossorigin=", "", "", "", false},
    {"<http://whatever.com>; rel=preconnect; crossorigin",
     "http://whatever.com", "preconnect", "", true},
    {"<http://whatever.com>; rel=preconnect; crossorigin ",
     "http://whatever.com", "preconnect", "", true},
    {"<http://whatever.com>; rel=preconnect; crossorigin;",
     "http://whatever.com", "preconnect", "", true},
    {"<http://whatever.com>; rel=preconnect; crossorigin, "
     "<http://whatever2.com>; rel=preconnect",
     "http://whatever.com", "preconnect", "", true},
    {"<http://whatever.com>; rel=preconnect; crossorigin , "
     "<http://whatever2.com>; rel=preconnect",
     "http://whatever.com", "preconnect", "", true},
    {"<http://whatever.com>; rel=preconnect; "
     "crossorigin,<http://whatever2.com>; rel=preconnect",
     "http://whatever.com", "preconnect", "", true},
    {"<http://whatever.com>; rel=preconnect; crossorigin=anonymous",
     "http://whatever.com", "preconnect", "anonymous", true},
    {"<http://whatever.com>; rel=preconnect; crossorigin=use-credentials",
     "http://whatever.com", "preconnect", "use-credentials", true},
    {"<http://whatever.com>; rel=preconnect; crossorigin=whatever",
     "http://whatever.com", "preconnect", "whatever", true},
    {"<http://whatever.com>; rel=preconnect; crossorig|in=whatever",
     "http://whatever.com", "preconnect", nullptr, true},
    {"<http://whatever.com>; rel=preconnect; crossorigin|=whatever",
     "http://whatever.com", "preconnect", nullptr, true},
};

void PrintTo(const CrossOriginTestCase& test, std::ostream* os) {
  *os << testing::PrintToString(test.header_value);
}

class CrossOriginLinkHeaderTest
    : public testing::TestWithParam<CrossOriginTestCase> {};

TEST_P(CrossOriginLinkHeaderTest, CrossOrigin) {
  const CrossOriginTestCase test_case = GetParam();
  LinkHeaderSet header_set(test_case.header_value);
  ASSERT_GE(header_set.size(), 1u);
  LinkHeader& header = header_set[0];
  EXPECT_EQ(test_case.url, header.Url().Ascii());
  EXPECT_EQ(test_case.rel, header.Rel().Ascii());
  EXPECT_EQ(test_case.valid, header.Valid());
  if (!test_case.crossorigin)
    EXPECT_TRUE(header.CrossOrigin().IsNull());
  else
    EXPECT_EQ(test_case.crossorigin, header.CrossOrigin().Ascii());
}

INSTANTIATE_TEST_SUITE_P(LinkHeaderTest,
                         CrossOriginLinkHeaderTest,
                         testing::ValuesIn(g_cross_origin_test_cases));

}  // namespace
}  // namespace blink
