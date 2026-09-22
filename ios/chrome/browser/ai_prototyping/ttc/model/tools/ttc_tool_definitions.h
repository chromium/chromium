// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_TOOL_DEFINITIONS_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_TOOL_DEFINITIONS_H_

#import <Foundation/Foundation.h>

#import <string_view>

namespace ttc {

// Canonical navigation and browsing tool names matching Desktop Chrome's
// tools.mojom contract. Listed in strict alphabetical order.
inline constexpr std::string_view kToolGoBack = "go_back";
inline constexpr std::string_view kToolGoForward = "go_forward";
inline constexpr std::string_view kToolOpenUrl = "open_url";

// Returns the array of function declaration dictionaries matching Gemini
// Live's FunctionDeclaration OpenAPI schema for the default navigation toolset.
// Descriptions are model-facing prompts and intentionally in English.
//
// @return An array of dictionaries conforming to Gemini's FunctionDeclaration
//         format.
NSArray<NSDictionary*>* GetDefaultToolDeclarations();

// Returns the function declaration for a specific tool by canonical name.
//
// @param tool_name Canonical name of the tool (e.g. `kToolOpenUrl`).
// @return The declaration dictionary matching the tool schema, or nil if
//         `tool_name` is unrecognized or empty.
NSDictionary* GetToolDeclarationByName(NSString* tool_name);

// Serializes a `toolResponse` frame acknowledging a `toolCall` back to Gemini
// Live. Matches the `toolResponse.functionResponses` envelope expected by
// the bidirectional Live API.
//
// @param call_id Unique identifier of the tool call from Gemini Live.
//                Must not be empty.
// @param tool_name Canonical name of the executed tool. Must not be empty.
// @param response_dict Output payload of the tool execution. Nested under
//                      @{"output": response_dict}. May be nil (treated as
//                      empty).
// @return Serialized UTF-8 JSON NSData representing the toolResponse frame,
//         or nil if `call_id` or `tool_name` is empty or if serialization
//         fails.
NSData* CreateToolResponsePayload(NSString* call_id,
                                  NSString* tool_name,
                                  NSDictionary* response_dict);

}  // namespace ttc

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_MODEL_TOOLS_TTC_TOOL_DEFINITIONS_H_
