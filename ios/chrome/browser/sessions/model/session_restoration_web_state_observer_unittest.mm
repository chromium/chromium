// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_restoration_web_state_observer.h"

#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

// A Wrapper around a id<CRWWebViewScrollViewObserver>.
@interface SessionRestorationScrollObserverWrapper : NSObject
@property(nonatomic, strong) id<CRWWebViewScrollViewObserver> value;
@end

@implementation SessionRestorationScrollObserverWrapper
@end

namespace {

// Increments count when called.
void IncrementCounter(size_t* counter) {
  ++*counter;
}

// Creates a CRWWebViewProxy that captures the first observer registered
// to its CRWWebViewScrollViewProxy and store it into `wrapper`.
id MockWebViewProxy(SessionRestorationScrollObserverWrapper* wrapper) {
  // Create a mock CRWWebViewScrollViewProxy that captures the observer
  // and store it into `wrapper.value`. It also check that the observer
  // is correctly unregistered.
  id scroll_view_proxy = OCMClassMock([CRWWebViewScrollViewProxy class]);
  OCMStub([scroll_view_proxy addObserver:[OCMArg any]])
      .andDo(^(NSInvocation* invocation) {
        __unsafe_unretained id observer = nil;
        [invocation getArgument:&observer atIndex:2];
        wrapper.value = observer;
      });
  OCMStub([scroll_view_proxy
      removeObserver:[OCMArg checkWithBlock:^BOOL(id observer) {
        if (observer != wrapper.value) {
          return NO;
        }
        wrapper.value = nil;
        return YES;
      }]]);

  // Register the fake CRWWebViewScrollViewProxy.
  id proxy = OCMProtocolMock(@protocol(CRWWebViewProxy));
  OCMStub([proxy scrollViewProxy]).andReturn(scroll_view_proxy);
  return proxy;
}

}  // anonymous namespace

class SessionRestorationWebStateObserverTest : public PlatformTest {
 public:
  void SetUp() override {
    web_state_ = std::make_unique<web::FakeWebState>();
    web_state_->SetIsRealized(true);
    web_state_->SetWebFramesManager(
        std::make_unique<web::FakeWebFramesManager>());
    web_state_->SetNavigationManager(
        std::make_unique<web::FakeNavigationManager>());

    // Create a SessionRestorationScrollObserverWrapper to hold the captured
    // scroll observer. This allows capturing the observer while mocking in
    // a safe way as the block does not need to reference `this`.
    wrapper_ = [[SessionRestorationScrollObserverWrapper alloc] init];
    web_state_->SetWebViewProxy(MockWebViewProxy(wrapper_));
  }

  void TearDown() override {
    // The observer needs to be destroyed before the WebState.
    SessionRestorationWebStateObserver::RemoveFromWebState(web_state_.get());

    // The observer needs to be unregistered on destruction.
    ASSERT_FALSE(wrapper_.value);
    wrapper_ = nil;

    PlatformTest::TearDown();
  }

  // Creates and returns the SessionRestorationWebStateObserver for `web_state`.
  SessionRestorationWebStateObserver* CreateSessionRestorationWebStateObserver(
      web::WebState* web_state,
      base::RepeatingClosure closure) {
    SessionRestorationWebStateObserver::CreateForWebState(
        web_state, base::IgnoreArgs<web::WebState*>(closure));

    return SessionRestorationWebStateObserver::FromWebState(web_state);
  }

  // Returns the fake WebState.
  web::FakeWebState* web_state() { return web_state_.get(); }

  // Returns the captured observer. Can be used to simulate scroll events.
  id<CRWWebViewScrollViewObserver> scroll_observer() { return wrapper_.value; }

  // Returns the fake WebFramesManager.
  web::FakeWebFramesManager* web_frames_manager() {
    return static_cast<web::FakeWebFramesManager*>(
        web_state_->GetPageWorldWebFramesManager());
  }

 private:
  // The fake WebState used by the tests.
  std::unique_ptr<web::FakeWebState> web_state_;

  // Captured observer.
  __strong SessionRestorationScrollObserverWrapper* wrapper_ = nil;
};

// Tests that SessionRestorationWebStateObserver consider the WebState as
// clean on creation.
TEST_F(SessionRestorationWebStateObserverTest, Creation) {
  web_state()->SetIsRealized(false);

  size_t call_count = 0;
  SessionRestorationWebStateObserver* observer =
      CreateSessionRestorationWebStateObserver(
          web_state(), base::BindRepeating(&IncrementCounter, &call_count));

  EXPECT_FALSE(observer->is_dirty());
  EXPECT_EQ(call_count, 0u);
}

// Tests that SessionRestorationWebStateObserver consider the WebState as
// clean on creation even if the WebState is already realized.
TEST_F(SessionRestorationWebStateObserverTest, Creation_Realized) {
  web_state()->SetIsRealized(true);

  size_t call_count = 0;
  SessionRestorationWebStateObserver* observer =
      CreateSessionRestorationWebStateObserver(
          web_state(), base::BindRepeating(&IncrementCounter, &call_count));

  EXPECT_FALSE(observer->is_dirty());
  EXPECT_EQ(call_count, 0u);
}

// Tests that SessionRestorationWebStateObserver does not mark WebState
// dirty upon realization.
TEST_F(SessionRestorationWebStateObserverTest, Realization) {
  web_state()->SetIsRealized(false);

  size_t call_count = 0;
  SessionRestorationWebStateObserver* observer =
      CreateSessionRestorationWebStateObserver(
          web_state(), base::BindRepeating(&IncrementCounter, &call_count));

  web_state()->ForceRealized();
  EXPECT_FALSE(observer->is_dirty());
  EXPECT_EQ(call_count, 0u);
}

// Tests that SessionRestorationWebStateObserver mark the WebState as
// dirty upon scroll events.
TEST_F(SessionRestorationWebStateObserverTest, ScrollEvent) {
  web_state()->SetIsRealized(true);

  size_t call_count = 0;
  SessionRestorationWebStateObserver* observer =
      CreateSessionRestorationWebStateObserver(
          web_state(), base::BindRepeating(&IncrementCounter, &call_count));

  [scroll_observer() webViewScrollViewDidEndDragging:nil willDecelerate:NO];
  [scroll_observer() webViewScrollViewDidEndDragging:nil willDecelerate:NO];

  EXPECT_TRUE(observer->is_dirty());
  EXPECT_EQ(call_count, 1u);
}

// Tests that SessionRestorationWebStateObserver mark the WebState as
// dirty upon navigation committed.
TEST_F(SessionRestorationWebStateObserverTest, NavigationCommitted) {
  web_state()->SetIsRealized(true);

  size_t call_count = 0;
  SessionRestorationWebStateObserver* observer =
      CreateSessionRestorationWebStateObserver(
          web_state(), base::BindRepeating(&IncrementCounter, &call_count));

  web::FakeNavigationContext context;
  context.SetIsDownload(false);
  web_state()->OnNavigationFinished(&context);
  web_state()->OnNavigationFinished(&context);

  EXPECT_TRUE(observer->is_dirty());
  EXPECT_EQ(call_count, 1u);
}

// Tests that SessionRestorationWebStateObserver consider the WebState as
// clean on navigation that are download.
TEST_F(SessionRestorationWebStateObserverTest, NavigationCommitted_Download) {
  web_state()->SetIsRealized(true);

  size_t call_count = 0;
  SessionRestorationWebStateObserver* observer =
      CreateSessionRestorationWebStateObserver(
          web_state(), base::BindRepeating(&IncrementCounter, &call_count));

  web::FakeNavigationContext context;
  context.SetIsDownload(true);
  web_state()->OnNavigationFinished(&context);
  web_state()->OnNavigationFinished(&context);

  EXPECT_FALSE(observer->is_dirty());
  EXPECT_EQ(call_count, 0u);
}

// Tests that SessionRestorationWebStateObserver mark the WebState as
// dirty when the WebState is shown (which updates the last active time).
TEST_F(SessionRestorationWebStateObserverTest, WasShown) {
  web_state()->SetIsRealized(true);

  size_t call_count = 0;
  SessionRestorationWebStateObserver* observer =
      CreateSessionRestorationWebStateObserver(
          web_state(), base::BindRepeating(&IncrementCounter, &call_count));

  web_state()->WasShown();
  web_state()->WasShown();

  EXPECT_TRUE(observer->is_dirty());
  EXPECT_EQ(call_count, 1u);
}

// Tests that SessionRestorationWebStateObserver mark the WebState as
// dirty when a WebFrame becomes available.
TEST_F(SessionRestorationWebStateObserverTest, WebFrameAvailable) {
  web_state()->SetIsRealized(true);

  size_t call_count = 0;
  SessionRestorationWebStateObserver* observer =
      CreateSessionRestorationWebStateObserver(
          web_state(), base::BindRepeating(&IncrementCounter, &call_count));

  web_frames_manager()->AddWebFrame(
      web::FakeWebFrame::CreateChildWebFrame(GURL("https://example.com/c")));

  EXPECT_TRUE(observer->is_dirty());
  EXPECT_EQ(call_count, 1u);
}

// Tests that SessionRestorationWebStateObserver consider the WebState as
// clean when main frame become available if this is the main frame.
TEST_F(SessionRestorationWebStateObserverTest, WebFrameAvailable_MainFrame) {
  web_state()->SetIsRealized(true);

  size_t call_count = 0;
  SessionRestorationWebStateObserver* observer =
      CreateSessionRestorationWebStateObserver(
          web_state(), base::BindRepeating(&IncrementCounter, &call_count));

  web_frames_manager()->AddWebFrame(
      web::FakeWebFrame::CreateMainWebFrame(GURL("https://example.com")));

  EXPECT_FALSE(observer->is_dirty());
  EXPECT_EQ(call_count, 0u);
}

// Tests that SessionRestorationWebStateObserver consider the WebState as
// clean when WebFrame become available but no navigation occurred.
TEST_F(SessionRestorationWebStateObserverTest, WebFrameAvailable_NoNavigation) {
  web_state()->SetIsRealized(true);

  size_t call_count = 0;
  SessionRestorationWebStateObserver* observer =
      CreateSessionRestorationWebStateObserver(
          web_state(), base::BindRepeating(&IncrementCounter, &call_count));

  web::FakeNavigationContext context;
  context.SetIsDownload(false);
  web_state()->OnNavigationFinished(&context);
  web_state()->OnNavigationFinished(&context);

  EXPECT_TRUE(observer->is_dirty());
  EXPECT_EQ(call_count, 1u);

  // Reset the dirty flag.
  observer->clear_dirty();
  EXPECT_FALSE(observer->is_dirty());

  web_frames_manager()->AddWebFrame(
      web::FakeWebFrame::CreateChildWebFrame(GURL("https://example.com/c")));

  EXPECT_FALSE(observer->is_dirty());
  EXPECT_EQ(call_count, 1u);
}
