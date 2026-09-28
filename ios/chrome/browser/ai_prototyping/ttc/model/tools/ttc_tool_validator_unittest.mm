// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_validator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/optimization_guide/proto/features/actions_data.pb.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_actuation_data_types.h"
#import "ios/chrome/browser/ai_prototyping/ttc/model/tools/ttc_tool_definitions.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

namespace {

NSString* ToNSString(std::string_view str) {
  return base::SysUTF8ToNSString(str);
}

using TTCToolValidatorTest = PlatformTest;

// Tests syntactic validation for valid tool calls.
TEST_F(TTCToolValidatorTest, TestValidToolCalls) {
  @autoreleasepool {
    auto result =
        [TTCToolValidator validateToolName:ToNSString(ttc::kToolOpenUrl)
                                 arguments:@{@"url" : @"https://example.com"}
                                    callID:@"call_1"];
    EXPECT_TRUE(result.has_value());

    result = [TTCToolValidator validateToolName:ToNSString(ttc::kToolGoBack)
                                      arguments:@{}
                                         callID:@"call_2"];
    EXPECT_TRUE(result.has_value());

    result = [TTCToolValidator validateToolName:ToNSString(ttc::kToolGoForward)
                                      arguments:nil
                                         callID:@"call_3"];
    EXPECT_TRUE(result.has_value());
  }
}

// Tests defensive rejection against invalid types and empty identifiers.
TEST_F(TTCToolValidatorTest, TestDefensiveTypeRejections) {
  @autoreleasepool {
    // Non-string or empty tool name.
    auto result = [TTCToolValidator validateToolName:(id) @(123)
                                           arguments:@{}
                                              callID:@"call_1"];
    EXPECT_FALSE(result.has_value());
    EXPECT_NSEQ(result.error(), ToNSString(ttc::kErrorMessageInvalidToolCall));

    result = [TTCToolValidator validateToolName:@""
                                      arguments:@{}
                                         callID:@"call_1"];
    EXPECT_FALSE(result.has_value());
    EXPECT_NSEQ(result.error(), ToNSString(ttc::kErrorMessageInvalidToolCall));

    // Non-string or empty call ID.
    result = [TTCToolValidator validateToolName:ToNSString(ttc::kToolGoBack)
                                      arguments:@{}
                                         callID:(id) @(456)];
    EXPECT_FALSE(result.has_value());
    EXPECT_NSEQ(result.error(), ToNSString(ttc::kErrorMessageInvalidToolCall));

    result = [TTCToolValidator validateToolName:ToNSString(ttc::kToolGoBack)
                                      arguments:@{}
                                         callID:@""];
    EXPECT_FALSE(result.has_value());
    EXPECT_NSEQ(result.error(), ToNSString(ttc::kErrorMessageInvalidToolCall));

    // Non-dictionary arguments.
    result = [TTCToolValidator validateToolName:ToNSString(ttc::kToolGoBack)
                                      arguments:(id) @"not_a_dictionary"
                                         callID:@"call_1"];
    EXPECT_FALSE(result.has_value());
    EXPECT_NSEQ(result.error(), ToNSString(ttc::kErrorMessageInvalidArguments));
  }
}

// Tests URL validation edge cases and disallowed schemes.
TEST_F(TTCToolValidatorTest, TestUrlValidation) {
  @autoreleasepool {
    // Missing URL.
    auto result =
        [TTCToolValidator validateToolName:ToNSString(ttc::kToolOpenUrl)
                                 arguments:@{}
                                    callID:@"call_1"];
    EXPECT_FALSE(result.has_value());
    EXPECT_NSEQ(result.error(), ToNSString(ttc::kErrorMessageInvalidArguments));

    // Invalid URL.
    result =
        [TTCToolValidator validateToolName:ToNSString(ttc::kToolOpenUrl)
                                 arguments:@{@"url" : @"not a valid url ://"}
                                    callID:@"call_2"];
    EXPECT_FALSE(result.has_value());
    EXPECT_NSEQ(result.error(), ToNSString(ttc::kErrorMessageInvalidUrl));

    // Disallowed schemes.
    NSArray<NSString*>* disallowedUrls = @[
      @"javascript:alert(1)", @"chrome://flags", @"file:///etc/passwd",
      @"data:text/html,<h1>hi</h1>"
    ];
    for (NSString* url in disallowedUrls) {
      result = [TTCToolValidator validateToolName:ToNSString(ttc::kToolOpenUrl)
                                        arguments:@{@"url" : url}
                                           callID:@"call_test"];
      EXPECT_FALSE(result.has_value());
      EXPECT_NSEQ(result.error(), ToNSString(ttc::kErrorMessageInvalidUrl));
    }

    // Disallow opening in new tab with boolean values.
    result = [TTCToolValidator validateToolName:ToNSString(ttc::kToolOpenUrl)
                                      arguments:@{
                                        @"url" : @"https://example.com",
                                        @"new_tab" : @YES
                                      }
                                         callID:@"call_new_tab"];
    EXPECT_FALSE(result.has_value());
    EXPECT_NSEQ(result.error(),
                ToNSString(ttc::kErrorMessageNewTabUnsupported));

    // String "true" or "1" should also be rejected for new_tab.
    result = [TTCToolValidator validateToolName:ToNSString(ttc::kToolOpenUrl)
                                      arguments:@{
                                        @"url" : @"https://example.com",
                                        @"new_tab" : @"true"
                                      }
                                         callID:@"call_new_tab_str"];
    EXPECT_FALSE(result.has_value());
    EXPECT_NSEQ(result.error(),
                ToNSString(ttc::kErrorMessageNewTabUnsupported));

    // Arbitrary non-boolean string like "today" is invalid and rejected.
    result = [TTCToolValidator validateToolName:ToNSString(ttc::kToolOpenUrl)
                                      arguments:@{
                                        @"url" : @"https://example.com",
                                        @"new_tab" : @"today"
                                      }
                                         callID:@"call_new_tab_invalid"];
    EXPECT_FALSE(result.has_value());
    EXPECT_NSEQ(result.error(),
                ToNSString(ttc::kErrorMessageNewTabUnsupported));

    // False boolean values should succeed.
    result = [TTCToolValidator validateToolName:ToNSString(ttc::kToolOpenUrl)
                                      arguments:@{
                                        @"url" : @"https://example.com",
                                        @"new_tab" : @NO
                                      }
                                         callID:@"call_new_tab_false"];
    EXPECT_TRUE(result.has_value());

    result = [TTCToolValidator validateToolName:ToNSString(ttc::kToolOpenUrl)
                                      arguments:@{
                                        @"url" : @"https://example.com",
                                        @"new_tab" : @"false"
                                      }
                                         callID:@"call_new_tab_false_str"];
    EXPECT_TRUE(result.has_value());
  }
}

// Tests creation of TTCActuationRequest from single tool call.
TEST_F(TTCToolValidatorTest, TestCreateActuationRequestSingle) {
  @autoreleasepool {
    auto requestResult = [TTCToolValidator
        createActuationRequestWithToolName:ToNSString(ttc::kToolOpenUrl)
                                 arguments:@{@"url" : @"https://example.com"}
                                    callID:@"call_single"];
    ASSERT_TRUE(requestResult.has_value());
    TTCActuationRequest* request = requestResult.value();
    EXPECT_NSEQ(request.callID, @"call_single");
    EXPECT_EQ(request.actionProtos.count, 1u);

    optimization_guide::proto::Action action;
    NSData* data = request.actionProtos[0];
    ASSERT_TRUE(action.ParseFromArray([data bytes], [data length]));
    EXPECT_TRUE(action.has_navigate());
    EXPECT_EQ(action.navigate().url(), "https://example.com/");

    // Back tool request.
    auto backResult = [TTCToolValidator
        createActuationRequestWithToolName:ToNSString(ttc::kToolGoBack)
                                 arguments:@{}
                                    callID:@"call_back"];
    ASSERT_TRUE(backResult.has_value());
    TTCActuationRequest* backRequest = backResult.value();
    EXPECT_EQ(backRequest.actionProtos.count, 1u);
    optimization_guide::proto::Action backAction;
    ASSERT_TRUE(
        backAction.ParseFromArray([backRequest.actionProtos[0] bytes],
                                  [backRequest.actionProtos[0] length]));
    EXPECT_TRUE(backAction.has_back());

    // Forward tool request.
    auto forwardResult = [TTCToolValidator
        createActuationRequestWithToolName:ToNSString(ttc::kToolGoForward)
                                 arguments:@{}
                                    callID:@"call_forward"];
    ASSERT_TRUE(forwardResult.has_value());
    TTCActuationRequest* forwardRequest = forwardResult.value();
    EXPECT_EQ(forwardRequest.actionProtos.count, 1u);
    optimization_guide::proto::Action forwardAction;
    ASSERT_TRUE(
        forwardAction.ParseFromArray([forwardRequest.actionProtos[0] bytes],
                                     [forwardRequest.actionProtos[0] length]));
    EXPECT_TRUE(forwardAction.has_forward());
  }
}

// Tests creation of TTCActuationRequest from batch of tool calls.
TEST_F(TTCToolValidatorTest, TestCreateActuationRequestBatch) {
  @autoreleasepool {
    // Empty tool calls rejected.
    auto emptyResult =
        [TTCToolValidator createActuationRequestWithToolCalls:@[]];
    EXPECT_FALSE(emptyResult.has_value());
    EXPECT_NSEQ(emptyResult.error(),
                ToNSString(ttc::kErrorMessageEmptyToolCalls));

    // Invalid array element rejected.
    auto invalidItemResult = [TTCToolValidator
        createActuationRequestWithToolCalls:(id) @[ @"not_a_dictionary" ]];
    EXPECT_FALSE(invalidItemResult.has_value());
    EXPECT_NSEQ(invalidItemResult.error(),
                ToNSString(ttc::kErrorMessageInvalidToolCall));

    NSArray<NSDictionary*>* toolCalls = @[
      @{
        @"name" : ToNSString(ttc::kToolOpenUrl),
        @"id" : @"call_batch_1",
        @"args" : @{@"url" : @"https://google.com"}
      },
      @{
        @"name" : ToNSString(ttc::kToolGoBack),
        @"id" : @"call_batch_2",
        @"args" : @{}
      }
    ];

    auto requestResult =
        [TTCToolValidator createActuationRequestWithToolCalls:toolCalls];
    ASSERT_TRUE(requestResult.has_value());
    TTCActuationRequest* request = requestResult.value();
    EXPECT_NSEQ(request.callID, @"call_batch_1");
    EXPECT_EQ(request.callIDs.count, 2u);
    EXPECT_NSEQ(request.callIDs[0], @"call_batch_1");
    EXPECT_NSEQ(request.callIDs[1], @"call_batch_2");
    EXPECT_EQ(request.actionProtos.count, 2u);

    optimization_guide::proto::Action action1;
    ASSERT_TRUE(action1.ParseFromArray([request.actionProtos[0] bytes],
                                       [request.actionProtos[0] length]));
    EXPECT_TRUE(action1.has_navigate());
    EXPECT_EQ(action1.navigate().url(), "https://google.com/");

    optimization_guide::proto::Action action2;
    ASSERT_TRUE(action2.ParseFromArray([request.actionProtos[1] bytes],
                                       [request.actionProtos[1] length]));
    EXPECT_TRUE(action2.has_back());
  }
}

// Tests TTCActuationResponse initialization and property access.
TEST_F(TTCToolValidatorTest, TestTTCActuationResponse) {
  @autoreleasepool {
    NSData* dummyResult =
        [@"actions_result_data" dataUsingEncoding:NSUTF8StringEncoding];
    TTCActuationResponse* response = [[TTCActuationResponse alloc]
             initWithResultCode:actor::mojom::ActionResultCode::kOk
                   errorMessage:nil
        serializedActionsResult:dummyResult
                         callID:@"call_123"];

    EXPECT_EQ(response.resultCode, actor::mojom::ActionResultCode::kOk);
    EXPECT_NSEQ(response.errorMessage, nil);
    EXPECT_NSEQ(response.serializedActionsResult, dummyResult);
    EXPECT_NSEQ(response.callID, @"call_123");

    // Convenience error response without serialized result proto.
    TTCActuationResponse* errorResponse = [[TTCActuationResponse alloc]
        initWithResultCode:actor::mojom::ActionResultCode::kArgumentsInvalid
              errorMessage:@"Something went wrong"
                    callID:@"call_error"];

    EXPECT_EQ(errorResponse.resultCode,
              actor::mojom::ActionResultCode::kArgumentsInvalid);
    EXPECT_NSEQ(errorResponse.errorMessage, @"Something went wrong");
    EXPECT_NSEQ(errorResponse.serializedActionsResult, nil);
    EXPECT_NSEQ(errorResponse.callID, @"call_error");
  }
}

// Tests TTCActuationRequest batch call IDs accessor.
TEST_F(TTCToolValidatorTest, TestTTCActuationRequestBatchCallIDs) {
  @autoreleasepool {
    NSArray<NSString*>* callIDs = @[ @"call_1", @"call_2", @"call_3" ];
    TTCActuationRequest* request =
        [[TTCActuationRequest alloc] initWithActionProtos:@[]
                                               taskUpdate:@"In progress"
                                                  callIDs:callIDs];

    EXPECT_NSEQ(request.callID, @"call_1");
    EXPECT_NSEQ(request.callIDs, callIDs);
    EXPECT_NSEQ(request.taskUpdate, @"In progress");
  }
}

}  // namespace
