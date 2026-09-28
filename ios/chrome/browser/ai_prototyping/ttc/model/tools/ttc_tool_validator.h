// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_TOOL_VALIDATOR_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_TOOL_VALIDATOR_H_

#import <Foundation/Foundation.h>

#import <string_view>

#import "base/types/expected.h"

@class TTCActuationRequest;

namespace ttc {

// Canonical error messages for tool validation, sorted alphabetically.
inline constexpr std::string_view kErrorMessageEmptyToolCalls =
    "Tool calls array cannot be empty.";
inline constexpr std::string_view kErrorMessageInvalidArguments =
    "Invalid tool arguments.";
inline constexpr std::string_view kErrorMessageInvalidToolCall =
    "Invalid tool call.";
inline constexpr std::string_view kErrorMessageInvalidUrl =
    "Invalid URL provided in arguments.";
inline constexpr std::string_view kErrorMessageNewTabUnsupported =
    "Opening in a new tab is not supported.";
inline constexpr std::string_view kErrorMessageUnknownTool =
    "Unknown tool name.";

}  // namespace ttc

// Validates incoming TalkToChrome function calls and acts as an adapter
// constructing `TTCActuationRequest` payloads for `TTCActuationHandler`.
// Note: Generated `Action` protobufs intentionally do not include `tab_id` or
// `window_id`; these runtime session identifiers are injected downstream by
// `TTCActuationHandler` using active `WebState` tracking.
@interface TTCToolValidator : NSObject

- (instancetype)init NS_UNAVAILABLE;

// Validates the tool name and arguments syntactically.
// @param name The tool name (e.g., `open_url`, `go_back`).
// @param arguments The tool arguments dictionary.
// @param callID The correlation identifier from the client.
// @return Void on success, or an error message describing the violation.
+ (base::expected<void, NSString*>)validateToolName:(NSString*)name
                                          arguments:(NSDictionary*)arguments
                                             callID:(NSString*)callID;

// Validates a tool call and creates an actuation request containing the
// serialized `optimization_guide::proto::Action` proto.
// @param name The tool name.
// @param arguments The tool arguments dictionary.
// @param callID The correlation identifier.
// @return A populated `TTCActuationRequest` on success, or an error string.
+ (base::expected<TTCActuationRequest*, NSString*>)
    createActuationRequestWithToolName:(NSString*)name
                             arguments:(NSDictionary*)arguments
                                callID:(NSString*)callID;

// Validates a batch of tool call dictionaries and creates an actuation request
// containing all serialized `optimization_guide::proto::Action` protos.
// Each dictionary must contain `@"name"`, `@"id"`, and optional `@"args"`.
// @param toolCalls Array of tool call dictionaries.
// @return A populated `TTCActuationRequest` on success, or an error string.
+ (base::expected<TTCActuationRequest*, NSString*>)
    createActuationRequestWithToolCalls:(NSArray<NSDictionary*>*)toolCalls;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_TOOL_VALIDATOR_H_
