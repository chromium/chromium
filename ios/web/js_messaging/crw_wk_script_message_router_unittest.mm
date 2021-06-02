// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/js_messaging/crw_wk_script_message_router.h"

#include "base/memory/ptr_util.h"
#import "ios/web/common/web_view_creation_util.h"
#include "ios/web/public/test/fakes/fake_browser_state.h"
#include "ios/web/public/test/scoped_testing_web_client.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"
#import "testing/gtest_mac.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// A subclass of WKUserContentController that checks what scripts handler are
// added or removed.
// Mocks are not used as WKUserContentController mocks cause crashes on iPads.
@interface WKUserContentControllerForTests : WKUserContentController
// The ordered list of script handler names expected to be added.
- (void)expectAddScriptCallWithName:(NSArray*)names;
// The ordered list of script handler names expected to be removed.
- (void)expectRemoveScriptCallWithName:(NSArray*)names;
// Checks that exactly the expected calls have been made.
- (void)checkExpectations;

@end

@implementation WKUserContentControllerForTests {
  NSArray* _addScriptExpected;
  NSArray* _removeScriptExpected;
}

- (void)expectAddScriptCallWithName:(NSArray*)names {
  EXPECT_EQ(_addScriptExpected.count, 0u);
  _addScriptExpected = names;
}

- (void)expectRemoveScriptCallWithName:(NSArray*)names {
  EXPECT_EQ(_removeScriptExpected.count, 0u);
  _removeScriptExpected = names;
}

- (void)checkExpectations {
  EXPECT_EQ(_addScriptExpected.count, 0u);
  EXPECT_EQ(_removeScriptExpected.count, 0u);
}

- (void)addScriptMessageHandler:(id<WKScriptMessageHandler>)scriptMessageHandler
                           name:(NSString*)name {
  ASSERT_GT(_addScriptExpected.count, 0u);
  EXPECT_NSEQ(_addScriptExpected[0], name);
  _addScriptExpected = [_addScriptExpected
      subarrayWithRange:NSMakeRange(1, _addScriptExpected.count - 1)];
  [super addScriptMessageHandler:scriptMessageHandler name:name];
}

- (void)removeScriptMessageHandlerForName:(NSString*)name {
  ASSERT_GT(_removeScriptExpected.count, 0u);
  EXPECT_NSEQ(_removeScriptExpected[0], name);
  _removeScriptExpected = [_removeScriptExpected
      subarrayWithRange:NSMakeRange(1, _removeScriptExpected.count - 1)];
  [super removeScriptMessageHandlerForName:name];
}

@end

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
  CRWWKScriptMessageRouterTest() = default;

 protected:
  void SetUp() override {
    web::WebTest::SetUp();

    web::WKWebViewConfigurationProvider& configuration_provider =
        web::WKWebViewConfigurationProvider::FromBrowserState(
            GetBrowserState());
    WKWebViewConfiguration* configuration =
        configuration_provider.GetWebViewConfiguration();
    // Mock WKUserContentController object.
    user_content_controller_ = [[WKUserContentControllerForTests alloc] init];
    configuration.userContentController = user_content_controller_;

    // Create testable CRWWKScriptMessageRouter.
    router_ = static_cast<id<WKScriptMessageHandler>>(
        [[CRWWKScriptMessageRouter alloc]
            initWithUserContentController:user_content_controller_]);

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
    web_view1_ = web::BuildWKWebView(CGRectZero, GetBrowserState());
    web_view2_ = web::BuildWKWebView(CGRectZero, GetBrowserState());
    web_view3_ = web::BuildWKWebView(CGRectZero, GetBrowserState());
  }
  void TearDown() override {
    [user_content_controller_ checkExpectations];
    web::WebTest::TearDown();
  }

  // WKUserContentController used to create testable router.
  WKUserContentControllerForTests* user_content_controller_;

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
};

// Tests CRWWKScriptMessageRouter designated initializer.
TEST_F(CRWWKScriptMessageRouterTest, Initialization) {
  EXPECT_TRUE(router_);
}

// Tests registration/deregistation of message handlers.
TEST_F(CRWWKScriptMessageRouterTest, HandlerRegistration) {
  [user_content_controller_ expectAddScriptCallWithName:@[ name1_, name2_ ]];
  [user_content_controller_ expectRemoveScriptCallWithName:@[ name1_, name2_ ]];

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
  [user_content_controller_ expectAddScriptCallWithName:@[ name1_ ]];
  // -removeScriptMessageHandlerForName must not be called.

  [router_ setScriptMessageHandler:handler1_ name:name1_ webView:web_view1_];
  [router_ setScriptMessageHandler:handler2_ name:name1_ webView:web_view2_];

  [router_ removeScriptMessageHandlerForName:name1_ webView:web_view1_];
}

// Tests deregistation of all message handlers.
TEST_F(CRWWKScriptMessageRouterTest, RemoveAllHandlers) {
  [user_content_controller_ expectAddScriptCallWithName:@[ name1_, name2_ ]];
  [user_content_controller_ expectRemoveScriptCallWithName:@[ name2_, name1_ ]];

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
  [user_content_controller_
      expectAddScriptCallWithName:@[ name1_, name2_, name3_ ]];
  [user_content_controller_ expectRemoveScriptCallWithName:@[ name2_ ]];
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

  [user_content_controller_ expectAddScriptCallWithName:@[ name1_, name2_ ]];

  [router_ setScriptMessageHandler:handler1 name:name1_ webView:web_view1_];
  [router_ setScriptMessageHandler:handler2 name:name2_ webView:web_view2_];
  [router_ setScriptMessageHandler:handler3 name:name2_ webView:web_view3_];

  [router_ userContentController:user_content_controller_
         didReceiveScriptMessage:message1];
  [router_ userContentController:user_content_controller_
         didReceiveScriptMessage:message2];
  [router_ userContentController:user_content_controller_
         didReceiveScriptMessage:message3];

  EXPECT_EQ(3, last_called_handler);
}

}  // namespace
