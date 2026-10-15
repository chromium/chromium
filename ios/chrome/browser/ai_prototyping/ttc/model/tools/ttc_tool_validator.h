// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_TOOL_VALIDATOR_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_TOOL_VALIDATOR_H_

#import <string>

#import "base/types/expected.h"
#import "base/values.h"
#import "components/ttc/app/public/tool_types.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_data_types.h"

namespace ttc {

// Canonical error messages for tool validation, sorted alphabetically.
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
// `TtcActuationRequest` payloads for `TtcActuationHandler`.
// Note: Generated `Action` protobufs intentionally do not include `tab_id` or
// `window_id`; these runtime session identifiers are injected downstream by
// `TtcActuationHandler` using active `WebState` tracking.
class TtcToolValidator {
 public:
  TtcToolValidator() = delete;

  // Validates `tool_request` syntactically. Returns `base::ok()` on success, or
  // an error message describing the violation.
  static base::expected<void, std::string> ValidateToolCall(
      const ttc::ToolRequest& tool_request);

  // Validates `tool_request` and creates an actuation request containing the
  // serialized `optimization_guide::proto::Action` proto.
  static base::expected<TtcActuationRequest, std::string>
  CreateActuationRequest(const ttc::ToolRequest& tool_request);
};

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_TOOL_VALIDATOR_H_
