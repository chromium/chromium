// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/url_pattern_set.h"

#include <stddef.h>

#include <sstream>

#include "base/containers/contains.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

namespace {

void AddPattern(URLPatternSet* set, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  set->AddPattern(URLPattern(schemes, pattern));
}

}  // namespace

TEST(URLPatternSetTest, Empty) {
  URLPatternSet set;
  EXPECT_FALSE(set.MatchesURL(GURL("http://www.foo.com/bar")));
  EXPECT_FALSE(set.MatchesURL(GURL()));
  EXPECT_FALSE(set.MatchesURL(GURL("invalid")));
}

TEST(URLPatternSetTest, One) {
  URLPatternSet set;
  AddPattern(&set, "http://www.google.com/*");

  EXPECT_TRUE(set.MatchesURL(GURL("http://www.google.com/")));
  EXPECT_TRUE(set.MatchesURL(GURL("http://www.google.com/monkey")));
  EXPECT_FALSE(set.MatchesURL(GURL("https://www.google.com/")));
  EXPECT_FALSE(set.MatchesURL(GURL("https://www.microsoft.com/")));
}

TEST(URLPatternSetTest, Two) {
  URLPatternSet set;
  AddPattern(&set, "http://www.google.com/*");
  AddPattern(&set, "http://www.yahoo.com/*");

  EXPECT_TRUE(set.MatchesURL(GURL("http://www.google.com/monkey")));
  EXPECT_TRUE(set.MatchesURL(GURL("http://www.yahoo.com/monkey")));
  EXPECT_FALSE(set.MatchesURL(GURL("https://www.apple.com/monkey")));
}

TEST(URLPatternSetTest, StreamOperatorEmpty) {
  URLPatternSet set;

  std::ostringstream stream;
  stream << set;
  EXPECT_EQ("{ }", stream.str());
}

TEST(URLPatternSetTest, StreamOperatorOne) {
  URLPatternSet set;
  AddPattern(&set, "http://www.google.com/*");

  std::ostringstream stream;
  stream << set;
  EXPECT_EQ("{ \"http://www.google.com/*\" }", stream.str());
}

TEST(URLPatternSetTest, StreamOperatorTwo) {
  URLPatternSet set;
  AddPattern(&set, "http://www.google.com/*");
  AddPattern(&set, "http://www.yahoo.com/*");

  std::ostringstream stream;
  stream << set;
  EXPECT_EQ("{ \"http://www.google.com/*\", \"http://www.yahoo.com/*\" }",
            stream.str());
}

TEST(URLPatternSetTest, OverlapsWith) {
  URLPatternSet set1;
  AddPattern(&set1, "http://www.google.com/f*");
  AddPattern(&set1, "http://www.yahoo.com/b*");

  URLPatternSet set2;
  AddPattern(&set2, "http://www.reddit.com/f*");
  AddPattern(&set2, "http://www.yahoo.com/z*");

  URLPatternSet set3;
  AddPattern(&set3, "http://www.google.com/q/*");
  AddPattern(&set3, "http://www.yahoo.com/b/*");

  EXPECT_FALSE(set1.OverlapsWith(set2));
  EXPECT_FALSE(set2.OverlapsWith(set1));

  EXPECT_TRUE(set1.OverlapsWith(set3));
  EXPECT_TRUE(set3.OverlapsWith(set1));
}

TEST(URLPatternSetTest, CreateDifference) {
  URLPatternSet expected;
  URLPatternSet set1;
  URLPatternSet set2;
  AddPattern(&set1, "http://www.google.com/f*");
  AddPattern(&set1, "http://www.yahoo.com/b*");

  // Subtract an empty set.
  URLPatternSet result = URLPatternSet::CreateDifference(set1, set2);
  EXPECT_EQ(set1, result);

  // Subtract a real set.
  AddPattern(&set2, "http://www.reddit.com/f*");
  AddPattern(&set2, "http://www.yahoo.com/z*");
  AddPattern(&set2, "http://www.google.com/f*");

  AddPattern(&expected, "http://www.yahoo.com/b*");

  result = URLPatternSet::CreateDifference(set1, set2);
  EXPECT_EQ(expected, result);
  EXPECT_FALSE(result.is_empty());
  EXPECT_TRUE(set1.Contains(result));
  EXPECT_FALSE(result.Contains(set2));
  EXPECT_FALSE(set2.Contains(result));

  URLPatternSet intersection = URLPatternSet::CreateIntersection(
      result, set2, URLPatternSet::IntersectionBehavior::kStringComparison);
  EXPECT_TRUE(intersection.is_empty());
}

TEST(URLPatternSetTest, CreateIntersection_StringComparison) {
  URLPatternSet empty_set;
  URLPatternSet expected;
  URLPatternSet set1;
  AddPattern(&set1, "http://www.google.com/f*");
  AddPattern(&set1, "http://www.yahoo.com/b*");

  // Intersection with an empty set.
  URLPatternSet result = URLPatternSet::CreateIntersection(
      set1, empty_set, URLPatternSet::IntersectionBehavior::kStringComparison);
  EXPECT_EQ(expected, result);
  EXPECT_TRUE(result.is_empty());
  EXPECT_TRUE(empty_set.Contains(result));
  EXPECT_TRUE(result.Contains(empty_set));
  EXPECT_TRUE(set1.Contains(result));

  // Intersection with a real set.
  URLPatternSet set2;
  AddPattern(&set2, "http://www.reddit.com/f*");
  AddPattern(&set2, "http://www.yahoo.com/z*");
  AddPattern(&set2, "http://www.google.com/f*");

  AddPattern(&expected, "http://www.google.com/f*");

  result = URLPatternSet::CreateIntersection(
      set1, set2, URLPatternSet::IntersectionBehavior::kStringComparison);
  EXPECT_EQ(expected, result);
  EXPECT_FALSE(result.is_empty());
  EXPECT_TRUE(set1.Contains(result));
  EXPECT_TRUE(set2.Contains(result));
}

TEST(URLPatternSetTest, CreateIntersection_PatternsContainedByBoth) {
  {
    URLPatternSet set1;
    AddPattern(&set1, "http://*.google.com/*");
    AddPattern(&set1, "http://*.yahoo.com/*");

    URLPatternSet set2;
    AddPattern(&set2, "http://google.com/*");

    // The semantic intersection should contain only those patterns that are in
    // both set 1 and set 2, or "http://google.com/*".
    URLPatternSet intersection = URLPatternSet::CreateIntersection(
        set1, set2,
        URLPatternSet::IntersectionBehavior::kPatternsContainedByBoth);
    ASSERT_EQ(1u, intersection.size());
    EXPECT_EQ("http://google.com/*", intersection.begin()->GetAsString());
  }

  {
    // IntersectionBehavior::kPatternsContainedByBoth doesn't handle funny
    // intersections, where the resultant pattern is neither of the two
    // compared patterns. So the intersection of these two is not
    // http://www.google.com/*, but rather nothing.
    URLPatternSet set1;
    AddPattern(&set1, "http://*/*");
    URLPatternSet set2;
    AddPattern(&set2, "*://www.google.com/*");
    EXPECT_TRUE(
        URLPatternSet::CreateIntersection(
            set1, set2,
            URLPatternSet::IntersectionBehavior::kPatternsContainedByBoth)
            .is_empty());
  }
}

TEST(URLPatternSetTest, CreateIntersection_Detailed) {
  struct {
    std::vector<std::string> set1;
    std::vector<std::string> set2;
    std::vector<std::string> expected_intersection;
  } test_cases[] = {
      {{"https://*.google.com/*", "https://chromium.org/*"},
       {"*://maps.google.com/*", "*://chromium.org/foo"},
       {"https://maps.google.com/*", "https://chromium.org/foo"}},
      {{"https://*/*", "http://*/*"},
       {"*://google.com/*", "*://chromium.org/*"},
       {"https://google.com/*", "http://google.com/*", "https://chromium.org/*",
        "http://chromium.org/*"}},
      {{"<all_urls>"},
       {"https://chromium.org/*", "*://google.com/*"},
       {"https://chromium.org/*", "*://google.com/*"}},
      {{"*://*/maps", "*://*.example.com/*"},
       {"https://*.google.com/*", "https://www.example.com/*"},
       {"https://*.google.com/maps", "https://www.example.com/*"}},
      {{"https://*/maps", "https://*.google.com/*"},
       {"https://*.google.com/*"},
       {"https://*.google.com/*"}},
      {{"http://*/*"}, {"https://*/*"}, {}},
      {{"https://*.google.com/*", "https://maps.google.com/*"},
       {"https://*.google.com/*", "https://*/*"},
       // NOTE: We don't currently do any additional "collapsing" step after
       // finding the intersection. We potentially could, to reduce the number
       // of patterns we need to store.
       {"https://*.google.com/*", "https://maps.google.com/*"}},
  };

  constexpr int kValidSchemes = URLPattern::SCHEME_ALL;
  constexpr char kTestCaseDescriptionTemplate[] =
      "Running Test Case:\n"
      "    Set1:            %s\n"
      "    Set2:            %s\n"
      "    Expected Result: %s";
  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(base::StringPrintf(
        kTestCaseDescriptionTemplate,
        base::JoinString(test_case.set1, ", ").c_str(),
        base::JoinString(test_case.set2, ", ").c_str(),
        base::JoinString(test_case.expected_intersection, ", ").c_str()));

    URLPatternSet set1;
    for (const auto& pattern_str : test_case.set1) {
      URLPattern pattern(kValidSchemes);
      ASSERT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse(pattern_str))
          << "Failed to parse: " << pattern_str;
      set1.AddPattern(pattern);
    }

    URLPatternSet set2;
    for (const auto& pattern_str : test_case.set2) {
      URLPattern pattern(kValidSchemes);
      ASSERT_EQ(URLPattern::ParseResult::kSuccess, pattern.Parse(pattern_str))
          << "Failed to parse: " << pattern_str;
      set2.AddPattern(pattern);
    }

    URLPatternSet intersection1 = URLPatternSet::CreateIntersection(
        set1, set2, URLPatternSet::IntersectionBehavior::kDetailed);
    URLPatternSet intersection2 = URLPatternSet::CreateIntersection(
        set2, set1, URLPatternSet::IntersectionBehavior::kDetailed);

    EXPECT_THAT(
        intersection1.ToStringVector(),
        testing::UnorderedElementsAreArray(test_case.expected_intersection));
    EXPECT_THAT(
        intersection2.ToStringVector(),
        testing::UnorderedElementsAreArray(test_case.expected_intersection));
  }
}

TEST(URLPatternSetTest, CreateUnion) {
  URLPatternSet empty_set;

  URLPatternSet set1;
  AddPattern(&set1, "http://www.google.com/f*");
  AddPattern(&set1, "http://www.yahoo.com/b*");

  URLPatternSet expected;
  AddPattern(&expected, "http://www.google.com/f*");
  AddPattern(&expected, "http://www.yahoo.com/b*");

  // Union with an empty set.
  URLPatternSet result = URLPatternSet::CreateUnion(set1, empty_set);
  EXPECT_EQ(expected, result);

  // Union with a real set.
  URLPatternSet set2;
  AddPattern(&set2, "http://www.reddit.com/f*");
  AddPattern(&set2, "http://www.yahoo.com/z*");
  AddPattern(&set2, "http://www.google.com/f*");

  AddPattern(&expected, "http://www.reddit.com/f*");
  AddPattern(&expected, "http://www.yahoo.com/z*");

  result = URLPatternSet::CreateUnion(set1, set2);
  EXPECT_EQ(expected, result);
}

TEST(URLPatternSetTest, Contains) {
  URLPatternSet set1;
  URLPatternSet set2;
  URLPatternSet empty_set;

  AddPattern(&set1, "http://www.google.com/*");
  AddPattern(&set1, "http://www.yahoo.com/*");

  AddPattern(&set2, "http://www.reddit.com/*");

  EXPECT_FALSE(set1.Contains(set2));
  EXPECT_TRUE(set1.Contains(empty_set));
  EXPECT_FALSE(empty_set.Contains(set1));

  AddPattern(&set2, "http://www.yahoo.com/*");

  EXPECT_FALSE(set1.Contains(set2));
  EXPECT_FALSE(set2.Contains(set1));

  AddPattern(&set2, "http://www.google.com/*");

  EXPECT_FALSE(set1.Contains(set2));
  EXPECT_TRUE(set2.Contains(set1));

  // Note that this checks if individual patterns contain other patterns, not
  // just equality. For example:
  AddPattern(&set1, "http://*.reddit.com/*");
  EXPECT_TRUE(set1.Contains(set2));
  EXPECT_FALSE(set2.Contains(set1));
}

TEST(URLPatternSetTest, Duplicates) {
  URLPatternSet set1;
  URLPatternSet set2;

  AddPattern(&set1, "http://www.google.com/*");
  AddPattern(&set2, "http://www.google.com/*");

  AddPattern(&set1, "http://www.google.com/*");

  // The sets should still be equal after adding a duplicate.
  EXPECT_EQ(set2, set1);
}

TEST(URLPatternSetTest, ToValueAndPopulate) {
  URLPatternSet set1;
  URLPatternSet set2;

  std::vector<std::string> patterns;
  patterns.push_back("http://www.google.com/*");
  patterns.push_back("http://www.yahoo.com/*");

  for (const auto& pattern : patterns) {
    AddPattern(&set1, pattern);
  }

  std::string error;
  bool allow_file_access = false;
  base::Value::List value = set1.ToValue();
  set2.Populate(value, URLPattern::SCHEME_ALL, allow_file_access, &error);
  EXPECT_EQ(set1, set2);

  set2.ClearPatterns();
  set2.Populate(patterns, URLPattern::SCHEME_ALL, allow_file_access, &error);
  EXPECT_EQ(set1, set2);
}

TEST(URLPatternSetTest, AddOrigin) {
  URLPatternSet set;
  EXPECT_TRUE(set.AddOrigin(
      URLPattern::SCHEME_ALL, GURL("https://www.google.com/")));
  EXPECT_TRUE(set.MatchesURL(GURL("https://www.google.com/foo/bar")));
  EXPECT_FALSE(set.MatchesURL(GURL("http://www.google.com/foo/bar")));
  EXPECT_FALSE(set.MatchesURL(GURL("https://en.google.com/foo/bar")));
  set.ClearPatterns();

  EXPECT_TRUE(set.AddOrigin(
      URLPattern::SCHEME_ALL, GURL("https://google.com/")));
  EXPECT_FALSE(set.MatchesURL(GURL("https://www.google.com/foo/bar")));
  EXPECT_TRUE(set.MatchesURL(GURL("https://google.com/foo/bar")));

  EXPECT_FALSE(set.AddOrigin(
      URLPattern::SCHEME_HTTP, GURL("https://google.com/")));
}

TEST(URLPatternSet, AddOriginIPv6) {
  {
    URLPatternSet set;
    EXPECT_TRUE(set.AddOrigin(URLPattern::SCHEME_HTTP,
                              GURL("http://[2607:f8b0:4005:805::200e]/*")));
  }
  {
    URLPatternSet set;
    EXPECT_TRUE(set.AddOrigin(URLPattern::SCHEME_HTTP,
                              GURL("http://[2607:f8b0:4005:805::200e]/")));
  }
}

TEST(URLPatternSetTest, ToStringVector) {
  URLPatternSet set;
  AddPattern(&set, "https://google.com/");
  AddPattern(&set, "https://google.com/");
  AddPattern(&set, "https://yahoo.com/");

  std::vector<std::string> string_vector = set.ToStringVector();

  EXPECT_EQ(2UL, string_vector.size());

  EXPECT_TRUE(base::Contains(string_vector, "https://google.com/"));
  EXPECT_TRUE(base::Contains(string_vector, "https://yahoo.com/"));
}

TEST(URLPatternSetTest, MatchesHost) {
  URLPatternSet set;
  AddPattern(&set, "https://example.com/");
  AddPattern(&set, "https://*.google.com/");
  AddPattern(&set, "https://*.sub.yahoo.com/");

  struct {
    std::string url;
    bool require_match_subdomains;
    bool expect_matches_host;
  } test_cases[] = {
      // Simple cases to test if the url's host is contained within any patterns
      // in `set`.
      {"http://example.com", false, true},
      {"http://images.google.com/path", false, true},

      // Test subdomain matching for patterns in `set`.
      {"http://example.com", true, false},
      {"http://yahoo.com", true, false},
      {"http://sub.yahoo.com", true, true},
      {"http://asdf.sub.yahoo.com", true, true},
      {"http://google.com", true, true},
  };

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.url);
    EXPECT_EQ(test_case.expect_matches_host,
              set.MatchesHost(GURL(test_case.url),
                              test_case.require_match_subdomains));
  }

  // Test subdomain matching for a pattern that matches any .com site, and a
  // pattern that matches with all urls.
  AddPattern(&set, "https://*.com/");

  EXPECT_TRUE(set.MatchesHost(GURL("http://anything.com"), true));
  EXPECT_FALSE(set.MatchesHost(GURL("http://anything.ca"), false));

  AddPattern(&set, "<all_urls>");
  EXPECT_TRUE(set.MatchesHost(GURL("http://anything.ca"), true));
}

}  // namespace extensions
