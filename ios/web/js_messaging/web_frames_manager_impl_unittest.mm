// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/js_messaging/web_frames_manager_impl.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/js_messaging/crw_wk_script_message_router.h"
#include "ios/web/js_messaging/web_frame_impl.h"
#include "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Frame ids are base16 string of 128 bit numbers.
const char kMainFrameId[] = "1effd8f52a067c8d3a01762d3c41dfd1";
const char kFrame1Id[] = "1effd8f52a067c8d3a01762d3c41dfd2";
const char kFrame2Id[] = "1effd8f52a067c8d3a01762d3c41dfd3";

// A base64 encoded sample key.
const char kFrameKey[] = "R7lsXtR74c6R9A9k691gUQ8JAd0be+w//Lntgcbjwrc=";

// Message command sent when a frame becomes available.
NSString* const kFrameBecameAvailableMessageName = @"FrameBecameAvailable";
// Message command sent when a frame is unloading.
NSString* const kFrameBecameUnavailableMessageName = @"FrameBecameUnavailable";

}  // namespace

namespace web {

class WebFramesManagerImplTest : public PlatformTest,
                                 public WebFramesManagerDelegate {
 protected:
  WebFramesManagerImplTest()
      : frames_manager_(*this),
        user_content_controller_(OCMClassMock([WKUserContentController class])),
        router_([[CRWWKScriptMessageRouter alloc]
            initWithUserContentController:user_content_controller_]),
        web_view_(OCMClassMock([WKWebView class])),
        main_frame_(kMainFrameId,
                    /*is_main_frame=*/true,
                    GURL("https://www.main.test")),
        frame_1_(kFrame1Id,
                 /*is_main_frame=*/false,
                 GURL("https://www.frame1.test")),
        frame_2_(kFrame2Id,
                 /*is_main_frame=*/false,
                 GURL("https://www.frame2.test")) {
    // Notify |frames_manager_| that its associated WKWebView has changed from
    // nil to |web_view_|.
    frames_manager_.OnWebViewUpdated(nil, web_view_, router_);
  }

  // Sends a JS message of a newly loaded web frame to |router_| which will
  // dispatch it to |frames_manager_|.
  void SendFrameBecameAvailableMessage(const FakeWebFrame& web_frame) {
    // Mock WKSecurityOrigin.
    WKSecurityOrigin* security_origin = OCMClassMock([WKSecurityOrigin class]);
    OCMStub([security_origin host])
        .andReturn(
            base::SysUTF8ToNSString(web_frame.GetSecurityOrigin().host()));
    OCMStub([security_origin port])
        .andReturn(web_frame.GetSecurityOrigin().EffectiveIntPort());
    OCMStub([security_origin protocol])
        .andReturn(
            base::SysUTF8ToNSString(web_frame.GetSecurityOrigin().scheme()));

    // Mock WKFrameInfo.
    WKFrameInfo* frame_info = OCMClassMock([WKFrameInfo class]);
    OCMStub([frame_info isMainFrame]).andReturn(web_frame.IsMainFrame());
    OCMStub([frame_info securityOrigin]).andReturn(security_origin);

    // Mock WKScriptMessage for "FrameBecameAvailable" message.
    NSDictionary* body = @{
      @"crwFrameId" : base::SysUTF8ToNSString(web_frame.GetFrameId()),
      @"crwFrameKey" : base::SysUTF8ToNSString(kFrameKey),
      @"crwFrameLastReceivedMessageId" : @-1,
    };
    WKScriptMessage* message = OCMClassMock([WKScriptMessage class]);
    OCMStub([message body]).andReturn(body);
    OCMStub([message frameInfo]).andReturn(frame_info);
    OCMStub([message name]).andReturn(kFrameBecameAvailableMessageName);
    OCMStub([message webView]).andReturn(web_view_);

    [(id<WKScriptMessageHandler>)router_
          userContentController:user_content_controller_
        didReceiveScriptMessage:message];
  }

  // Sends a JS message of a newly unloaded web frame to |router_| which will
  // dispatch it to |frames_manager_|.
  void SendFrameBecameUnavailableMessage(const FakeWebFrame& web_frame) {
    // Mock WKSecurityOrigin.
    WKSecurityOrigin* security_origin = OCMClassMock([WKSecurityOrigin class]);
    OCMStub([security_origin host])
        .andReturn(
            base::SysUTF8ToNSString(web_frame.GetSecurityOrigin().host()));
    OCMStub([security_origin port])
        .andReturn(web_frame.GetSecurityOrigin().EffectiveIntPort());
    OCMStub([security_origin protocol])
        .andReturn(
            base::SysUTF8ToNSString(web_frame.GetSecurityOrigin().scheme()));

    // Mock WKFrameInfo.
    WKFrameInfo* frame_info = OCMClassMock([WKFrameInfo class]);
    OCMStub([frame_info isMainFrame]).andReturn(web_frame.IsMainFrame());
    OCMStub([frame_info securityOrigin]).andReturn(security_origin);

    // Mock WKScriptMessage for "FrameBecameUnavailable" message.
    WKScriptMessage* message = OCMClassMock([WKScriptMessage class]);
    OCMStub([message body])
        .andReturn(base::SysUTF8ToNSString(web_frame.GetFrameId()));
    OCMStub([message frameInfo]).andReturn(frame_info);
    OCMStub([message name]).andReturn(kFrameBecameUnavailableMessageName);
    OCMStub([message webView]).andReturn(web_view_);

    [(id<WKScriptMessageHandler>)router_
          userContentController:user_content_controller_
        didReceiveScriptMessage:message];
  }

  // WebFramesManagerDelegate.
  void OnWebFrameAvailable(WebFrame* frame) override {}
  void OnWebFrameUnavailable(WebFrame* frame) override {}
  WebState* GetWebState() override { return &test_web_state_; }

  TestWebState test_web_state_;
  WebFramesManagerImpl frames_manager_;
  WKUserContentController* user_content_controller_ = nil;
  CRWWKScriptMessageRouter* router_ = nil;
  WKWebView* web_view_ = nil;
  const FakeWebFrame main_frame_;
  const FakeWebFrame frame_1_;
  const FakeWebFrame frame_2_;
};

// Tests main web frame construction/destruction.
TEST_F(WebFramesManagerImplTest, MainWebFrame) {
  SendFrameBecameAvailableMessage(main_frame_);

  EXPECT_EQ(1ul, frames_manager_.GetAllWebFrames().size());
  WebFrame* main_frame = frames_manager_.GetMainWebFrame();
  WebFrame* main_frame_by_id =
      frames_manager_.GetFrameWithId(main_frame_.GetFrameId());
  ASSERT_TRUE(main_frame);
  ASSERT_TRUE(main_frame_by_id);
  EXPECT_EQ(main_frame, main_frame_by_id);
  EXPECT_TRUE(main_frame->IsMainFrame());
  EXPECT_EQ(main_frame_.GetSecurityOrigin(), main_frame->GetSecurityOrigin());

  SendFrameBecameUnavailableMessage(main_frame_);
  EXPECT_EQ(0ul, frames_manager_.GetAllWebFrames().size());
  EXPECT_FALSE(frames_manager_.GetMainWebFrame());
  EXPECT_FALSE(frames_manager_.GetFrameWithId(main_frame_.GetFrameId()));
}

// Tests multiple web frames construction/destruction.
TEST_F(WebFramesManagerImplTest, MultipleWebFrame) {
  // Add main frame.
  SendFrameBecameAvailableMessage(main_frame_);
  EXPECT_EQ(1ul, frames_manager_.GetAllWebFrames().size());
  // Check main frame.
  WebFrame* main_frame = frames_manager_.GetMainWebFrame();
  WebFrame* main_frame_by_id =
      frames_manager_.GetFrameWithId(main_frame_.GetFrameId());
  ASSERT_TRUE(main_frame);
  EXPECT_EQ(main_frame, main_frame_by_id);
  EXPECT_TRUE(main_frame->IsMainFrame());
  EXPECT_EQ(main_frame_.GetSecurityOrigin(), main_frame->GetSecurityOrigin());

  // Add frame 1.
  SendFrameBecameAvailableMessage(frame_1_);
  EXPECT_EQ(2ul, frames_manager_.GetAllWebFrames().size());
  // Check main frame.
  main_frame = frames_manager_.GetMainWebFrame();
  main_frame_by_id = frames_manager_.GetFrameWithId(main_frame_.GetFrameId());
  ASSERT_TRUE(main_frame);
  EXPECT_EQ(main_frame, main_frame_by_id);
  EXPECT_TRUE(main_frame->IsMainFrame());
  EXPECT_EQ(main_frame_.GetSecurityOrigin(), main_frame->GetSecurityOrigin());
  // Check frame 1.
  WebFrame* frame_1 = frames_manager_.GetFrameWithId(frame_1_.GetFrameId());
  ASSERT_TRUE(frame_1);
  EXPECT_FALSE(frame_1->IsMainFrame());
  EXPECT_EQ(frame_1_.GetSecurityOrigin(), frame_1->GetSecurityOrigin());

  // Add frame 2.
  SendFrameBecameAvailableMessage(frame_2_);
  EXPECT_EQ(3ul, frames_manager_.GetAllWebFrames().size());
  // Check main frame.
  main_frame = frames_manager_.GetMainWebFrame();
  main_frame_by_id = frames_manager_.GetFrameWithId(main_frame_.GetFrameId());
  ASSERT_TRUE(main_frame);
  EXPECT_EQ(main_frame, main_frame_by_id);
  EXPECT_TRUE(main_frame->IsMainFrame());
  EXPECT_EQ(main_frame_.GetSecurityOrigin(), main_frame->GetSecurityOrigin());
  // Check frame 1.
  frame_1 = frames_manager_.GetFrameWithId(frame_1_.GetFrameId());
  ASSERT_TRUE(frame_1);
  EXPECT_FALSE(frame_1->IsMainFrame());
  EXPECT_EQ(frame_1_.GetSecurityOrigin(), frame_1->GetSecurityOrigin());
  // Check frame 2.
  WebFrame* frame_2 = frames_manager_.GetFrameWithId(frame_2_.GetFrameId());
  ASSERT_TRUE(frame_2);
  EXPECT_FALSE(frame_2->IsMainFrame());
  EXPECT_EQ(frame_2_.GetSecurityOrigin(), frame_2->GetSecurityOrigin());

  // Remove frame 1.
  SendFrameBecameUnavailableMessage(frame_1_);
  EXPECT_EQ(2ul, frames_manager_.GetAllWebFrames().size());
  // Check main frame.
  main_frame = frames_manager_.GetMainWebFrame();
  main_frame_by_id = frames_manager_.GetFrameWithId(main_frame_.GetFrameId());
  ASSERT_TRUE(main_frame);
  EXPECT_EQ(main_frame, main_frame_by_id);
  EXPECT_TRUE(main_frame->IsMainFrame());
  EXPECT_EQ(main_frame_.GetSecurityOrigin(), main_frame->GetSecurityOrigin());
  // Check frame 1.
  frame_1 = frames_manager_.GetFrameWithId(frame_1_.GetFrameId());
  EXPECT_FALSE(frame_1);
  // Check frame 2.
  frame_2 = frames_manager_.GetFrameWithId(frame_2_.GetFrameId());
  ASSERT_TRUE(frame_2);
  EXPECT_FALSE(frame_2->IsMainFrame());
  EXPECT_EQ(frame_2_.GetSecurityOrigin(), frame_2->GetSecurityOrigin());

  // Remove main frame.
  SendFrameBecameUnavailableMessage(main_frame_);
  EXPECT_EQ(1ul, frames_manager_.GetAllWebFrames().size());
  // Check main frame.
  main_frame = frames_manager_.GetMainWebFrame();
  main_frame_by_id = frames_manager_.GetFrameWithId(main_frame_.GetFrameId());
  EXPECT_FALSE(main_frame);
  EXPECT_FALSE(main_frame_by_id);
  // Check frame 1.
  frame_1 = frames_manager_.GetFrameWithId(frame_1_.GetFrameId());
  EXPECT_FALSE(frame_1);
  // Check frame 2.
  frame_2 = frames_manager_.GetFrameWithId(frame_2_.GetFrameId());
  ASSERT_TRUE(frame_2);
  EXPECT_FALSE(frame_2->IsMainFrame());
  EXPECT_EQ(frame_2_.GetSecurityOrigin(), frame_2->GetSecurityOrigin());

  // Remove frame 2.
  SendFrameBecameUnavailableMessage(frame_2_);
  EXPECT_EQ(0ul, frames_manager_.GetAllWebFrames().size());
  // Check main frame.
  main_frame = frames_manager_.GetMainWebFrame();
  main_frame_by_id = frames_manager_.GetFrameWithId(main_frame_.GetFrameId());
  EXPECT_FALSE(main_frame);
  EXPECT_FALSE(main_frame_by_id);
  // Check frame 1.
  frame_1 = frames_manager_.GetFrameWithId(frame_1_.GetFrameId());
  EXPECT_FALSE(frame_1);
  // Check frame 2.
  frame_2 = frames_manager_.GetFrameWithId(frame_2_.GetFrameId());
  EXPECT_FALSE(frame_2);
}

// Tests WebFramesManagerImpl::RemoveAllWebFrames.
TEST_F(WebFramesManagerImplTest, RemoveAllWebFrames) {
  SendFrameBecameAvailableMessage(main_frame_);
  SendFrameBecameAvailableMessage(frame_1_);
  SendFrameBecameAvailableMessage(frame_2_);
  EXPECT_EQ(3ul, frames_manager_.GetAllWebFrames().size());

  frames_manager_.RemoveAllWebFrames();
  EXPECT_EQ(0ul, frames_manager_.GetAllWebFrames().size());
  // Check main frame.
  EXPECT_FALSE(frames_manager_.GetMainWebFrame());
  EXPECT_FALSE(frames_manager_.GetFrameWithId(main_frame_.GetFrameId()));
  // Check frame 1.
  EXPECT_FALSE(frames_manager_.GetFrameWithId(frame_1_.GetFrameId()));
  // Check frame 2.
  EXPECT_FALSE(frames_manager_.GetFrameWithId(frame_2_.GetFrameId()));
}

// Tests that WebFramesManagerImpl will ignore JS messages from previous
// WKWebView after WebFramesManagerImpl::OnWebViewUpdated is called with a new
// WKWebView, and that all web frames of previous WKWebView are removed.
TEST_F(WebFramesManagerImplTest, OnWebViewUpdated) {
  SendFrameBecameAvailableMessage(main_frame_);
  SendFrameBecameAvailableMessage(frame_1_);
  EXPECT_EQ(2ul, frames_manager_.GetAllWebFrames().size());

  // Notify WebFramesManagerImpl that its associated WKWebView has changed from
  // |web_view_| to a new WKWebView.
  WKWebView* web_view_2 = OCMClassMock([WKWebView class]);
  frames_manager_.OnWebViewUpdated(web_view_, web_view_2, router_);

  // Send JS message of loaded/unloaded web frames in previous WKWebView (i.e.
  // web_view_). |frames_manager_| should have unregistered JS message handlers
  // for |web_view_| and removed all web frames, so no web frame should be
  // added.
  SendFrameBecameAvailableMessage(frame_1_);
  EXPECT_EQ(0ul, frames_manager_.GetAllWebFrames().size());
  SendFrameBecameAvailableMessage(frame_2_);
  EXPECT_EQ(0ul, frames_manager_.GetAllWebFrames().size());
  SendFrameBecameUnavailableMessage(frame_1_);
  EXPECT_EQ(0ul, frames_manager_.GetAllWebFrames().size());
}

}  // namespace web
