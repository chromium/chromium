// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_validator.h"

#import <optional>
#import <string>
#import <utility>

#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_data_types.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_definitions.h"
#import "url/gurl.h"

namespace {

// Tool call dictionary and argument keys.
constexpr char kNameKey[] = "name";
constexpr char kIdKey[] = "id";
constexpr char kArgsKey[] = "args";
constexpr char kUrlKey[] = "url";
constexpr char kNewTabKey[] = "new_tab";

// Task update strings.
NSString* const kSingleToolTaskUpdate = @"Executing TTC tool";
NSString* const kBatchToolTaskUpdate = @"Executing TTC tools";

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

// Serializes a protobuf action to `NSData`.
NSData* SerializeActionProto(const optimization_guide::proto::Action& action) {
  std::string serialized;
  action.SerializeToString(&serialized);
  return [NSData dataWithBytes:serialized.data() length:serialized.size()];
}

}  // namespace

// static
base::expected<void, std::string> TtcToolValidator::ValidateToolCall(
    const std::string& name,
    const base::DictValue& arguments,
    const std::string& call_id) {
  if (name.empty() || call_id.empty()) {
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
base::expected<TTCActuationRequest*, std::string>
TtcToolValidator::CreateActuationRequest(const std::string& name,
                                         const base::DictValue& arguments,
                                         const std::string& call_id) {
  base::expected<void, std::string> validation =
      ValidateToolCall(name, arguments, call_id);
  if (!validation.has_value()) {
    return base::unexpected(validation.error());
  }

  NSData* action_data = SerializeActionProto(BuildActionProto(name, arguments));
  TTCActuationRequest* request = [[TTCActuationRequest alloc]
      initWithActionProtos:@[ action_data ]
                taskUpdate:kSingleToolTaskUpdate
                    callID:base::SysUTF8ToNSString(call_id)];
  return request;
}

// static
base::expected<TTCActuationRequest*, std::string>
TtcToolValidator::CreateActuationRequestWithToolCalls(
    const base::ListValue& tool_calls) {
  if (tool_calls.empty()) {
    return base::unexpected(ttc::kErrorMessageEmptyToolCalls);
  }

  NSMutableArray<NSData*>* action_protos = [NSMutableArray array];
  NSMutableArray<NSString*>* call_ids = [NSMutableArray array];
  const base::DictValue empty_args;

  for (const base::Value& item : tool_calls) {
    const base::DictValue* tool_call = item.GetIfDict();
    if (!tool_call) {
      return base::unexpected(ttc::kErrorMessageInvalidToolCall);
    }

    const std::string* name = tool_call->FindString(kNameKey);
    const std::string* call_id = tool_call->FindString(kIdKey);
    if (!name || !call_id) {
      return base::unexpected(ttc::kErrorMessageInvalidToolCall);
    }

    const base::Value* args_value = tool_call->Find(kArgsKey);
    const base::DictValue* args = &empty_args;
    if (args_value) {
      args = args_value->GetIfDict();
      if (!args) {
        return base::unexpected(ttc::kErrorMessageInvalidArguments);
      }
    }

    base::expected<void, std::string> validation =
        ValidateToolCall(*name, *args, *call_id);
    if (!validation.has_value()) {
      return base::unexpected(validation.error());
    }

    [call_ids addObject:base::SysUTF8ToNSString(*call_id)];
    [action_protos
        addObject:SerializeActionProto(BuildActionProto(*name, *args))];
  }

  TTCActuationRequest* request =
      [[TTCActuationRequest alloc] initWithActionProtos:action_protos
                                             taskUpdate:kBatchToolTaskUpdate
                                                callIDs:call_ids];
  return request;
}
