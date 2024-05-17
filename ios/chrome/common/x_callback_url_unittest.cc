// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/common/x_callback_url.h"

#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/url_features.h"

namespace {

struct XCallbackURLEncodeTestCase {
  const char* scheme;
  const char* action;
  GURL success_url;
  GURL error_url;
  GURL cancel_url;
  std::map<std::string, std::string> parameters;

  const char* expected;
};

using XCallbackURLTest = PlatformTest;

// Non-special URLs behavior is affected by the
// StandardCompliantNonSpecialSchemeURLParsing feature.
// See https://crbug.com/40063064 for details.
class XCallbackURLParamTest : public ::testing::TestWithParam<bool> {
 public:
  XCallbackURLParamTest()
      : use_standard_compliant_non_special_scheme_url_parsing_(GetParam()) {
    scoped_feature_list_.InitWithFeatureState(
        url::kStandardCompliantNonSpecialSchemeURLParsing,
        use_standard_compliant_non_special_scheme_url_parsing_);
  }

 protected:
  bool use_standard_compliant_non_special_scheme_url_parsing_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(XCallbackURLParamTest, IsXCallbackURL) {
  EXPECT_TRUE(IsXCallbackURL(GURL("chrome://x-callback-url")));
  EXPECT_TRUE(IsXCallbackURL(GURL("https://x-callback-url")));
  EXPECT_TRUE(IsXCallbackURL(GURL("exotic-scheme://x-callback-url")));

  EXPECT_TRUE(IsXCallbackURL(GURL("chrome://x-callback-url/action")));
  EXPECT_TRUE(IsXCallbackURL(GURL("https://x-callback-url/action")));
  EXPECT_TRUE(IsXCallbackURL(GURL("exotic-scheme://x-callback-url/action")));

  EXPECT_FALSE(IsXCallbackURL(GURL()));
  EXPECT_FALSE(IsXCallbackURL(GURL("chrome://version")));
  EXPECT_FALSE(IsXCallbackURL(GURL("https://www.google.com")));
}

INSTANTIATE_TEST_SUITE_P(All, XCallbackURLParamTest, ::testing::Bool());

TEST_F(XCallbackURLTest, URLWithScheme) {
  const XCallbackURLEncodeTestCase test_cases[] = {
      {
          "chrome",
          "",
          GURL(),
          GURL(),
          GURL(),
          {},

          "chrome://x-callback-url/",
      },
      {
          "chrome",
          "command",
          GURL(),
          GURL(),
          GURL(),
          {},

          "chrome://x-callback-url/command",
      },
      {
          "chrome",
          "command",
          GURL("chrome://callback/?success=1"),
          GURL("chrome://callback/?success=0"),
          GURL("chrome://callback/?cancelled=1"),
          {},

          "chrome://x-callback-url/"
          "command?x-success=chrome%3A%2F%2Fcallback%2F"
          "%3Fsuccess%3D1&x-error=chrome%3A%2F%2Fcallback%2F"
          "%3Fsuccess%3D0&x-cancel=chrome%3A%2F%2Fcallback%2F"
          "%3Fcancelled%3D1",
      },
      {
          "chrome",
          "command",
          GURL(),
          GURL(),
          GURL(),
          {{"foo", "bar baz"}, {"qux", ""}},

          "chrome://x-callback-url/command?foo=bar+baz&qux=",
      },
      {
          "chrome",
          "command",
          GURL("chrome://callback/?success=1"),
          GURL("chrome://callback/?success=0"),
          GURL("chrome://callback/?cancelled=1"),
          {{"foo", "bar baz"}, {"qux", ""}},

          "chrome://x-callback-url/"
          "command?x-success=chrome%3A%2F%2Fcallback%2F%3Fsuccess%3D1"
          "&x-error=chrome%3A%2F%2Fcallback%2F%3Fsuccess%3D0&x-cancel=chrome"
          "%3A%2F%2Fcallback%2F%3Fcancelled%3D1&foo=bar+baz&qux=",
      },
      {
          "chrome",
          "command",
          GURL("chrome://path/with%20spaces"),
          GURL(),
          GURL(),
          {},

          "chrome://x-callback-url/command?x-success="
          "chrome%3A%2F%2Fpath%2Fwith%2520spaces",
      },
  };

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    const XCallbackURLEncodeTestCase& test_case = test_cases[i];
    const GURL x_callback_url = CreateXCallbackURLWithParameters(
        test_case.scheme, test_case.action, test_case.success_url,
        test_case.error_url, test_case.cancel_url, test_case.parameters);
    EXPECT_EQ(test_case.expected, x_callback_url.spec());
  }
}

struct XCallbackURLDecodeTestCase {
  GURL x_callback_url;

  std::map<std::string, std::string> expected;
};

TEST_F(XCallbackURLTest, QueryParameters) {
  const XCallbackURLDecodeTestCase test_cases[] = {
      {
          GURL("chrome://x-callback-url/"),

          {},
      },
      {
          GURL("chrome://x-callback-url/command"),

          {},
      },
      {
          GURL("chrome://x-callback-url/"
               "command?x-success=chrome%3A%2F%2Fcallback%2F%3Fsuccess%3D1"
               "&x-error=chrome%3A%2F%2Fcallback%2F%3Fsuccess%3D0&x-cancel="
               "chrome%3A%2F%2Fcallback%2F%3Fcancelled%3D1"),

          {{"x-success", "chrome://callback/?success=1"},
           {"x-error", "chrome://callback/?success=0"},
           {"x-cancel", "chrome://callback/?cancelled=1"}},
      },
      {
          GURL("chrome://x-callback-url/command?foo=bar+baz&qux="),

          {{"foo", "bar baz"}, {"qux", ""}},
      },
      {
          GURL("chrome://x-callback-url/"
               "command?x-success=chrome%3A%2F%2Fcallback%2F%3Fsuccess%3D1"
               "&x-error=chrome%3A%2F%2Fcallback%2F%3Fsuccess%3D0&x-cancel="
               "chrome%3A%2F%2Fcallback%2F%3Fcancelled%3D1&foo=bar+baz&qux="),

          {{"x-success", "chrome://callback/?success=1"},
           {"x-error", "chrome://callback/?success=0"},
           {"x-cancel", "chrome://callback/?cancelled=1"},
           {"foo", "bar baz"},
           {"qux", ""}},
      },
      {
          GURL("chrome://x-callback-url/command?x-success="
               "chrome%3A%2F%2Fpath%2Fwith%2520spaces"),

          {{"x-success", "chrome://path/with%20spaces"}},
      },
  };

  for (size_t i = 0; i < std::size(test_cases); ++i) {
    const XCallbackURLDecodeTestCase& test_case = test_cases[i];
    const std::map<std::string, std::string> parameters =
        ExtractQueryParametersFromXCallbackURL(test_case.x_callback_url);
    EXPECT_EQ(test_case.expected, parameters);
  }
}

}  // namespace
