// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/shared_dictionary/simple_url_pattern_matcher.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/test/gmock_expected_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace {

class PatternInitMatcher : public testing::MatcherInterface<
                               const SimpleUrlPatternMatcher::PatternInit&> {
 public:
  PatternInitMatcher(std::optional<std::string> protocol_matcher,
                     std::optional<std::string> username_matcher,
                     std::optional<std::string> password_matcher,
                     std::optional<std::string> hostname_matcher,
                     std::optional<std::string> port_matcher,
                     std::optional<std::string> pathname_matcher,
                     std::optional<std::string> search_matcher,
                     std::optional<std::string> hash_matcher)
      : protocol_matcher_(std::move(protocol_matcher)),
        username_matcher_(std::move(username_matcher)),
        password_matcher_(std::move(password_matcher)),
        hostname_matcher_(std::move(hostname_matcher)),
        port_matcher_(std::move(port_matcher)),
        pathname_matcher_(std::move(pathname_matcher)),
        search_matcher_(std::move(search_matcher)),
        hash_matcher_(std::move(hash_matcher)) {}
  ~PatternInitMatcher() override = default;

  PatternInitMatcher(const PatternInitMatcher&) = delete;
  PatternInitMatcher& operator=(const PatternInitMatcher&) = delete;
  PatternInitMatcher(PatternInitMatcher&&) = delete;
  PatternInitMatcher& operator=(PatternInitMatcher&&) = delete;

  bool MatchAndExplain(
      const SimpleUrlPatternMatcher::PatternInit& pattern,
      testing::MatchResultListener* result_listener) const override {
    return ExplainMatchResult(
        testing::AllOf(
            testing::Property("protocol",
                              &SimpleUrlPatternMatcher::PatternInit::protocol,
                              testing::Eq(protocol_matcher_)),
            testing::Property("username",
                              &SimpleUrlPatternMatcher::PatternInit::username,
                              testing::Eq(username_matcher_)),
            testing::Property("password",
                              &SimpleUrlPatternMatcher::PatternInit::password,
                              testing::Eq(password_matcher_)),
            testing::Property("hostname",
                              &SimpleUrlPatternMatcher::PatternInit::hostname,
                              testing::Eq(hostname_matcher_)),
            testing::Property("port",
                              &SimpleUrlPatternMatcher::PatternInit::port,
                              testing::Eq(port_matcher_)),
            testing::Property("pathname",
                              &SimpleUrlPatternMatcher::PatternInit::pathname,
                              testing::Eq(pathname_matcher_)),
            testing::Property("search",
                              &SimpleUrlPatternMatcher::PatternInit::search,
                              testing::Eq(search_matcher_)),
            testing::Property("hash",
                              &SimpleUrlPatternMatcher::PatternInit::hash,
                              testing::Eq(hash_matcher_))),
        pattern, result_listener);
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "matches ";
    Describe(*os);
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "does not match ";
    Describe(*os);
  }

 private:
  void Describe(std::ostream& os) const {
    os << "PatternInit {\n"
       << "  protocol: " << testing::PrintToString(protocol_matcher_) << "\n"
       << "  username: " << testing::PrintToString(username_matcher_) << "\n"
       << "  password: " << testing::PrintToString(password_matcher_) << "\n"
       << "  hostname: " << testing::PrintToString(hostname_matcher_) << "\n"
       << "  port: " << testing::PrintToString(port_matcher_) << "\n"
       << "  pathname: " << testing::PrintToString(pathname_matcher_) << "\n"
       << "  search: " << testing::PrintToString(search_matcher_) << "\n"
       << "  hash: " << testing::PrintToString(hash_matcher_) << "\n"
       << "}";
  }

  const std::optional<std::string> protocol_matcher_;
  const std::optional<std::string> username_matcher_;
  const std::optional<std::string> password_matcher_;
  const std::optional<std::string> hostname_matcher_;
  const std::optional<std::string> port_matcher_;
  const std::optional<std::string> pathname_matcher_;
  const std::optional<std::string> search_matcher_;
  const std::optional<std::string> hash_matcher_;
};

testing::Matcher<const SimpleUrlPatternMatcher::PatternInit&> ExpectPatternInit(
    std::optional<std::string> protocol,
    std::optional<std::string> username,
    std::optional<std::string> password,
    std::optional<std::string> hostname,
    std::optional<std::string> port,
    std::optional<std::string> pathname,
    std::optional<std::string> search,
    std::optional<std::string> hash) {
  return testing::Matcher<const SimpleUrlPatternMatcher::PatternInit&>(
      new PatternInitMatcher(std::move(protocol), std::move(username),
                             std::move(password), std::move(hostname),
                             std::move(port), std::move(pathname),
                             std::move(search), std::move(hash)));
}

}  // namespace

class SimpleUrlPatternMatcherTest : public testing::Test {
 public:
  SimpleUrlPatternMatcherTest() = default;
  ~SimpleUrlPatternMatcherTest() override = default;

 protected:
  static base::expected<SimpleUrlPatternMatcher::PatternInit, std::string>
  CreatePatternInit(const std::string_view& url_pattern, const GURL& base_url) {
    return SimpleUrlPatternMatcher::CreatePatternInit(
        url_pattern, base_url,
        /*protocol_matcher_out*=*/nullptr,
        /*should_treat_as_standard_url_out=*/nullptr);
  }
  static base::expected<std::unique_ptr<SimpleUrlPatternMatcher>, std::string>
  CreateMatcher(const std::string_view& constructor_string,
                const GURL& base_url) {
    return SimpleUrlPatternMatcher::Create(constructor_string, base_url);
  }
};

TEST_F(SimpleUrlPatternMatcherTest, Create) {
  struct {
    std::string_view constructor_string;
    std::string_view base_url;
    std::optional<testing::Matcher<const SimpleUrlPatternMatcher::PatternInit&>>
        expected_pattern;
    std::optional<std::string_view> expected_error;
    std::vector<std::string_view> match_urls;
    std::vector<std::string_view> non_match_urls;
  } test_cases[] = {

      // Test cases for SimpleUrlPatternMatcher creation success

      // Absolute path
      {.constructor_string = "/foo",
       .base_url = "https://example.com/foo/bar.html",
       .expected_pattern = ExpectPatternInit(
           /*protocol=*/"https", /*username=*/std::nullopt,
           /*password=*/std::nullopt, /*hostname=*/"example.com",
           /*port=*/"", /*pathname=*/"/foo",
           /*search=*/std::nullopt,
           /*hash=*/std::nullopt),
       .match_urls = {"https://example.com/foo", "https://example.com/foo?bar"},
       .non_match_urls = {"https://example.com/bar", "https://example.com/foo/",
                          "https://example.com/foo/baz"}},

      // Absolute path ending with /
      {.constructor_string = "/foo/",
       .base_url = "https://example.com/foo/bar.html",
       .expected_pattern = ExpectPatternInit(
           /*protocol=*/"https", /*username=*/std::nullopt,
           /*password=*/std::nullopt, /*hostname=*/"example.com",
           /*port=*/"", /*pathname=*/"/foo/",
           /*search=*/std::nullopt,
           /*hash=*/std::nullopt),
       .match_urls = {"https://example.com/foo/",
                      "https://example.com/foo/?bar"},
       .non_match_urls = {"https://example.com/foo",
                          "https://example.com/foo?bar",
                          "https://example.com/foo/baz"}},

      // Relative path
      {.constructor_string = "hoge",
       .base_url = "https://example.com/foo/bar.html",
       .expected_pattern = ExpectPatternInit(
           /*protocol=*/"https", /*username=*/std::nullopt,
           /*password=*/std::nullopt, /*hostname=*/"example.com",
           /*port=*/"", /*pathname=*/"/foo/hoge",
           /*search=*/std::nullopt,
           /*hash=*/std::nullopt),
       .match_urls = {"https://example.com/foo/hoge",
                      "https://example.com/foo/hoge?bar"},
       .non_match_urls = {"https://example.com/hoge",
                          "https://example.com/hoge/baz"}},

      // Relative path ending with /
      {.constructor_string = "hoge/",
       .base_url = "https://example.com/foo/bar.html",
       .expected_pattern = ExpectPatternInit(
           /*protocol=*/"https", /*username=*/std::nullopt,
           /*password=*/std::nullopt, /*hostname=*/"example.com",
           /*port=*/"", /*pathname=*/"/foo/hoge/",
           /*search=*/std::nullopt,
           /*hash=*/std::nullopt),
       .match_urls = {"https://example.com/foo/hoge/",
                      "https://example.com/foo/hoge/?bar"},
       .non_match_urls = {"https://example.com/hoge",
                          "https://example.com/hoge?bar"}},

      // Absolute URL
      {.constructor_string = "https://example.com/piyo/fuga",
       .base_url = "https://example.com/foo/bar.html",
       .expected_pattern = ExpectPatternInit(
           /*protocol=*/"https", /*username=*/std::nullopt,
           /*password=*/std::nullopt, /*hostname=*/"example.com",
           /*port=*/"", /*pathname=*/"/piyo/fuga",
           /*search=*/std::nullopt,
           /*hash=*/std::nullopt),
       .match_urls = {"https://example.com/piyo/fuga",
                      "https://example.com/piyo/fuga?bar"},
       .non_match_urls = {"https://example.com/foo/",
                          "https://example.com/foo/piyo/fuga",
                          "https://example.com/piyo"}},

      // Absolute URL with different hostname
      {.constructor_string = "https://example.net/piyo/fuga",
       .base_url = "https://example.com/foo/bar.html",
       .expected_pattern = ExpectPatternInit(
           /*protocol=*/"https", /*username=*/std::nullopt,
           /*password=*/std::nullopt, /*hostname=*/"example.net",
           /*port=*/"", /*pathname=*/"/piyo/fuga",
           /*search=*/std::nullopt,
           /*hash=*/std::nullopt),
       .match_urls = {"https://example.net/piyo/fuga",
                      "https://example.net/piyo/fuga?bar"},
       .non_match_urls = {"https://example.com/foo/",
                          "https://example.com/foo/piyo/fuga"}},

      // Absolute URL with a default port
      {.constructor_string = "https://example.com:443/piyo/fuga",
       .base_url = "https://example.com/foo/bar.html",
       .expected_pattern = ExpectPatternInit(
           /*protocol=*/"https", /*username=*/std::nullopt,
           /*password=*/std::nullopt, /*hostname=*/"example.com",
           /*port=*/"", /*pathname=*/"/piyo/fuga",
           /*search=*/std::nullopt,
           /*hash=*/std::nullopt),
       .match_urls = {"https://example.com/piyo/fuga",
                      "https://example.com:443/piyo/fuga"},
       .non_match_urls = {"https://example.com:444/piyo/fuga/",
                          "https://example.com:80/piyo/fuga"}},

      // Absolute URL with a non-default port
      {.constructor_string = "https://example.com:444/piyo/fuga",
       .base_url = "https://example.com/foo/bar.html",
       .expected_pattern = ExpectPatternInit(
           /*protocol=*/"https", /*username=*/std::nullopt,
           /*password=*/std::nullopt, /*hostname=*/"example.com",
           /*port=*/"444", /*pathname=*/"/piyo/fuga",
           /*search=*/std::nullopt,
           /*hash=*/std::nullopt),
       .match_urls = {"https://example.com:444/piyo/fuga"},
       .non_match_urls = {"https://example.com/piyo/fuga/",
                          "https://example.com:443/piyo/fuga",
                          "https://example.com:80/piyo/fuga"}},

      // Empty pathname
      {.constructor_string = "https://example.com",
       .base_url = "https://example.com/foo/bar.html",
       .expected_pattern = ExpectPatternInit(
           /*protocol=*/"https", /*username=*/std::nullopt,
           /*password=*/std::nullopt, /*hostname=*/"example.com",
           /*port=*/"", /*pathname=*/std::nullopt,
           /*search=*/std::nullopt,
           /*hash=*/std::nullopt),
       .match_urls = {"https://example.com", "https://example.com/piyo/"},
       .non_match_urls = {"https://example.net/"}},

      // pathname = `/`
      {.constructor_string = "https://example.com/",
       .base_url = "https://example.com/foo/bar.html",
       .expected_pattern = ExpectPatternInit(
           /*protocol=*/"https", /*username=*/std::nullopt,
           /*password=*/std::nullopt, /*hostname=*/"example.com",
           /*port=*/"", /*pathname=*/"/",
           /*search=*/std::nullopt,
           /*hash=*/std::nullopt),
       .match_urls = {"https://example.com"},
       .non_match_urls = {"https://example.net/", "https://example.com/piyo/"}},

      // Relative path with a query string
      {.constructor_string = "hoge?*",
       .base_url = "https://example.com/foo/bar.html",
       .expected_pattern = ExpectPatternInit(
           /*protocol=*/"https", /*username=*/std::nullopt,
           /*password=*/std::nullopt, /*hostname=*/"example.com",
           /*port=*/"", /*pathname=*/"/foo/hoge",
           /*search=*/"*",
           /*hash=*/std::nullopt),
       .match_urls = {"https://example.com/foo/hoge",
                      "https://example.com/foo/hoge?bar"},
       .non_match_urls = {"https://example.com/hoge",
                          "https://example.com/hoge/baz"}},

      // Relative path with a query string with a non-standard base_url
      {.constructor_string = "hoge?*",
       .base_url = "non-standard:example.com/foo/bar.html",
       .expected_pattern = ExpectPatternInit(
           /*protocol=*/"non-standard", /*username=*/std::nullopt,
           /*password=*/std::nullopt, /*hostname=*/"",
           /*port=*/"", /*pathname=*/"hoge",
           /*search=*/"*",
           /*hash=*/std::nullopt),
       .match_urls = {"non-standard:hoge", "non-standard:hoge?bar"},
       .non_match_urls = {"non-standard:example.com/foo/hoge",
                          "non-standard:example.com/foo/hoge?bar"}},

      // Pattern group hostname
      {.constructor_string = "https://{:subdomain.}?example.com/piyo/fuga",
       .base_url = "https://example.com/foo/bar.html",
       .expected_pattern = ExpectPatternInit(
           /*protocol=*/"https", /*username=*/std::nullopt,
           /*password=*/std::nullopt, /*hostname=*/"{:subdomain.}?example.com",
           /*port=*/"", /*pathname=*/"/piyo/fuga",
           /*search=*/std::nullopt,
           /*hash=*/std::nullopt),
       .match_urls = {"https://example.com/piyo/fuga",
                      "https://a.example.com/piyo/fuga"},
       .non_match_urls = {"https://example.net/piyo/fuga"}},

      // Escaped pathname
      {.constructor_string = "\\/piyo\\/fuga",
       .base_url = "https://example.com/foo/bar.html",
       .expected_pattern = ExpectPatternInit(
           /*protocol=*/"https", /*username=*/std::nullopt,
           /*password=*/std::nullopt, /*hostname=*/"example.com",
           /*port=*/"", /*pathname=*/"\\/piyo\\/fuga",
           /*search=*/std::nullopt,
           /*hash=*/std::nullopt),
       .match_urls = {"https://example.com/piyo/fuga",
                      "https://example.com/piyo/fuga?bar"},
       .non_match_urls = {"https://example.com\\/piyo/fuga"}},

      // No slash in the non-standard base URL's pathname
      {.constructor_string = "hoge/piyo",
       .base_url = "non-standard:",
       .expected_pattern = ExpectPatternInit(
           /*protocol=*/"non-standard", /*username=*/std::nullopt,
           /*password=*/std::nullopt, /*hostname=*/"",
           /*port=*/"", /*pathname=*/"hoge/piyo",
           /*search=*/std::nullopt,
           /*hash=*/std::nullopt),
       .match_urls = {"non-standard:hoge/piyo", "non-standard:hoge/piyo"},
       .non_match_urls = {"https://hoge/piyo"}},

      // Empty constructor string
      {.constructor_string = "",
       .base_url = "https://example.com/foo/bar.html",
       .expected_pattern = ExpectPatternInit(
           /*protocol=*/"https", /*username=*/std::nullopt,
           /*password=*/std::nullopt, /*hostname=*/"example.com",
           /*port=*/"", /*pathname=*/"/foo/",
           /*search=*/std::nullopt,
           /*hash=*/std::nullopt),
       .match_urls = {"https://example.com/foo/",
                      "https://example.com/foo/?bar"},
       .non_match_urls = {"https://example.com/foo",
                          "https://example.com/foo/bar",
                          "https://example.com/foo/bar/baz"}},

      // Test cases for SimpleUrlPatternMatcher creation failure

      // Invalid constructor string (pathname)
      {.constructor_string = "{",
       .base_url = "https://urlpattern.example/foo/bar",
       .expected_error = "Failed to parse pattern for pathname"},

      // Invalid constructor string (protocol)
      {.constructor_string = "\t://example.com/",
       .base_url = "https://urlpattern.example/foo/bar",
       .expected_error = "Failed to parse pattern for protocol"},

      // Unsupported regexp group
      {.constructor_string = "/(\\d+)/",
       .base_url = "https://urlpattern.example/foo/bar",
       .expected_error = "Regexp groups are not supported for pathname"},

      // Invalid base URL
      {.constructor_string = "/foo/",
       .base_url = "",
       .expected_error = "Invalid base URL"}};

  for (const auto& test : test_cases) {
    SCOPED_TRACE(
        base::StrCat({"constructor_string: \"", test.constructor_string,
                      "\", base_url: \"", test.base_url, "\""}));
    if (!test.expected_error) {
      ASSERT_OK_AND_ASSIGN(auto matcher, CreateMatcher(test.constructor_string,
                                                       GURL(test.base_url)));
      ASSERT_OK_AND_ASSIGN(
          auto pattern,
          CreatePatternInit(test.constructor_string, GURL(test.base_url)));
      EXPECT_THAT(pattern, *test.expected_pattern);
      for (const auto& match_url : test.match_urls) {
        EXPECT_TRUE(matcher->Match(GURL(match_url))) << match_url;
      }
      for (const auto& non_match_url : test.non_match_urls) {
        EXPECT_FALSE(matcher->Match(GURL(non_match_url))) << non_match_url;
      }
    } else {
      auto create_matcher_result =
          CreateMatcher(test.constructor_string, GURL(test.base_url));
      EXPECT_FALSE(create_matcher_result.has_value());
      EXPECT_EQ(create_matcher_result.error(), test.expected_error);
    }
  }
}
}  // namespace network
