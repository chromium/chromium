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

// Tool argument keys.
constexpr char kNewTabKey[] = "new_tab";
constexpr char kUrlKey[] = "url";

// Task update string.
constexpr char kSingleToolTaskUpdate[] = "Executing TTC tool";

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
  if (name == ttc::kToolOpenUrl) {
    const std::string* url_string = arguments.FindString(kUrlKey);
    GURL gurl(url_string ? *url_string : std::string());
    action.mutable_navigate()->set_url(gurl.spec());
  } else if (name == ttc::kToolGoBack) {
    action.mutable_back();
  } else if (name == ttc::kToolGoForward) {
    action.mutable_forward();
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
base::expected<void, std::string> TtcToolValidator::ValidateToolCall(
    const ttc::ToolRequest& tool_request) {
  const std::string& name = tool_request.name;
  const base::DictValue& arguments = tool_request.arguments;
  if (name.empty()) {
    return base::unexpected(ttc::kErrorMessageInvalidToolCall);
  }

  if (name == ttc::kToolGoBack || name == ttc::kToolGoForward) {
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
    if (new_tab_value) {
      std::optional<bool> parsed_bool = ParseBooleanValue(*new_tab_value);
      if (!parsed_bool.has_value() || *parsed_bool) {
        return base::unexpected(ttc::kErrorMessageNewTabUnsupported);
      }
    }

    return base::ok();
  }

  return base::unexpected(ttc::kErrorMessageUnknownTool);
}

// static
base::expected<TtcActuationRequest, std::string>
TtcToolValidator::CreateActuationRequest(const ttc::ToolRequest& tool_request) {
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
