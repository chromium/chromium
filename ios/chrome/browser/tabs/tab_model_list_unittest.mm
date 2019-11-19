// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_model_list.h"

#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#import "ios/chrome/browser/main/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_list_observer.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::_;
using testing::Eq;

class MockTabModelListObserver : public TabModelListObserver {
 public:
  MockTabModelListObserver() : TabModelListObserver() {}

  MOCK_METHOD2(TabModelRegisteredWithBrowserState,
               void(TabModel* tab_model,
                    ios::ChromeBrowserState* browser_state));
  MOCK_METHOD2(TabModelUnregisteredFromBrowserState,
               void(TabModel* tab_model,
                    ios::ChromeBrowserState* browser_state));
};

class TabModelListTest : public PlatformTest {
 public:
  TabModelListTest()
      : scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(
                TestChromeBrowserState::Builder().Build())),
        web_state_list_delegate_(
            std::make_unique<BrowserWebStateListDelegate>()),
        web_state_list_(
            std::make_unique<WebStateList>(web_state_list_delegate_.get())),
        otr_web_state_list_delegate_(
            std::make_unique<BrowserWebStateListDelegate>()),
        otr_web_state_list_(
            std::make_unique<WebStateList>(web_state_list_delegate_.get())) {}

  TabModel* CreateTabModel() {
    return [[TabModel alloc]
        initWithSessionService:[[TestSessionService alloc] init]
                  browserState:browser_state()
                  webStateList:web_state_list_.get()];
  }

  TabModel* CreateOffTheRecordTabModel() {
    return [[TabModel alloc]
        initWithSessionService:[[TestSessionService alloc] init]
                  browserState:otr_browser_state()
                  webStateList:otr_web_state_list_.get()];
  }

  NSArray<TabModel*>* RegisteredTabModels() {
    return TabModelList::GetTabModelsForChromeBrowserState(browser_state());
  }

  NSArray<TabModel*>* RegisteredOffTheRecordTabModels() {
    return TabModelList::GetTabModelsForChromeBrowserState(otr_browser_state());
  }

  ios::ChromeBrowserState* browser_state() {
    return GetApplicationContext()
        ->GetChromeBrowserStateManager()
        ->GetLastUsedBrowserState();
  }

  ios::ChromeBrowserState* otr_browser_state() {
    return browser_state()->GetOffTheRecordChromeBrowserState();
  }

 private:
  web::WebTaskEnvironment task_environment_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;

  std::unique_ptr<WebStateListDelegate> web_state_list_delegate_;
  std::unique_ptr<WebStateList> web_state_list_;

  std::unique_ptr<WebStateListDelegate> otr_web_state_list_delegate_;
  std::unique_ptr<WebStateList> otr_web_state_list_;

  DISALLOW_COPY_AND_ASSIGN(TabModelListTest);
};

TEST_F(TabModelListTest, RegisterAndUnregisterTabModels) {
  EXPECT_EQ([RegisteredTabModels() count], 0u);
  EXPECT_EQ([RegisteredOffTheRecordTabModels() count], 0u);

  MockTabModelListObserver observer;
  TabModelList::AddObserver(&observer);

  EXPECT_CALL(observer,
              TabModelRegisteredWithBrowserState(_, Eq(browser_state())))
      .Times(1);

  EXPECT_CALL(observer,
              TabModelRegisteredWithBrowserState(_, Eq(otr_browser_state())))
      .Times(0);
  TabModel* tab_model = CreateTabModel();
  EXPECT_EQ([RegisteredTabModels() count], 1u);
  EXPECT_EQ([RegisteredOffTheRecordTabModels() count], 0u);
  EXPECT_NE([RegisteredTabModels() indexOfObject:tab_model],
            static_cast<NSUInteger>(NSNotFound));

  EXPECT_CALL(observer, TabModelUnregisteredFromBrowserState(
                            Eq(tab_model), Eq(browser_state())))
      .Times(1);
  EXPECT_CALL(observer, TabModelUnregisteredFromBrowserState(
                            Eq(tab_model), Eq(otr_browser_state())))
      .Times(0);
  [tab_model disconnect];
  EXPECT_EQ([RegisteredTabModels() count], 0u);

  TabModelList::RemoveObserver(&observer);
}

TEST_F(TabModelListTest, RegisterAndUnregisterOffTheRecordTabModels) {
  EXPECT_EQ([RegisteredTabModels() count], 0u);
  EXPECT_EQ([RegisteredOffTheRecordTabModels() count], 0u);

  MockTabModelListObserver observer;
  TabModelList::AddObserver(&observer);

  EXPECT_CALL(observer,
              TabModelRegisteredWithBrowserState(_, Eq(otr_browser_state())))
      .Times(1);

  EXPECT_CALL(observer,
              TabModelRegisteredWithBrowserState(_, Eq(browser_state())))
      .Times(0);
  TabModel* tab_model = CreateOffTheRecordTabModel();
  EXPECT_EQ([RegisteredTabModels() count], 0u);
  EXPECT_EQ([RegisteredOffTheRecordTabModels() count], 1u);
  EXPECT_NE([RegisteredOffTheRecordTabModels() indexOfObject:tab_model],
            static_cast<NSUInteger>(NSNotFound));

  EXPECT_CALL(observer, TabModelUnregisteredFromBrowserState(
                            Eq(tab_model), Eq(otr_browser_state())))
      .Times(1);
  EXPECT_CALL(observer, TabModelUnregisteredFromBrowserState(
                            Eq(tab_model), Eq(browser_state())))
      .Times(0);
  [tab_model disconnect];
  EXPECT_EQ([RegisteredOffTheRecordTabModels() count], 0u);

  TabModelList::RemoveObserver(&observer);
}

TEST_F(TabModelListTest, SupportsMultipleTabModels) {
  EXPECT_EQ([RegisteredTabModels() count], 0u);

  TabModel* tab_model1 = CreateTabModel();
  EXPECT_EQ([RegisteredTabModels() count], 1u);
  EXPECT_NE([RegisteredTabModels() indexOfObject:tab_model1],
            static_cast<NSUInteger>(NSNotFound));

  TabModel* tab_model2 = CreateTabModel();
  EXPECT_EQ([RegisteredTabModels() count], 2u);
  EXPECT_NE([RegisteredTabModels() indexOfObject:tab_model2],
            static_cast<NSUInteger>(NSNotFound));

  [tab_model1 disconnect];
  [tab_model2 disconnect];

  EXPECT_EQ([RegisteredTabModels() count], 0u);
}

TEST_F(TabModelListTest, SupportsMultipleOffTheRecordTabModels) {
  EXPECT_EQ([RegisteredOffTheRecordTabModels() count], 0u);

  TabModel* tab_model1 = CreateOffTheRecordTabModel();
  EXPECT_EQ([RegisteredOffTheRecordTabModels() count], 1u);
  EXPECT_NE([RegisteredOffTheRecordTabModels() indexOfObject:tab_model1],
            static_cast<NSUInteger>(NSNotFound));

  TabModel* tab_model2 = CreateOffTheRecordTabModel();
  EXPECT_EQ([RegisteredOffTheRecordTabModels() count], 2u);
  EXPECT_NE([RegisteredOffTheRecordTabModels() indexOfObject:tab_model2],
            static_cast<NSUInteger>(NSNotFound));

  [tab_model1 disconnect];
  [tab_model2 disconnect];

  EXPECT_EQ([RegisteredOffTheRecordTabModels() count], 0u);
}
