// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/web_frame_impl.h"

#import <WebKit/WebKit.h>

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/json/json_reader.h"
#import "base/run_loop.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/values.h"
#import "ios/web/js_messaging/java_script_feature_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_test.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {

const char kFrameId[] = "1effd8f52a067c8d3a01762d3c41dfd8";

}  // namespace

namespace web {

class WebFrameImplTest : public web::WebTest {
 protected:
  WebFrameImplTest() {
    mock_frame_info_ = OCMClassMock([WKFrameInfo class]);
    mock_web_view_ = OCMClassMock([WKWebView class]);

    OCMStub([mock_web_view_ evaluateJavaScript:OCMOCK_ANY
                                       inFrame:OCMOCK_ANY
                                inContentWorld:OCMOCK_ANY
                             completionHandler:OCMOCK_ANY])
        .andDo(^(NSInvocation* invocation) {
          [invocation retainArguments];
          [invocation getArgument:&last_received_script_ atIndex:2];
        });
    OCMStub([mock_frame_info_ webView]).andReturn(mock_web_view_);
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
  NSString* last_received_script_;

  FakeWebState fake_web_state_;
  GURL security_origin_;
};

// Tests creation of a WebFrame for the main frame.
TEST_F(WebFrameImplTest, CreateWebFrameForMainFrame) {
  WebFrameImpl web_frame([[WKFrameInfo alloc] init], kFrameId,
                         /*is_main_frame=*/true, security_origin_,
                         &fake_web_state_);

  EXPECT_EQ(&fake_web_state_, web_frame.GetWebState());
  EXPECT_TRUE(web_frame.IsMainFrame());
  EXPECT_EQ(security_origin_, web_frame.GetSecurityOrigin());
  EXPECT_EQ(kFrameId, web_frame.GetFrameId());
}

// Tests that the WebFrame properly creates JavaScript for the main frame.
TEST_F(WebFrameImplTest, CallJavaScriptFunctionMainFrame) {
  WebFrameImpl web_frame(mock_frame_info_, kFrameId,
                         /*is_main_frame=*/true, security_origin_,
                         &fake_web_state_);

  base::Value::List function_params;
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  EXPECT_NSEQ(@"__gCrWeb.functionName()", last_received_script_);

  function_params.Append("param1");
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  EXPECT_NSEQ(@"__gCrWeb.functionName(\"param1\")", last_received_script_);

  function_params.Append(true);
  function_params.Append(27);
  function_params.Append(3.14);
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  EXPECT_NSEQ(@"__gCrWeb.functionName(\"param1\",true,27,3.14)",
              last_received_script_);
}

// Tests that the WebFrame creates JavaScript for an iframe.
TEST_F(WebFrameImplTest, CallJavaScriptFunctionIFrame) {
  WebFrameImpl web_frame(mock_frame_info_, kFrameId,
                         /*is_main_frame=*/false, security_origin_,
                         &fake_web_state_);

  base::Value::List function_params;

  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  EXPECT_NSEQ(@"__gCrWeb.functionName()", last_received_script_);

  function_params.Append("param1");
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  EXPECT_NSEQ(@"__gCrWeb.functionName(\"param1\")", last_received_script_);

  function_params.Append(true);
  function_params.Append(27);
  function_params.Append(3.14);
  EXPECT_TRUE(
      web_frame.CallJavaScriptFunction("functionName", function_params));
  EXPECT_NSEQ(@"__gCrWeb.functionName(\"param1\",true,27,3.14)",
              last_received_script_);
}

// Tests that the WebFrame can execute arbitrary JavaScript.
TEST_F(WebFrameImplTest, ExecuteJavaScript) {
  NSString* script = @"__gCrWeb = {};"
                     @"__gCrWeb['fakeFunction'] = function() {"
                     @"  return '10';"
                     @"}";

  WebFrameImpl web_frame(mock_frame_info_, kFrameId,
                         /*is_main_frame=*/true, security_origin_,
                         &fake_web_state_);

  EXPECT_TRUE(web_frame.ExecuteJavaScript(base::SysNSStringToUTF16(script)));

  WebFrameImpl web_frame2(mock_frame_info_, kFrameId,
                          /*is_main_frame=*/false, security_origin_,
                          &fake_web_state_);
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
                         &fake_web_state_);

  EXPECT_TRUE(
      web_frame.ExecuteJavaScript(base::SysNSStringToUTF16(script),
                                  base::BindOnce(^(const base::Value* value){
                                  })));

  WebFrameImpl web_frame2(mock_frame_info_, kFrameId,
                          /*is_main_frame=*/false, security_origin_,
                          &fake_web_state_);

  EXPECT_TRUE(
      web_frame2.ExecuteJavaScript(base::SysNSStringToUTF16(script),
                                   base::BindOnce(^(const base::Value* value){
                                   })));
}

}  // namespace web
