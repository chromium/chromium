// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_validator.h"

#import <optional>
#import <string>
#import <string_view>
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

// Creates a post-processed `node_id` dictionary containing `document_id` and
// `dom_node_id`.
base::DictValue CreateNodeIdDict(std::string_view doc_id, int dom_node_id) {
  return base::DictValue()
      .Set("document_id", doc_id)
      .Set("dom_node_id", dom_node_id);
}

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

  result = TtcToolValidator::ValidateToolCall(
      ttc::ToolRequest{ttc::kToolCloseSession, base::DictValue()});
  EXPECT_TRUE(result.has_value());

  result = TtcToolValidator::ValidateToolCall(
      ttc::ToolRequest{ttc::kToolCloseCurrentTab, base::DictValue()});
  EXPECT_TRUE(result.has_value());

  result = TtcToolValidator::ValidateToolCall(
      ttc::ToolRequest{ttc::kToolCreateTab, base::DictValue()});
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

  // `close_session` is a manual task hook and does not create an Actor request.
  base::expected<TtcActuationRequest, std::string> close_session_request =
      TtcToolValidator::CreateActuationRequest(
          ttc::ToolRequest{ttc::kToolCloseSession, base::DictValue()});
  ASSERT_FALSE(close_session_request.has_value());
  EXPECT_EQ(close_session_request.error(), ttc::kErrorMessageInvalidToolCall);
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

  // Opening in a new tab with boolean or string-boolean values succeeds.
  base::DictValue new_tab_bool_args;
  new_tab_bool_args.Set("url", "https://example.com");
  new_tab_bool_args.Set("new_tab", true);
  result = TtcToolValidator::ValidateToolCall(
      ttc::ToolRequest{ttc::kToolOpenUrl, std::move(new_tab_bool_args)});
  EXPECT_TRUE(result.has_value());

  base::DictValue new_tab_str_args;
  new_tab_str_args.Set("url", "https://example.com");
  new_tab_str_args.Set("new_tab", "true");
  result = TtcToolValidator::ValidateToolCall(
      ttc::ToolRequest{ttc::kToolOpenUrl, std::move(new_tab_str_args)});
  EXPECT_TRUE(result.has_value());

  // Arbitrary non-boolean string like "today" is invalid and rejected.
  base::DictValue new_tab_invalid_args;
  new_tab_invalid_args.Set("url", "https://example.com");
  new_tab_invalid_args.Set("new_tab", "today");
  result = TtcToolValidator::ValidateToolCall(
      ttc::ToolRequest{ttc::kToolOpenUrl, std::move(new_tab_invalid_args)});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), ttc::kErrorMessageInvalidArguments);

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

// Tests that `IsOpenUrlInNewTab` returns true only for `open_url` requests
// where `new_tab` parses to `true`, and `CreateActuationRequest` rejects
// `open_url(new_tab=true)` because it is executed directly by
// `TtcActuationHandler`.
TEST_F(TtcToolValidatorTest,
       OpenUrlInNewTabIsDetectedAndNotMappedToActorProto) {
  base::DictValue args;
  args.Set("url", "https://example.com");
  args.Set("new_tab", true);
  ttc::ToolRequest new_tab_request{ttc::kToolOpenUrl, std::move(args)};

  EXPECT_TRUE(TtcToolValidator::IsOpenUrlInNewTab(new_tab_request));

  base::expected<TtcActuationRequest, std::string> request_result =
      TtcToolValidator::CreateActuationRequest(new_tab_request);
  ASSERT_FALSE(request_result.has_value());
  EXPECT_EQ(request_result.error(), ttc::kErrorMessageInvalidToolCall);
}

// Tests that `CreateActuationRequest` for `open_url` with `new_tab: false`
// emits a single `NavigateAction` without creating a tab.
TEST_F(TtcToolValidatorTest,
       CreateActuationRequestOpenUrlInCurrentTabEmitsSingleNavigateAction) {
  base::DictValue args;
  args.Set("url", "https://example.com");
  args.Set("new_tab", false);
  ttc::ToolRequest current_tab_request{ttc::kToolOpenUrl, std::move(args)};

  EXPECT_FALSE(TtcToolValidator::IsOpenUrlInNewTab(current_tab_request));

  base::expected<TtcActuationRequest, std::string> request_result =
      TtcToolValidator::CreateActuationRequest(current_tab_request);

  ASSERT_TRUE(request_result.has_value());
  const TtcActuationRequest& request = request_result.value();
  ASSERT_EQ(request.action_protos.size(), 1u);

  optimization_guide::proto::Action action;
  ASSERT_TRUE(action.ParseFromString(request.action_protos[0]));
  ASSERT_TRUE(action.has_navigate());
  EXPECT_EQ(action.navigate().url(), "https://example.com/");
}

// Tests validation and action creation for `close_current_tab` and
// `create_tab`.
TEST_F(TtcToolValidatorTest, CreateActuationRequestForTabManagementTools) {
  // `create_tab` creates a foreground tab.
  base::expected<TtcActuationRequest, std::string> create_result =
      TtcToolValidator::CreateActuationRequest(
          ttc::ToolRequest{ttc::kToolCreateTab, base::DictValue()});
  ASSERT_TRUE(create_result.has_value());
  ASSERT_EQ(create_result->action_protos.size(), 1u);
  optimization_guide::proto::Action create_action;
  ASSERT_TRUE(create_action.ParseFromString(create_result->action_protos[0]));
  ASSERT_TRUE(create_action.has_create_tab());
  EXPECT_TRUE(create_action.create_tab().foreground());

  // `close_current_tab` emits a `CloseTabAction` with `tab_id` unset for
  // downstream injection.
  base::expected<TtcActuationRequest, std::string> close_result =
      TtcToolValidator::CreateActuationRequest(
          ttc::ToolRequest{ttc::kToolCloseCurrentTab, base::DictValue()});
  ASSERT_TRUE(close_result.has_value());
  ASSERT_EQ(close_result->action_protos.size(), 1u);
  optimization_guide::proto::Action close_action;
  ASSERT_TRUE(close_action.ParseFromString(close_result->action_protos[0]));
  ASSERT_TRUE(close_action.has_close_tab());
  EXPECT_FALSE(close_action.close_tab().has_tab_id());
}

// Tests validation and `ClickAction` creation for `click_element`, including
// rejection of missing, non-dictionary, missing/empty-`document_id`, or
// missing/non-positive `dom_node_id` targets.
TEST_F(TtcToolValidatorTest,
       CreateActuationRequestForClickElementAndTargetValidation) {
  base::DictValue valid_args;
  valid_args.Set("node_id", CreateNodeIdDict("doc-token-1", 42));

  base::expected<TtcActuationRequest, std::string> click_result =
      TtcToolValidator::CreateActuationRequest(
          ttc::ToolRequest{ttc::kToolClickElement, std::move(valid_args)});

  ASSERT_TRUE(click_result.has_value());
  ASSERT_EQ(click_result->action_protos.size(), 1u);
  optimization_guide::proto::Action action;
  ASSERT_TRUE(action.ParseFromString(click_result->action_protos[0]));
  ASSERT_TRUE(action.has_click());
  EXPECT_EQ(action.click().click_type(),
            optimization_guide::proto::ClickAction::LEFT);
  EXPECT_EQ(action.click().click_count(),
            optimization_guide::proto::ClickAction::SINGLE);
  EXPECT_EQ(action.click().target().content_node_id(), 42);
  EXPECT_EQ(action.click().target().document_identifier().serialized_token(),
            "doc-token-1");

  // Missing `node_id`, raw integer `node_id`, missing or empty `document_id`,
  // and missing or non-positive `dom_node_id` values must fail with
  // `kErrorMessageInvalidArguments`.
  std::vector<base::DictValue> invalid_arg_cases;
  invalid_arg_cases.push_back(base::DictValue());
  invalid_arg_cases.push_back(base::DictValue().Set("node_id", 42));
  invalid_arg_cases.push_back(base::DictValue().Set(
      "node_id", base::DictValue().Set("dom_node_id", 42)));
  invalid_arg_cases.push_back(
      base::DictValue().Set("node_id", CreateNodeIdDict("", 42)));
  invalid_arg_cases.push_back(base::DictValue().Set(
      "node_id", base::DictValue().Set("document_id", "doc-token-1")));
  invalid_arg_cases.push_back(
      base::DictValue().Set("node_id", CreateNodeIdDict("doc-token-1", 0)));
  invalid_arg_cases.push_back(
      base::DictValue().Set("node_id", CreateNodeIdDict("doc-token-1", -5)));

  for (size_t i = 0; i < invalid_arg_cases.size(); ++i) {
    SCOPED_TRACE(i);
    base::expected<TtcActuationRequest, std::string> invalid_result =
        TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
            ttc::kToolClickElement, std::move(invalid_arg_cases[i])});
    ASSERT_FALSE(invalid_result.has_value());
    EXPECT_EQ(invalid_result.error(), ttc::kErrorMessageInvalidArguments);
  }
}

// Tests validation and `SelectAction` creation for `select_option`, including
// rejection of missing `value` or invalid `node_id` targets.
TEST_F(TtcToolValidatorTest, CreateActuationRequestForSelectOption) {
  base::DictValue select_args;
  select_args.Set("node_id", CreateNodeIdDict("doc-token-1", 15));
  select_args.Set("value", "option_b");

  base::expected<TtcActuationRequest, std::string> select_result =
      TtcToolValidator::CreateActuationRequest(
          ttc::ToolRequest{ttc::kToolSelectOption, std::move(select_args)});

  ASSERT_TRUE(select_result.has_value());
  ASSERT_EQ(select_result->action_protos.size(), 1u);
  optimization_guide::proto::Action select_action;
  ASSERT_TRUE(select_action.ParseFromString(select_result->action_protos[0]));
  ASSERT_TRUE(select_action.has_select());
  EXPECT_EQ(select_action.select().value(), "option_b");
  EXPECT_EQ(select_action.select().target().content_node_id(), 15);
  EXPECT_EQ(
      select_action.select().target().document_identifier().serialized_token(),
      "doc-token-1");

  std::vector<base::DictValue> invalid_select_cases;
  invalid_select_cases.push_back(
      base::DictValue().Set("node_id", CreateNodeIdDict("doc-token-1", 15)));
  invalid_select_cases.push_back(base::DictValue().Set("value", "option_b"));
  invalid_select_cases.push_back(
      base::DictValue()
          .Set("node_id", CreateNodeIdDict("doc-token-1", 0))
          .Set("value", "option_b"));

  for (size_t i = 0; i < invalid_select_cases.size(); ++i) {
    SCOPED_TRACE(i);
    base::expected<TtcActuationRequest, std::string> invalid_select =
        TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
            ttc::kToolSelectOption, std::move(invalid_select_cases[i])});
    ASSERT_FALSE(invalid_select.has_value());
    EXPECT_EQ(invalid_select.error(), ttc::kErrorMessageInvalidArguments);
  }
}

// Tests validation and `TypeAction` creation for `set_text`, including
// rejection of missing `text` or invalid `node_id` targets.
TEST_F(TtcToolValidatorTest, CreateActuationRequestForSetText) {
  base::DictValue set_text_args;
  set_text_args.Set("node_id", CreateNodeIdDict("doc-token-1", 27));
  set_text_args.Set("text", "search query");

  base::expected<TtcActuationRequest, std::string> set_text_result =
      TtcToolValidator::CreateActuationRequest(
          ttc::ToolRequest{ttc::kToolSetText, std::move(set_text_args)});

  ASSERT_TRUE(set_text_result.has_value());
  ASSERT_EQ(set_text_result->action_protos.size(), 1u);
  optimization_guide::proto::Action type_action;
  ASSERT_TRUE(type_action.ParseFromString(set_text_result->action_protos[0]));
  ASSERT_TRUE(type_action.has_type());
  EXPECT_EQ(type_action.type().text(), "search query");
  EXPECT_EQ(type_action.type().mode(),
            optimization_guide::proto::TypeAction::DELETE_EXISTING);
  EXPECT_FALSE(type_action.type().follow_by_enter());
  EXPECT_EQ(type_action.type().target().content_node_id(), 27);
  EXPECT_EQ(
      type_action.type().target().document_identifier().serialized_token(),
      "doc-token-1");

  std::vector<base::DictValue> invalid_set_text_cases;
  invalid_set_text_cases.push_back(
      base::DictValue().Set("node_id", CreateNodeIdDict("doc-token-1", 27)));
  invalid_set_text_cases.push_back(
      base::DictValue().Set("text", "search query"));
  invalid_set_text_cases.push_back(base::DictValue()
                                       .Set("node_id", CreateNodeIdDict("", 27))
                                       .Set("text", "search query"));

  for (size_t i = 0; i < invalid_set_text_cases.size(); ++i) {
    SCOPED_TRACE(i);
    base::expected<TtcActuationRequest, std::string> invalid_set_text =
        TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
            ttc::kToolSetText, std::move(invalid_set_text_cases[i])});
    ASSERT_FALSE(invalid_set_text.has_value());
    EXPECT_EQ(invalid_set_text.error(), ttc::kErrorMessageInvalidArguments);
  }
}

// Tests that `CreateActuationRequest` for `scroll_page` with `"page"`
// granularity on the root viewport (`dom_node_id == 0`) maps all four
// directions and leaves `distance` and `target` unset.
TEST_F(
    TtcToolValidatorTest,
    CreateActuationRequestForScrollPageWithPageGranularityLeavesDistanceAndTargetUnset) {
  struct DirectionCase {
    const char* name;
    optimization_guide::proto::ScrollAction::ScrollDirection expected;
  };
  const DirectionCase kDirections[] = {
      {"up", optimization_guide::proto::ScrollAction::UP},
      {"down", optimization_guide::proto::ScrollAction::DOWN},
      {"left", optimization_guide::proto::ScrollAction::LEFT},
      {"right", optimization_guide::proto::ScrollAction::RIGHT},
  };

  for (const DirectionCase& dir_case : kDirections) {
    SCOPED_TRACE(dir_case.name);
    base::DictValue page_args;
    page_args.Set("granularity", "page");
    page_args.Set("direction", dir_case.name);
    page_args.Set("node_id", CreateNodeIdDict("", 0));

    base::expected<TtcActuationRequest, std::string> page_result =
        TtcToolValidator::CreateActuationRequest(
            ttc::ToolRequest{ttc::kToolScrollPage, std::move(page_args)});

    ASSERT_TRUE(page_result.has_value());
    ASSERT_EQ(page_result->action_protos.size(), 1u);
    optimization_guide::proto::Action action;
    ASSERT_TRUE(action.ParseFromString(page_result->action_protos[0]));
    ASSERT_TRUE(action.has_scroll());
    EXPECT_EQ(action.scroll().direction(), dir_case.expected);
    EXPECT_FALSE(action.scroll().has_distance());
    EXPECT_FALSE(action.scroll().has_target());
  }
}

// Tests that `CreateActuationRequest` for `scroll_page` with `"document"`
// granularity on a scrollable container (`dom_node_id > 0`) sets the sentinel
// distance (`100000.0f`) and populates `target`.
TEST_F(
    TtcToolValidatorTest,
    CreateActuationRequestForScrollPageWithDocumentGranularitySetsDistanceAndTarget) {
  base::DictValue doc_args;
  doc_args.Set("granularity", "document");
  doc_args.Set("direction", "down");
  doc_args.Set("node_id", CreateNodeIdDict("doc-token-1", 42));

  base::expected<TtcActuationRequest, std::string> doc_result =
      TtcToolValidator::CreateActuationRequest(
          ttc::ToolRequest{ttc::kToolScrollPage, std::move(doc_args)});

  ASSERT_TRUE(doc_result.has_value());
  ASSERT_EQ(doc_result->action_protos.size(), 1u);
  optimization_guide::proto::Action doc_action;
  ASSERT_TRUE(doc_action.ParseFromString(doc_result->action_protos[0]));
  ASSERT_TRUE(doc_action.has_scroll());
  EXPECT_EQ(doc_action.scroll().direction(),
            optimization_guide::proto::ScrollAction::DOWN);
  EXPECT_TRUE(doc_action.scroll().has_distance());
  EXPECT_FLOAT_EQ(doc_action.scroll().distance(), 100000.0f);
  EXPECT_EQ(doc_action.scroll().target().content_node_id(), 42);
  EXPECT_EQ(
      doc_action.scroll().target().document_identifier().serialized_token(),
      "doc-token-1");
}

// Tests that `CreateActuationRequest` for `scroll_page` rejects missing or
// invalid `granularity`, `direction`, and `node_id` arguments.
TEST_F(TtcToolValidatorTest,
       CreateActuationRequestForScrollPageWithInvalidArgumentsFails) {
  std::vector<base::DictValue> invalid_scroll_cases;
  invalid_scroll_cases.push_back(base::DictValue()
                                     .Set("direction", "down")
                                     .Set("node_id", CreateNodeIdDict("", 0)));
  invalid_scroll_cases.push_back(base::DictValue()
                                     .Set("granularity", "page")
                                     .Set("node_id", CreateNodeIdDict("", 0)));
  invalid_scroll_cases.push_back(
      base::DictValue().Set("granularity", "page").Set("direction", "down"));
  invalid_scroll_cases.push_back(base::DictValue()
                                     .Set("granularity", "page")
                                     .Set("direction", "down")
                                     .Set("node_id", base::DictValue()));
  invalid_scroll_cases.push_back(
      base::DictValue()
          .Set("granularity", "page")
          .Set("direction", "down")
          .Set("node_id", CreateNodeIdDict("doc", -1)));
  invalid_scroll_cases.push_back(base::DictValue()
                                     .Set("granularity", "page")
                                     .Set("direction", "down")
                                     .Set("node_id", CreateNodeIdDict("", 42)));
  invalid_scroll_cases.push_back(base::DictValue()
                                     .Set("granularity", "line")
                                     .Set("direction", "down")
                                     .Set("node_id", CreateNodeIdDict("", 0)));
  invalid_scroll_cases.push_back(base::DictValue()
                                     .Set("granularity", "page")
                                     .Set("direction", "diagonal")
                                     .Set("node_id", CreateNodeIdDict("", 0)));

  for (size_t i = 0; i < invalid_scroll_cases.size(); ++i) {
    SCOPED_TRACE(i);
    base::expected<TtcActuationRequest, std::string> invalid_result =
        TtcToolValidator::CreateActuationRequest(ttc::ToolRequest{
            ttc::kToolScrollPage, std::move(invalid_scroll_cases[i])});

    ASSERT_FALSE(invalid_result.has_value());
    EXPECT_EQ(invalid_result.error(), ttc::kErrorMessageInvalidArguments);
  }
}

}  // namespace
