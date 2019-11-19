// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ssl/ios_ssl_error_tab_helper.h"

#include "ios/chrome/browser/interstitials/ios_security_interstitial_page.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class TestInterstitialPage : public IOSSecurityInterstitialPage {
 public:
  // |*destroyed_tracker| is set to true in the destructor.
  TestInterstitialPage(web::WebState* web_state,
                       const GURL& request_url,
                       bool* destroyed_tracker)
      : IOSSecurityInterstitialPage(web_state, request_url),
        destroyed_tracker_(destroyed_tracker) {}

  ~TestInterstitialPage() override { *destroyed_tracker_ = true; }

 protected:
  bool ShouldCreateNewNavigation() const override { return false; }

  void PopulateInterstitialStrings(
      base::DictionaryValue* load_time_data) const override {}

  void AfterShow() override {}

 private:
  bool* destroyed_tracker_;
};

class IOSSSLErrorTabHelperTest : public web::WebTestWithWebState {
 protected:
  void SetUp() override {
    web::WebTestWithWebState::SetUp();
    IOSSSLErrorTabHelper::CreateForWebState(web_state());
  }

  std::unique_ptr<web::NavigationContext> CreateContext(bool committed,
                                                        bool is_same_document) {
    std::unique_ptr<web::FakeNavigationContext> context =
        std::make_unique<web::FakeNavigationContext>();
    context->SetHasCommitted(committed);
    context->SetIsSameDocument(is_same_document);
    return context;
  }

  // The lifetime of the blocking page is managed by the
  // IOSSSLErrorTabHelper for the test's web_state.
  // |destroyed_tracker| will be set to true when the corresponding blocking
  // page is destroyed.
  void CreateAssociatedBlockingPage(web::NavigationContext* context,
                                    bool* destroyed_tracker) {
    IOSSSLErrorTabHelper::AssociateBlockingPage(
        web_state(), context->GetNavigationId(),
        std::make_unique<TestInterstitialPage>(web_state(), GURL(),
                                               destroyed_tracker));
  }
};

// Tests that the helper properly handles the lifetime of a single blocking
// page, interleaved with other navigations.
TEST_F(IOSSSLErrorTabHelperTest, SingleBlockingPage) {
  std::unique_ptr<web::NavigationContext> blocking_page_context =
      CreateContext(true, false);
  bool blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(blocking_page_context.get(),
                               &blocking_page_destroyed);
  IOSSSLErrorTabHelper* helper =
      IOSSSLErrorTabHelper::FromWebState(web_state());

  // Test that a same-document navigation doesn't destroy the blocking page if
  // its navigation hasn't committed yet.
  std::unique_ptr<web::NavigationContext> same_document_context =
      CreateContext(true, true);
  helper->DidFinishNavigation(web_state(), same_document_context.get());
  EXPECT_FALSE(blocking_page_destroyed);

  // Test that a committed (non-same-document) navigation doesn't destroy the
  // blocking page if its navigation hasn't committed yet.
  std::unique_ptr<web::NavigationContext> committed_context1 =
      CreateContext(true, false);
  helper->DidFinishNavigation(web_state(), committed_context1.get());
  EXPECT_FALSE(blocking_page_destroyed);

  // Simulate committing the interstitial.
  helper->DidFinishNavigation(web_state(), blocking_page_context.get());
  EXPECT_FALSE(blocking_page_destroyed);

  // Test that a subsequent committed navigation releases the blocking page
  // stored for the currently committed navigation.
  std::unique_ptr<web::NavigationContext> committed_context2 =
      CreateContext(true, false);
  helper->DidFinishNavigation(web_state(), committed_context2.get());
  EXPECT_TRUE(blocking_page_destroyed);
}

// Tests that the helper properly handles the lifetime of multiple blocking
// pages, committed in a different order than they are created.
TEST_F(IOSSSLErrorTabHelperTest, MultipleBlockingPage) {
  // Simulate associating the first interstitial.
  std::unique_ptr<web::NavigationContext> context1 = CreateContext(true, false);
  bool blocking_page1_destroyed = false;
  CreateAssociatedBlockingPage(context1.get(), &blocking_page1_destroyed);

  // We can directly retrieve the helper for testing once
  // CreateAssociatedBlockingPage() was called.
  IOSSSLErrorTabHelper* helper =
      IOSSSLErrorTabHelper::FromWebState(web_state());

  // Simulate commiting the first interstitial.
  helper->DidFinishNavigation(web_state(), context1.get());
  EXPECT_FALSE(blocking_page1_destroyed);

  // Associate the second interstitial.
  std::unique_ptr<web::NavigationContext> context2 = CreateContext(true, false);
  bool blocking_page2_destroyed = false;
  CreateAssociatedBlockingPage(context2.get(), &blocking_page2_destroyed);
  EXPECT_FALSE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);

  // Associate the third interstitial.
  std::unique_ptr<web::NavigationContext> context3 = CreateContext(true, false);
  bool blocking_page3_destroyed = false;
  CreateAssociatedBlockingPage(context3.get(), &blocking_page3_destroyed);
  EXPECT_FALSE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);
  EXPECT_FALSE(blocking_page3_destroyed);

  // Simulate committing the third interstitial.
  helper->DidFinishNavigation(web_state(), context3.get());
  EXPECT_TRUE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);
  EXPECT_FALSE(blocking_page3_destroyed);

  // Simulate committing the second interstitial.
  helper->DidFinishNavigation(web_state(), context2.get());
  EXPECT_TRUE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);
  EXPECT_TRUE(blocking_page3_destroyed);

  // Test that a subsequent committed navigation releases the last blocking
  // page.
  std::unique_ptr<web::NavigationContext> committed_context4 =
      CreateContext(true, false);
  helper->DidFinishNavigation(web_state(), committed_context4.get());
  EXPECT_TRUE(blocking_page2_destroyed);
}

// Tests that the helper properly handles a navigation that finishes without
// committing.
TEST_F(IOSSSLErrorTabHelperTest, NavigationDoesNotCommit) {
  std::unique_ptr<web::NavigationContext> committed_context =
      CreateContext(true, false);
  bool committed_blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(committed_context.get(),
                               &committed_blocking_page_destroyed);
  IOSSSLErrorTabHelper* helper =
      IOSSSLErrorTabHelper::FromWebState(web_state());
  helper->DidFinishNavigation(web_state(), committed_context.get());
  EXPECT_FALSE(committed_blocking_page_destroyed);

  // Simulate a navigation that does not commit.
  std::unique_ptr<web::NavigationContext> non_committed_context =
      CreateContext(false, false);
  bool non_committed_blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(non_committed_context.get(),
                               &non_committed_blocking_page_destroyed);
  helper->DidFinishNavigation(web_state(), non_committed_context.get());

  // The blocking page for the non-committed navigation should have been cleaned
  // up, but the one for the previous committed navigation should still be
  // around.
  EXPECT_TRUE(non_committed_blocking_page_destroyed);
  EXPECT_FALSE(committed_blocking_page_destroyed);

  // When a navigation does commit, the previous one should be cleaned up.
  std::unique_ptr<web::NavigationContext> next_committed_context =
      CreateContext(true, false);
  helper->DidFinishNavigation(web_state(), next_committed_context.get());
  EXPECT_TRUE(committed_blocking_page_destroyed);
}
