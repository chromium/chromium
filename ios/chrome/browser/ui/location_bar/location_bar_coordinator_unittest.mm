// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"

#include <memory>
#include <string>
#include <vector>

#include "components/omnibox/browser/test_location_bar_model.h"
#include "components/variations/variations_http_header_provider.h"
#include "ios/chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/main/test_browser.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator_delegate.h"
#include "ios/chrome/browser/url_loading/test_url_loading_service.h"
#include "ios/chrome/browser/url_loading/url_loading_params.h"
#include "ios/chrome/browser/url_loading/url_loading_service_factory.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using variations::VariationsHttpHeaderProvider;

@interface TestToolbarCoordinatorDelegate : NSObject<ToolbarCoordinatorDelegate>

@end

@implementation TestToolbarCoordinatorDelegate {
  std::unique_ptr<LocationBarModel> _model;
}

- (void)locationBarDidBecomeFirstResponder {
}
- (void)locationBarDidResignFirstResponder {
}
- (void)locationBarBeganEdit {
}

- (LocationBarModel*)locationBarModel {
  if (!_model) {
    _model = std::make_unique<TestLocationBarModel>();
  }

  return _model.get();
}

@end

namespace {

class LocationBarCoordinatorTest : public PlatformTest {
 protected:
  LocationBarCoordinatorTest() : web_state_list_(&web_state_list_delegate_) {}

  void SetUp() override {
    PlatformTest::SetUp();

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.AddTestingFactory(
        ios::TemplateURLServiceFactory::GetInstance(),
        ios::TemplateURLServiceFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        ios::AutocompleteClassifierFactory::GetInstance(),
        ios::AutocompleteClassifierFactory::GetDefaultFactory());
    test_cbs_builder.AddTestingFactory(
        UrlLoadingServiceFactory::GetInstance(),
        UrlLoadingServiceFactory::GetDefaultFactory());
    browser_state_ = test_cbs_builder.Build();

    browser_ =
        std::make_unique<TestBrowser>(browser_state_.get(), &web_state_list_);

    auto web_state = std::make_unique<web::TestWebState>();
    web_state->SetBrowserState(browser_state_.get());
    web_state->SetCurrentURL(GURL("http://test/"));
    web_state_list_.InsertWebState(0, std::move(web_state),
                                   WebStateList::INSERT_FORCE_INDEX,
                                   WebStateOpener());

    delegate_ = [[TestToolbarCoordinatorDelegate alloc] init];

    coordinator_ = [[LocationBarCoordinator alloc] init];
    coordinator_.browser = browser_.get();
    coordinator_.delegate = delegate_;
    coordinator_.commandDispatcher = [[CommandDispatcher alloc] init];
  }

  void TearDown() override {
    // Started coordinator has to be stopped before WebStateList destruction.
    [coordinator_ stop];

    VariationsHttpHeaderProvider::GetInstance()->ResetForTesting();

    PlatformTest::TearDown();
  }

  web::WebTaskEnvironment task_environment_;
  LocationBarCoordinator* coordinator_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  std::unique_ptr<Browser> browser_;
  TestToolbarCoordinatorDelegate* delegate_;
};

TEST_F(LocationBarCoordinatorTest, Stops) {
  EXPECT_TRUE(coordinator_.locationBarViewController == nil);
  [coordinator_ start];
  EXPECT_TRUE(coordinator_.locationBarViewController != nil);
  [coordinator_ stop];
  EXPECT_TRUE(coordinator_.locationBarViewController == nil);
}

// Removes the existing WebState to ensure that nothing breaks when there is no
// active WebState.
TEST_F(LocationBarCoordinatorTest, RemoveLastWebState) {
  web_state_list_.CloseWebStateAt(0, 0);
}

// Calls -loadGURLFromLocationBar:transition: with https://www.google.com/ URL.
// Verifies that URLLoader receives correct load request, which also includes
// variations header.
TEST_F(LocationBarCoordinatorTest, LoadGoogleUrl) {
  ASSERT_EQ(VariationsHttpHeaderProvider::ForceIdsResult::SUCCESS,
            VariationsHttpHeaderProvider::GetInstance()->ForceVariationIds(
                /*variation_ids=*/{"100"}, /*command_line_variation_ids=*/""));

  GURL url("https://www.google.com/");
  ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  WindowOpenDisposition disposition = WindowOpenDisposition::SWITCH_TO_TAB;
  [coordinator_ start];
  [coordinator_ loadGURLFromLocationBar:url
                            postContent:nil
                             transition:transition
                            disposition:disposition];

  TestUrlLoadingService* url_loader =
      (TestUrlLoadingService*)UrlLoadingServiceFactory::GetForBrowserState(
          browser_state_.get());

  EXPECT_EQ(url, url_loader->last_params.web_params.url);
  EXPECT_TRUE(url_loader->last_params.web_params.referrer.url.is_empty());
  EXPECT_EQ(web::ReferrerPolicyDefault,
            url_loader->last_params.web_params.referrer.policy);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      transition, url_loader->last_params.web_params.transition_type));
  EXPECT_FALSE(url_loader->last_params.web_params.is_renderer_initiated);
  ASSERT_EQ(1U, url_loader->last_params.web_params.extra_headers.count);
  EXPECT_GT([url_loader->last_params.web_params.extra_headers[@"X-Client-Data"]
                length],
            0U);
  EXPECT_EQ(disposition, url_loader->last_params.disposition);
}

// Calls -loadGURLFromLocationBar:transition: with https://www.nongoogle.com/
// URL. Verifies that URLLoader receives correct load request without variations
// header.
TEST_F(LocationBarCoordinatorTest, LoadNonGoogleUrl) {
  ASSERT_EQ(VariationsHttpHeaderProvider::ForceIdsResult::SUCCESS,
            VariationsHttpHeaderProvider::GetInstance()->ForceVariationIds(
                /*variation_ids=*/{"100"}, /*command_line_variation_ids=*/""));

  GURL url("https://www.nongoogle.com/");
  ui::PageTransition transition = ui::PAGE_TRANSITION_TYPED;
  WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB;
  [coordinator_ start];
  [coordinator_ loadGURLFromLocationBar:url
                            postContent:nil
                             transition:transition
                            disposition:disposition];

  TestUrlLoadingService* url_loader =
      (TestUrlLoadingService*)UrlLoadingServiceFactory::GetForBrowserState(
          browser_state_.get());

  EXPECT_EQ(url, url_loader->last_params.web_params.url);
  EXPECT_TRUE(url_loader->last_params.web_params.referrer.url.is_empty());
  EXPECT_EQ(web::ReferrerPolicyDefault,
            url_loader->last_params.web_params.referrer.policy);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      transition, url_loader->last_params.web_params.transition_type));
  EXPECT_FALSE(url_loader->last_params.web_params.is_renderer_initiated);
  ASSERT_EQ(0U, url_loader->last_params.web_params.extra_headers.count);
  EXPECT_EQ(disposition, url_loader->last_params.disposition);
}

}  // namespace
