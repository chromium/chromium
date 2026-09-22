// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_definitions.h"

#import <Foundation/Foundation.h>

#import <string_view>

#import "base/strings/sys_string_conversions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

using TTCToolDefinitionsTest = PlatformTest;

// Tests that GetDefaultToolDeclarations returns the expected set of
// navigation tools with valid OpenAPI schemas.
TEST_F(TTCToolDefinitionsTest, TestGetDefaultToolDeclarations) {
  @autoreleasepool {
    NSArray<NSDictionary*>* declarations = ttc::GetDefaultToolDeclarations();
    ASSERT_NE(declarations, nil);
    EXPECT_EQ([declarations count], 3u);

    NSMutableSet<NSString*>* tool_names = [NSMutableSet set];
    for (NSDictionary* decl in declarations) {
      ASSERT_TRUE([decl isKindOfClass:[NSDictionary class]]);
      NSString* name = decl[@"name"];
      ASSERT_GT(name.length, 0u);
      [tool_names addObject:name];

      NSString* description = decl[@"description"];
      EXPECT_GT(description.length, 0u);

      if ([name isEqualToString:base::SysUTF8ToNSString(ttc::kToolOpenUrl)]) {
        NSDictionary* parameters = decl[@"parameters"];
        ASSERT_NE(parameters, nil);
        EXPECT_NSEQ(parameters[@"type"], @"OBJECT");
        ASSERT_NE(parameters[@"properties"], nil);
        EXPECT_NSEQ(parameters[@"required"], (@[ @"url", @"new_tab" ]));
      } else {
        EXPECT_EQ(decl[@"parameters"], nil);
      }
    }

    NSSet<NSString*>* expected_tool_names = [NSSet setWithArray:@[
      base::SysUTF8ToNSString(ttc::kToolGoBack),
      base::SysUTF8ToNSString(ttc::kToolGoForward),
      base::SysUTF8ToNSString(ttc::kToolOpenUrl),
    ]];
    EXPECT_NSEQ(tool_names, expected_tool_names);
  }
}

// Tests that GetToolDeclarationByName retrieves the correct declaration.
TEST_F(TTCToolDefinitionsTest, TestGetToolDeclarationByName) {
  @autoreleasepool {
    NSDictionary* open_url_decl = ttc::GetToolDeclarationByName(
        base::SysUTF8ToNSString(ttc::kToolOpenUrl));
    ASSERT_NE(open_url_decl, nil);
    EXPECT_NSEQ(open_url_decl[@"name"],
                base::SysUTF8ToNSString(ttc::kToolOpenUrl));
    EXPECT_NSEQ(open_url_decl[@"description"], @"Opens a URL in the browser.");

    NSDictionary* props = open_url_decl[@"parameters"][@"properties"];
    ASSERT_NE(props[@"url"], nil);
    EXPECT_NSEQ(props[@"url"][@"type"], @"STRING");
    EXPECT_NSEQ(props[@"url"][@"description"],
                @"The complete URL to open (e.g. \"https://example.com\").");

    ASSERT_NE(props[@"new_tab"], nil);
    EXPECT_NSEQ(props[@"new_tab"][@"type"], @"BOOLEAN");
    EXPECT_NSEQ(props[@"new_tab"][@"description"],
                @"If true, opens the URL in a new tab; otherwise, navigates "
                @"the current tab.");

    NSArray* required = open_url_decl[@"parameters"][@"required"];
    ASSERT_TRUE([required isKindOfClass:[NSArray class]]);
    EXPECT_NSEQ(required, (@[ @"url", @"new_tab" ]));

    NSDictionary* back_decl = ttc::GetToolDeclarationByName(
        base::SysUTF8ToNSString(ttc::kToolGoBack));
    ASSERT_NE(back_decl, nil);
    EXPECT_NSEQ(back_decl[@"name"], base::SysUTF8ToNSString(ttc::kToolGoBack));
    EXPECT_NSEQ(back_decl[@"description"],
                @"Go back to the previous page in history.");

    NSDictionary* forward_decl = ttc::GetToolDeclarationByName(
        base::SysUTF8ToNSString(ttc::kToolGoForward));
    ASSERT_NE(forward_decl, nil);
    EXPECT_NSEQ(forward_decl[@"name"],
                base::SysUTF8ToNSString(ttc::kToolGoForward));
    EXPECT_NSEQ(forward_decl[@"description"],
                @"Go forward to the next page in history.");

    // Non-existent and empty tool names return nil.
    EXPECT_EQ(ttc::GetToolDeclarationByName(@"reload_page"), nil);
    EXPECT_EQ(ttc::GetToolDeclarationByName(@"non_existent_tool"), nil);
    EXPECT_EQ(ttc::GetToolDeclarationByName(@""), nil);
    EXPECT_EQ(ttc::GetToolDeclarationByName(nil), nil);
  }
}

// Tests that CreateToolResponsePayload produces valid toolResponse JSON.
TEST_F(TTCToolDefinitionsTest, TestCreateToolResponsePayload) {
  @autoreleasepool {
    NSString* call_id = @"call_42";
    NSString* tool_name = base::SysUTF8ToNSString(ttc::kToolOpenUrl);
    NSDictionary* response =
        @{@"status" : @"success", @"url" : @"https://chromium.org"};

    NSData* payload =
        ttc::CreateToolResponsePayload(call_id, tool_name, response);
    ASSERT_NE(payload, nil);

    NSError* error = nil;
    NSDictionary* dict = [NSJSONSerialization JSONObjectWithData:payload
                                                         options:0
                                                           error:&error];
    ASSERT_NSEQ(error, nil);
    ASSERT_TRUE([dict isKindOfClass:[NSDictionary class]]);

    NSDictionary* tool_response = dict[@"toolResponse"];
    ASSERT_NE(tool_response, nil);

    NSArray* function_responses = tool_response[@"functionResponses"];
    ASSERT_NE(function_responses, nil);
    ASSERT_EQ([function_responses count], 1u);

    NSDictionary* function_response = function_responses[0];
    EXPECT_NSEQ(function_response[@"id"], call_id);
    EXPECT_NSEQ(function_response[@"name"], tool_name);

    NSDictionary* output = function_response[@"response"][@"output"];
    ASSERT_NE(output, nil);
    EXPECT_NSEQ(output[@"status"], @"success");
    EXPECT_NSEQ(output[@"url"], @"https://chromium.org");

    // Nil or empty call ID or tool name should yield nil.
    EXPECT_EQ(ttc::CreateToolResponsePayload(nil, tool_name, response), nil);
    EXPECT_EQ(ttc::CreateToolResponsePayload(@"", tool_name, response), nil);
    EXPECT_EQ(ttc::CreateToolResponsePayload(call_id, nil, response), nil);
    EXPECT_EQ(ttc::CreateToolResponsePayload(call_id, @"", response), nil);
  }
}

// Tests edge cases and exception safety in CreateToolResponsePayload.
TEST_F(TTCToolDefinitionsTest, TestCreateToolResponsePayloadEdgeCases) {
  @autoreleasepool {
    NSString* call_id = @"call_99";
    NSString* tool_name = base::SysUTF8ToNSString(ttc::kToolOpenUrl);

    // 1. Nil response dict should produce empty output dictionary.
    NSData* nil_payload =
        ttc::CreateToolResponsePayload(call_id, tool_name, nil);
    ASSERT_NE(nil_payload, nil);
    NSDictionary* nil_dict = [NSJSONSerialization JSONObjectWithData:nil_payload
                                                             options:0
                                                               error:nil];
    EXPECT_NSEQ(nil_dict[@"toolResponse"][@"functionResponses"][0][@"response"]
                        [@"output"],
                @{});

    // 2. Pre-wrapped response envelope with a single 'output' key.
    NSDictionary* wrapped = @{@"output" : @{@"result" : @"done"}};
    NSData* wrapped_payload =
        ttc::CreateToolResponsePayload(call_id, tool_name, wrapped);
    ASSERT_NE(wrapped_payload, nil);
    NSDictionary* wrapped_dict =
        [NSJSONSerialization JSONObjectWithData:wrapped_payload
                                        options:0
                                          error:nil];
    EXPECT_NSEQ(wrapped_dict[@"toolResponse"][@"functionResponses"][0]
                            [@"response"][@"output"][@"result"],
                @"done");

    // 3. Un-serializable object should fail gracefully without throwing
    // exception.
    NSDictionary* invalid_dict = @{@"unsupported" : [NSDate date]};
    EXPECT_EQ(ttc::CreateToolResponsePayload(call_id, tool_name, invalid_dict),
              nil);

    // 4. Non-dictionary response should fail gracefully without crashing.
    NSDictionary* non_dict = (NSDictionary*)@"not_a_dictionary";
    EXPECT_EQ(ttc::CreateToolResponsePayload(call_id, tool_name, non_dict),
              nil);
  }
}

}  // namespace
