// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/ios_blocking_page_tab_helper.h"

#import "base/memory/raw_ptr.h"
#import "ios/components/security_interstitials/ios_security_interstitial_page.h"
#import "ios/web/public/test/fakes/fake_navigation_context.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/platform_test.h"
#import "url/gurl.h"

namespace security_interstitials {

class TestInterstitialPage : public IOSSecurityInterstitialPage {
 public:
  // `*destroyed_tracker` is set to true in the destructor.
  TestInterstitialPage(web::WebState* web_state,
                       const GURL& request_url,
                       bool* destroyed_tracker)
      : IOSSecurityInterstitialPage(web_state,
                                    request_url,
                                    /*argument_name=*/nullptr),
        destroyed_tracker_(destroyed_tracker) {}

  ~TestInterstitialPage() override {
    if (destroyed_tracker_)
      *destroyed_tracker_ = true;
  }

 private:
  void HandleCommand(SecurityInterstitialCommand command) override {}
  bool ShouldCreateNewNavigation() const override { return false; }
  void PopulateInterstitialStrings(
      base::Value::Dict& load_time_data) const override {}

  raw_ptr<bool> destroyed_tracker_ = nullptr;
};

class IOSBlockingPageTabHelperTest : public PlatformTest {
 protected:
  IOSBlockingPageTabHelperTest() {
    IOSBlockingPageTabHelper::CreateForWebState(&web_state_);
  }

  std::unique_ptr<web::NavigationContext> CreateContext(bool committed,
                                                        bool is_same_document) {
    std::unique_ptr<web::FakeNavigationContext> context =
        std::make_unique<web::FakeNavigationContext>();
    context->SetHasCommitted(committed);
    context->SetIsSameDocument(is_same_document);
    return context;
  }

  IOSBlockingPageTabHelper* helper() {
    return IOSBlockingPageTabHelper::FromWebState(&web_state_);
  }

  // Creates a blocking page and associates it with `context`'s navigation ID
  // in the tab helper.  Returns the created blocking page.  `destroyed_tracker`
  // is an out-parameter that is reset to true when the blocking page is
  // destroyed.
  IOSSecurityInterstitialPage* CreateAssociatedBlockingPage(
      web::NavigationContext* context,
      bool* destroyed_tracker) {
    std::unique_ptr<IOSSecurityInterstitialPage> passed_blocking_page =
        std::make_unique<TestInterstitialPage>(&web_state_, GURL(),
                                               destroyed_tracker);
    IOSSecurityInterstitialPage* blocking_page = passed_blocking_page.get();
    helper()->AssociateBlockingPage(context->GetNavigationId(),
                                    std::move(passed_blocking_page));
    return blocking_page;
  }

  web::FakeWebState web_state_;
};

// Tests that the helper properly handles the lifetime of a single blocking
// page, interleaved with other navigations.
TEST_F(IOSBlockingPageTabHelperTest, SingleBlockingPage) {
  std::unique_ptr<web::NavigationContext> blocking_page_context =
      CreateContext(/*committed=*/true, /*is_same_document=*/false);
  bool blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(blocking_page_context.get(),
                               &blocking_page_destroyed);

  // Test that a same-document navigation doesn't destroy the blocking page if
  // its navigation hasn't committed yet.
  std::unique_ptr<web::NavigationContext> same_document_context =
      CreateContext(/*committed=*/true, /*is_same_document=*/true);
  web_state_.OnNavigationFinished(same_document_context.get());
  EXPECT_FALSE(blocking_page_destroyed);

  // Test that a committed (non-same-document) navigation doesn't destroy the
  // blocking page if its navigation hasn't committed yet.
  std::unique_ptr<web::NavigationContext> committed_context1 =
      CreateContext(/*committed=*/true, /*is_same_document=*/false);
  web_state_.OnNavigationFinished(committed_context1.get());
  EXPECT_FALSE(blocking_page_destroyed);

  // Simulate committing the interstitial.
  web_state_.OnNavigationFinished(blocking_page_context.get());
  EXPECT_FALSE(blocking_page_destroyed);

  // Test that a subsequent committed navigation releases the blocking page
  // stored for the currently committed navigation.
  std::unique_ptr<web::NavigationContext> committed_context2 =
      CreateContext(/*committed=*/true, /*is_same_document=*/false);
  web_state_.OnNavigationFinished(committed_context2.get());
  EXPECT_TRUE(blocking_page_destroyed);
}

// Tests that the helper properly handles the lifetime of multiple blocking
// pages, committed in a different order than they are created.
TEST_F(IOSBlockingPageTabHelperTest, MultipleBlockingPage) {
  // Simulate associating the first interstitial.
  std::unique_ptr<web::NavigationContext> context1 =
      CreateContext(/*committed=*/true, /*is_same_document=*/false);
  bool blocking_page1_destroyed = false;
  CreateAssociatedBlockingPage(context1.get(), &blocking_page1_destroyed);

  // Simulate commiting the first interstitial.
  web_state_.OnNavigationFinished(context1.get());
  EXPECT_FALSE(blocking_page1_destroyed);

  // Associate the second interstitial.
  std::unique_ptr<web::NavigationContext> context2 =
      CreateContext(/*committed=*/true, /*is_same_document=*/false);
  bool blocking_page2_destroyed = false;
  CreateAssociatedBlockingPage(context2.get(), &blocking_page2_destroyed);
  EXPECT_FALSE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);

  // Associate the third interstitial.
  std::unique_ptr<web::NavigationContext> context3 =
      CreateContext(/*committed=*/true, /*is_same_document=*/false);
  bool blocking_page3_destroyed = false;
  CreateAssociatedBlockingPage(context3.get(), &blocking_page3_destroyed);
  EXPECT_FALSE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);
  EXPECT_FALSE(blocking_page3_destroyed);

  // Simulate committing the third interstitial.
  web_state_.OnNavigationFinished(context3.get());
  EXPECT_TRUE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);
  EXPECT_FALSE(blocking_page3_destroyed);

  // Simulate committing the second interstitial.
  web_state_.OnNavigationFinished(context2.get());
  EXPECT_TRUE(blocking_page1_destroyed);
  EXPECT_FALSE(blocking_page2_destroyed);
  EXPECT_TRUE(blocking_page3_destroyed);

  // Test that a subsequent committed navigation releases the last blocking
  // page.
  std::unique_ptr<web::NavigationContext> committed_context4 =
      CreateContext(/*committed=*/true, /*is_same_document=*/false);
  web_state_.OnNavigationFinished(committed_context4.get());
  EXPECT_TRUE(blocking_page2_destroyed);
}

// Tests that the helper properly handles a navigation that finishes without
// committing.
TEST_F(IOSBlockingPageTabHelperTest, NavigationDoesNotCommit) {
  std::unique_ptr<web::NavigationContext> committed_context =
      CreateContext(/*committed=*/true, /*is_same_document=*/false);
  bool committed_blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(committed_context.get(),
                               &committed_blocking_page_destroyed);
  web_state_.OnNavigationFinished(committed_context.get());
  EXPECT_FALSE(committed_blocking_page_destroyed);

  // Simulate a navigation that does not commit.
  std::unique_ptr<web::NavigationContext> non_committed_context =
      CreateContext(/*committed=*/false, /*is_same_document=*/false);
  bool non_committed_blocking_page_destroyed = false;
  CreateAssociatedBlockingPage(non_committed_context.get(),
                               &non_committed_blocking_page_destroyed);
  web_state_.OnNavigationFinished(non_committed_context.get());

  // The blocking page for the non-committed navigation should have been cleaned
  // up, but the one for the previous committed navigation should still be
  // around.
  EXPECT_TRUE(non_committed_blocking_page_destroyed);
  EXPECT_FALSE(committed_blocking_page_destroyed);

  // When a navigation does commit, the previous one should be cleaned up.
  std::unique_ptr<web::NavigationContext> next_committed_context =
      CreateContext(/*committed=*/true, /*is_same_document=*/false);
  web_state_.OnNavigationFinished(next_committed_context.get());
  EXPECT_TRUE(committed_blocking_page_destroyed);
}

// Tests that a blocking page that is associated with a navigation ID after the
// navigation is committed is correctly used as the current blocking page for
// the last commited navigation ID.
TEST_F(IOSBlockingPageTabHelperTest, BlockingPageAssociatedAfterCommit) {
  // Commit the navigation, then associate the blocking page.
  std::unique_ptr<web::NavigationContext> context =
      CreateContext(/*committed=*/true, /*is_same_document=*/false);
  web_state_.OnNavigationFinished(context.get());
  IOSSecurityInterstitialPage* page =
      CreateAssociatedBlockingPage(context.get(), nullptr);

  // Verify that the blocking page is used as the current page.
  EXPECT_EQ(page, helper()->GetCurrentBlockingPage());
}

}  // namespace security_interstitials
