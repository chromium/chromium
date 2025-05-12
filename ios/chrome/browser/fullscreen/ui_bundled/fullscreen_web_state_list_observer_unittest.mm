// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_web_state_list_observer.h"

#import <memory>

#import "base/check_deref.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_model.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/fullscreen_model_test_util.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_controller.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/test/test_fullscreen_mediator.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/security/ssl_status.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/ui/crw_web_view_proxy.h"
#import "ios/web/public/ui/crw_web_view_scroll_view_proxy.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace {
// A FakeWebState subclass that returns mock objects for the view proxies.
class FakeWebStateWithProxy : public web::FakeWebState {
 public:
  FakeWebStateWithProxy() {
    scroll_view_proxy_ = [[CRWWebViewScrollViewProxy alloc] init];
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
  FullscreenWebStateListObserverTest() {
    profile_ = TestProfileIOS::Builder().Build();
    browser_ = std::make_unique<TestBrowser>(profile_.get());
    TestFullscreenController::CreateForBrowser(browser_.get());
    TestFullscreenController* controller =
        TestFullscreenController::FromBrowser(browser_.get());
    mediator_ = std::make_unique<TestFullscreenMediator>(
        controller, controller->getModel());
    observer_ = std::make_unique<FullscreenWebStateListObserver>(
        controller, controller->getModel(), mediator_.get());
    observer_->SetWebStateList(browser_->GetWebStateList());
  }

  ~FullscreenWebStateListObserverTest() override {
    mediator_->Disconnect();
    observer_->Disconnect();
  }

  FullscreenModel* model() {
    return TestFullscreenController::FromBrowser(browser_.get())->getModel();
  }
  WebStateList& web_state_list() {
    return CHECK_DEREF(browser_->GetWebStateList());
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<TestProfileIOS> profile_;
  std::unique_ptr<TestBrowser> browser_;
  std::unique_ptr<TestFullscreenMediator> mediator_;
  std::unique_ptr<FullscreenWebStateListObserver> observer_;
};

TEST_F(FullscreenWebStateListObserverTest, ObserveActiveWebState) {
  // Insert a WebState into the list.  The observer should create a
  // FullscreenWebStateObserver for the newly activated WebState.
  auto inserted_web_state = std::make_unique<FakeWebStateWithProxy>();
  FakeWebStateWithProxy* web_state = inserted_web_state.get();
  auto passed_navigation_manager =
      std::make_unique<web::FakeNavigationManager>();
  web::FakeNavigationManager* navigation_manager =
      passed_navigation_manager.get();
  web_state->SetNavigationManager(std::move(passed_navigation_manager));
  web_state_list().InsertWebState(
      std::move(inserted_web_state),
      WebStateList::InsertionParams::Automatic().Activate());
  // Simulate a scroll to 0.5 progress.
  SetUpFullscreenModelForTesting(model(), 100.0);
  SimulateFullscreenUserScrollForProgress(model(), 0.5);
  EXPECT_EQ(model()->progress(), 0.5);
  // Simulate a navigation.  The model should be reset by the observers.
  std::unique_ptr<web::NavigationItem> committed_item =
      web::NavigationItem::Create();
  navigation_manager->SetLastCommittedItem(committed_item.get());
  web::FakeNavigationContext context;
  web_state->OnNavigationFinished(&context);
  EXPECT_FALSE(model()->has_base_offset());
  EXPECT_EQ(model()->progress(), 1.0);
}
