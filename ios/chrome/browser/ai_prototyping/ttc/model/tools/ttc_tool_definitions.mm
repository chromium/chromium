// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_definitions.h"

#import "base/strings/sys_string_conversions.h"

namespace {

// Note: All tool descriptions below are model-facing system instructions
// passed to Gemini Live for function calling, not user-facing UI strings.
// User-visible UI elements displaying tool activity must use l10n_util.

// Builds the function declaration dictionary for `go_back`.
NSDictionary* BuildGoBackDeclaration() {
  return @{
    @"name" : base::SysUTF8ToNSString(ttc::kToolGoBack),
    @"description" : @"Go back to the previous page in history.",
  };
}

// Builds the function declaration dictionary for `go_forward`.
NSDictionary* BuildGoForwardDeclaration() {
  return @{
    @"name" : base::SysUTF8ToNSString(ttc::kToolGoForward),
    @"description" : @"Go forward to the next page in history.",
  };
}

// Builds the function declaration dictionary for `open_url`.
NSDictionary* BuildOpenUrlDeclaration() {
  return @{
    @"name" : base::SysUTF8ToNSString(ttc::kToolOpenUrl),
    @"description" : @"Opens a URL in the browser.",
    @"parameters" : @{
      @"type" : @"OBJECT",
      @"properties" : @{
        @"url" : @{
          @"type" : @"STRING",
          @"description" :
              @"The complete URL to open (e.g. \"https://example.com\").",
        },
        @"new_tab" : @{
          @"type" : @"BOOLEAN",
          @"description" :
              @"If true, opens the URL in a new tab; otherwise, navigates the "
              @"current tab.",
        },
      },
      @"required" : @[ @"url", @"new_tab" ],
    },
  };
}

}  // namespace

namespace ttc {

#pragma mark - Tool Declarations

NSArray<NSDictionary*>* GetDefaultToolDeclarations() {
  return @[
    BuildGoBackDeclaration(),
    BuildGoForwardDeclaration(),
    BuildOpenUrlDeclaration(),
  ];
}

NSDictionary* GetToolDeclarationByName(NSString* tool_name) {
  if (tool_name.length == 0) {
    return nil;
  }
  for (NSDictionary* decl in GetDefaultToolDeclarations()) {
    if ([decl[@"name"] isEqualToString:tool_name]) {
      return decl;
    }
  }
  return nil;
}

#pragma mark - Tool Response Serialization

NSData* CreateToolResponsePayload(NSString* call_id,
                                  NSString* tool_name,
                                  NSDictionary* response_dict) {
  if (call_id.length == 0 || tool_name.length == 0) {
    return nil;
  }

  if (response_dict && ![response_dict isKindOfClass:[NSDictionary class]]) {
    return nil;
  }

  // Gemini Live expects the function response envelope:
  // { "response": { "output": <result_object> } }
  // Preserve caller-provided "output" if already formatted; otherwise wrap it.
  NSDictionary* response_envelope = nil;
  if (response_dict.count == 1 && response_dict[@"output"]) {
    response_envelope = response_dict;
  } else {
    response_envelope = @{@"output" : response_dict ?: @{}};
  }

  NSDictionary* function_response = @{
    @"id" : call_id,
    @"name" : tool_name,
    @"response" : response_envelope,
  };

  NSDictionary* tool_response_payload = @{
    @"toolResponse" : @{
      @"functionResponses" : @[ function_response ],
    },
  };

  if (![NSJSONSerialization isValidJSONObject:tool_response_payload]) {
    return nil;
  }

  NSError* error = nil;
  NSData* json_data =
      [NSJSONSerialization dataWithJSONObject:tool_response_payload
                                      options:0
                                        error:&error];
  if (error || !json_data) {
    return nil;
  }
  return json_data;
}

}  // namespace ttc
