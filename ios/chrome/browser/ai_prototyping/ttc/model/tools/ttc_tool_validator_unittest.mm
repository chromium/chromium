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
  auto result = TtcToolValidator::ValidateToolCall(ttc::kToolOpenUrl,
                                                   open_url_args, "call_1");
  EXPECT_TRUE(result.has_value());

  base::DictValue empty_args;
  result = TtcToolValidator::ValidateToolCall(ttc::kToolGoBack, empty_args,
                                              "call_2");
  EXPECT_TRUE(result.has_value());

  result = TtcToolValidator::ValidateToolCall(ttc::kToolGoForward, empty_args,
                                              "call_3");
  EXPECT_TRUE(result.has_value());
}

// Tests defensive rejection against empty identifiers and unknown tools.
TEST_F(TtcToolValidatorTest, TestDefensiveRejections) {
  base::DictValue empty_args;

  // Empty tool name.
  auto result = TtcToolValidator::ValidateToolCall("", empty_args, "call_1");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ttc::kErrorMessageInvalidToolCall);

  // Empty call ID.
  result = TtcToolValidator::ValidateToolCall(ttc::kToolGoBack, empty_args, "");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ttc::kErrorMessageInvalidToolCall);

  // Unknown tool name.
  result =
      TtcToolValidator::ValidateToolCall("unknown_tool", empty_args, "call_1");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ttc::kErrorMessageUnknownTool);
}

// Tests URL validation edge cases and disallowed schemes.
TEST_F(TtcToolValidatorTest, TestUrlValidation) {
  // Missing URL.
  base::DictValue empty_args;
  auto result = TtcToolValidator::ValidateToolCall(ttc::kToolOpenUrl,
                                                   empty_args, "call_1");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ttc::kErrorMessageInvalidArguments);

  // Invalid URL.
  base::DictValue invalid_url_args;
  invalid_url_args.Set("url", "not a valid url ://");
  result = TtcToolValidator::ValidateToolCall(ttc::kToolOpenUrl,
                                              invalid_url_args, "call_2");
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
    base::DictValue args;
    args.Set("url", url);
    result = TtcToolValidator::ValidateToolCall(ttc::kToolOpenUrl, args,
                                                "call_test");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), ttc::kErrorMessageInvalidUrl);
  }

  // Disallow opening in new tab with boolean values.
  base::DictValue new_tab_bool_args;
  new_tab_bool_args.Set("url", "https://example.com");
  new_tab_bool_args.Set("new_tab", true);
  result = TtcToolValidator::ValidateToolCall(
      ttc::kToolOpenUrl, new_tab_bool_args, "call_new_tab");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ttc::kErrorMessageNewTabUnsupported);

  // String "true" or "1" should also be rejected for new_tab.
  base::DictValue new_tab_str_args;
  new_tab_str_args.Set("url", "https://example.com");
  new_tab_str_args.Set("new_tab", "true");
  result = TtcToolValidator::ValidateToolCall(
      ttc::kToolOpenUrl, new_tab_str_args, "call_new_tab_str");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ttc::kErrorMessageNewTabUnsupported);

  // Arbitrary non-boolean string like "today" is invalid and rejected.
  base::DictValue new_tab_invalid_args;
  new_tab_invalid_args.Set("url", "https://example.com");
  new_tab_invalid_args.Set("new_tab", "today");
  result = TtcToolValidator::ValidateToolCall(
      ttc::kToolOpenUrl, new_tab_invalid_args, "call_new_tab_invalid");
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ttc::kErrorMessageNewTabUnsupported);

  // False boolean values should succeed.
  base::DictValue new_tab_false_args;
  new_tab_false_args.Set("url", "https://example.com");
  new_tab_false_args.Set("new_tab", false);
  result = TtcToolValidator::ValidateToolCall(
      ttc::kToolOpenUrl, new_tab_false_args, "call_new_tab_false");
  EXPECT_TRUE(result.has_value());

  base::DictValue new_tab_false_str_args;
  new_tab_false_str_args.Set("url", "https://example.com");
  new_tab_false_str_args.Set("new_tab", "false");
  result = TtcToolValidator::ValidateToolCall(
      ttc::kToolOpenUrl, new_tab_false_str_args, "call_new_tab_false_str");
  EXPECT_TRUE(result.has_value());
}

// Tests creation of TtcActuationRequest from single tool call.
TEST_F(TtcToolValidatorTest, TestCreateActuationRequestSingle) {
  base::DictValue open_url_args;
  open_url_args.Set("url", "https://example.com");
  auto request_result = TtcToolValidator::CreateActuationRequest(
      ttc::kToolOpenUrl, open_url_args, "call_single");
  ASSERT_TRUE(request_result.has_value());
  const TtcActuationRequest& request = request_result.value();
  EXPECT_EQ(request.call_id(), "call_single");
  ASSERT_EQ(request.action_protos.size(), 1u);

  optimization_guide::proto::Action action;
  ASSERT_TRUE(action.ParseFromString(request.action_protos[0]));
  EXPECT_TRUE(action.has_navigate());
  EXPECT_EQ(action.navigate().url(), "https://example.com/");

  // Back tool request.
  base::DictValue empty_args;
  auto back_result = TtcToolValidator::CreateActuationRequest(
      ttc::kToolGoBack, empty_args, "call_back");
  ASSERT_TRUE(back_result.has_value());
  const TtcActuationRequest& back_request = back_result.value();
  ASSERT_EQ(back_request.action_protos.size(), 1u);
  optimization_guide::proto::Action back_action;
  ASSERT_TRUE(back_action.ParseFromString(back_request.action_protos[0]));
  EXPECT_TRUE(back_action.has_back());

  // Forward tool request.
  auto forward_result = TtcToolValidator::CreateActuationRequest(
      ttc::kToolGoForward, empty_args, "call_forward");
  ASSERT_TRUE(forward_result.has_value());
  const TtcActuationRequest& forward_request = forward_result.value();
  ASSERT_EQ(forward_request.action_protos.size(), 1u);
  optimization_guide::proto::Action forward_action;
  ASSERT_TRUE(forward_action.ParseFromString(forward_request.action_protos[0]));
  EXPECT_TRUE(forward_action.has_forward());
}

// Tests creation of TtcActuationRequest from batch of tool calls.
TEST_F(TtcToolValidatorTest, TestCreateActuationRequestBatch) {
  // Empty tool calls rejected.
  base::ListValue empty_calls;
  auto empty_result =
      TtcToolValidator::CreateActuationRequestWithToolCalls(empty_calls);
  ASSERT_FALSE(empty_result.has_value());
  EXPECT_EQ(empty_result.error(), ttc::kErrorMessageEmptyToolCalls);

  // Invalid array element rejected.
  base::ListValue invalid_calls;
  invalid_calls.Append("not_a_dictionary");
  auto invalid_item_result =
      TtcToolValidator::CreateActuationRequestWithToolCalls(invalid_calls);
  ASSERT_FALSE(invalid_item_result.has_value());
  EXPECT_EQ(invalid_item_result.error(), ttc::kErrorMessageInvalidToolCall);

  base::DictValue args_1;
  args_1.Set("url", "https://google.com");
  base::DictValue call_1;
  call_1.Set("name", ttc::kToolOpenUrl);
  call_1.Set("id", "call_batch_1");
  call_1.Set("args", std::move(args_1));

  base::DictValue call_2;
  call_2.Set("name", ttc::kToolGoBack);
  call_2.Set("id", "call_batch_2");
  call_2.Set("args", base::DictValue());

  base::ListValue tool_calls;
  tool_calls.Append(std::move(call_1));
  tool_calls.Append(std::move(call_2));

  auto request_result =
      TtcToolValidator::CreateActuationRequestWithToolCalls(tool_calls);
  ASSERT_TRUE(request_result.has_value());
  const TtcActuationRequest& request = request_result.value();
  EXPECT_EQ(request.call_id(), "call_batch_1");
  ASSERT_EQ(request.call_ids.size(), 2u);
  EXPECT_EQ(request.call_ids[0], "call_batch_1");
  EXPECT_EQ(request.call_ids[1], "call_batch_2");
  ASSERT_EQ(request.action_protos.size(), 2u);

  optimization_guide::proto::Action action1;
  ASSERT_TRUE(action1.ParseFromString(request.action_protos[0]));
  EXPECT_TRUE(action1.has_navigate());
  EXPECT_EQ(action1.navigate().url(), "https://google.com/");

  optimization_guide::proto::Action action2;
  ASSERT_TRUE(action2.ParseFromString(request.action_protos[1]));
  EXPECT_TRUE(action2.has_back());
}

// Tests TtcActuationResponse initialization and field access.
TEST_F(TtcToolValidatorTest, TestTtcActuationResponse) {
  TtcActuationResponse response(actor::mojom::ActionResultCode::kOk,
                                std::string(), "call_123");

  EXPECT_EQ(response.result_code, actor::mojom::ActionResultCode::kOk);
  EXPECT_TRUE(response.error_message.empty());
  EXPECT_EQ(response.call_id, "call_123");

  TtcActuationResponse error_response(
      actor::mojom::ActionResultCode::kArgumentsInvalid, "Something went wrong",
      "call_error");

  EXPECT_EQ(error_response.result_code,
            actor::mojom::ActionResultCode::kArgumentsInvalid);
  EXPECT_EQ(error_response.error_message, "Something went wrong");
  EXPECT_EQ(error_response.call_id, "call_error");
}

// Tests TtcActuationRequest batch call IDs accessor.
TEST_F(TtcToolValidatorTest, TestTtcActuationRequestBatchCallIDs) {
  std::vector<std::string> call_ids = {"call_1", "call_2", "call_3"};
  TtcActuationRequest request({}, "In progress", call_ids);

  EXPECT_EQ(request.call_id(), "call_1");
  EXPECT_EQ(request.call_ids, call_ids);
  EXPECT_EQ(request.task_update, "In progress");

  TtcActuationRequest empty_ids_request({}, "In progress",
                                        std::vector<std::string>{});
  EXPECT_EQ(empty_ids_request.call_id(), std::nullopt);
}

}  // namespace
