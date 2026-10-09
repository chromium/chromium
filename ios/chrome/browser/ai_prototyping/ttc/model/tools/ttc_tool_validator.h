// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_TOOL_VALIDATOR_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_TOOL_VALIDATOR_H_

#import <Foundation/Foundation.h>

#import <string>

#import "base/types/expected.h"
#import "base/values.h"

@class TTCActuationRequest;

namespace ttc {

// Canonical error messages for tool validation, sorted alphabetically.
inline constexpr char kErrorMessageEmptyToolCalls[] =
    "Tool calls array cannot be empty.";
inline constexpr char kErrorMessageInvalidArguments[] =
    "Invalid tool arguments.";
inline constexpr char kErrorMessageInvalidToolCall[] = "Invalid tool call.";
inline constexpr char kErrorMessageInvalidUrl[] =
    "Invalid URL provided in arguments.";
inline constexpr char kErrorMessageNewTabUnsupported[] =
    "Opening in a new tab is not supported.";
inline constexpr char kErrorMessageUnknownTool[] = "Unknown tool name.";

}  // namespace ttc

// Validates incoming TTC tool calls and acts as an adapter constructing
// `TTCActuationRequest` payloads for `TTCActuationHandler`.
// Note: Generated `Action` protobufs intentionally do not include `tab_id` or
// `window_id`; these runtime session identifiers are injected downstream by
// `TTCActuationHandler` using active `WebState` tracking.
class TtcToolValidator {
 public:
  TtcToolValidator() = delete;

  // Validates the tool name, arguments, and correlation `call_id`
  // syntactically. Returns `base::ok()` on success, or an error message
  // describing the violation.
  static base::expected<void, std::string> ValidateToolCall(
      const std::string& name,
      const base::DictValue& arguments,
      const std::string& call_id);

  // Validates a tool call and creates an actuation request containing the
  // serialized `optimization_guide::proto::Action` proto.
  static base::expected<TTCActuationRequest*, std::string>
  CreateActuationRequest(const std::string& name,
                         const base::DictValue& arguments,
                         const std::string& call_id);

  // Validates a batch of tool call dictionaries and creates an actuation
  // request containing all serialized `optimization_guide::proto::Action`
  // protos. Each dictionary in `tool_calls` must contain `"name"`, `"id"`, and
  // optional `"args"`.
  static base::expected<TTCActuationRequest*, std::string>
  CreateActuationRequestWithToolCalls(const base::ListValue& tool_calls);
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_TOOL_VALIDATOR_H_
