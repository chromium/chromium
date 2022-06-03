// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/file_highlighter.h"

#include <string>

#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kManifest[] =
"{\n"
"  \"name\": \"Content Scripts\",\n"
"  \"version\": \"2.0\",\n"
"  // this is a comment with the word permissions.\n"
"  /* This is a multine\n"
"     comment with the word permissions\n"
"     that shouldn't be highlighted */\n"
"  \"permissions\": [\n"
"    /* This is a tricky comment because it has brackets }]*/\n"
"    \"tabs\"\n"
"  ],\n"
"  \"content_scripts\": [\n"
"    {\n"
"      \"matches\": [\"*://aaronboodman.com/*\", \"*://rdcronin.com/*\"],\n"
"      \"js\": [\"myscript.js\"]\n"
"    }\n"
"  ],\n"
"  \"test_key\": {\n"
"    \"escaped_quoted\\\"\",\n"
"    \"/*foo*/\"\n"
"  },\n"
"  \"manifest_version\": 2,\n"
"  \"international_key\": \"還是不要\"\n"
"}";

}  // namespace

TEST(ManifestHighlighterUnitTest, ManifestHighlighterUnitTest) {
  // Get a full key.
  const char kPermissionsFeature[] =
      "\"permissions\": [\n"
      "    /* This is a tricky comment because it has brackets }]*/\n"
      "    \"tabs\"\n"
      "  ]";
  ManifestHighlighter permissions(kManifest, "permissions", std::string());
  EXPECT_EQ(kPermissionsFeature, permissions.GetFeature());

  // Get a specific portion of a key.
  const char kTabsFeature[] = "\"tabs\"";
  ManifestHighlighter tabs(kManifest, "permissions", "tabs");
  EXPECT_EQ(kTabsFeature, tabs.GetFeature());

  // Get a single-character, non-quoted entity of a key.
  const char kManifestVersionFeature[] = "2";
  ManifestHighlighter version(kManifest, "manifest_version", "2");
  EXPECT_EQ(kManifestVersionFeature, version.GetFeature());

  // Get a compound portion of a key, including quoted '//' (which shouldn't be
  // mistaken for comments).
  const char kMatchesFeature[] =
      "\"matches\": [\"*://aaronboodman.com/*\", \"*://rdcronin.com/*\"]";
  ManifestHighlighter matches(kManifest, "content_scripts", "matches");
  EXPECT_EQ(kMatchesFeature, matches.GetFeature());

  // If a feature isn't present, we should get an empty string.
  ManifestHighlighter not_present(kManifest, "a_fake_feature", std::string());
  EXPECT_EQ(std::string(), not_present.GetFeature());

  // If we request a specific portion of a key which is not found, we should
  // get an empty string.
  ManifestHighlighter specific_portion_not_present(
      kManifest, "permissions", "a_fake_feature");
  EXPECT_EQ(std::string(), specific_portion_not_present.GetFeature());

  const char kEscapedQuotedFeature[] = "\"escaped_quoted\\\"\"";
  ManifestHighlighter escaped_quoted(
      kManifest, "test_key", "escaped_quoted\\\"");
  EXPECT_EQ(kEscapedQuotedFeature, escaped_quoted.GetFeature());

  const char kFeatureWithComment[] = "\"/*foo*/\"";
  ManifestHighlighter feature_with_comment(kManifest, "test_key", "/*foo*/");
  EXPECT_EQ(kFeatureWithComment, feature_with_comment.GetFeature());

  // Check with non-ascii characters.
  const char kInternationalFeature[] = "\"international_key\": \"還是不要\"";
  ManifestHighlighter international_feature(
      kManifest, "international_key", std::string());
  EXPECT_EQ(kInternationalFeature, international_feature.GetFeature());

  // Empty manifest. Check that there is no crash.
  const char kEmptyManifest[] = "";
  ManifestHighlighter no_feature(kEmptyManifest, std::string(), std::string());
  EXPECT_EQ(std::string(), no_feature.GetFeature());
}

TEST(SouceHighlighterUnitTest, SourceHighlighterUnitTest) {
  const char kBasicSourceFile[] = "line one\nline two\nline three";

  SourceHighlighter basic1(kBasicSourceFile, 1u);
  EXPECT_EQ("line one", basic1.GetFeature());
  SourceHighlighter basic2(kBasicSourceFile, 2u);
  EXPECT_EQ("line two", basic2.GetFeature());
  SourceHighlighter basic3(kBasicSourceFile, 3u);
  EXPECT_EQ("line three", basic3.GetFeature());

  const char kNoNewlineSourceFile[] = "thisisonelonglinewithnobreaksinit";

  SourceHighlighter full_line(kNoNewlineSourceFile, 1u);
  EXPECT_EQ(kNoNewlineSourceFile, full_line.GetFeature());

  SourceHighlighter line_zero(kNoNewlineSourceFile, 0u);
  EXPECT_EQ(std::string(), line_zero.GetFeature());

  SourceHighlighter out_of_bounds(kNoNewlineSourceFile, 2u);
  EXPECT_EQ(std::string(), out_of_bounds.GetFeature());
}

}  // namespace extensions
