// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_test_with_web_state.h"

#import "base/containers/contains.h"
#import "base/ios/ios_util.h"
#import "ios/testing/embedded_test_server_handlers.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "net/test/embedded_test_server/embedded_test_server.h"
#import "net/test/embedded_test_server/request_handler_util.h"
#import "testing/gmock/include/gmock/gmock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::Truly;

namespace {

// Mocks WebStateObserver navigation callbacks.
class WebStateObserverMock : public web::WebStateObserver {
 public:
  WebStateObserverMock() = default;

  WebStateObserverMock(const WebStateObserverMock&) = delete;
  WebStateObserverMock& operator=(const WebStateObserverMock&) = delete;

  MOCK_METHOD2(WebFrameDidBecomeAvailable,
               void(web::WebState*, web::WebFrame*));
  MOCK_METHOD2(WebFrameWillBecomeUnavailable,
               void(web::WebState*, web::WebFrame*));
  void WebStateDestroyed(web::WebState* web_state) override { NOTREACHED(); }
};

// A predicate that returns true if `frame` is a main frame.
bool IsMainFrame(web::WebFrame* frame) {
  return frame->IsMainFrame();
}

// Verifies that the web frame passed to the observer is the main frame.
ACTION_P(VerifyMainWebFrame, web_state) {
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state->GetPageWorldWebFramesManager()->GetMainWebFrame(), arg1);
}

// Verifies that the web frame passed to the observer is a child frame.
ACTION_P(VerifyChildWebFrame, web_state) {
  EXPECT_EQ(web_state, arg0);

  web::WebFramesManager* manager = web_state->GetPageWorldWebFramesManager();
  auto frames = manager->GetAllWebFrames();
  EXPECT_TRUE(base::Contains(frames, arg1));
  EXPECT_NE(manager->GetMainWebFrame(), arg1);
}
}

namespace web {

class WebFrameWebStateObserverInttest : public WebTestWithWebState {
 protected:
  void SetUp() override {
    WebTestWithWebState::SetUp();
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &net::test_server::HandlePrefixedRequest, "/echo-query",
        base::BindRepeating(&testing::HandlePageWithContents)));
    ASSERT_TRUE(test_server_.Start());

    web_state()->AddObserver(&observer_);
  }

  void TearDown() override {
    web_state()->RemoveObserver(&observer_);
    WebTestWithWebState::TearDown();
  }

  net::EmbeddedTestServer test_server_;
  testing::StrictMock<WebStateObserverMock> observer_;
};

// Web frame events should be registered on HTTP navigation.
TEST_F(WebFrameWebStateObserverInttest, SingleWebFrameHTTP) {
  EXPECT_CALL(observer_, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));

  test::LoadUrl(web_state(), test_server_.GetURL("/echo-query?test"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "test"));

  EXPECT_CALL(observer_, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));
  EXPECT_CALL(observer_, WebFrameWillBecomeUnavailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));

  test::LoadUrl(web_state(), test_server_.GetURL("/echo-query?secondPage"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "secondPage"));
}

// Web frame events should be registered on HTTPS navigation.
TEST_F(WebFrameWebStateObserverInttest, SingleWebFrameHTTPS) {
  EXPECT_CALL(observer_, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));

  // Load a first page to avoid having an item inserted during the `LoadHtml`.
  test::LoadUrl(web_state(), test_server_.GetURL("/echo-query?test"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "test"));

  EXPECT_CALL(observer_, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));
  EXPECT_CALL(observer_, WebFrameWillBecomeUnavailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));

  LoadHtml(@"<p></p>", GURL("https://testurl1"));

  EXPECT_CALL(observer_, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));
  EXPECT_CALL(observer_, WebFrameWillBecomeUnavailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));

  LoadHtml(@"<p></p>", GURL("https://testurl2"));
}

// Web frame event should be registered on HTTPS navigation with iframe.
TEST_F(WebFrameWebStateObserverInttest, TwoWebFrameHTTPS) {
  EXPECT_CALL(observer_, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));

  // Load a first page to avoid having an item inserted during the `LoadHtml`.
  test::LoadUrl(web_state(), test_server_.GetURL("/echo-query?test"));
  ASSERT_TRUE(test::WaitForWebViewContainingText(web_state(), "test"));

  // The order in which the main and child frames become available is not
  // guaranteed due to the async nature of messaging. The following expectations
  // use separate matchers to identify main and child frames so that they can
  // be matched in any order.
  EXPECT_CALL(observer_,
              WebFrameDidBecomeAvailable(web_state(), Truly(IsMainFrame)))
      .WillOnce(VerifyMainWebFrame(web_state()));
  EXPECT_CALL(observer_,
              WebFrameDidBecomeAvailable(web_state(), Not(Truly(IsMainFrame))))
      .WillOnce(VerifyChildWebFrame(web_state()));
  EXPECT_CALL(observer_, WebFrameWillBecomeUnavailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));

  LoadHtml(@"<p><iframe/></p>", GURL("https://testurl1"));

  EXPECT_CALL(observer_,
              WebFrameDidBecomeAvailable(web_state(), Truly(IsMainFrame)))
      .WillOnce(VerifyMainWebFrame(web_state()));
  EXPECT_CALL(observer_,
              WebFrameDidBecomeAvailable(web_state(), Not(Truly(IsMainFrame))))
      .WillOnce(VerifyChildWebFrame(web_state()));
  EXPECT_CALL(observer_,
              WebFrameWillBecomeUnavailable(web_state(), Truly(IsMainFrame)))
      .WillOnce(VerifyMainWebFrame(web_state()));
  EXPECT_CALL(observer_, WebFrameWillBecomeUnavailable(web_state(),
                                                       Not(Truly(IsMainFrame))))
      .WillOnce(VerifyChildWebFrame(web_state()));

  LoadHtml(@"<p><iframe/></p>", GURL("https://testurl2"));
}

}  // namespace web
