// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/web_frame_impl.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import <memory>
#import <string_view>
#import <utility>

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/json/json_reader.h"
#import "base/run_loop.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "base/time/time.h"
#import "base/values.h"
#import "components/test/ios/test_utils.h"
#import "ios/web/common/features.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_test.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

using JavaScriptCompletionHandler = void (^)(id, NSError*);

const char kFrameId[] = "1effd8f52a067c8d3a01762d3c41dfd8";

const char kFrameInfoRequestUrl[] = "https://test.com";

constexpr std::string_view kInterestingScriptErrorHistogram =
    "IOS.JavaScript.InterestingScriptError";

// Returns a `WKErrorDomain` JavaScript exception error for testing.
NSError* CreateJavaScriptExceptionError() {
  return [NSError errorWithDomain:WKErrorDomain
                             code:WKErrorJavaScriptExceptionOccurred
                         userInfo:@{
                           @"WKJavaScriptExceptionMessage" :
                               @"ReferenceError: Can't find variable: __gCrWeb",
                         }];
}

// Returns a `WKErrorJavaScriptResultTypeIsUnsupported` error for testing.
NSError* CreateUnsupportedResultTypeError() {
  return [NSError errorWithDomain:WKErrorDomain
                             code:WKErrorJavaScriptResultTypeIsUnsupported
                         userInfo:nil];
}

}  // namespace

namespace web {

class WebFrameImplTest : public web::WebTest {
 protected:
  WebFrameImplTest() {
    mock_ns_url_request_ = OCMClassMock([NSURLRequest class]);
    mock_frame_info_ = OCMClassMock([WKFrameInfo class]);
    mock_web_view_ = OCMClassMock([WKWebView class]);

    OCMStub([mock_web_view_
        evaluateJavaScript:AssignValueToVariable(last_received_script_)
                   inFrame:OCMOCK_ANY
            inContentWorld:AssignValueToVariable(last_received_content_world_)
         completionHandler:CopyValueToVariable(
                               last_received_completion_handler_)]);
    OCMStub([mock_frame_info_ webView]).andReturn(mock_web_view_);
    NSURL* url = [[NSURL alloc]
        initWithString:base::SysUTF8ToNSString(kFrameInfoRequestUrl)];
    OCMStub([mock_ns_url_request_ URL]).andReturn(url);
    OCMStub([mock_frame_info_ request]).andReturn(mock_ns_url_request_);
  }

  void SetUp() override {
    web::WebTest::SetUp();

    JavaScriptFeatureManager* java_script_feature_manager =
        JavaScriptFeatureManager::FromBrowserState(GetBrowserState());
    java_script_feature_manager->ConfigureFeatures({});

    fake_web_state_.SetBrowserState(GetBrowserState());
  }

  void TearDown() override {
    last_received_completion_handler_ = nil;
    web::WebTest::TearDown();
  }

  id mock_frame_info_;
  id mock_web_view_;
  id mock_ns_url_request_;
  NSString* last_received_script_;
  WKContentWorld* last_received_content_world_;
  JavaScriptCompletionHandler last_received_completion_handler_ = nil;

  FakeWebState fake_web_state_;
  url::Origin security_origin_;
};

// Tests creation of a WebFrame for the main frame.
TEST_F(WebFrameImplTest, CreateWebFrameForMainFrame) {
  WebFrameImpl web_frame(mock_frame_info_, kFrameId,
                         /*is_main_frame=*/true, security_origin_,
                         &fake_web_state_, ContentWorld::kPageContentWorld);

  EXPECT_EQ(&fake_web_state_, web_frame.GetWebState());
  EXPECT_TRUE(web_frame.IsMainFrame());
  EXPECT_EQ(security_origin_, web_frame.GetSecurityOrigin());
  EXPECT_EQ(web_frame.GetUrl(), GURL(kFrameInfoRequestUrl));
  EXPECT_EQ(kFrameId, web_frame.GetFrameId());
}

// Tests that the WebFrame properly creates JavaScript for the main frame.
TEST_F(WebFrameImplTest, CallJavaScriptFunctionMainFrame) {
  WebFrameImpl web_frame(mock_frame_info_, kFrameId,
                         /*is_main_frame=*/true, security_origin_,
                         &fake_web_state_, ContentWorld::kPageContentWorld);

  base::ListValue function_params;
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  EXPECT_NSEQ(@"__gCrWeb.callFunctionInGcrWeb(\"\", \"functionName\", [])",
              last_received_script_);
  EXPECT_NSEQ(WKContentWorld.pageWorld, last_received_content_world_);

  function_params.Append("param1");
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  EXPECT_NSEQ(@"__gCrWeb.callFunctionInGcrWeb(\"\", \"functionName\", "
              @"[\"param1\"])",
              last_received_script_);
  EXPECT_NSEQ(WKContentWorld.pageWorld, last_received_content_world_);

  function_params.Append(true);
  function_params.Append(27);
  function_params.Append(3.14);
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  EXPECT_NSEQ(@"__gCrWeb.callFunctionInGcrWeb(\"\", \"functionName\", "
              @"[\"param1\",true,27,3.14])",
              last_received_script_);
  EXPECT_NSEQ(WKContentWorld.pageWorld, last_received_content_world_);
}

// Tests that the WebFrame creates JavaScript for an iframe.
TEST_F(WebFrameImplTest, CallJavaScriptFunctionIFrame) {
  WebFrameImpl web_frame(mock_frame_info_, kFrameId,
                         /*is_main_frame=*/false, security_origin_,
                         &fake_web_state_, ContentWorld::kIsolatedWorld);

  base::ListValue function_params;

  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  EXPECT_NSEQ(@"__gCrWeb.callFunctionInGcrWeb(\"\", \"functionName\", [])",
              last_received_script_);
  EXPECT_NSEQ(WKContentWorld.defaultClientWorld, last_received_content_world_);

  function_params.Append("param1");
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  EXPECT_NSEQ(@"__gCrWeb.callFunctionInGcrWeb(\"\", \"functionName\", "
              @"[\"param1\"])",
              last_received_script_);
  EXPECT_NSEQ(WKContentWorld.defaultClientWorld, last_received_content_world_);

  function_params.Append(true);
  function_params.Append(27);
  function_params.Append(3.14);
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  EXPECT_NSEQ(@"__gCrWeb.callFunctionInGcrWeb(\"\", \"functionName\", "
              @"[\"param1\",true,27,3.14])",
              last_received_script_);
  EXPECT_NSEQ(WKContentWorld.defaultClientWorld, last_received_content_world_);
}

// Tests that the WebFrame properly creates JavaScript for the main frame for
// async calls.
TEST_F(WebFrameImplTest, CallAsyncJavaScriptFunctionMainFrame) {
  __block NSString* received_script = nil;
  __block NSDictionary* received_arguments = nil;
  __block WKContentWorld* received_world = nil;

  OCMStub([mock_web_view_
      callAsyncJavaScript:AssignValueToVariable(received_script)
                arguments:AssignValueToVariable(received_arguments)
                  inFrame:OCMOCK_ANY
           inContentWorld:AssignValueToVariable(received_world)
        completionHandler:OCMOCK_ANY]);

  WebFrameImpl web_frame(mock_frame_info_, kFrameId,
                         /*is_main_frame=*/true, security_origin_,
                         &fake_web_state_, ContentWorld::kPageContentWorld);

  base::DictValue function_params;
  EXPECT_TRUE(web_frame.CallAsyncJavaScriptFunction(
      "api.functionName", function_params,
      base::BindOnce(^(const base::Value* value, NSError* error){
      })));

  EXPECT_NSEQ(@"return __gCrWeb.callFunctionInGcrWeb('api', 'functionName', "
              @"[crw_args]);",
              received_script);
  EXPECT_NSEQ(WKContentWorld.pageWorld, received_world);
  ASSERT_TRUE(received_arguments);
  EXPECT_TRUE(
      [received_arguments[@"crw_args"] isKindOfClass:[NSDictionary class]]);
  EXPECT_EQ(0UL, [received_arguments[@"crw_args"] count]);

  function_params.Set("key", "param1");
  EXPECT_TRUE(web_frame.CallAsyncJavaScriptFunction(
      "api.functionName", function_params,
      base::BindOnce(^(const base::Value* value, NSError* error){
      })));

  EXPECT_NSEQ(@"return __gCrWeb.callFunctionInGcrWeb('api', 'functionName', "
              @"[crw_args]);",
              received_script);
  ASSERT_TRUE(received_arguments);
  EXPECT_TRUE(
      [received_arguments[@"crw_args"] isKindOfClass:[NSDictionary class]]);
  EXPECT_NSEQ(@"param1", received_arguments[@"crw_args"][@"key"]);
}

// Tests that the WebFrame creates JavaScript for an iframe for async calls.
TEST_F(WebFrameImplTest, CallAsyncJavaScriptFunctionIFrame) {
  __block NSString* received_script = nil;
  __block NSDictionary* received_arguments = nil;
  __block WKContentWorld* received_world = nil;

  OCMStub([mock_web_view_
      callAsyncJavaScript:AssignValueToVariable(received_script)
                arguments:AssignValueToVariable(received_arguments)
                  inFrame:OCMOCK_ANY
           inContentWorld:AssignValueToVariable(received_world)
        completionHandler:OCMOCK_ANY]);

  WebFrameImpl web_frame(mock_frame_info_, kFrameId,
                         /*is_main_frame=*/false, security_origin_,
                         &fake_web_state_, ContentWorld::kIsolatedWorld);

  base::DictValue function_params;
  EXPECT_TRUE(web_frame.CallAsyncJavaScriptFunction(
      "api.functionName", function_params,
      base::BindOnce(^(const base::Value* value, NSError* error){
      })));

  EXPECT_NSEQ(@"return __gCrWeb.callFunctionInGcrWeb('api', 'functionName', "
              @"[crw_args]);",
              received_script);
  EXPECT_NSEQ(WKContentWorld.defaultClientWorld, received_world);
  ASSERT_TRUE(received_arguments);
  EXPECT_TRUE(
      [received_arguments[@"crw_args"] isKindOfClass:[NSDictionary class]]);
  EXPECT_EQ(0UL, [received_arguments[@"crw_args"] count]);
}

// Tests that the WebFrame can execute arbitrary JavaScript.
TEST_F(WebFrameImplTest, ExecuteJavaScript) {
  NSString* script = @"__gCrWeb = {};"
                     @"__gCrWeb['fakeFunction'] = function() {"
                     @"  return '10';"
                     @"}";

  WebFrameImpl web_frame(mock_frame_info_, kFrameId,
                         /*is_main_frame=*/true, security_origin_,
                         &fake_web_state_, ContentWorld::kPageContentWorld);

  EXPECT_TRUE(web_frame.ExecuteJavaScript(base::SysNSStringToUTF16(script)));

  WebFrameImpl web_frame2(mock_frame_info_, kFrameId,
                          /*is_main_frame=*/false, security_origin_,
                          &fake_web_state_, ContentWorld::kPageContentWorld);
  EXPECT_TRUE(web_frame2.ExecuteJavaScript(base::SysNSStringToUTF16(script)));
}

// Tests that the WebFrame can execute arbitrary JavaScript given a callback.
TEST_F(WebFrameImplTest, ExecuteJavaScriptWithCallback) {
  NSString* script = @"__gCrWeb = {};"
                     @"__gCrWeb['fakeFunction'] = function() {"
                     @"  return '10';"
                     @"}";

  WebFrameImpl web_frame(mock_frame_info_, kFrameId,
                         /*is_main_frame=*/true, security_origin_,
                         &fake_web_state_, ContentWorld::kPageContentWorld);

  EXPECT_TRUE(
      web_frame.ExecuteJavaScript(base::SysNSStringToUTF16(script),
                                  base::BindOnce(^(const base::Value* value){
                                  })));

  WebFrameImpl web_frame2(mock_frame_info_, kFrameId,
                          /*is_main_frame=*/false, security_origin_,
                          &fake_web_state_, ContentWorld::kPageContentWorld);

  EXPECT_TRUE(
      web_frame2.ExecuteJavaScript(base::SysNSStringToUTF16(script),
                                   base::BindOnce(^(const base::Value* value){
                                   })));
}

// Tests that the WebFrame can execute asynchronous JavaScript.
TEST_F(WebFrameImplTest, ExecuteAsyncJavaScript) {
  __block NSString* received_script = nil;
  __block NSDictionary* received_arguments = nil;
  __block WKContentWorld* received_world = nil;

  OCMStub([mock_web_view_
      callAsyncJavaScript:AssignValueToVariable(received_script)
                arguments:AssignValueToVariable(received_arguments)
                  inFrame:OCMOCK_ANY
           inContentWorld:AssignValueToVariable(received_world)
        completionHandler:OCMOCK_ANY]);

  base::DictValue parameters;
  parameters.Set("value", "10");

  NSString* script = @"return Promise.resolve('10');";

  WebFrameImpl web_frame(mock_frame_info_, kFrameId,
                         /*is_main_frame=*/true, security_origin_,
                         &fake_web_state_, ContentWorld::kPageContentWorld);
  EXPECT_TRUE(web_frame.ExecuteAsyncJavaScriptInContentWorld(
      base::SysNSStringToUTF16(script), parameters,
      JavaScriptFeatureManager::GetContentWorldForBrowserState(
          ContentWorld::kPageContentWorld, GetBrowserState()),
      base::BindOnce(^(const base::Value* value, NSError* error){
      })));

  EXPECT_NSEQ(script, received_script);
  EXPECT_NSEQ(WKContentWorld.pageWorld, received_world);
  ASSERT_TRUE(received_arguments);
  EXPECT_NSEQ(@"10", received_arguments[@"value"]);
}

// Tests that a rejected Promise in JavaScript results in an NSError in the
// callback.
TEST_F(WebFrameImplTest, ExecuteAsyncJavaScriptHandlesRejection) {
  __block bool called = false;
  __block NSError* received_error = nil;

  OCMStub([mock_web_view_ callAsyncJavaScript:OCMOCK_ANY
                                    arguments:OCMOCK_ANY
                                      inFrame:OCMOCK_ANY
                               inContentWorld:OCMOCK_ANY
                            completionHandler:OCMOCK_ANY])
      .andDo(^(NSInvocation* invocation) {
        void (^completionHandler)(id, NSError*);
        [invocation getArgument:&completionHandler atIndex:6];
        NSError* error =
            [NSError errorWithDomain:WKErrorDomain
                                code:WKErrorJavaScriptExceptionOccurred
                            userInfo:@{
                              @"WKJavaScriptExceptionMessage" : @"Async Failure"
                            }];
        completionHandler(nil, error);
      });

  NSString* script = @"return Promise.reject(new Error('Async Failure'));";

  WebFrameImpl web_frame(mock_frame_info_, kFrameId,
                         /*is_main_frame=*/true, security_origin_,
                         &fake_web_state_, ContentWorld::kPageContentWorld);

  base::DictValue empty_params;
  EXPECT_TRUE(web_frame.ExecuteAsyncJavaScriptInContentWorld(
      base::SysNSStringToUTF16(script), empty_params,
      JavaScriptFeatureManager::GetContentWorldForBrowserState(
          ContentWorld::kPageContentWorld, GetBrowserState()),
      base::BindOnce(^(const base::Value* value, NSError* error) {
        called = true;
        received_error = error;
      })));

  EXPECT_TRUE(called);
  EXPECT_TRUE(received_error);
  EXPECT_NSEQ(received_error.domain, WKErrorDomain);
  EXPECT_EQ(received_error.code, WKErrorJavaScriptExceptionOccurred);
  NSString* exception_message =
      received_error.userInfo[@"WKJavaScriptExceptionMessage"];
  EXPECT_TRUE([exception_message containsString:@"Async Failure"]);
}

// Tests that a JavaScript error in a fire-and-forget `CallJavaScriptFunction`
// call is logged when `WebFrame` is still valid.
TEST_F(WebFrameImplTest, CallJavaScriptFunctionLogsErrorForValidWebFrame) {
  base::HistogramTester histogram_tester;
  WebFrameImpl web_frame(mock_frame_info_, kFrameId,
                         /*is_main_frame=*/true, security_origin_,
                         &fake_web_state_, ContentWorld::kPageContentWorld);

  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("api.functionName", base::ListValue()));
  ASSERT_TRUE(last_received_completion_handler_);
  last_received_completion_handler_(nil, CreateJavaScriptExceptionError());

  histogram_tester.ExpectUniqueSample(kInterestingScriptErrorHistogram, true,
                                      1);
}

// Tests that a cached unsupported-result error is logged once a subsequent
// `CallJavaScriptFunction` call succeeds on the same `WebFrame`.
TEST_F(WebFrameImplTest, CallJavaScriptFunctionProcessesCachedErrorsOnSuccess) {
  base::HistogramTester histogram_tester;
  WebFrameImpl web_frame(mock_frame_info_, kFrameId,
                         /*is_main_frame=*/true, security_origin_,
                         &fake_web_state_, ContentWorld::kPageContentWorld);

  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("api.functionName", base::ListValue()));
  ASSERT_TRUE(last_received_completion_handler_);
  last_received_completion_handler_(nil, CreateUnsupportedResultTypeError());
  histogram_tester.ExpectTotalCount(kInterestingScriptErrorHistogram, 0);

  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("api.functionName", base::ListValue()));
  ASSERT_TRUE(last_received_completion_handler_);
  last_received_completion_handler_(@"js_response", nil);

  histogram_tester.ExpectUniqueSample(kInterestingScriptErrorHistogram, true,
                                      1);
}

// Tests that `CallJavaScriptFunction` ignores both errors and cached errors if
// the `WebFrame` is destroyed before JavaScript execution completes.
TEST_F(WebFrameImplTest,
       CallJavaScriptFunctionIgnoresCompletionAfterWebFrameDestroyed) {
  base::HistogramTester histogram_tester;
  auto web_frame = std::make_unique<WebFrameImpl>(
      mock_frame_info_, kFrameId,
      /*is_main_frame=*/true, security_origin_, &fake_web_state_,
      ContentWorld::kPageContentWorld);

  // Cache an unsupported-result error while `web_frame` is still valid.
  EXPECT_TRUE(
      web_frame->CallJavaScriptFunction("api.functionName", base::ListValue()));
  ASSERT_TRUE(last_received_completion_handler_);
  last_received_completion_handler_(nil, CreateUnsupportedResultTypeError());

  // Start two in-flight calls that settle only after `web_frame` is destroyed.
  EXPECT_TRUE(
      web_frame->CallJavaScriptFunction("api.functionName", base::ListValue()));
  JavaScriptCompletionHandler error_completion =
      last_received_completion_handler_;
  ASSERT_TRUE(error_completion);

  EXPECT_TRUE(
      web_frame->CallJavaScriptFunction("api.functionName", base::ListValue()));
  JavaScriptCompletionHandler success_completion =
      last_received_completion_handler_;
  ASSERT_TRUE(success_completion);

  web_frame.reset();

  base::test::ScopedFeatureList scoped_feature_list(
      features::kAssertOnJavaScriptErrors);
  error_completion(nil, CreateJavaScriptExceptionError());
  success_completion(@"js_response", nil);

  histogram_tester.ExpectTotalCount(kInterestingScriptErrorHistogram, 0);
}

// Tests that `CallJavaScriptFunction` with a reply callback ignores JavaScript
// errors if the `WebFrame` is destroyed before execution completes.
TEST_F(WebFrameImplTest,
       CallJavaScriptFunctionWithCallbackIgnoresErrorAfterWebFrameDestroyed) {
  base::HistogramTester histogram_tester;
  auto web_frame = std::make_unique<WebFrameImpl>(
      mock_frame_info_, kFrameId,
      /*is_main_frame=*/true, security_origin_, &fake_web_state_,
      ContentWorld::kPageContentWorld);

  __block int callback_count = 0;
  auto callback = base::BindOnce(^(const base::Value* value) {
    EXPECT_FALSE(value);
    callback_count++;
  });
  EXPECT_TRUE(
      web_frame->CallJavaScriptFunction("api.functionName", base::ListValue(),
                                        std::move(callback), base::Seconds(5)));
  JavaScriptCompletionHandler completion = last_received_completion_handler_;
  ASSERT_TRUE(completion);

  // Destroying `web_frame` cancels the pending request with `nullptr`.
  web_frame.reset();
  EXPECT_EQ(1, callback_count);

  base::test::ScopedFeatureList scoped_feature_list(
      features::kAssertOnJavaScriptErrors);
  completion(nil, CreateJavaScriptExceptionError());

  EXPECT_EQ(1, callback_count);
  histogram_tester.ExpectTotalCount(kInterestingScriptErrorHistogram, 0);
}

// Tests that `ExecuteJavaScript` logs JavaScript execution errors and runs the
// completion callback when `WebFrame` is valid.
TEST_F(WebFrameImplTest, ExecuteJavaScriptLogsErrorForValidWebFrame) {
  base::HistogramTester histogram_tester;
  WebFrameImpl web_frame(mock_frame_info_, kFrameId,
                         /*is_main_frame=*/true, security_origin_,
                         &fake_web_state_, ContentWorld::kPageContentWorld);

  __block bool callback_called = false;
  __block NSError* received_error = nil;
  auto callback = base::BindOnce(^(const base::Value* value, NSError* error) {
    EXPECT_FALSE(value);
    callback_called = true;
    received_error = error;
  });
  EXPECT_TRUE(web_frame.ExecuteJavaScript(u"__gCrWeb.fakeFunction()",
                                          std::move(callback)));
  ASSERT_TRUE(last_received_completion_handler_);

  NSError* expected_error = CreateJavaScriptExceptionError();
  last_received_completion_handler_(nil, expected_error);

  EXPECT_TRUE(callback_called);
  EXPECT_NSEQ(expected_error, received_error);
  histogram_tester.ExpectUniqueSample(kInterestingScriptErrorHistogram, true,
                                      1);
}

// Tests that `ExecuteJavaScript` skips error logging if `WebFrame` is destroyed
// before execution completes, while still invoking the caller's callback.
TEST_F(WebFrameImplTest, ExecuteJavaScriptIgnoresErrorAfterWebFrameDestroyed) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAssertOnJavaScriptErrors);
  auto web_frame = std::make_unique<WebFrameImpl>(
      mock_frame_info_, kFrameId,
      /*is_main_frame=*/true, security_origin_, &fake_web_state_,
      ContentWorld::kPageContentWorld);

  __block bool error_callback_called = false;
  __block NSError* received_error = nil;
  auto error_callback =
      base::BindOnce(^(const base::Value* value, NSError* error) {
        EXPECT_FALSE(value);
        error_callback_called = true;
        received_error = error;
      });
  EXPECT_TRUE(web_frame->ExecuteJavaScript(u"__gCrWeb.fakeFunction()",
                                           std::move(error_callback)));
  JavaScriptCompletionHandler error_completion =
      last_received_completion_handler_;
  ASSERT_TRUE(error_completion);

  __block bool success_callback_called = false;
  auto success_callback =
      base::BindOnce(^(const base::Value* value, NSError* error) {
        ASSERT_TRUE(value);
        ASSERT_TRUE(value->is_string());
        EXPECT_EQ("js_response", value->GetString());
        EXPECT_FALSE(error);
        success_callback_called = true;
      });
  EXPECT_TRUE(web_frame->ExecuteJavaScript(u"__gCrWeb.fakeFunction()",
                                           std::move(success_callback)));
  JavaScriptCompletionHandler success_completion =
      last_received_completion_handler_;
  ASSERT_TRUE(success_completion);

  web_frame.reset();

  NSError* expected_error = CreateJavaScriptExceptionError();
  error_completion(nil, expected_error);
  success_completion(@"js_response", nil);

  EXPECT_TRUE(error_callback_called);
  EXPECT_NSEQ(expected_error, received_error);
  EXPECT_TRUE(success_callback_called);
  histogram_tester.ExpectTotalCount(kInterestingScriptErrorHistogram, 0);
}

// Tests that `CallAsyncJavaScriptFunction` skips error logging if `WebFrame` is
// destroyed before async execution settles, while still invoking `callback`.
TEST_F(WebFrameImplTest,
       CallAsyncJavaScriptFunctionIgnoresErrorAfterWebFrameDestroyed) {
  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list(
      features::kAssertOnJavaScriptErrors);

  __block JavaScriptCompletionHandler async_completion = nil;
  OCMStub([mock_web_view_
      callAsyncJavaScript:OCMOCK_ANY
                arguments:OCMOCK_ANY
                  inFrame:OCMOCK_ANY
           inContentWorld:OCMOCK_ANY
        completionHandler:CopyValueToVariable(async_completion)]);

  auto web_frame = std::make_unique<WebFrameImpl>(
      mock_frame_info_, kFrameId,
      /*is_main_frame=*/true, security_origin_, &fake_web_state_,
      ContentWorld::kPageContentWorld);

  __block bool callback_called = false;
  __block NSError* received_error = nil;
  auto callback = base::BindOnce(^(const base::Value* value, NSError* error) {
    EXPECT_FALSE(value);
    callback_called = true;
    received_error = error;
  });
  EXPECT_TRUE(web_frame->CallAsyncJavaScriptFunction(
      "api.functionName", base::DictValue(), std::move(callback)));
  ASSERT_TRUE(async_completion);

  web_frame.reset();

  NSError* expected_error = CreateJavaScriptExceptionError();
  async_completion(nil, expected_error);

  EXPECT_TRUE(callback_called);
  EXPECT_NSEQ(expected_error, received_error);
  histogram_tester.ExpectTotalCount(kInterestingScriptErrorHistogram, 0);
}

// Tests that JavaScript execution errors and cached errors are ignored if
// `WebState` is destroyed before execution completes, even while `WebFrame`
// remains alive.
TEST_F(WebFrameImplTest, IgnoresErrorsAfterWebStateDestroyed) {
  base::HistogramTester histogram_tester;
  auto fake_web_state = std::make_unique<FakeWebState>();
  fake_web_state->SetBrowserState(GetBrowserState());
  WebFrameImpl web_frame(mock_frame_info_, kFrameId,
                         /*is_main_frame=*/true, security_origin_,
                         fake_web_state.get(), ContentWorld::kPageContentWorld);

  // Cache an unsupported-result error before `fake_web_state` is destroyed.
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("api.functionName", base::ListValue()));
  ASSERT_TRUE(last_received_completion_handler_);
  last_received_completion_handler_(nil, CreateUnsupportedResultTypeError());

  // Start in-flight calls across both error and success branches of all
  // completion paths before destroying `fake_web_state`.
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("api.functionName", base::ListValue()));
  JavaScriptCompletionHandler fire_and_forget_error =
      last_received_completion_handler_;
  ASSERT_TRUE(fire_and_forget_error);

  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("api.functionName", base::ListValue()));
  JavaScriptCompletionHandler fire_and_forget_success =
      last_received_completion_handler_;
  ASSERT_TRUE(fire_and_forget_success);

  __block int reply_callback_count = 0;
  auto reply_error_callback = base::BindOnce(^(const base::Value* value) {
    EXPECT_FALSE(value);
    reply_callback_count++;
  });
  EXPECT_TRUE(web_frame.CallJavaScriptFunction(
      "api.functionName", base::ListValue(), std::move(reply_error_callback),
      base::Seconds(5)));
  JavaScriptCompletionHandler reply_error = last_received_completion_handler_;
  ASSERT_TRUE(reply_error);

  auto reply_success_callback = base::BindOnce(^(const base::Value* value) {
    EXPECT_FALSE(value);
    reply_callback_count++;
  });
  EXPECT_TRUE(web_frame.CallJavaScriptFunction(
      "api.functionName", base::ListValue(), std::move(reply_success_callback),
      base::Seconds(5)));
  JavaScriptCompletionHandler reply_success = last_received_completion_handler_;
  ASSERT_TRUE(reply_success);

  __block bool execute_error_called = false;
  __block NSError* received_error = nil;
  auto execute_error_callback =
      base::BindOnce(^(const base::Value* value, NSError* error) {
        EXPECT_FALSE(value);
        execute_error_called = true;
        received_error = error;
      });
  EXPECT_TRUE(web_frame.ExecuteJavaScript(u"__gCrWeb.fakeFunction()",
                                          std::move(execute_error_callback)));
  JavaScriptCompletionHandler execute_error = last_received_completion_handler_;
  ASSERT_TRUE(execute_error);

  __block bool execute_success_called = false;
  auto execute_success_callback =
      base::BindOnce(^(const base::Value* value, NSError* error) {
        ASSERT_TRUE(value);
        ASSERT_TRUE(value->is_string());
        EXPECT_EQ("js_response", value->GetString());
        EXPECT_FALSE(error);
        execute_success_called = true;
      });
  EXPECT_TRUE(web_frame.ExecuteJavaScript(u"__gCrWeb.fakeFunction()",
                                          std::move(execute_success_callback)));
  JavaScriptCompletionHandler execute_success =
      last_received_completion_handler_;
  ASSERT_TRUE(execute_success);

  // Destroy `fake_web_state` while `web_frame` remains alive. Both pending
  // `CallJavaScriptFunction` callbacks are canceled with `nullptr`.
  fake_web_state.reset();
  EXPECT_EQ(2, reply_callback_count);

  base::test::ScopedFeatureList scoped_feature_list(
      features::kAssertOnJavaScriptErrors);
  NSError* expected_error = CreateJavaScriptExceptionError();
  fire_and_forget_error(nil, expected_error);
  fire_and_forget_success(@"js_response", nil);
  reply_error(nil, expected_error);
  reply_success(@"js_response", nil);
  execute_error(nil, expected_error);
  execute_success(@"js_response", nil);

  EXPECT_EQ(2, reply_callback_count);
  EXPECT_TRUE(execute_error_called);
  EXPECT_NSEQ(expected_error, received_error);
  EXPECT_TRUE(execute_success_called);
  histogram_tester.ExpectTotalCount(kInterestingScriptErrorHistogram, 0);
}

}  // namespace web
