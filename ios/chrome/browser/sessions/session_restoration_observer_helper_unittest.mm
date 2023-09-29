// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_restoration_observer_helper.h"

#import "base/test/scoped_feature_list.h"
#import "base/test/task_environment.h"
#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"
#import "ios/chrome/browser/sessions/test_session_restoration_observer.h"
#import "ios/chrome/browser/sessions/test_session_service.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "testing/platform_test.h"

// To get access to web::features::kEnableSessionSerializationOptimizations.
// TODO(crbug.com/1383087): remove once the feature is fully launched.
#import "ios/web/common/features.h"

using SessionRestorationObserverHelperTest = PlatformTest;

// Tests that registering an observer works with legacy session restoration.
TEST_F(SessionRestorationObserverHelperTest, ObserverRegistration_Legacy) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      web::features::kEnableSessionSerializationOptimizations);

  base::test::TaskEnvironment scoped_task_environment;
  std::unique_ptr<TestChromeBrowserState> browser_state =
      TestChromeBrowserState::Builder().Build();

  std::unique_ptr<Browser> browser = std::make_unique<TestBrowser>(
      browser_state.get(), std::make_unique<FakeWebStateListDelegate>());

  TestSessionRestorationObserver observer;
  ASSERT_FALSE(observer.IsInObserverList());
  ASSERT_FALSE(SessionRestorationBrowserAgent::FromBrowser(browser.get()));

  // Check that registering the observer when the BrowserAgent has not been
  // created still works, but does nothing.
  AddSessionRestorationObserver(browser.get(), &observer);
  EXPECT_FALSE(observer.IsInObserverList());

  RemoveSessionRestorationObserver(browser.get(), &observer);
  EXPECT_FALSE(observer.IsInObserverList());

  // Create the BrowserAgent.
  SessionRestorationBrowserAgent::CreateForBrowser(
      browser.get(), [[TestSessionService alloc] init],
      /* enable_pinned_tabs */ true);
  ASSERT_TRUE(SessionRestorationBrowserAgent::FromBrowser(browser.get()));

  // Check that registering the observer when the BrowserAgent has been
  // created works correctly.
  AddSessionRestorationObserver(browser.get(), &observer);
  EXPECT_TRUE(observer.IsInObserverList());

  RemoveSessionRestorationObserver(browser.get(), &observer);
  EXPECT_FALSE(observer.IsInObserverList());
}

// Tests that registering an observer works with optimized session restoration.
TEST_F(SessionRestorationObserverHelperTest, ObserverRegistration_Optimized) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      web::features::kEnableSessionSerializationOptimizations);

  base::test::TaskEnvironment scoped_task_environment;
  std::unique_ptr<TestChromeBrowserState> browser_state =
      TestChromeBrowserState::Builder().Build();

  std::unique_ptr<Browser> browser = std::make_unique<TestBrowser>(
      browser_state.get(), std::make_unique<FakeWebStateListDelegate>());

  TestSessionRestorationObserver observer;
  ASSERT_FALSE(observer.IsInObserverList());

  // Check that registering the observer works.
  AddSessionRestorationObserver(browser.get(), &observer);
  EXPECT_TRUE(observer.IsInObserverList());

  RemoveSessionRestorationObserver(browser.get(), &observer);
  EXPECT_FALSE(observer.IsInObserverList());
}
