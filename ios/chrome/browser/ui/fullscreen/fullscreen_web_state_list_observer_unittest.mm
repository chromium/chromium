// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/fullscreen/fullscreen_web_state_list_observer.h"

#include <memory>

#import "ios/chrome/browser/ui/fullscreen/fullscreen_model.h"
#import "ios/chrome/browser/ui/fullscreen/test/fullscreen_model_test_util.h"
#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/ui/fullscreen/test/test_fullscreen_mediator.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/security/ssl_status.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// A TestWebState subclass that returns mock objects for the view proxies.
class TestWebStateWithProxy : public web::TestWebState {
 public:
  TestWebStateWithProxy() {
    scroll_view_proxy_ = OCMClassMock([CRWWebViewScrollViewProxy class]);
    id web_view_proxy_mock = OCMProtocolMock(@protocol(CRWWebViewProxy));
    [[[web_view_proxy_mock stub] andReturn:scroll_view_proxy_] scrollViewProxy];
    web_view_proxy_ = web_view_proxy_mock;
  }
  id<CRWWebViewProxy> GetWebViewProxy() const override {
    return web_view_proxy_;
  }

 private:
  // The mocked proxy objects.
  __strong CRWWebViewScrollViewProxy* scroll_view_proxy_;
  __strong id<CRWWebViewProxy> web_view_proxy_;
};
}  // namespace

class FullscreenWebStateListObserverTest : public PlatformTest {
 public:
  FullscreenWebStateListObserverTest()
      : PlatformTest(),
        controller_(&model_),
        mediator_(&controller_, &model_),
        web_state_list_(&web_state_list_delegate_),
        observer_(&controller_, &model_, &mediator_) {
    observer_.SetWebStateList(&web_state_list_);
  }

  ~FullscreenWebStateListObserverTest() override {
    mediator_.Disconnect();
    observer_.Disconnect();
  }

  FullscreenModel& model() { return model_; }
  WebStateList& web_state_list() { return web_state_list_; }

 private:
  FullscreenModel model_;
  TestFullscreenController controller_;
  TestFullscreenMediator mediator_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  FullscreenWebStateListObserver observer_;
};

TEST_F(FullscreenWebStateListObserverTest, ObserveActiveWebState) {
  // Insert a WebState into the list.  The observer should create a
  // FullscreenWebStateObserver for the newly activated WebState.
  std::unique_ptr<TestWebStateWithProxy> inserted_web_state =
      std::make_unique<TestWebStateWithProxy>();
  TestWebStateWithProxy* web_state = inserted_web_state.get();
  std::unique_ptr<web::TestNavigationManager> passed_navigation_manager =
      std::make_unique<web::TestNavigationManager>();
  web::TestNavigationManager* navigation_manager =
      passed_navigation_manager.get();
  web_state->SetNavigationManager(std::move(passed_navigation_manager));
  web_state_list().InsertWebState(0, std::move(inserted_web_state),
                                  WebStateList::INSERT_ACTIVATE,
                                  WebStateOpener());
  // Simulate a scroll to 0.5 progress.
  SetUpFullscreenModelForTesting(&model(), 100.0);
  SimulateFullscreenUserScrollForProgress(&model(), 0.5);
  EXPECT_EQ(model().progress(), 0.5);
  // Simulate a navigation.  The model should be reset by the observers.
  std::unique_ptr<web::NavigationItem> committed_item =
      web::NavigationItem::Create();
  navigation_manager->SetLastCommittedItem(committed_item.get());
  web::FakeNavigationContext context;
  web_state->OnNavigationFinished(&context);
  EXPECT_FALSE(model().has_base_offset());
  EXPECT_EQ(model().progress(), 1.0);
}
