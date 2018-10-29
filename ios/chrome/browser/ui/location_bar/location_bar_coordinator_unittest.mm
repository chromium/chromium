// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/location_bar/location_bar_coordinator.h"

#include <memory>
#include <string>
#include <vector>

#include "components/omnibox/browser/test_toolbar_model.h"
#include "components/variations/variations_http_header_provider.h"
#include "ios/chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/search_engines/template_url_service_factory.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_coordinator_delegate.h"
#include "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/chrome/test/fakes/fake_url_loader.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using variations::VariationsHttpHeaderProvider;

@interface TestToolbarCoordinatorDelegate : NSObject<ToolbarCoordinatorDelegate>

@end

@implementation TestToolbarCoordinatorDelegate {
  std::unique_ptr<ToolbarModel> _model;
}

- (void)locationBarDidBecomeFirstResponder {
}
- (void)locationBarDidResignFirstResponder {
}
- (void)locationBarBeganEdit {
}

- (ToolbarModel*)toolbarModel {
  if (!_model) {
    _model = std::make_unique<TestToolbarModel>();
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
    browser_state_ = test_cbs_builder.Build();

    auto web_state = std::make_unique<web::TestWebState>();
    web_state->SetBrowserState(browser_state_.get());
    web_state->SetCurrentURL(GURL("http://test/"));
    web_state_list_.InsertWebState(0, std::move(web_state),
                                   WebStateList::INSERT_FORCE_INDEX,
                                   WebStateOpener());

    delegate_ = [[TestToolbarCoordinatorDelegate alloc] init];
    url_loader_ = [[FakeURLLoader alloc] init];

    coordinator_ = [[LocationBarCoordinator alloc] init];
    coordinator_.browserState = browser_state_.get();
    coordinator_.webStateList = &web_state_list_;
    coordinator_.delegate = delegate_;
    coordinator_.commandDispatcher = [[CommandDispatcher alloc] init];
    coordinator_.URLLoader = url_loader_;
  }

  void TearDown() override {
    // Started coordinator has to be stopped before WebStateList destruction.
    [coordinator_ stop];

    VariationsHttpHeaderProvider::GetInstance()->ResetForTesting();

    PlatformTest::TearDown();
  }

  web::TestWebThreadBundle web_thread_bundle_;
  LocationBarCoordinator* coordinator_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  TestToolbarCoordinatorDelegate* delegate_;
  FakeURLLoader* url_loader_;
};

TEST_F(LocationBarCoordinatorTest, Stops) {
  EXPECT_TRUE(coordinator_.view == nil);
  [coordinator_ start];
  EXPECT_TRUE(coordinator_.view != nil);
  [coordinator_ stop];
  EXPECT_TRUE(coordinator_.view == nil);
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
  [coordinator_ start];
  [coordinator_ loadGURLFromLocationBar:url transition:transition];

  EXPECT_EQ(url, url_loader_.url);
  EXPECT_TRUE(url_loader_.referrer.url.is_empty());
  EXPECT_EQ(web::ReferrerPolicyDefault, url_loader_.referrer.policy);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(transition, url_loader_.transition));
  EXPECT_FALSE(url_loader_.rendererInitiated);
  ASSERT_EQ(1U, url_loader_.extraHeaders.count);
  EXPECT_GT([url_loader_.extraHeaders[@"X-Client-Data"] length], 0U);
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
  [coordinator_ start];
  [coordinator_ loadGURLFromLocationBar:url transition:transition];

  EXPECT_EQ(url, url_loader_.url);
  EXPECT_TRUE(url_loader_.referrer.url.is_empty());
  EXPECT_EQ(web::ReferrerPolicyDefault, url_loader_.referrer.policy);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(transition, url_loader_.transition));
  EXPECT_FALSE(url_loader_.rendererInitiated);
  ASSERT_EQ(0U, url_loader_.extraHeaders.count);
}

}  // namespace
