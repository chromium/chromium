// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_validator.h"

#import <optional>
#import <string>
#import <utility>
#import <vector>

#import "base/values.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_data_types.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_definitions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

using TtcToolValidatorTest = PlatformTest;

// Tests syntactic validation for valid tool calls.
TEST_F(TtcToolValidatorTest, TestValidToolCalls) {
  base::DictValue open_url_args;
  open_url_args.Set("url", "https://example.com");
  auto result = TtcToolValidator::ValidateToolCall(
      ttc::ToolRequest{ttc::kToolOpenUrl, std::move(open_url_args)});
  EXPECT_TRUE(result.has_value());

  result = TtcToolValidator::ValidateToolCall(
      ttc::ToolRequest{ttc::kToolGoBack, base::DictValue()});
  EXPECT_TRUE(result.has_value());

  result = TtcToolValidator::ValidateToolCall(
      ttc::ToolRequest{ttc::kToolGoForward, base::DictValue()});
  EXPECT_TRUE(result.has_value());
}

// Tests defensive rejection against an empty tool name and unknown tools.
TEST_F(TtcToolValidatorTest, TestDefensiveRejections) {
  // Empty tool name.
  auto result = TtcToolValidator::ValidateToolCall(
      ttc::ToolRequest{"", base::DictValue()});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ttc::kErrorMessageInvalidToolCall);

  // Unknown tool name.
  result = TtcToolValidator::ValidateToolCall(
      ttc::ToolRequest{"unknown_tool", base::DictValue()});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ttc::kErrorMessageUnknownTool);
}

// Tests URL validation edge cases and disallowed schemes.
TEST_F(TtcToolValidatorTest, TestUrlValidation) {
  // Missing URL.
  auto result = TtcToolValidator::ValidateToolCall(
      ttc::ToolRequest{ttc::kToolOpenUrl, base::DictValue()});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ttc::kErrorMessageInvalidArguments);

  // Invalid URL.
  base::DictValue invalid_url_args;
  invalid_url_args.Set("url", "not a valid url ://");
  result = TtcToolValidator::ValidateToolCall(
      ttc::ToolRequest{ttc::kToolOpenUrl, std::move(invalid_url_args)});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ttc::kErrorMessageInvalidUrl);

  // Disallowed schemes.
  const std::vector<std::string> disallowed_urls = {
      "javascript:alert(1)",
      "chrome://flags",
      "file:///etc/passwd",
      "data:text/html,<h1>hi</h1>",
  };
  for (const std::string& url : disallowed_urls) {
    SCOPED_TRACE(url);
    base::DictValue args;
    args.Set("url", url);
    result = TtcToolValidator::ValidateToolCall(
        ttc::ToolRequest{ttc::kToolOpenUrl, std::move(args)});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ttc::kErrorMessageInvalidUrl);
  }

  // Disallow opening in new tab with boolean values.
  base::DictValue new_tab_bool_args;
  new_tab_bool_args.Set("url", "https://example.com");
  new_tab_bool_args.Set("new_tab", true);
  result = TtcToolValidator::ValidateToolCall(
      ttc::ToolRequest{ttc::kToolOpenUrl, std::move(new_tab_bool_args)});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ttc::kErrorMessageNewTabUnsupported);

  // String "true" or "1" should also be rejected for new_tab.
  base::DictValue new_tab_str_args;
  new_tab_str_args.Set("url", "https://example.com");
  new_tab_str_args.Set("new_tab", "true");
  result = TtcToolValidator::ValidateToolCall(
      ttc::ToolRequest{ttc::kToolOpenUrl, std::move(new_tab_str_args)});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ttc::kErrorMessageNewTabUnsupported);

  // Arbitrary non-boolean string like "today" is invalid and rejected.
  base::DictValue new_tab_invalid_args;
  new_tab_invalid_args.Set("url", "https://example.com");
  new_tab_invalid_args.Set("new_tab", "today");
  result = TtcToolValidator::ValidateToolCall(
      ttc::ToolRequest{ttc::kToolOpenUrl, std::move(new_tab_invalid_args)});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ttc::kErrorMessageNewTabUnsupported);

  // False boolean values should succeed.
  base::DictValue new_tab_false_args;
  new_tab_false_args.Set("url", "https://example.com");
  new_tab_false_args.Set("new_tab", false);
  result = TtcToolValidator::ValidateToolCall(
      ttc::ToolRequest{ttc::kToolOpenUrl, std::move(new_tab_false_args)});
  EXPECT_TRUE(result.has_value());

  base::DictValue new_tab_false_str_args;
  new_tab_false_str_args.Set("url", "https://example.com");
  new_tab_false_str_args.Set("new_tab", "false");
  result = TtcToolValidator::ValidateToolCall(
      ttc::ToolRequest{ttc::kToolOpenUrl, std::move(new_tab_false_str_args)});
  EXPECT_TRUE(result.has_value());
}

// Tests creation of `TtcActuationRequest` from a single tool call.
TEST_F(TtcToolValidatorTest, TestCreateActuationRequestSingle) {
  base::DictValue open_url_args;
  open_url_args.Set("url", "https://example.com");
  auto request_result = TtcToolValidator::CreateActuationRequest(
      ttc::ToolRequest{ttc::kToolOpenUrl, std::move(open_url_args)});
  ASSERT_TRUE(request_result.has_value());
  const TtcActuationRequest& request = request_result.value();
  ASSERT_EQ(request.action_protos.size(), 1u);

  optimization_guide::proto::Action action;
  ASSERT_TRUE(action.ParseFromString(request.action_protos[0]));
  EXPECT_TRUE(action.has_navigate());
  EXPECT_EQ(action.navigate().url(), "https://example.com/");

  // Back tool request.
  auto back_result = TtcToolValidator::CreateActuationRequest(
      ttc::ToolRequest{ttc::kToolGoBack, base::DictValue()});
  ASSERT_TRUE(back_result.has_value());
  const TtcActuationRequest& back_request = back_result.value();
  ASSERT_EQ(back_request.action_protos.size(), 1u);
  optimization_guide::proto::Action back_action;
  ASSERT_TRUE(back_action.ParseFromString(back_request.action_protos[0]));
  EXPECT_TRUE(back_action.has_back());

  // Forward tool request.
  auto forward_result = TtcToolValidator::CreateActuationRequest(
      ttc::ToolRequest{ttc::kToolGoForward, base::DictValue()});
  ASSERT_TRUE(forward_result.has_value());
  const TtcActuationRequest& forward_request = forward_result.value();
  ASSERT_EQ(forward_request.action_protos.size(), 1u);
  optimization_guide::proto::Action forward_action;
  ASSERT_TRUE(forward_action.ParseFromString(forward_request.action_protos[0]));
  EXPECT_TRUE(forward_action.has_forward());
}

}  // namespace
