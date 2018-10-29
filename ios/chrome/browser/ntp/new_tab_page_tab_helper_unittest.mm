// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/new_tab_page_tab_helper.h"

#include <memory>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/favicon/ios_chrome_large_icon_service_factory.h"
#include "ios/chrome/browser/ntp/new_tab_page_tab_helper_delegate.h"
#include "ios/chrome/browser/ntp_snippets/ios_chrome_content_suggestions_service_factory.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kTestURL[] = "http://foo.bar";
}  // namespace

// Test fixture for testing NewTabPageTabHelper class.
class NewTabPageTabHelperTest : public PlatformTest {
 protected:
  NewTabPageTabHelperTest()
      : scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(base::FilePath())) {
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeContentSuggestionsServiceFactory::GetInstance(),
        IOSChromeContentSuggestionsServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        IOSChromeLargeIconServiceFactory::GetInstance(),
        IOSChromeLargeIconServiceFactory::GetDefaultFactory());

    chrome_browser_state_ = test_cbs_builder.Build();

    auto test_navigation_manager =
        std::make_unique<web::TestNavigationManager>();
    test_navigation_manager_ = test_navigation_manager.get();
    pending_item_ = web::NavigationItem::Create();
    test_navigation_manager->SetPendingItem(pending_item_.get());
    test_web_state_.SetNavigationManager(std::move(test_navigation_manager));
    test_web_state_.SetBrowserState(chrome_browser_state_.get());

    delegate_ = OCMProtocolMock(@protocol(NewTabPageTabHelperDelegate));
  }

  NewTabPageTabHelper* tab_helper() {
    return NewTabPageTabHelper::FromWebState(&test_web_state_);
  }

  void CreateTabHelper() {
    NewTabPageTabHelper::CreateForWebState(&test_web_state_, delegate_);
  }

  id delegate_;
  web::TestWebThreadBundle thread_bundle_;
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  std::unique_ptr<WebStateList> web_state_list_;
  FakeWebStateListDelegate web_state_list_delegate_;
  std::unique_ptr<web::NavigationItem> pending_item_;
  web::TestNavigationManager* test_navigation_manager_;
  web::TestWebState test_web_state_;
};

// Tests a newly created NTP webstate.
TEST_F(NewTabPageTabHelperTest, TestAlreadyNTP) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(kBrowserContainerContainsNTP);

  GURL url(kChromeUINewTabURL);
  test_web_state_.SetVisibleURL(url);
  CreateTabHelper();
  EXPECT_TRUE(tab_helper()->IsActive());
}

// Tests a newly created non-NTP webstate.
TEST_F(NewTabPageTabHelperTest, TestNotNTP) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(kBrowserContainerContainsNTP);

  GURL url(kTestURL);
  test_web_state_.SetVisibleURL(url);
  CreateTabHelper();
  EXPECT_FALSE(tab_helper()->IsActive());
}

// Tests navigating back and forth between an NTP and non-NTP page.
TEST_F(NewTabPageTabHelperTest, TestToggleToAndFromNTP) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(kBrowserContainerContainsNTP);

  CreateTabHelper();
  EXPECT_FALSE(tab_helper()->IsActive());

  GURL url(kChromeUINewTabURL);
  web::FakeNavigationContext context;
  context.SetUrl(url);
  test_web_state_.OnNavigationFinished(&context);
  EXPECT_TRUE(tab_helper()->IsActive());

  GURL not_ntp_url(kTestURL);
  context.SetUrl(not_ntp_url);
  test_web_state_.OnNavigationFinished(&context);
  EXPECT_FALSE(tab_helper()->IsActive());

  context.SetUrl(url);
  test_web_state_.OnNavigationFinished(&context);
  EXPECT_TRUE(tab_helper()->IsActive());

  context.SetUrl(not_ntp_url);
  test_web_state_.OnNavigationFinished(&context);
  EXPECT_FALSE(tab_helper()->IsActive());
}
