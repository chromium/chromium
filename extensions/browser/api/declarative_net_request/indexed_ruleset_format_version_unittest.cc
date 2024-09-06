// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "extensions/browser/api/declarative_net_request/utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions::declarative_net_request {
namespace {

const char* kFlatbufferSchemaExpected = R"(
include "components/url_pattern_index/flat/url_pattern_index.fbs";
namespace extensions.declarative_net_request.flat;
enum ActionType : ubyte {
  block,
  allow,
  redirect,
  upgrade_scheme,
  modify_headers,
  allow_all_requests,
  count
}
table QueryKeyValue {
  key : string (required);
  value : string (required);
  replace_only: bool = false;
}
table UrlTransform {
   scheme : string;
   host : string;
   clear_port : bool = false;
   port : string;
   clear_path : bool = false;
   path : string;
   clear_query : bool = false;
   query : string;
   remove_query_params : [string];
   add_or_replace_query_params : [QueryKeyValue];
   clear_fragment : bool = false;
   fragment : string;
   username : string;
   password : string;
}
table UrlRuleMetadata {
  id : uint (key);
  action : ActionType;
  redirect_url : string;
  transform : UrlTransform;
  request_headers: [ModifyHeaderInfo];
  response_headers: [ModifyHeaderInfo];
}
table EmbedderConditions {
  tab_ids_included : [int];
  tab_ids_excluded : [int];
  response_headers: [HeaderCondition];
  excluded_response_headers: [HeaderCondition];
}
enum IndexType : ubyte {
  before_request_except_allow_all_requests = 0,
  allow_all_requests,
  modify_headers,
  count
}
table RegexFilterOptions {
  match_all: bool = false;
}
enum HeaderOperation : ubyte {
  append,
  set,
  remove
}
table ModifyHeaderInfo {
  operation: HeaderOperation;
  header: string;
  value: string;
  regex_filter: string;
  regex_substitution: string;
  regex_options: RegexFilterOptions;
}
table HeaderCondition {
  header: string;
  values: [string];
  excluded_values: [string];
}
table RegexRule {
  url_rule: url_pattern_index.flat.UrlRule;
  action_type: ActionType;
  regex_substitution: string;
}
table ExtensionIndexedRuleset {
  before_request_index_list : [url_pattern_index.flat.UrlPatternIndex];
  headers_received_index_list : [url_pattern_index.flat.UrlPatternIndex];
  before_request_regex_rules: [RegexRule];
  headers_received_regex_rules: [RegexRule];
  extension_metadata : [UrlRuleMetadata];
}
root_type ExtensionIndexedRuleset;
file_identifier "EXTR";
)";

const char* kSingleLineComment = "//";
const char* kMultiLineCommentStart = "/*";
const char* kMultiLineCommentEnd = "*/";
const char* kNewline = "\n";

// Strips comments from |input| and removes all whitespace. Note: this is not a
// rigorous implementation.
std::string StripCommentsAndWhitespace(const std::string& input) {
  std::string result;

  for (auto& line : base::SplitString(input, kNewline, base::KEEP_WHITESPACE,
                                      base::SPLIT_WANT_NONEMPTY)) {
    // Remove single line comments.
    size_t index = line.find(kSingleLineComment);
    if (index != std::string::npos) {
      line.erase(index);
    }

    // Remove any whitespace.
    std::string str;
    base::RemoveChars(line, base::kWhitespaceASCII, &str);
    result += str;
  }

  // Remove multi line comments.
  while (true) {
    size_t start = result.find(kMultiLineCommentStart);
    if (start == std::string::npos) {
      break;
    }

    size_t end = result.find(kMultiLineCommentEnd, start + 2);
    // No ending found for the comment.
    if (end == std::string::npos) {
      break;
    }

    size_t end_comment_index = end + 1;
    size_t comment_length = end_comment_index - start + 1;
    result.erase(start, comment_length);
  }

  return result;
}

using IndexedRulesetFormatVersionTest = ::testing::Test;

// Ensures that we update the indexed ruleset format version when the flatbuffer
// schema is modified.
TEST_F(IndexedRulesetFormatVersionTest, CheckVersionUpdated) {
  base::FilePath source_root;
  ASSERT_TRUE(
      base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_root));

  base::FilePath flatbuffer_schema_path = source_root.AppendASCII(
      "extensions/browser/api/declarative_net_request/flat/"
      "extension_ruleset.fbs");
  ASSERT_TRUE(base::PathExists(flatbuffer_schema_path));

  std::string flatbuffer_schema;
  ASSERT_TRUE(
      base::ReadFileToString(flatbuffer_schema_path, &flatbuffer_schema));

  EXPECT_EQ(StripCommentsAndWhitespace(kFlatbufferSchemaExpected),
            StripCommentsAndWhitespace(flatbuffer_schema))
      << "Schema change detected; update this test and the schema version.";
  EXPECT_EQ(34, GetIndexedRulesetFormatVersionForTesting())
      << "Update this test if you update the schema version.";
}

// Test to sanity check the behavior of StripCommentsAndWhitespace.
TEST_F(IndexedRulesetFormatVersionTest, StripCommentsAndWhitespace) {
  std::string input = R"(
      // This is a single line comment.
      Some text // Another comment.
      /* Multi-line
        Comment */ More text.
      /* Another multi-line
      comment */ Yet more text.
  )";

  std::string expected_output = "SometextMoretext.Yetmoretext.";
  EXPECT_EQ(expected_output, StripCommentsAndWhitespace(input));
}

}  // namespace
}  // namespace extensions::declarative_net_request
