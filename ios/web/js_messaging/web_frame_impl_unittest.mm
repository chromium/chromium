// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/web_frame_impl.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebKit.h>

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/json/json_reader.h"
#import "base/run_loop.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "components/test/ios/test_utils.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_test.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

const char kFrameId[] = "1effd8f52a067c8d3a01762d3c41dfd8";

const char kFrameInfoRequestUrl[] = "https://test.com";

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
         completionHandler:OCMOCK_ANY]);
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

  id mock_frame_info_;
  id mock_web_view_;
  id mock_ns_url_request_;
  NSString* last_received_script_;
  WKContentWorld* last_received_content_world_;

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

}  // namespace web
