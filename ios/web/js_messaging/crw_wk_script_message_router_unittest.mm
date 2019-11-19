// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/crw_wk_script_message_router.h"

#include "base/mac/scoped_block.h"
#include "base/memory/ptr_util.h"
#import "ios/web/common/web_view_creation_util.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/public/test/fakes/test_web_client.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/web_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns WKScriptMessage mock.
id GetScriptMessageMock(WKFrameInfo* frame_info,
                        WKWebView* web_view,
                        NSString* name) {
  id result = [OCMockObject mockForClass:[WKScriptMessage class]];
  [[[result stub] andReturn:frame_info] frameInfo];
  [[[result stub] andReturn:web_view] webView];
  [[[result stub] andReturn:name] name];
  return result;
}

// Test fixture for CRWWKScriptMessageRouter.
class CRWWKScriptMessageRouterTest : public web::WebTest {
 public:
  CRWWKScriptMessageRouterTest()
      : web_client_(base::WrapUnique(new web::WebClient)) {}

 protected:
  void SetUp() override {
    web::WebTest::SetUp();
    // Mock WKUserContentController object.
    controller_mock_ =
        [OCMockObject mockForClass:[WKUserContentController class]];
    [controller_mock_ setExpectationOrderMatters:YES];

    // Create testable CRWWKScriptMessageRouter.
    router_ = static_cast<id<WKScriptMessageHandler>>(
        [[CRWWKScriptMessageRouter alloc]
            initWithUserContentController:controller_mock_]);

    // Prepare test data.
    handler1_ = [^{
    } copy];
    handler2_ = [^{
    } copy];
    handler3_ = [^{
    } copy];
    name1_ = [@"name1" copy];
    name2_ = [@"name2" copy];
    name3_ = [@"name3" copy];
    web_view1_ = web::BuildWKWebView(CGRectZero, &browser_state_);
    web_view2_ = web::BuildWKWebView(CGRectZero, &browser_state_);
    web_view3_ = web::BuildWKWebView(CGRectZero, &browser_state_);
  }
  void TearDown() override {
    EXPECT_OCMOCK_VERIFY(controller_mock_);
    web::WebTest::TearDown();
  }

  // WKUserContentController mock used to create testable router.
  id controller_mock_;

  // CRWWKScriptMessageRouter set up for testing.
  id router_;

  // Tests data.
  typedef void (^WKScriptMessageHandler)(WKScriptMessage*);
  WKScriptMessageHandler handler1_;
  WKScriptMessageHandler handler2_;
  WKScriptMessageHandler handler3_;
  NSString* name1_;
  NSString* name2_;
  NSString* name3_;
  WKWebView* web_view1_;
  WKWebView* web_view2_;
  WKWebView* web_view3_;

 private:
  // WebClient and BrowserState for testing.
  web::ScopedTestingWebClient web_client_;
  web::TestBrowserState browser_state_;
};

// Tests CRWWKScriptMessageRouter designated initializer.
TEST_F(CRWWKScriptMessageRouterTest, Initialization) {
  EXPECT_TRUE(router_);
}

// Tests registration/deregistation of message handlers.
TEST_F(CRWWKScriptMessageRouterTest, HandlerRegistration) {
  [[controller_mock_ expect] addScriptMessageHandler:router_ name:name1_];
  [[controller_mock_ expect] addScriptMessageHandler:router_ name:name2_];

  [[controller_mock_ expect] removeScriptMessageHandlerForName:name1_];
  [[controller_mock_ expect] removeScriptMessageHandlerForName:name2_];

  [router_ setScriptMessageHandler:handler1_ name:name1_ webView:web_view1_];
  [router_ setScriptMessageHandler:handler2_ name:name2_ webView:web_view2_];
  [router_ setScriptMessageHandler:handler3_ name:name2_ webView:web_view3_];

  [router_ removeScriptMessageHandlerForName:name1_ webView:web_view1_];
  [router_ removeScriptMessageHandlerForName:name2_ webView:web_view2_];
  [router_ removeScriptMessageHandlerForName:name2_ webView:web_view3_];
}

// Tests registration of message handlers. Test ensures that
// WKScriptMessageHandler is not removed if CRWWKScriptMessageRouter has valid
// message handlers.
TEST_F(CRWWKScriptMessageRouterTest, HandlerRegistrationLeak) {
  [[controller_mock_ expect] addScriptMessageHandler:router_ name:name1_];

  // -removeScriptMessageHandlerForName must not be called.

  [router_ setScriptMessageHandler:handler1_ name:name1_ webView:web_view1_];
  [router_ setScriptMessageHandler:handler2_ name:name1_ webView:web_view2_];

  [router_ removeScriptMessageHandlerForName:name1_ webView:web_view1_];
}

// Tests deregistation of all message handlers.
TEST_F(CRWWKScriptMessageRouterTest, RemoveAllHandlers) {
  [[controller_mock_ expect] addScriptMessageHandler:router_ name:name1_];
  [[controller_mock_ expect] addScriptMessageHandler:router_ name:name2_];

  [[controller_mock_ expect] removeScriptMessageHandlerForName:name2_];
  [[controller_mock_ expect] removeScriptMessageHandlerForName:name1_];

  [router_ setScriptMessageHandler:handler1_ name:name1_ webView:web_view1_];
  [router_ setScriptMessageHandler:handler2_ name:name2_ webView:web_view1_];
  [router_ setScriptMessageHandler:handler3_ name:name1_ webView:web_view2_];

  [router_ removeAllScriptMessageHandlersForWebView:web_view1_];
  [router_ removeAllScriptMessageHandlersForWebView:web_view2_];
}

// Tests deregistation of all message handlers. Test ensures that
// WKScriptMessageHandler is not removed if CRWWKScriptMessageRouter has valid
// message handlers.
TEST_F(CRWWKScriptMessageRouterTest, RemoveAllHandlersLeak) {
  [[controller_mock_ expect] addScriptMessageHandler:router_ name:name1_];
  [[controller_mock_ expect] addScriptMessageHandler:router_ name:name2_];
  [[controller_mock_ expect] addScriptMessageHandler:router_ name:name3_];

  [[controller_mock_ expect] removeScriptMessageHandlerForName:name2_];
  // -removeScriptMessageHandlerForName:name1_ must not be called.

  [router_ setScriptMessageHandler:handler1_ name:name1_ webView:web_view1_];
  [router_ setScriptMessageHandler:handler2_ name:name2_ webView:web_view1_];
  [router_ setScriptMessageHandler:handler2_ name:name3_ webView:web_view2_];
  [router_ setScriptMessageHandler:handler3_ name:name1_ webView:web_view2_];

  [router_ removeAllScriptMessageHandlersForWebView:web_view1_];
}

// Tests proper routing of WKScriptMessage object depending on message name and
// web view.
TEST_F(CRWWKScriptMessageRouterTest, Routing) {
  // It's expected that messages handlers will be called once and in order.
  WKFrameInfo* frame_info = [[WKFrameInfo alloc] init];
  __block NSInteger last_called_handler = 0;
  id message1 = GetScriptMessageMock(frame_info, web_view1_, name1_);
  id handler1 = ^(WKScriptMessage* message) {
    EXPECT_EQ(0, last_called_handler);
    EXPECT_EQ(message1, message);
    last_called_handler = 1;
  };
  id message2 = GetScriptMessageMock(frame_info, web_view2_, name2_);
  id handler2 = ^(WKScriptMessage* message) {
    EXPECT_EQ(1, last_called_handler);
    EXPECT_EQ(message2, message);
    last_called_handler = 2;
  };
  id message3 = GetScriptMessageMock(frame_info, web_view3_, name2_);
  id handler3 = ^(WKScriptMessage* message) {
    EXPECT_EQ(2, last_called_handler);
    EXPECT_EQ(message3, message);
    last_called_handler = 3;
  };

  [[controller_mock_ expect] addScriptMessageHandler:router_ name:name1_];
  [[controller_mock_ expect] addScriptMessageHandler:router_ name:name2_];

  [router_ setScriptMessageHandler:handler1 name:name1_ webView:web_view1_];
  [router_ setScriptMessageHandler:handler2 name:name2_ webView:web_view2_];
  [router_ setScriptMessageHandler:handler3 name:name2_ webView:web_view3_];

  [router_ userContentController:controller_mock_
         didReceiveScriptMessage:message1];
  [router_ userContentController:controller_mock_
         didReceiveScriptMessage:message2];
  [router_ userContentController:controller_mock_
         didReceiveScriptMessage:message3];

  EXPECT_EQ(3, last_called_handler);
}

}  // namespace
