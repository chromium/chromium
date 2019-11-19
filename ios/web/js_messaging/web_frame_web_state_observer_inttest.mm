// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/test/web_test_with_web_state.h"

#include "base/ios/ios_util.h"
#include "base/test/scoped_feature_list.h"
#include "ios/web/common/features.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#include "ios/web/public/web_state_observer.h"
#include "testing/gmock/include/gmock/gmock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::Truly;

namespace {

// WebFrameWebStateObserverInttest is parameterized on this enum to test both
// LegacyNavigationManager and WKBasedNavigationManager.
enum class NavigationManagerChoice {
  LEGACY,
  WK_BASED,
};

// Mocks WebStateObserver navigation callbacks.
class WebStateObserverMock : public web::WebStateObserver {
 public:
  WebStateObserverMock() = default;

  MOCK_METHOD2(WebFrameDidBecomeAvailable,
               void(web::WebState*, web::WebFrame*));
  MOCK_METHOD2(WebFrameWillBecomeUnavailable,
               void(web::WebState*, web::WebFrame*));
  void WebStateDestroyed(web::WebState* web_state) override { NOTREACHED(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebStateObserverMock);
};

// A predicate that returns true if |frame| is a main frame.
bool IsMainFrame(web::WebFrame* frame) {
  return frame->IsMainFrame();
}

// Verifies that the web frame passed to the observer is the main frame.
ACTION_P(VerifyMainWebFrame, web_state) {
  EXPECT_EQ(web_state, arg0);
  EXPECT_EQ(web_state->GetWebFramesManager()->GetMainWebFrame(), arg1);
}

// Verifies that the web frame passed to the observer is a child frame.
ACTION_P(VerifyChildWebFrame, web_state) {
  EXPECT_EQ(web_state, arg0);

  web::WebFramesManager* manager = web_state->GetWebFramesManager();
  auto frames = manager->GetAllWebFrames();
  EXPECT_TRUE(frames.end() != std::find(frames.begin(), frames.end(), arg1));
  EXPECT_NE(manager->GetMainWebFrame(), arg1);
}
}

namespace web {

class WebFrameWebStateObserverInttest
    : public WebTestWithWebState,
      public ::testing::WithParamInterface<NavigationManagerChoice> {
 protected:
  void SetUp() override {
    if (GetParam() == NavigationManagerChoice::LEGACY) {
      scoped_feature_list_.InitAndDisableFeature(
          web::features::kSlimNavigationManager);
    } else {
      scoped_feature_list_.InitAndEnableFeature(
          web::features::kSlimNavigationManager);
    }

    WebTestWithWebState::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Web frame events should be registered on HTTP navigation.
TEST_P(WebFrameWebStateObserverInttest, SingleWebFrameHTTP) {
  testing::StrictMock<WebStateObserverMock> observer;
  web_state()->AddObserver(&observer);
  EXPECT_CALL(observer, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));
  LoadHtml(@"<p></p>", GURL("http://testurl1"));
  EXPECT_CALL(observer, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));
  EXPECT_CALL(observer, WebFrameWillBecomeUnavailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));
  LoadHtml(@"<p></p>", GURL("http://testurl2"));
  web_state()->RemoveObserver(&observer);
}

// Web frame events should be registered on HTTPS navigation.
TEST_P(WebFrameWebStateObserverInttest, SingleWebFrameHTTPS) {
  testing::StrictMock<WebStateObserverMock> observer;
  web_state()->AddObserver(&observer);
  EXPECT_CALL(observer, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));
  LoadHtml(@"<p></p>", GURL("https://testurl1"));
  EXPECT_CALL(observer, WebFrameDidBecomeAvailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));
  EXPECT_CALL(observer, WebFrameWillBecomeUnavailable(web_state(), testing::_))
      .WillOnce(VerifyMainWebFrame(web_state()));
  LoadHtml(@"<p></p>", GURL("https://testurl2"));
  web_state()->RemoveObserver(&observer);
}

// Web frame event should be registered on HTTPS navigation with iframe.
TEST_P(WebFrameWebStateObserverInttest, TwoWebFrameHTTPS) {
  testing::StrictMock<WebStateObserverMock> observer;
  web_state()->AddObserver(&observer);

  // The order in which the main and child frames become available is not
  // guaranteed due to the async nature of messaging. The following expectations
  // use separate matchers to identify main and child frames so that they can
  // be matched in any order.
  EXPECT_CALL(observer,
              WebFrameDidBecomeAvailable(web_state(), Truly(IsMainFrame)))
      .WillOnce(VerifyMainWebFrame(web_state()));
  EXPECT_CALL(observer,
              WebFrameDidBecomeAvailable(web_state(), Not(Truly(IsMainFrame))))
      .WillOnce(VerifyChildWebFrame(web_state()));
  LoadHtml(@"<p><iframe/></p>", GURL("https://testurl1"));

  EXPECT_CALL(observer,
              WebFrameDidBecomeAvailable(web_state(), Truly(IsMainFrame)))
      .WillOnce(VerifyMainWebFrame(web_state()));
  EXPECT_CALL(observer,
              WebFrameDidBecomeAvailable(web_state(), Not(Truly(IsMainFrame))))
      .WillOnce(VerifyChildWebFrame(web_state()));
  EXPECT_CALL(observer,
              WebFrameWillBecomeUnavailable(web_state(), Truly(IsMainFrame)))
      .WillOnce(VerifyMainWebFrame(web_state()));
  EXPECT_CALL(observer, WebFrameWillBecomeUnavailable(web_state(),
                                                      Not(Truly(IsMainFrame))))
      .WillOnce(VerifyChildWebFrame(web_state()));
  LoadHtml(@"<p><iframe/></p>", GURL("https://testurl2"));

  web_state()->RemoveObserver(&observer);
}

INSTANTIATE_TEST_SUITE_P(ProgrammaticWebFrameWebStateObserverInttest,
                         WebFrameWebStateObserverInttest,
                         ::testing::Values(NavigationManagerChoice::LEGACY,
                                           NavigationManagerChoice::WK_BASED));

}  // namespace web
