// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_validator.h"

#import <optional>
#import <string>
#import <utility>
#import <vector>

#import "base/strings/string_util.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_data_types.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_definitions.h"
#import "url/gurl.h"

namespace {

// Tool argument keys and values, sorted alphabetically.
constexpr char kDirectionDown[] = "down";
constexpr char kDirectionKey[] = "direction";
constexpr char kDirectionLeft[] = "left";
constexpr char kDirectionRight[] = "right";
constexpr char kDirectionUp[] = "up";
constexpr char kDocumentIdKey[] = "document_id";
constexpr char kDomNodeIdKey[] = "dom_node_id";
constexpr char kGranularityDocument[] = "document";
constexpr char kGranularityKey[] = "granularity";
constexpr char kGranularityPage[] = "page";
constexpr char kNewTabKey[] = "new_tab";
constexpr char kNodeIdKey[] = "node_id";
constexpr char kTextKey[] = "text";
constexpr char kUrlKey[] = "url";
constexpr char kValueKey[] = "value";

// Large sentinel scroll distance (in CSS pixels) used for `"document"`
// granularity to scroll to the boundary of the document or container.
constexpr float kDocumentScrollDistance = 100000.0f;

// Task update string.
constexpr char kSingleToolTaskUpdate[] = "Executing TTC tool";

// Parses a post-processed `node_id` dictionary into `out_target` (if non-null).
// Returns true if `dict` contains a non-empty `document_id` string and a
// positive integer `dom_node_id`.
bool ParseTargetFromDict(const base::DictValue& dict,
                         optimization_guide::proto::ActionTarget* out_target) {
  const std::string* doc_id = dict.FindString(kDocumentIdKey);
  std::optional<int> dom_node_id = dict.FindInt(kDomNodeIdKey);
  if (!doc_id || doc_id->empty() || !dom_node_id.has_value() ||
      *dom_node_id <= 0) {
    return false;
  }
  if (out_target) {
    out_target->set_content_node_id(*dom_node_id);
    out_target->mutable_document_identifier()->set_serialized_token(*doc_id);
  }
  return true;
}

// Parses the required post-processed `node_id` dictionary in `arguments` into
// `out_target` (if non-null). Returns true if `arguments` contains a `node_id`
// dictionary with a non-empty `document_id` string and a positive
// `dom_node_id`.
bool ParseElementTarget(const base::DictValue& arguments,
                        optimization_guide::proto::ActionTarget* out_target) {
  const base::DictValue* node_id_dict = arguments.FindDict(kNodeIdKey);
  return node_id_dict && ParseTargetFromDict(*node_id_dict, out_target);
}

// Parses the required `node_id` dictionary for `scroll_page`. When
// `dom_node_id == 0` (the root viewport sentinel), leaves `out_target` unset so
// `ScrollTool` targets the root window even when `document_id` is empty. When
// `dom_node_id > 0`, delegates to `ParseTargetFromDict`.
bool ParseScrollTarget(const base::DictValue& arguments,
                       optimization_guide::proto::ActionTarget* out_target) {
  const base::DictValue* node_id_dict = arguments.FindDict(kNodeIdKey);
  if (!node_id_dict) {
    return false;
  }
  std::optional<int> dom_node_id = node_id_dict->FindInt(kDomNodeIdKey);
  if (!dom_node_id.has_value() || *dom_node_id < 0) {
    return false;
  }
  if (*dom_node_id == 0) {
    return true;
  }
  return ParseTargetFromDict(*node_id_dict, out_target);
}

// Parses a scroll direction string into the corresponding `ScrollDirection`
// proto enum value, or `std::nullopt` if unrecognized.
std::optional<optimization_guide::proto::ScrollAction::ScrollDirection>
ParseScrollDirection(const std::string& direction) {
  const std::string lower = base::ToLowerASCII(direction);
  if (lower == kDirectionUp) {
    return optimization_guide::proto::ScrollAction::UP;
  }
  if (lower == kDirectionDown) {
    return optimization_guide::proto::ScrollAction::DOWN;
  }
  if (lower == kDirectionLeft) {
    return optimization_guide::proto::ScrollAction::LEFT;
  }
  if (lower == kDirectionRight) {
    return optimization_guide::proto::ScrollAction::RIGHT;
  }
  return std::nullopt;
}

// Converts a boolean-like `base::Value` into an optional bool.
std::optional<bool> ParseBooleanValue(const base::Value& value) {
  if (value.is_bool()) {
    return value.GetBool();
  }
  if (value.is_int()) {
    if (value.GetInt() == 1) {
      return true;
    }
    if (value.GetInt() == 0) {
      return false;
    }
    return std::nullopt;
  }
  if (value.is_string()) {
    const std::string lower = base::ToLowerASCII(value.GetString());
    if (lower == "true" || lower == "yes" || lower == "1") {
      return true;
    }
    if (lower == "false" || lower == "no" || lower == "0") {
      return false;
    }
  }
  return std::nullopt;
}

// Constructs an `optimization_guide::proto::Action` for a validated tool call.
optimization_guide::proto::Action BuildActionProto(
    const std::string& name,
    const base::DictValue& arguments) {
  optimization_guide::proto::Action action;
  if (name == ttc::kToolClickElement) {
    optimization_guide::proto::ClickAction* click_action =
        action.mutable_click();
    click_action->set_click_type(optimization_guide::proto::ClickAction::LEFT);
    click_action->set_click_count(
        optimization_guide::proto::ClickAction::SINGLE);
    ParseElementTarget(arguments, click_action->mutable_target());
  } else if (name == ttc::kToolCloseCurrentTab) {
    action.mutable_close_tab();
  } else if (name == ttc::kToolCreateTab) {
    action.mutable_create_tab()->set_foreground(true);
  } else if (name == ttc::kToolGoBack) {
    action.mutable_back();
  } else if (name == ttc::kToolGoForward) {
    action.mutable_forward();
  } else if (name == ttc::kToolOpenUrl) {
    const std::string* url_string = arguments.FindString(kUrlKey);
    GURL gurl(url_string ? *url_string : std::string());
    action.mutable_navigate()->set_url(gurl.spec());
  } else if (name == ttc::kToolScrollPage) {
    optimization_guide::proto::ScrollAction* scroll_action =
        action.mutable_scroll();
    if (const std::string* direction = arguments.FindString(kDirectionKey)) {
      if (std::optional<
              optimization_guide::proto::ScrollAction::ScrollDirection>
              parsed_dir = ParseScrollDirection(*direction)) {
        scroll_action->set_direction(*parsed_dir);
      }
    }
    const std::string* granularity = arguments.FindString(kGranularityKey);
    if (granularity &&
        base::ToLowerASCII(*granularity) == kGranularityDocument) {
      scroll_action->set_distance(kDocumentScrollDistance);
    }
    optimization_guide::proto::ActionTarget target;
    if (ParseScrollTarget(arguments, &target) && target.has_content_node_id()) {
      *scroll_action->mutable_target() = std::move(target);
    }
  } else if (name == ttc::kToolSelectOption) {
    optimization_guide::proto::SelectAction* select_action =
        action.mutable_select();
    const std::string* value = arguments.FindString(kValueKey);
    select_action->set_value(value ? *value : std::string());
    ParseElementTarget(arguments, select_action->mutable_target());
  } else if (name == ttc::kToolSetText) {
    optimization_guide::proto::TypeAction* type_action = action.mutable_type();
    // Replaces existing field text rather than appending, and avoids premature
    // form submission.
    type_action->set_mode(
        optimization_guide::proto::TypeAction::DELETE_EXISTING);
    type_action->set_follow_by_enter(false);
    const std::string* text = arguments.FindString(kTextKey);
    type_action->set_text(text ? *text : std::string());
    ParseElementTarget(arguments, type_action->mutable_target());
  }
  return action;
}

// Serializes a protobuf action to `std::string`.
std::string SerializeActionProto(
    const optimization_guide::proto::Action& action) {
  std::string serialized;
  action.SerializeToString(&serialized);
  return serialized;
}

}  // namespace

// static
bool TtcToolValidator::IsOpenUrlInNewTab(const ttc::ToolRequest& tool_request) {
  if (tool_request.name != ttc::kToolOpenUrl) {
    return false;
  }
  const base::Value* new_tab_value = tool_request.arguments.Find(kNewTabKey);
  return new_tab_value && ParseBooleanValue(*new_tab_value).value_or(false);
}

// static
base::expected<void, std::string> TtcToolValidator::ValidateToolCall(
    const ttc::ToolRequest& tool_request) {
  const std::string& name = tool_request.name;
  const base::DictValue& arguments = tool_request.arguments;
  if (name.empty()) {
    return base::unexpected(ttc::kErrorMessageInvalidToolCall);
  }

  if (name == ttc::kToolCloseCurrentTab || name == ttc::kToolCloseSession ||
      name == ttc::kToolCreateTab || name == ttc::kToolGoBack ||
      name == ttc::kToolGoForward) {
    return base::ok();
  }

  if (name == ttc::kToolClickElement) {
    if (!ParseElementTarget(arguments, nullptr)) {
      return base::unexpected(ttc::kErrorMessageInvalidArguments);
    }
    return base::ok();
  }

  if (name == ttc::kToolOpenUrl) {
    const std::string* url_value = arguments.FindString(kUrlKey);
    if (!url_value || url_value->empty()) {
      return base::unexpected(ttc::kErrorMessageInvalidArguments);
    }

    GURL gurl(*url_value);
    if (!gurl.is_valid() || !gurl.SchemeIsHTTPOrHTTPS()) {
      return base::unexpected(ttc::kErrorMessageInvalidUrl);
    }

    const base::Value* new_tab_value = arguments.Find(kNewTabKey);
    if (new_tab_value && !ParseBooleanValue(*new_tab_value).has_value()) {
      return base::unexpected(ttc::kErrorMessageInvalidArguments);
    }

    return base::ok();
  }

  if (name == ttc::kToolScrollPage) {
    const std::string* granularity = arguments.FindString(kGranularityKey);
    const std::string* direction = arguments.FindString(kDirectionKey);
    if (!granularity || !direction || !ParseScrollTarget(arguments, nullptr)) {
      return base::unexpected(ttc::kErrorMessageInvalidArguments);
    }
    const std::string lower_granularity = base::ToLowerASCII(*granularity);
    if (lower_granularity != kGranularityPage &&
        lower_granularity != kGranularityDocument) {
      return base::unexpected(ttc::kErrorMessageInvalidArguments);
    }
    if (!ParseScrollDirection(*direction).has_value()) {
      return base::unexpected(ttc::kErrorMessageInvalidArguments);
    }
    return base::ok();
  }

  if (name == ttc::kToolSelectOption) {
    if (!ParseElementTarget(arguments, nullptr) ||
        !arguments.FindString(kValueKey)) {
      return base::unexpected(ttc::kErrorMessageInvalidArguments);
    }
    return base::ok();
  }

  if (name == ttc::kToolSetText) {
    if (!ParseElementTarget(arguments, nullptr) ||
        !arguments.FindString(kTextKey)) {
      return base::unexpected(ttc::kErrorMessageInvalidArguments);
    }
    return base::ok();
  }

  return base::unexpected(ttc::kErrorMessageUnknownTool);
}

// static
base::expected<TtcActuationRequest, std::string>
TtcToolValidator::CreateActuationRequest(const ttc::ToolRequest& tool_request) {
  // `close_session` and `open_url` with `new_tab=true` are executed directly by
  // `TtcActuationHandler` rather than mapped to an Actor protobuf action.
  if (tool_request.name == ttc::kToolCloseSession ||
      IsOpenUrlInNewTab(tool_request)) {
    return base::unexpected(ttc::kErrorMessageInvalidToolCall);
  }

  base::expected<void, std::string> validation = ValidateToolCall(tool_request);
  if (!validation.has_value()) {
    return base::unexpected(validation.error());
  }

  std::vector<std::string> action_protos = {
      SerializeActionProto(
          BuildActionProto(tool_request.name, tool_request.arguments)),
  };
  return TtcActuationRequest(std::move(action_protos), kSingleToolTaskUpdate);
}
