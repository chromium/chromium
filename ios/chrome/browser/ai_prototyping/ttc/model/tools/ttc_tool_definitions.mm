// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_definitions.h"

#import "base/strings/sys_string_conversions.h"

namespace {

// JSON schema and response envelope keys.
NSString* const kNameKey = @"name";
NSString* const kDescriptionKey = @"description";
NSString* const kParametersKey = @"parameters";
NSString* const kTypeKey = @"type";
NSString* const kPropertiesKey = @"properties";
NSString* const kRequiredKey = @"required";
NSString* const kUrlKey = @"url";
NSString* const kNewTabKey = @"new_tab";
NSString* const kObjectType = @"OBJECT";
NSString* const kStringType = @"STRING";
NSString* const kBooleanType = @"BOOLEAN";
NSString* const kOutputKey = @"output";
NSString* const kIdKey = @"id";
NSString* const kResponseKey = @"response";
NSString* const kToolResponseKey = @"toolResponse";
NSString* const kFunctionResponsesKey = @"functionResponses";

// Note: All tool descriptions below are schema definitions passed to the
// backend for function calling, not user-facing UI strings.
// User-visible UI elements displaying tool activity must use l10n_util.

// Builds the function declaration dictionary for `go_back`.
NSDictionary* BuildGoBackDeclaration() {
  return @{
    kNameKey : base::SysUTF8ToNSString(ttc::kToolGoBack),
    kDescriptionKey : @"Go back to the previous page in history.",
  };
}

// Builds the function declaration dictionary for `go_forward`.
NSDictionary* BuildGoForwardDeclaration() {
  return @{
    kNameKey : base::SysUTF8ToNSString(ttc::kToolGoForward),
    kDescriptionKey : @"Go forward to the next page in history.",
  };
}

// Builds the function declaration dictionary for `open_url`.
NSDictionary* BuildOpenUrlDeclaration() {
  return @{
    kNameKey : base::SysUTF8ToNSString(ttc::kToolOpenUrl),
    kDescriptionKey : @"Opens a URL in the browser.",
    kParametersKey : @{
      kTypeKey : kObjectType,
      kPropertiesKey : @{
        kUrlKey : @{
          kTypeKey : kStringType,
          kDescriptionKey :
              @"The complete URL to open (e.g. \"https://example.com\").",
        },
        kNewTabKey : @{
          kTypeKey : kBooleanType,
          kDescriptionKey :
              @"If true, opens the URL in a new tab; otherwise, navigates the "
              @"current tab.",
        },
      },
      kRequiredKey : @[ kUrlKey, kNewTabKey ],
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
    if ([decl[kNameKey] isEqualToString:tool_name]) {
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

  // The backend expects the function response envelope:
  // { "response": { "output": <result_object> } }
  // Preserve caller-provided "output" if already formatted; otherwise wrap it.
  NSDictionary* response_envelope = nil;
  if (response_dict.count == 1 && response_dict[kOutputKey]) {
    response_envelope = response_dict;
  } else {
    response_envelope = @{kOutputKey : response_dict ?: @{}};
  }

  NSDictionary* function_response = @{
    kIdKey : call_id,
    kNameKey : tool_name,
    kResponseKey : response_envelope,
  };

  NSDictionary* tool_response_payload = @{
    kToolResponseKey : @{
      kFunctionResponsesKey : @[ function_response ],
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
