// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include <memory>

#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/tabs/legacy_tab_helper.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_controller.h"
#import "ios/chrome/browser/ui/tabs/tab_strip_view.h"
#import "ios/chrome/browser/ui/tabs/tab_view.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface TabStripControllerTestTab : NSObject

- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, copy) NSString* title;

@end

@implementation TabStripControllerTestTab {
  web::WebState* _webState;
}

@synthesize title = _title;

- (instancetype)initWithWebState:(web::WebState*)webState {
  if ((self = [super init])) {
    DCHECK(webState);
    _webState = webState;
  }
  return self;
}

- (web::WebState*)webState {
  return _webState;
}

@end

@interface TabStripControllerTestTabModel : NSObject

@property(nonatomic, assign) ios::ChromeBrowserState* browserState;

@end

@implementation TabStripControllerTestTabModel {
  FakeWebStateListDelegate _webStateListDelegate;
  std::unique_ptr<WebStateList> _webStateList;
}

@synthesize browserState = _browserState;

- (instancetype)init {
  if ((self = [super init])) {
    _webStateList = std::make_unique<WebStateList>(&_webStateListDelegate);
  }
  return self;
}

- (void)browserStateDestroyed {
  _webStateList->CloseAllWebStates(WebStateList::CLOSE_NO_FLAGS);
  _webStateList.reset();
  _browserState = nullptr;
}

- (TabStripControllerTestTab*)addTabForTestingWithTitle:(NSString*)title {
  auto testWebState = std::make_unique<web::TestWebState>();
  testWebState->SetNavigationManager(
      std::make_unique<web::TestNavigationManager>());

  TabStripControllerTestTab* tab =
      [[TabStripControllerTestTab alloc] initWithWebState:testWebState.get()];
  tab.title = title;

  LegacyTabHelper::CreateForWebStateForTesting(testWebState.get(),
                                               static_cast<Tab*>(tab));
  _webStateList->InsertWebState(0, std::move(testWebState),
                                WebStateList::INSERT_NO_FLAGS,
                                WebStateOpener());

  return tab;
}

- (Tab*)currentTab {
  web::WebState* activeWebState = _webStateList->GetActiveWebState();
  return activeWebState ? LegacyTabHelper::GetTabForWebState(activeWebState)
                        : nil;
}

- (Tab*)tabAtIndex:(NSUInteger)index {
  DCHECK(index < static_cast<NSUInteger>(INT_MAX));
  DCHECK(static_cast<int>(index) < _webStateList->count());
  return LegacyTabHelper::GetTabForWebState(
      _webStateList->GetWebStateAt(static_cast<int>(index)));
}

- (NSUInteger)indexOfTab:(Tab*)tab {
  const int index = _webStateList->GetIndexOfWebState(tab.webState);
  return index == WebStateList::kInvalidIndex ? NSNotFound
                                              : static_cast<NSUInteger>(index);
}

- (BOOL)isEmpty {
  return _webStateList->empty();
}

- (NSUInteger)count {
  return static_cast<NSUInteger>(_webStateList->count());
}

- (void)closeTabAtIndex:(NSUInteger)index {
  DCHECK(index < static_cast<NSUInteger>(INT_MAX));
  DCHECK(static_cast<int>(index) < _webStateList->count());
  _webStateList->CloseWebStateAt(static_cast<int>(index),
                                 WebStateList::CLOSE_NO_FLAGS);
}

- (void)addObserver:(id<TabModelObserver>)observer {
  // Do nothing.
}

- (void)removeObserver:(id<TabModelObserver>)observer {
  // Do nothing.
}

- (WebStateList*)webStateList {
  return _webStateList.get();
}

@end

namespace {

class TabStripControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    if (!IsIPadIdiom())
      return;

    // Need a ChromeBrowserState for the tab model.
    TestChromeBrowserState::Builder test_cbs_builder;
    chrome_browser_state_ = test_cbs_builder.Build();

    // Setup mock TabModel.
    tab_model_ = [[TabStripControllerTestTabModel alloc] init];
    tab_model_.browserState = chrome_browser_state_.get();

    // Populate the TabModel.
    tab1_ = [tab_model_ addTabForTestingWithTitle:@"Tab Title 1"];
    tab2_ = [tab_model_ addTabForTestingWithTitle:@"Tab Title 2"];

    controller_ = [[TabStripController alloc]
        initWithTabModel:static_cast<TabModel*>(tab_model_)
                   style:NORMAL
              dispatcher:nil];

    // Force the view to load.
    UIWindow* window = [[UIWindow alloc] initWithFrame:CGRectZero];
    [window addSubview:[controller_ view]];
    window_ = window;
  }

  void TearDown() override {
    if (!IsIPadIdiom())
      return;

    [tab_model_ browserStateDestroyed];
  }

  web::TestWebThreadBundle thread_bundle_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  TabStripController* controller_;
  UIWindow* window_;
  TabStripControllerTestTabModel* tab_model_;
  TabStripControllerTestTab* tab1_;
  TabStripControllerTestTab* tab2_;
};

TEST_F(TabStripControllerTest, LoadAndDisplay) {
  if (!IsIPadIdiom())
    return;

  // There should be two TabViews and one new tab button nested within the
  // parent view (which contains exactly one scroll view).
  EXPECT_EQ(3U,
            [[[[[controller_ view] subviews] objectAtIndex:0] subviews] count]);
}

}  // namespace
