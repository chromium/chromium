// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_manager_impl.h"

#include <array>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ios/web/common/features.h"
#import "ios/web/navigation/crw_session_controller+private_constructors.h"
#import "ios/web/navigation/legacy_navigation_manager_impl.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#import "ios/web/navigation/wk_based_navigation_manager_impl.h"
#import "ios/web/navigation/wk_navigation_util.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/public/test/fakes/test_navigation_manager.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#import "ios/web/public/web_client.h"
#import "ios/web/test/fakes/crw_fake_back_forward_list.h"
#import "ios/web/test/fakes/crw_fake_session_controller_delegate.h"
#include "ios/web/test/test_url_constants.h"
#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "url/scheme_host_port.h"
#include "url/url_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForPageLoadTimeout;

namespace web {
namespace {

// URL scheme that will be rewritten by UrlRewriter installed in
// NavigationManagerTest fixture. Scheme will be changed to kTestWebUIScheme.
const char kSchemeToRewrite[] = "navigationmanagerschemetorewrite";

// Replaces |kSchemeToRewrite| scheme with |kTestWebUIScheme|.
bool UrlRewriter(GURL* url, BrowserState* browser_state) {
  if (url->scheme() == kSchemeToRewrite) {
    GURL::Replacements scheme_replacements;
    scheme_replacements.SetSchemeStr(kTestWebUIScheme);
    *url = url->ReplaceComponents(scheme_replacements);
  }
  return false;
}

// Query parameter that will be appended by AppendingUrlRewriter if it is
// installed into NavigationManager by a test case.
const char kRewrittenQueryParam[] = "navigationmanagerrewrittenquery";

// Appends |kRewrittenQueryParam| to |url|.
bool AppendingUrlRewriter(GURL* url, BrowserState* browser_state) {
  GURL::Replacements query_replacements;
  query_replacements.SetQueryStr(kRewrittenQueryParam);
  *url = url->ReplaceComponents(query_replacements);
  return false;
}

// Mock class for NavigationManagerDelegate.
class MockNavigationManagerDelegate : public NavigationManagerDelegate {
 public:
  void SetSessionController(CRWSessionController* session_controller) {
    session_controller_ = session_controller;
  }

  void SetWKWebView(id web_view) { mock_web_view_ = web_view; }
  void SetWebState(WebState* web_state) { web_state_ = web_state; }

  MOCK_METHOD0(ClearTransientContent, void());
  MOCK_METHOD0(ClearDialogs, void());
  MOCK_METHOD0(RecordPageStateInNavigationItem, void());
  MOCK_METHOD2(OnGoToIndexSameDocumentNavigation,
               void(NavigationInitiationType type, bool has_user_gesture));
  MOCK_METHOD0(WillChangeUserAgentType, void());
  MOCK_METHOD1(LoadCurrentItem, void(NavigationInitiationType type));
  MOCK_METHOD0(LoadIfNecessary, void());
  MOCK_METHOD0(Reload, void());
  MOCK_METHOD1(OnNavigationItemsPruned, void(size_t));
  MOCK_METHOD1(OnNavigationItemCommitted, void(NavigationItem* item));
  MOCK_METHOD0(RemoveWebView, void());
  MOCK_METHOD4(GoToBackForwardListItem,
               void(WKBackForwardListItem*,
                    NavigationItem*,
                    NavigationInitiationType,
                    bool));
  MOCK_METHOD0(GetPendingItem, NavigationItemImpl*());

 private:
  WebState* GetWebState() override { return web_state_; }

  id<CRWWebViewNavigationProxy> GetWebViewNavigationProxy() const override {
    return mock_web_view_;
  }

  CRWSessionController* session_controller_;
  id mock_web_view_;
  WebState* web_state_ = nullptr;
};

}  // namespace

// NavigationManagerTest is parameterized on this enum to test both the legacy
// implementation of navigation manager and the experimental implementation.
enum NavigationManagerChoice {
  TEST_LEGACY_NAVIGATION_MANAGER,
  TEST_WK_BASED_NAVIGATION_MANAGER,
};

// Programmatic test fixture for NavigationManagerImpl testing.
// GetParam() chooses whether to run tests on LegacyNavigationManagerImpl or
// WKBasedNavigationManagerImpl.
// TODO(crbug.com/734150): cleanup the LegacyNavigationManagerImpl use case.
class NavigationManagerTest
    : public PlatformTest,
      public ::testing::WithParamInterface<NavigationManagerChoice> {
 protected:
  NavigationManagerTest() {
    if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
      feature_list_.InitAndDisableFeature(
          web::features::kSlimNavigationManager);
      manager_.reset(new LegacyNavigationManagerImpl);
      session_controller_delegate_ =
          [[CRWFakeSessionControllerDelegate alloc] init];
      controller_ =
          [[CRWSessionController alloc] initWithBrowserState:&browser_state_];
      controller_.delegate = session_controller_delegate_;
      delegate_.SetSessionController(session_controller());
    } else {
      feature_list_.InitAndEnableFeature(web::features::kSlimNavigationManager);
      manager_.reset(new WKBasedNavigationManagerImpl);
      mock_web_view_ = OCMClassMock([WKWebView class]);
      mock_wk_list_ = [[CRWFakeBackForwardList alloc] init];
      OCMStub([mock_web_view_ backForwardList]).andReturn(mock_wk_list_);
      delegate_.SetWKWebView(mock_web_view_);
    }

    // Setup rewriter.
    BrowserURLRewriter::GetInstance()->AddURLRewriter(UrlRewriter);
    url::AddStandardScheme(kSchemeToRewrite, url::SCHEME_WITH_HOST);

    manager_->SetDelegate(&delegate_);
    manager_->SetBrowserState(&browser_state_);
    manager_->SetSessionController(controller_);
  }

  CRWSessionController* session_controller() { return controller_; }
  NavigationManagerImpl* navigation_manager() { return manager_.get(); }

  MockNavigationManagerDelegate& navigation_manager_delegate() {
    return delegate_;
  }

  // Manipulates the underlying session state to simulate the effect of
  // GoToIndex() on the navigation manager to facilitate testing of other
  // NavigationManager APIs. NavigationManager::GoToIndex() itself is tested
  // separately by verifying expectations on the delegate method calls.
  void SimulateGoToIndex(int index) {
    if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
      [session_controller() goToItemAtIndex:index discardNonCommittedItems:NO];
    } else {
      [mock_wk_list_ moveCurrentToIndex:index];
    }
  }

  // Makes delegate to return navigation item, which is stored in navigation
  // context in the real app.
  void SimulateReturningPendingItemFromDelegate(web::NavigationItemImpl* item) {
    if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
      session_controller_delegate_.pendingItem = item;
    } else {
      ON_CALL(navigation_manager_delegate(), GetPendingItem())
          .WillByDefault(testing::Return(item));
    }
  }

  CRWFakeBackForwardList* mock_wk_list_;
  WKWebView* mock_web_view_;
  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;
  CRWFakeSessionControllerDelegate* session_controller_delegate_ = nil;

 protected:
  TestBrowserState browser_state_;
  TestWebState web_state_;
  MockNavigationManagerDelegate delegate_;
  std::unique_ptr<NavigationManagerImpl> manager_;
  CRWSessionController* controller_;
};

// Tests state of an empty navigation manager.
TEST_P(NavigationManagerTest, EmptyManager) {
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
  EXPECT_EQ(-1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(0));
  EXPECT_EQ(-1, navigation_manager()->GetPreviousItemIndex());
}

// Tests that the simpler setter SetPreviousItemIndex() updates the previous
// item index without sanity check.
TEST_P(NavigationManagerTest, SetPreviousItemIndex) {
  EXPECT_EQ(-1, navigation_manager()->GetPreviousItemIndex());

  navigation_manager()->SetPreviousItemIndex(0);
  EXPECT_EQ(0, navigation_manager()->GetPreviousItemIndex());

  navigation_manager()->SetPreviousItemIndex(1);
  EXPECT_EQ(1, navigation_manager()->GetPreviousItemIndex());

  navigation_manager()->SetPreviousItemIndex(-1);
  EXPECT_EQ(-1, navigation_manager()->GetPreviousItemIndex());
}

// Tests that GetPendingItemIndex() returns -1 if there is no pending entry.
TEST_P(NavigationManagerTest, GetPendingItemIndexWithoutPendingEntry) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
}

// Tests that GetPendingItemIndex() returns -1 if there is a pending item.
TEST_P(NavigationManagerTest, GetPendingItemIndexWithPendingEntry) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
}

// Tests that setting and getting PendingItemIndex.
TEST_P(NavigationManagerTest, SetAndGetPendingItemIndex) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.test"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.test"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->SetPendingItemIndex(0);
  EXPECT_EQ(0, navigation_manager()->GetPendingItemIndex());
}

// Tests that GetPendingItemIndex() returns same index as was set by
// -[CRWSessionController setPendingItemIndex:].
TEST_P(NavigationManagerTest, GetPendingItemIndexWithIndexedPendingEntry) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"
                  backListURLs:@[ @"http://www.url.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    [session_controller() setPendingItemIndex:0];
    EXPECT_EQ(0, navigation_manager()->GetPendingItemIndex());
  }
}

// Tests that NavigationManagerImpl::GetPendingItem() returns item provided by
// the delegate.
TEST_P(NavigationManagerTest, GetPendingItemFromDelegate) {
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  auto item = std::make_unique<web::NavigationItemImpl>();
  SimulateReturningPendingItemFromDelegate(item.get());
  EXPECT_EQ(item.get(), navigation_manager()->GetPendingItem());
}

// Tests that NavigationManagerImpl::GetPendingItem() ignores item provided by
// the delegate if navigation manager has own pending item.
TEST_P(NavigationManagerTest, GetPendingItemIgnoringDelegate) {
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  auto item = std::make_unique<web::NavigationItemImpl>();
  SimulateReturningPendingItemFromDelegate(item.get());

  GURL url("http://www.url.test");
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  EXPECT_NE(item.get(), navigation_manager()->GetPendingItem());
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(url, navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that GetPendingItem() returns indexed pending item.
TEST_P(NavigationManagerTest, GetPendingItemWithIndexedPendingEntry) {
  GURL url("http://www.url.test");
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.test"];
  navigation_manager()->CommitPendingItem();
  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    [session_controller() setPendingItemIndex:0];
  } else {
    navigation_manager()->SetPendingItemIndex(0);
  }

  auto item = std::make_unique<web::NavigationItemImpl>();
  SimulateReturningPendingItemFromDelegate(item.get());

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_NE(item.get(), navigation_manager()->GetPendingItem());
  EXPECT_EQ(url, navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that going back or negative offset is not possible without a committed
// item.
TEST_P(NavigationManagerTest, CanGoBackWithoutCommitedItem) {
  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is not possible if there is a
// transient item, but not committed items.
TEST_P(NavigationManagerTest, CanGoBackWithTransientItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com"));

  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is possible if there is a transient
// item and at least one committed item.
TEST_P(NavigationManagerTest, CanGoBackWithTransientItemAndCommittedItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  // Setup a pending item for which a transient item can be added.
  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com/0"));

  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is not possible if there is ony one
// committed item and no transient item.
TEST_P(NavigationManagerTest, CanGoBackWithSingleCommitedItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests going back possibility with multiple committed items.
TEST_P(NavigationManagerTest, CanGoBackWithMultipleCommitedItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"
                  backListURLs:@[ @"http://www.url.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_
        setCurrentURL:@"http://www.url.com/1"
         backListURLs:@[ @"http://www.url.com", @"http://www.url.com/0" ]
      forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));

  SimulateGoToIndex(1);
  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));

  SimulateGoToIndex(0);
  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));

  SimulateGoToIndex(1);
  EXPECT_TRUE(navigation_manager()->CanGoBack());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going forward or positive offset is not possible without a
// committed item.
TEST_P(NavigationManagerTest, CanGoForwardWithoutCommitedItem) {
  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests that going forward or positive offset is not possible if there is ony
// one committed item and no transient item.
TEST_P(NavigationManagerTest, CanGoForwardWithSingleCommitedItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/"];
  navigation_manager()->CommitPendingItem();

  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests going forward possibility with multiple committed items.
TEST_P(NavigationManagerTest, CanGoForwardWithMultipleCommitedEntries) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"
                  backListURLs:@[ @"http://www.url.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_
        setCurrentURL:@"http://www.url.com/1"
         backListURLs:@[ @"http://www.url.com", @"http://www.url.com/0" ]
      forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));

  SimulateGoToIndex(1);
  EXPECT_TRUE(navigation_manager()->CanGoForward());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));

  SimulateGoToIndex(0);
  EXPECT_TRUE(navigation_manager()->CanGoForward());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));

  SimulateGoToIndex(1);
  EXPECT_TRUE(navigation_manager()->CanGoForward());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));

  SimulateGoToIndex(2);
  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests CanGoToOffset API for positive, negative and zero delta. Tested
// navigation manager will have redirect entries to make sure they are
// appropriately skipped.
TEST_P(NavigationManagerTest, OffsetsWithoutPendingIndex) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/redirect"), Referrer(),
      ui::PAGE_TRANSITION_CLIENT_REDIRECT,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/redirect"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[
                    @"http://www.url.com/0", @"http://www.url.com/redirect"
                  ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/2"
                  backListURLs:@[
                    @"http://www.url.com/0", @"http://www.url.com/redirect",
                    @"http://www.url.com/1"
                  ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/redirect"), Referrer(),
      ui::PAGE_TRANSITION_CLIENT_REDIRECT,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/redirect"
                  backListURLs:@[
                    @"http://www.url.com/0", @"http://www.url.com/redirect",
                    @"http://www.url.com/1", @"http://www.url.com/2"
                  ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  ASSERT_EQ(5, navigation_manager()->GetItemCount());
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());

  if (GetParam() == TEST_WK_BASED_NAVIGATION_MANAGER) {
    // WKBasedNavigationManagerImpl doesn't treat redirect specially because it
    // relies on WKWebView to handle that. See WKBasedNavigationManagerTest for
    // an similar test case of the CanGoToOffset API without redirects.
    return;
  }

  // Go to entry at index 1 and test API from that state.
  SimulateGoToIndex(1);
  ASSERT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  ASSERT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(-2, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(1));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(4, navigation_manager()->GetIndexForOffset(2));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(3));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(3));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(INT_MIN, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-1000000000, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000002, navigation_manager()->GetIndexForOffset(1000000000));

  // Go to entry at index 2 and test API from that state.
  SimulateGoToIndex(2);
  ASSERT_EQ(2, navigation_manager()->GetLastCommittedItemIndex());
  ASSERT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(1, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(4, navigation_manager()->GetIndexForOffset(1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(2));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(-2147483647, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-999999999, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000003, navigation_manager()->GetIndexForOffset(1000000000));

  // Go to entry at index 4 and test API from that state.
  SimulateGoToIndex(4);
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());
  ASSERT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(1, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(6, navigation_manager()->GetIndexForOffset(2));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(-2147483646, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-999999998, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000004, navigation_manager()->GetIndexForOffset(1000000000));

  // Test with existing transient entry.
  navigation_manager()->AddTransientItem(GURL("http://www.url.com"));
  ASSERT_EQ(5, navigation_manager()->GetItemCount());
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());
  ASSERT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(4, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-3));
  EXPECT_EQ(1, navigation_manager()->GetIndexForOffset(-3));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(6, navigation_manager()->GetIndexForOffset(2));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(-2147483645, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-999999997, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000004, navigation_manager()->GetIndexForOffset(1000000000));

  // Now test with pending item index.
  navigation_manager()->DiscardNonCommittedItems();

  // Set pending index to 1 and test API from that state.
  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    [session_controller() setPendingItemIndex:1];
  }
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());
  ASSERT_EQ(1, navigation_manager()->GetPendingItemIndex());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(-2, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(1));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(4, navigation_manager()->GetIndexForOffset(2));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(3));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(3));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(INT_MIN, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-1000000000, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000002, navigation_manager()->GetIndexForOffset(1000000000));

  // Set pending index to 2 and test API from that state.
  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    [session_controller() setPendingItemIndex:2];
  }
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());
  ASSERT_EQ(2, navigation_manager()->GetPendingItemIndex());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(1, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(4, navigation_manager()->GetIndexForOffset(1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(2));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(-2147483647, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-999999999, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000003, navigation_manager()->GetIndexForOffset(1000000000));

  // Set pending index to 4 and committed entry to 1 and test.
  SimulateGoToIndex(1);
  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    [session_controller() setPendingItemIndex:4];
  }
  ASSERT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  ASSERT_EQ(4, navigation_manager()->GetPendingItemIndex());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(1, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(6, navigation_manager()->GetIndexForOffset(2));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(-2147483646, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-999999998, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000004, navigation_manager()->GetIndexForOffset(1000000000));

  // Test with existing transient entry in the end of the stack.
  SimulateGoToIndex(4);
  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    [session_controller() setPendingItemIndex:-1];
  }
  navigation_manager()->AddTransientItem(GURL("http://www.url.com"));
  ASSERT_EQ(5, navigation_manager()->GetItemCount());
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());
  ASSERT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-1));
  EXPECT_EQ(4, navigation_manager()->GetIndexForOffset(-1));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-2));
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(-2));
  EXPECT_TRUE(navigation_manager()->CanGoToOffset(-3));
  EXPECT_EQ(1, navigation_manager()->GetIndexForOffset(-3));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
  EXPECT_EQ(5, navigation_manager()->GetIndexForOffset(1));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(2));
  EXPECT_EQ(6, navigation_manager()->GetIndexForOffset(2));
  // Test with large values.
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(INT_MIN));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1000000000));
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1000000000));
  EXPECT_EQ(INT_MAX, navigation_manager()->GetIndexForOffset(INT_MAX));
  EXPECT_EQ(-2147483645, navigation_manager()->GetIndexForOffset(INT_MIN));
  EXPECT_EQ(-999999997, navigation_manager()->GetIndexForOffset(-1000000000));
  EXPECT_EQ(1000000004, navigation_manager()->GetIndexForOffset(1000000000));
}

// Tests offsets with pending transient entries (specifically going back and
// forward from a pending navigation entry that is added to the middle of the
// navigation stack).
TEST_P(NavigationManagerTest, OffsetsWithPendingTransientEntry) {
  // This test directly manipulates the WKBackForwardListItem mocks stored in
  // mock_wk_list_ so that the associated NavigationItem objects are retained
  // throughout the test case.
  WKBackForwardListItem* wk_item0 =
      [CRWFakeBackForwardList itemWithURLString:@"http://www.url.com/0"];
  WKBackForwardListItem* wk_item1 =
      [CRWFakeBackForwardList itemWithURLString:@"http://www.url.com/1"];
  WKBackForwardListItem* wk_item2 =
      [CRWFakeBackForwardList itemWithURLString:@"http://www.url.com/2"];

  // Create a transient item in the middle of the navigation stack and go back
  // to it (pending index is 1, current index is 2).
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  mock_wk_list_.currentItem = wk_item0;
  mock_wk_list_.backList = nil;
  mock_wk_list_.forwardList = nil;
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  mock_wk_list_.currentItem = wk_item1;
  mock_wk_list_.backList = @[ wk_item0 ];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  mock_wk_list_.currentItem = wk_item2;
  mock_wk_list_.backList = @[ wk_item0, wk_item1 ];
  navigation_manager()->CommitPendingItem();

  // Under back-forward navigation, both WKBackForwardList and WKWebView.URL are
  // updated before |didStartProvisionalNavigation| callback, which calls
  // AddPendingItem. Simulate this behavior.
  OCMExpect([mock_web_view_ URL])
      .andReturn([NSURL URLWithString:@"http://www.url.com/1"]);
  mock_wk_list_.currentItem = wk_item1;
  mock_wk_list_.backList = @[ wk_item0 ];
  mock_wk_list_.forwardList = @[ wk_item2 ];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com/1"));

  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    [session_controller() setPendingItemIndex:1];
  }

  ASSERT_EQ(3, navigation_manager()->GetItemCount());
  ASSERT_EQ(2, navigation_manager()->GetLastCommittedItemIndex());
  ASSERT_EQ(1, navigation_manager()->GetPendingItemIndex());
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(1));
  EXPECT_EQ(0, navigation_manager()->GetIndexForOffset(-1));

  if (GetParam() == TEST_WK_BASED_NAVIGATION_MANAGER) {
    // TODO(crbug.com/734150): Investigate why this test still fails for the new
    // navigation manager.
    return;
  }

  // Now go forward to that middle transient item (pending index is 1,
  // current index is 0).
  SimulateGoToIndex(0);
  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    [session_controller() setPendingItemIndex:1];
  }
  EXPECT_EQ(3, navigation_manager()->GetItemCount());
  EXPECT_EQ(0, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_EQ(1, navigation_manager()->GetPendingItemIndex());
  EXPECT_EQ(2, navigation_manager()->GetIndexForOffset(1));
  EXPECT_EQ(0, navigation_manager()->GetIndexForOffset(-1));
}

// Tests that when given a pending item, adding a new pending item replaces the
// existing pending item if their URLs are different.
TEST_P(NavigationManagerTest, ReplacePendingItemIfDiffernetURL) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(existing_url, navigation_manager()->GetPendingItem()->GetURL());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  GURL new_url = GURL("http://www.new.com");
  navigation_manager()->AddPendingItem(
      new_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(new_url, navigation_manager()->GetPendingItem()->GetURL());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL doesn't replace the existing pending item if new pending item is not a
// form submission.
TEST_P(NavigationManagerTest, NotReplaceSameUrlPendingItemIfNotFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());

  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
        navigation_manager()->GetPendingItem()->GetTransitionType(),
        ui::PAGE_TRANSITION_TYPED));
  } else {
    // WKBasedNavigationManager assumes that AddPendingItem() is only called for
    // new navigation, so it always creates a new pending item.
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
        navigation_manager()->GetPendingItem()->GetTransitionType(),
        ui::PAGE_TRANSITION_LINK));
  }
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL replaces the existing pending item if new pending item is a form
// submission while existing pending item is not.
TEST_P(NavigationManagerTest, ReplaceSameUrlPendingItemIfFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_FORM_SUBMIT,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_FORM_SUBMIT));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL doesn't replace the existing pending item if the user agent override
// option is INHERIT.
TEST_P(NavigationManagerTest, NotReplaceSameUrlPendingItemIfOverrideInherit) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());

  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
        navigation_manager()->GetPendingItem()->GetTransitionType(),
        ui::PAGE_TRANSITION_TYPED));
  } else {
    // WKBasedNavigationManager assumes that AddPendingItem() is only called for
    // new navigation, so it always creates a new pending item.
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
        navigation_manager()->GetPendingItem()->GetTransitionType(),
        ui::PAGE_TRANSITION_LINK));
  }
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL replaces the existing pending item if the user agent override option is
// DESKTOP.
TEST_P(NavigationManagerTest, ReplaceSameUrlPendingItemIfOverrideDesktop) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL replaces the existing pending item if the user agent override option is
// MOBILE.
TEST_P(NavigationManagerTest, ReplaceSameUrlPendingItemIfOverrideMobile) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(web::UserAgentType::DESKTOP,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item
// succeeds if the new item's URL is different from the last committed item.
TEST_P(NavigationManagerTest, AddPendingItemIfDiffernetURL) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(existing_url,
            navigation_manager()->GetLastCommittedItem()->GetURL());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  GURL new_url = GURL("http://www.new.com");
  navigation_manager()->AddPendingItem(
      new_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(new_url, navigation_manager()->GetPendingItem()->GetURL());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL fails if the new item is not form submission.
TEST_P(NavigationManagerTest, NotAddSameUrlPendingItemIfNotFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    EXPECT_FALSE(navigation_manager()->GetPendingItem());
  } else {
    // WKBasedNavigationManager assumes that AddPendingItem() is only called for
    // new navigation, so it always creates a new pending item.
    ASSERT_TRUE(navigation_manager()->GetPendingItem());
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
        navigation_manager()->GetPendingItem()->GetTransitionType(),
        ui::PAGE_TRANSITION_LINK));
  }
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL succeeds if the new item is a form submission while the last
// committed item is not.
TEST_P(NavigationManagerTest, AddSameUrlPendingItemIfFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  // Add if new transition is a form submission.
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_FORM_SUBMIT,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_FORM_SUBMIT));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL fails if both the new item and the last committed item are form
// submissions.
TEST_P(NavigationManagerTest,
       NotAddSameUrlPendingItemIfDuplicateFormSubmission) {
  if (GetParam() == TEST_WK_BASED_NAVIGATION_MANAGER) {
    // TODO(crbug.com/734150): Enable this test when form submission check is
    // implemented.
    return;
  }
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_FORM_SUBMIT,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_FORM_SUBMIT,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL fails if the user agent override option is INHERIT.
TEST_P(NavigationManagerTest, NotAddSameUrlPendingItemIfOverrideInherit) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    EXPECT_FALSE(navigation_manager()->GetPendingItem());
  } else {
    // WKBasedNavigationManager assumes that AddPendingItem() is only called for
    // new navigation, so it always creates a new pending item.
    ASSERT_TRUE(navigation_manager()->GetPendingItem());
    EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
        navigation_manager()->GetPendingItem()->GetTransitionType(),
        ui::PAGE_TRANSITION_LINK));
  }
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL succeeds if the user agent override option is DESKTOP.
TEST_P(NavigationManagerTest, AddSameUrlPendingItemIfOverrideDesktop) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL succeeds if the user agent override option is MOBILE.
TEST_P(NavigationManagerTest, AddSameUrlPendingItemIfOverrideMobile) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(web::UserAgentType::DESKTOP,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that desktop user agent can be enforced to use for next pending item
// when UserAgentOverrideOption is DESKTOP.
TEST_P(NavigationManagerTest, OverrideUserAgentWithDesktop) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);

  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  NavigationItem* last_committed_item =
      navigation_manager()->GetLastCommittedItem();
  EXPECT_EQ(UserAgentType::MOBILE, last_committed_item->GetUserAgentType());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(UserAgentType::DESKTOP,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that mobile user agent can be enforced to use for next pending item
// when UserAgentOverrideOption is MOBILE.
TEST_P(NavigationManagerTest, OverrideUserAgentWithMobile) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  NavigationItem* last_committed_item =
      navigation_manager()->GetLastCommittedItem();
  last_committed_item->SetUserAgentType(UserAgentType::DESKTOP);
  EXPECT_EQ(UserAgentType::DESKTOP, last_committed_item->GetUserAgentType());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(UserAgentType::MOBILE,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
}

// Tests that the UserAgentType of an INHERIT item is propagated to subsequent
// item when UserAgentOverrideOption is INHERIT
TEST_P(NavigationManagerTest, OverrideUserAgentWithInheritAfterInherit) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.2.com"
                  backListURLs:@[ @"http://www.1.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());
}

// Tests that the UserAgentType of a MOBILE item is propagated to subsequent
// item when UserAgentOverrideOption is INHERIT
TEST_P(NavigationManagerTest, OverrideUserAgentWithInheritAfterMobile) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);

  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.2.com"
                  backListURLs:@[ @"http://www.1.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::MOBILE,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());
}

// Tests that the UserAgentType of a DESKTOP item is propagated to subsequent
// item when UserAgentOverrideOption is INHERIT
TEST_P(NavigationManagerTest, OverrideUserAgentWithInheritAfterDesktop) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);

  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::DESKTOP,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());

  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.2.com"
                  backListURLs:@[ @"http://www.1.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(web::UserAgentType::DESKTOP,
            navigation_manager()->GetLastCommittedItem()->GetUserAgentType());
}

// Tests that the UserAgentType is propagated to subsequent NavigationItems if
// a native URL exists in between navigations.
TEST_P(NavigationManagerTest, UserAgentTypePropagationPastNativeItems) {
  // This test manipuates the WKBackForwardListItems in mock_wk_list_ directly
  // because it relies on the associated NavigationItems.
  WKBackForwardListItem* wk_item1 =
      [CRWFakeBackForwardList itemWithURLString:@"http://www.1.com"];

  // GURL::Replacements that will replace a GURL's scheme with the test native
  // scheme.
  GURL::Replacements native_scheme_replacement;
  native_scheme_replacement.SetSchemeStr(kTestNativeContentScheme);

  // Create two non-native navigations that are separated by a native one.
  navigation_manager()->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  mock_wk_list_.currentItem = wk_item1;
  navigation_manager()->CommitPendingItem();

  NavigationItem* item1 = navigation_manager()->GetLastCommittedItem();
  ASSERT_EQ(web::UserAgentType::MOBILE, item1->GetUserAgentType());

  GURL item2_url = item1->GetURL().ReplaceComponents(native_scheme_replacement);
  navigation_manager()->AddPendingItem(
      item2_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  WKBackForwardListItem* wk_native_item2 = [CRWFakeBackForwardList
      itemWithURLString:base::SysUTF8ToNSString(item2_url.spec())];
  mock_wk_list_.currentItem = wk_native_item2;
  mock_wk_list_.backList = @[ wk_item1 ];
  navigation_manager()->CommitPendingItem();

  NavigationItem* native_item1 = navigation_manager()->GetLastCommittedItem();
  // Having a non-app-specific URL should not change the fact that the native
  // item should be skipped when determining user agent inheritance.
  native_item1->SetVirtualURL(GURL("http://non-app-specific-url"));
  ASSERT_EQ(web::UserAgentType::NONE, native_item1->GetUserAgentType());
  navigation_manager()->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  WKBackForwardListItem* wk_item2 =
      [CRWFakeBackForwardList itemWithURLString:@"http://www.2.com"];
  mock_wk_list_.currentItem = wk_item2;
  mock_wk_list_.backList = @[ wk_item1, wk_native_item2 ];
  navigation_manager()->CommitPendingItem();
  NavigationItem* item2 = navigation_manager()->GetLastCommittedItem();

  // Verify that |item1|'s UserAgentType is propagated to |item2|.
  EXPECT_EQ(item1->GetUserAgentType(), item2->GetUserAgentType());

  // Update |item2|'s UA type to DESKTOP and add a third non-native navigation,
  // once again separated by a native one.
  item2->SetUserAgentType(web::UserAgentType::DESKTOP);
  ASSERT_EQ(web::UserAgentType::DESKTOP, item2->GetUserAgentType());

  GURL item3_url = item2->GetURL().ReplaceComponents(native_scheme_replacement);
  navigation_manager()->AddPendingItem(
      item3_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  WKBackForwardListItem* wk_native_item3 = [CRWFakeBackForwardList
      itemWithURLString:base::SysUTF8ToNSString(item3_url.spec())];
  mock_wk_list_.currentItem = wk_native_item3;
  mock_wk_list_.backList = @[ wk_item1, wk_native_item2, wk_item2 ];
  navigation_manager()->CommitPendingItem();

  NavigationItem* native_item2 = navigation_manager()->GetLastCommittedItem();
  ASSERT_EQ(web::UserAgentType::NONE, native_item2->GetUserAgentType());
  navigation_manager()->AddPendingItem(
      GURL("http://www.3.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  WKBackForwardListItem* wk_item3 =
      [CRWFakeBackForwardList itemWithURLString:@"http://www.3.com"];
  mock_wk_list_.currentItem = wk_item3;
  mock_wk_list_.backList =
      @[ wk_item1, wk_native_item2, wk_item2, wk_native_item3 ];
  navigation_manager()->CommitPendingItem();

  NavigationItem* item3 = navigation_manager()->GetLastCommittedItem();

  // Verify that |item2|'s UserAgentType is propagated to |item3|.
  EXPECT_EQ(item2->GetUserAgentType(), item3->GetUserAgentType());
}

// Tests that adding transient item for a pending item with mobile user agent
// type results in a transient item with mobile user agent type.
TEST_P(NavigationManagerTest, AddTransientItemForMobilePendingItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  navigation_manager()->GetPendingItem()->SetUserAgentType(
      UserAgentType::MOBILE);

  navigation_manager()->AddTransientItem(GURL("http://www.url.com"));
  ASSERT_TRUE(navigation_manager()->GetTransientItem());
  EXPECT_EQ(UserAgentType::MOBILE,
            navigation_manager()->GetTransientItem()->GetUserAgentType());
  EXPECT_EQ(UserAgentType::MOBILE,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
}

// Tests that adding transient item for a pending item with desktop user agent
// type results in a transient item with desktop user agent type.
TEST_P(NavigationManagerTest, AddTransientItemForDesktopPendingItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  navigation_manager()->GetPendingItem()->SetUserAgentType(
      UserAgentType::DESKTOP);

  navigation_manager()->AddTransientItem(GURL("http://www.url.com"));
  ASSERT_TRUE(navigation_manager()->GetTransientItem());
  EXPECT_EQ(UserAgentType::DESKTOP,
            navigation_manager()->GetTransientItem()->GetUserAgentType());
  EXPECT_EQ(UserAgentType::DESKTOP,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL is no-op when there
// are no transient, pending and committed items.
TEST_P(NavigationManagerTest, ReloadEmptyWithNormalType) {
  ASSERT_FALSE(navigation_manager()->GetTransientItem());
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(0);
  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);

  ASSERT_FALSE(navigation_manager()->GetTransientItem());
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL leaves the url of
// the renderer initiated pending item unchanged when there is one.
TEST_P(NavigationManagerTest, ReloadRendererPendingItemWithNormalType) {
  GURL url_before_reload = GURL("http://www.url.com");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(1);
  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(url_before_reload,
            navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL leaves the url of
// the user initiated pending item unchanged when there is one.
TEST_P(NavigationManagerTest, ReloadUserPendingItemWithNormalType) {
  GURL url_before_reload = GURL("http://www.url.com");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(1);
  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(url_before_reload,
            navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL leaves the url of
// the last committed item unchanged when there is no pending item.
TEST_P(NavigationManagerTest, ReloadLastCommittedItemWithNormalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  GURL url_before_reload = GURL("http://www.url.com/1");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(1);
  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(url_before_reload,
            navigation_manager()->GetLastCommittedItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::NORMAL leaves the url of
// the last committed item unchanged when there is no pending item, but there
// forward items after last committed item.
TEST_P(NavigationManagerTest,
       ReloadLastCommittedItemWithNormalTypeWithForwardItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  GURL url_before_reload = GURL("http://www.url.com/1");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_
        setCurrentURL:@"http://www.url.com/2"
         backListURLs:@[ @"http://www.url.com/0", @"http://www.url.com/1" ]
      forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  SimulateGoToIndex(1);
  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(1);
  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(url_before_reload,
            navigation_manager()->GetLastCommittedItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL is
// no-op when there are no transient, pending and committed items.
TEST_P(NavigationManagerTest, ReloadEmptyWithOriginalType) {
  ASSERT_FALSE(navigation_manager()->GetTransientItem());
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(0);
  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);

  ASSERT_FALSE(navigation_manager()->GetTransientItem());
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the renderer initiated pending item's url to its original request url
// when there is one.
TEST_P(NavigationManagerTest, ReloadRendererPendingItemWithOriginalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  GURL expected_original_url = GURL("http://www.url.com/original");
  navigation_manager()->GetPendingItem()->SetOriginalRequestURL(
      expected_original_url);

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(1);
  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(expected_original_url,
            navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the user initiated pending item's url to its original request url
// when there is one.
TEST_P(NavigationManagerTest, ReloadUserPendingItemWithOriginalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  GURL expected_original_url = GURL("http://www.url.com/original");
  navigation_manager()->GetPendingItem()->SetOriginalRequestURL(
      expected_original_url);

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(1);
  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(expected_original_url,
            navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the last committed item's url to its original request url when there
// is no pending item.
TEST_P(NavigationManagerTest, ReloadLastCommittedItemWithOriginalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  GURL expected_original_url = GURL("http://www.url.com/1/original");
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  navigation_manager()->GetPendingItem()->SetOriginalRequestURL(
      expected_original_url);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1/original"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(1);
  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(expected_original_url,
            navigation_manager()->GetLastCommittedItem()->GetURL());
}

// Tests that calling |Reload| with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the last committed item's url to its original request url when there
// is no pending item, but there are forward items after last committed item.
TEST_P(NavigationManagerTest,
       ReloadLastCommittedItemWithOriginalTypeWithForwardItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  GURL expected_original_url = GURL("http://www.url.com/1/original");
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  navigation_manager()->GetPendingItem()->SetOriginalRequestURL(
      expected_original_url);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1/original"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/2"
                  backListURLs:@[
                    @"http://www.url.com/0", @"http://www.url.com/1/original"
                  ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  SimulateGoToIndex(1);
  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(1);
  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(expected_original_url,
            navigation_manager()->GetLastCommittedItem()->GetURL());
}

// Tests that ReloadWithUserAgentType triggers new navigation with the expected
// user agent override.
TEST_P(NavigationManagerTest, ReloadWithUserAgentType) {
  GURL url("http://www.1.com");
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      NavigationInitiationType::BROWSER_INITIATED,
      NavigationManager::UserAgentOverrideOption::MOBILE);
  GURL virtual_url("http://www.1.com/virtual");
  navigation_manager()->GetPendingItem()->SetVirtualURL(virtual_url);
  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();
  OCMExpect([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.1.com"]);

  EXPECT_CALL(navigation_manager_delegate(), WillChangeUserAgentType());
  EXPECT_CALL(navigation_manager_delegate(), RecordPageStateInNavigationItem());
  EXPECT_CALL(navigation_manager_delegate(), ClearTransientContent());
  EXPECT_CALL(navigation_manager_delegate(), ClearDialogs());
  EXPECT_CALL(navigation_manager_delegate(),
              LoadCurrentItem(NavigationInitiationType::BROWSER_INITIATED));

  navigation_manager()->ReloadWithUserAgentType(UserAgentType::DESKTOP);

  NavigationItem* pending_item =
      navigation_manager()->GetPendingItemInCurrentOrRestoredSession();
  if (!web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_EQ(url, pending_item->GetURL());
  } else {
    GURL reload_target_url;
    ASSERT_TRUE(wk_navigation_util::ExtractTargetURL(pending_item->GetURL(),
                                                     &reload_target_url));
    EXPECT_EQ(url, reload_target_url);
  }
  EXPECT_EQ(virtual_url, pending_item->GetVirtualURL());
  EXPECT_EQ(UserAgentType::DESKTOP, pending_item->GetUserAgentType());
}

// Tests that ReloadWithUserAgentType does not expose internal URLs.
TEST_P(NavigationManagerTest, ReloadWithUserAgentTypeOnIntenalUrl) {
  delegate_.SetWebState(&web_state_);
  web_state_.SetLoading(true);

  GURL url = wk_navigation_util::CreateRedirectUrl(GURL("http://www.1.com"));
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      NavigationInitiationType::BROWSER_INITIATED,
      NavigationManager::UserAgentOverrideOption::MOBILE);
  GURL virtual_url("http://www.1.com/virtual");
  navigation_manager()
      ->GetPendingItemInCurrentOrRestoredSession()
      ->SetVirtualURL(virtual_url);
  [mock_wk_list_ setCurrentURL:base::SysUTF8ToNSString(url.spec())];
  navigation_manager()->OnNavigationStarted(GURL("http://www.1.com/virtual"));

  navigation_manager()->ReloadWithUserAgentType(UserAgentType::DESKTOP);

  NavigationItem* pending_item =
      navigation_manager()->GetPendingItemInCurrentOrRestoredSession();
  if (!web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_EQ(url, pending_item->GetURL());
  } else {
    GURL reload_target_url;
    ASSERT_TRUE(wk_navigation_util::ExtractTargetURL(pending_item->GetURL(),
                                                     &reload_target_url));
    EXPECT_EQ("http://www.1.com/", reload_target_url.spec());
  }
  EXPECT_EQ(virtual_url, pending_item->GetVirtualURL());
  EXPECT_EQ(UserAgentType::DESKTOP, pending_item->GetUserAgentType());
}

// Tests that app-specific URLs are not rewritten for renderer-initiated loads
// or reloads unless requested by a page with app-specific url.
TEST_P(NavigationManagerTest, RewritingAppSpecificUrls) {
  // URL should not be rewritten as there is no committed URL.
  GURL url1(url::SchemeHostPort(kSchemeToRewrite, "test", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url1, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_EQ(url1, navigation_manager()->GetPendingItem()->GetURL());

  // URL should not be rewritten because last committed URL is not app-specific.
  [mock_wk_list_ setCurrentURL:base::SysUTF8ToNSString(url1.spec())];
  navigation_manager()->CommitPendingItem();

  GURL url2(url::SchemeHostPort(kSchemeToRewrite, "test2", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url2, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_EQ(url2, navigation_manager()->GetPendingItem()->GetURL());

  // URL should not be rewritten for user initiated reload navigations.
  GURL url_reload(
      url::SchemeHostPort(kSchemeToRewrite, "test-reload", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url_reload, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_EQ(url_reload, navigation_manager()->GetPendingItem()->GetURL());

  // URL should be rewritten for user initiated navigations.
  GURL url3(url::SchemeHostPort(kSchemeToRewrite, "test3", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url3, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  GURL rewritten_url3(
      url::SchemeHostPort(kTestWebUIScheme, "test3", 0).Serialize());
  EXPECT_EQ(rewritten_url3, navigation_manager()->GetPendingItem()->GetURL());

  // URL should be rewritten because last committed URL is app-specific.
  [mock_wk_list_ setCurrentURL:base::SysUTF8ToNSString(rewritten_url3.spec())
                  backListURLs:@[ base::SysUTF8ToNSString(url1.spec()) ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  GURL url4(url::SchemeHostPort(kSchemeToRewrite, "test4", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url4, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  GURL rewritten_url4(
      url::SchemeHostPort(kTestWebUIScheme, "test4", 0).Serialize());
  EXPECT_EQ(rewritten_url4, navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that transient URLRewriters are applied for pending items.
TEST_P(NavigationManagerTest, ApplyTransientRewriters) {
  navigation_manager()->AddTransientURLRewriter(&AppendingUrlRewriter);
  navigation_manager()->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  NavigationItem* pending_item = navigation_manager()->GetPendingItem();
  EXPECT_EQ(kRewrittenQueryParam, pending_item->GetURL().query());

  // Now that the transient rewriters are consumed, the next URL should not be
  // changed.
  GURL url("http://www.1.com");
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  EXPECT_EQ(url, navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that GetIndexOfItem() returns the correct values.
TEST_P(NavigationManagerTest, GetIndexOfItem) {
  // This test manipuates the WKBackForwardListItems in mock_wk_list_ directly
  // to retain the NavigationItem association.
  WKBackForwardListItem* wk_item0 =
      [CRWFakeBackForwardList itemWithURLString:@"http://www.url.com/0"];
  WKBackForwardListItem* wk_item1 =
      [CRWFakeBackForwardList itemWithURLString:@"http://www.url.com/1"];

  // Create two items and add them to the NavigationManagerImpl.
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  mock_wk_list_.currentItem = wk_item0;
  navigation_manager()->CommitPendingItem();

  NavigationItem* item0 = navigation_manager()->GetLastCommittedItem();
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  mock_wk_list_.currentItem = wk_item1;
  mock_wk_list_.backList = @[ wk_item0 ];
  navigation_manager()->CommitPendingItem();

  NavigationItem* item1 = navigation_manager()->GetLastCommittedItem();
  // Create an item that does not exist in the NavigationManagerImpl.
  std::unique_ptr<NavigationItem> item_not_found = NavigationItem::Create();
  // Verify GetIndexOfItem() results.
  EXPECT_EQ(0, navigation_manager()->GetIndexOfItem(item0));
  EXPECT_EQ(1, navigation_manager()->GetIndexOfItem(item1));
  EXPECT_EQ(-1, navigation_manager()->GetIndexOfItem(item_not_found.get()));
}

// Tests that GetBackwardItems() and GetForwardItems() return expected entries
// when current item is in the middle of the navigation history.
TEST_P(NavigationManagerTest, TestBackwardForwardItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_
        setCurrentURL:@"http://www.url.com/2"
         backListURLs:@[ @"http://www.url.com/0", @"http://www.url.com/1" ]
      forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_EQ(2, navigation_manager()->GetLastCommittedItemIndex());
  NavigationItemList back_items = navigation_manager()->GetBackwardItems();
  EXPECT_EQ(2U, back_items.size());
  EXPECT_EQ("http://www.url.com/1", back_items[0]->GetURL().spec());
  EXPECT_EQ("http://www.url.com/0", back_items[1]->GetURL().spec());
  EXPECT_TRUE(navigation_manager()->GetForwardItems().empty());

  SimulateGoToIndex(1);
  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  back_items = navigation_manager()->GetBackwardItems();
  EXPECT_EQ(1U, back_items.size());
  EXPECT_EQ("http://www.url.com/0", back_items[0]->GetURL().spec());
  NavigationItemList forward_items = navigation_manager()->GetForwardItems();
  EXPECT_EQ(1U, forward_items.size());
  EXPECT_EQ("http://www.url.com/2", forward_items[0]->GetURL().spec());
}

// Tests that Restore() creates the correct navigation state.
TEST_P(NavigationManagerTest, Restore) {
  GURL urls[3] = {GURL("http://www.url.com/0"), GURL("http://www.url.com/1"),
                  GURL("http://www.url.com/2")};
  std::vector<std::unique_ptr<NavigationItem>> items;
  for (size_t index = 0; index < base::size(urls); ++index) {
    items.push_back(NavigationItem::Create());
    items.back()->SetURL(urls[index]);
  }

  // Call Restore() and check that the NavigationItems are in the correct order
  // and that the last committed index is correct too.
  ASSERT_FALSE(navigation_manager()->IsRestoreSessionInProgress());
  navigation_manager()->Restore(1, std::move(items));
  __block bool restore_done = false;
  navigation_manager()->AddRestoreCompletionCallback(base::BindOnce(^{
    restore_done = true;
  }));

  if (GetParam() == TEST_WK_BASED_NAVIGATION_MANAGER) {
    // Session restore is asynchronous for WKBasedNavigationManager.
    ASSERT_TRUE(navigation_manager()->IsRestoreSessionInProgress());
    ASSERT_FALSE(restore_done);

    // Verify that restore session URL is pending.
    EXPECT_FALSE(navigation_manager()->GetPendingItem());
    NavigationItem* pending_item =
        navigation_manager()->GetPendingItemInCurrentOrRestoredSession();
    ASSERT_TRUE(pending_item);
    GURL pending_url = pending_item->GetURL();
    EXPECT_TRUE(pending_url.SchemeIsFile());
    EXPECT_EQ("restore_session.html", pending_url.ExtractFileName());
    EXPECT_EQ("http://www.url.com/0", pending_item->GetVirtualURL());
    navigation_manager()->OnNavigationStarted(pending_url);

    // Simulate the end effect of loading the restore session URL in web view.
    pending_item->SetURL(urls[1]);
    [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                    backListURLs:@[ @"http://www.url.com/0" ]
                 forwardListURLs:@[ @"http://www.url.com/2" ]];
    navigation_manager()->OnNavigationStarted(GURL("http://www.url.com/2"));
  }

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForPageLoadTimeout, ^{
    return restore_done;
  }));

  EXPECT_FALSE(navigation_manager()->IsRestoreSessionInProgress());
  ASSERT_EQ(3, navigation_manager()->GetItemCount());
  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_EQ(urls[1], navigation_manager()->GetLastCommittedItem()->GetURL());

  for (size_t i = 0; i < base::size(urls); ++i) {
    EXPECT_EQ(urls[i], navigation_manager()->GetItemAtIndex(i)->GetURL());
  }

  histogram_tester_.ExpectTotalCount(kRestoreNavigationItemCount, 1);
  histogram_tester_.ExpectBucketCount(kRestoreNavigationItemCount, 3, 1);
}

// Tests that pending item is not considered part of session history so that
// GetBackwardItems returns the second last committed item even if there is a
// pendign item.
TEST_P(NavigationManagerTest, NewPendingItemIsHiddenFromHistory) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_TRUE(navigation_manager()->GetPendingItem());

  NavigationItemList back_items = navigation_manager()->GetBackwardItems();
  EXPECT_EQ(1U, back_items.size());
  EXPECT_EQ("http://www.url.com/0", back_items[0]->GetURL().spec());
}

// Tests that all committed items are considered history if there is a transient
// item. This can happen if an interstitial was loaded for SSL error.
// See crbug.com/691311.
TEST_P(NavigationManagerTest,
       BackwardItemsShouldContainAllCommittedIfCurrentIsTransient) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com/1"));

  EXPECT_EQ(0, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_TRUE(navigation_manager()->GetTransientItem());

  NavigationItemList back_items = navigation_manager()->GetBackwardItems();
  EXPECT_EQ(1U, back_items.size());
  EXPECT_EQ("http://www.url.com/0", back_items[0]->GetURL().spec());
}

TEST_P(NavigationManagerTest, BackwardItemsShouldBeEmptyIfFirstIsTransient) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com/0"));

  EXPECT_EQ(-1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_TRUE(navigation_manager()->GetTransientItem());

  NavigationItemList back_items = navigation_manager()->GetBackwardItems();
  EXPECT_TRUE(back_items.empty());
}

TEST_P(NavigationManagerTest, VisibleItemIsTransientItemIfPresent) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com/1"));

  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  EXPECT_EQ("http://www.url.com/1",
            navigation_manager()->GetVisibleItem()->GetURL().spec());

  // Visible item is still transient item even if there is a committed item.
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->AddTransientItem(GURL("http://www.url.com/3"));

  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  EXPECT_EQ("http://www.url.com/3",
            navigation_manager()->GetVisibleItem()->GetURL().spec());
}

TEST_P(NavigationManagerTest, PendingItemIsVisibleIfNewAndUserInitiated) {
  delegate_.SetWebState(&web_state_);
  web_state_.SetLoading(true);

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  EXPECT_EQ("http://www.url.com/0",
            navigation_manager()->GetVisibleItem()->GetURL().spec());

  // Visible item is still the user initiated pending item even if there is a
  // committed item.
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  EXPECT_EQ("http://www.url.com/1",
            navigation_manager()->GetVisibleItem()->GetURL().spec());
}

TEST_P(NavigationManagerTest, PendingItemIsNotVisibleIfNotUserInitiated) {
  delegate_.SetWebState(&web_state_);
  web_state_.SetLoading(true);

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  EXPECT_EQ(nullptr, navigation_manager()->GetVisibleItem());
}

TEST_P(NavigationManagerTest, PendingItemIsNotVisibleIfNotNewNavigation) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  // Move pending item back to index 0.
  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    [session_controller() setPendingItemIndex:0];
  } else {
    OCMExpect([mock_web_view_ URL])
        .andReturn([NSURL URLWithString:@"http://www.url.com/0"]);
    [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"
                    backListURLs:nil
                 forwardListURLs:@[ @"http://www.url.com/1" ]];
    navigation_manager()->AddPendingItem(
        GURL("http://www.url.com/0"), Referrer(),
        ui::PAGE_TRANSITION_FORWARD_BACK,
        web::NavigationInitiationType::BROWSER_INITIATED,
        web::NavigationManager::UserAgentOverrideOption::INHERIT);
  }
  ASSERT_EQ(0, navigation_manager()->GetPendingItemIndex());

  delegate_.SetWebState(&web_state_);
  web_state_.SetLoading(true);
  OCMExpect([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.url.com/0"]);
  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  OCMExpect([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.url.com/0"]);
  EXPECT_EQ("http://www.url.com/1",
            navigation_manager()->GetVisibleItem()->GetURL().spec());
}

TEST_P(NavigationManagerTest, VisibleItemDefaultsToLastCommittedItem) {
  delegate_.SetWebState(&web_state_);
  web_state_.SetLoading(true);

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  OCMExpect([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.url.com/0"]);
  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  OCMExpect([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.url.com/0"]);
  EXPECT_EQ("http://www.url.com/0",
            navigation_manager()->GetVisibleItem()->GetURL().spec());
}

// Tests that |extra_headers| and |post_data| from WebLoadParams are added to
// the new navigation item if they are present.
TEST_P(NavigationManagerTest, LoadURLWithParamsWithExtraHeadersAndPostData) {
  NavigationManager::WebLoadParams params(GURL("http://www.url.com/0"));
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  params.extra_headers = @{@"Content-Type" : @"text/plain"};
  params.post_data = [NSData data];

  EXPECT_CALL(navigation_manager_delegate(), RecordPageStateInNavigationItem())
      .Times(1);
  EXPECT_CALL(navigation_manager_delegate(), ClearTransientContent()).Times(1);
  EXPECT_CALL(navigation_manager_delegate(), ClearDialogs()).Times(1);
  EXPECT_CALL(navigation_manager_delegate(),
              LoadCurrentItem(NavigationInitiationType::BROWSER_INITIATED))
      .Times(1);

  navigation_manager()->LoadURLWithParams(params);

  NavigationItem* pending_item = navigation_manager()->GetPendingItem();
  ASSERT_TRUE(pending_item);
  EXPECT_EQ("http://www.url.com/0", pending_item->GetURL().spec());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(pending_item->GetTransitionType(),
                                           ui::PAGE_TRANSITION_TYPED));
  EXPECT_NSEQ(pending_item->GetHttpRequestHeaders(),
              @{@"Content-Type" : @"text/plain"});
  EXPECT_TRUE(pending_item->HasPostData());
}

// Tests that LoadURLWithParams() calls RecordPageStateInNavigationItem() on the
// navigation manager deleget before navigating to the new URL.
TEST_P(NavigationManagerTest, LoadURLWithParamsSavesStateOnCurrentItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  NavigationManager::WebLoadParams params(GURL("http://www.url.com/1"));
  params.transition_type = ui::PAGE_TRANSITION_TYPED;

  EXPECT_CALL(navigation_manager_delegate(), RecordPageStateInNavigationItem())
      .Times(1);
  EXPECT_CALL(navigation_manager_delegate(), ClearTransientContent()).Times(1);
  EXPECT_CALL(navigation_manager_delegate(), ClearDialogs()).Times(1);
  EXPECT_CALL(navigation_manager_delegate(),
              LoadCurrentItem(NavigationInitiationType::BROWSER_INITIATED))
      .Times(1);

  navigation_manager()->LoadURLWithParams(params);

  NavigationItem* last_committed_item =
      navigation_manager()->GetLastCommittedItem();
  ASSERT_TRUE(last_committed_item);
  EXPECT_EQ("http://www.url.com/0", last_committed_item->GetURL().spec());
  NavigationItem* pending_item = navigation_manager()->GetPendingItem();
  ASSERT_TRUE(pending_item);
  EXPECT_EQ("http://www.url.com/1", pending_item->GetURL().spec());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(pending_item->GetTransitionType(),
                                           ui::PAGE_TRANSITION_TYPED));
  EXPECT_FALSE(pending_item->HasPostData());
}

TEST_P(NavigationManagerTest, UpdatePendingItemWithoutPendingItem) {
  navigation_manager()->UpdatePendingItemUrl(GURL("http://another.url.com"));
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
}

TEST_P(NavigationManagerTest, UpdatePendingItemWithPendingItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->UpdatePendingItemUrl(GURL("http://another.url.com"));

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ("http://another.url.com/",
            navigation_manager()->GetPendingItem()->GetURL().spec());
}

TEST_P(NavigationManagerTest,
       UpdatePendingItemWithPendingItemAlreadyCommitted) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();
  navigation_manager()->UpdatePendingItemUrl(GURL("http://another.url.com"));

  ASSERT_EQ(1, navigation_manager()->GetItemCount());
  EXPECT_EQ("http://www.url.com/",
            navigation_manager()->GetItemAtIndex(0)->GetURL().spec());
}

// Tests that LoadCurrentItem() is exercised when going to a different page.
TEST_P(NavigationManagerTest, GoToIndexDifferentDocument) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_FALSE(navigation_manager()->GetItemAtIndex(0)->GetTransitionType() &
               ui::PAGE_TRANSITION_FORWARD_BACK);

  EXPECT_CALL(navigation_manager_delegate(), RecordPageStateInNavigationItem());
  EXPECT_CALL(navigation_manager_delegate(), ClearTransientContent());
  EXPECT_CALL(navigation_manager_delegate(), ClearDialogs());
  EXPECT_CALL(navigation_manager_delegate(), WillChangeUserAgentType())
      .Times(0);

  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    EXPECT_CALL(navigation_manager_delegate(),
                OnGoToIndexSameDocumentNavigation(
                    NavigationInitiationType::BROWSER_INITIATED,
                    /*has_user_gesture=*/true))
        .Times(0);
    EXPECT_CALL(navigation_manager_delegate(),
                LoadCurrentItem(NavigationInitiationType::BROWSER_INITIATED));
  }

  navigation_manager()->GoToIndex(0);
  EXPECT_TRUE(navigation_manager()->GetItemAtIndex(0)->GetTransitionType() &
              ui::PAGE_TRANSITION_FORWARD_BACK);

  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    // Since LoadCurrentItem() is noop in test, we can only verify that the
    // pending item index has been updated to the GoToIndex() destination.
    EXPECT_EQ(0, navigation_manager()->GetPendingItemIndex());
  }
}

// Tests navigation between different documents with pending item that has the
// same-document as destigation navigation item. Test case for crbug.com/832734.
TEST_P(NavigationManagerTest,
       GoToIndexDifferentDocumentPassingThroghSameDocument) {
  if (GetParam() != TEST_LEGACY_NAVIGATION_MANAGER) {
    // crbug.com/832734 could not happen with WK-based navigation manager.
    return;
  }

  // First and second items are same documents. Third one is a different
  // document.
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/#hash"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()
      ->GetPendingItemInCurrentOrRestoredSession()
      ->SetIsCreatedFromHashChange(true);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/#hash"
                  backListURLs:@[ @"http://www.url.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_
        setCurrentURL:@"http://www.url2.com"
         backListURLs:@[ @"http://www.url.com", @"http://www.url.com/#hash" ]
      forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  // Simulate pending navigation to the second item (different document
  // navigation).
  [session_controller() setPendingItemIndex:1];

  // Navigate to first item, which is still a different document navigation.
  EXPECT_EQ(2, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_EQ(1, navigation_manager()->GetPendingItemIndex());
  EXPECT_FALSE(navigation_manager()->GetItemAtIndex(0)->GetTransitionType() &
               ui::PAGE_TRANSITION_FORWARD_BACK);

  EXPECT_CALL(navigation_manager_delegate(), RecordPageStateInNavigationItem());
  EXPECT_CALL(navigation_manager_delegate(), ClearTransientContent());
  EXPECT_CALL(navigation_manager_delegate(), ClearDialogs());
  EXPECT_CALL(navigation_manager_delegate(), WillChangeUserAgentType())
      .Times(0);

  EXPECT_CALL(navigation_manager_delegate(),
              OnGoToIndexSameDocumentNavigation(
                  NavigationInitiationType::BROWSER_INITIATED,
                  /*has_user_gesture=*/true))
      .Times(0);
  EXPECT_CALL(navigation_manager_delegate(),
              LoadCurrentItem(NavigationInitiationType::BROWSER_INITIATED));

  navigation_manager()->GoToIndex(0);
  EXPECT_TRUE(navigation_manager()->GetItemAtIndex(0)->GetTransitionType() &
              ui::PAGE_TRANSITION_FORWARD_BACK);

  // Since LoadCurrentItem() is noop in test, verify that the pending item index
  // has been updated to the GoToIndex() destination.
  EXPECT_EQ(0, navigation_manager()->GetPendingItemIndex());
}

// Tests that LoadCurrentItem() is not exercised for same-document navigation.
TEST_P(NavigationManagerTest, GoToIndexSameDocument) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0#hash"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  static_cast<NavigationItemImpl*>(navigation_manager()->GetPendingItem())
      ->SetIsCreatedFromHashChange(true);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0#hash"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_FALSE(navigation_manager()->GetItemAtIndex(0)->GetTransitionType() &
               ui::PAGE_TRANSITION_FORWARD_BACK);

  EXPECT_CALL(navigation_manager_delegate(), RecordPageStateInNavigationItem());
  EXPECT_CALL(navigation_manager_delegate(), ClearTransientContent());
  EXPECT_CALL(navigation_manager_delegate(), ClearDialogs());
  EXPECT_CALL(navigation_manager_delegate(), WillChangeUserAgentType())
      .Times(0);

  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    EXPECT_CALL(navigation_manager_delegate(),
                OnGoToIndexSameDocumentNavigation(
                    NavigationInitiationType::BROWSER_INITIATED,
                    /*has_user_gesture=*/true));
    EXPECT_CALL(navigation_manager_delegate(), LoadCurrentItem(testing::_))
        .Times(0);
  }

  navigation_manager()->GoToIndex(0);
  EXPECT_TRUE(navigation_manager()->GetItemAtIndex(0)->GetTransitionType() &
              ui::PAGE_TRANSITION_FORWARD_BACK);

  if (GetParam() == TEST_LEGACY_NAVIGATION_MANAGER) {
    EXPECT_EQ(0, navigation_manager()->GetLastCommittedItemIndex());
  }
}

// Tests that WillChangeUserAgentType() is triggered when going to a navigation
// item of different user agent type.
TEST_P(NavigationManagerTest, GoToIndexDifferentUserAgentType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_CALL(navigation_manager_delegate(), WillChangeUserAgentType());
  navigation_manager()->GoToIndex(0);
  EXPECT_TRUE(navigation_manager()->GetItemAtIndex(0)->GetTransitionType() &
              ui::PAGE_TRANSITION_FORWARD_BACK);
}

// Tests that NavigationManagerImpl::CommitPendingItem() is no-op when called
// with null.
TEST_P(NavigationManagerTest, CommitNilPendingItem) {
  ASSERT_EQ(0, navigation_manager()->GetItemCount());
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:nil
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem(nullptr);

  EXPECT_EQ(1, navigation_manager()->GetItemCount());
  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ("http://www.url.com/0",
            navigation_manager()->GetLastCommittedItem()->GetURL());
}

// Tests that NavigationManagerImpl::CommitPendingItem() for an invalid URL
// doesn't crash.
TEST_P(NavigationManagerTest, CommitEmptyPendingItem) {
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:nil
               forwardListURLs:nil];

  // Call CommitPendingItem() with a valid pending item.
  auto item = std::make_unique<web::NavigationItemImpl>();
  item->SetURL(GURL::EmptyGURL());
  navigation_manager()->CommitPendingItem(std::move(item));
}

// Tests NavigationManagerImpl::CommitPendingItem() with a valid pending item.
TEST_P(NavigationManagerTest, CommitNonNilPendingItem) {
  // Create navigation manager with a single forward item and no back items.
  [mock_wk_list_ setCurrentURL:@"http://www.url.test"
                  backListURLs:@[
                    @"www.url.test/0",
                  ]
               forwardListURLs:nil];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.test/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->CommitPendingItem();
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.test/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  navigation_manager()->CommitPendingItem();
  SimulateGoToIndex(0);
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    mock_wk_list_.backList = @[ mock_wk_list_.currentItem ];
    mock_wk_list_.currentItem =
        [CRWFakeBackForwardList itemWithURLString:@"http://www.url.com/new"];
    mock_wk_list_.forwardList = nil;
    ASSERT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  } else {
    ASSERT_EQ(0, navigation_manager()->GetLastCommittedItemIndex());
  }

  ASSERT_EQ(2, navigation_manager()->GetItemCount());

  // Emulate 2 simultanious navigations to verify that pending item index does
  // not prevent passed item commit.
  navigation_manager()->SetPendingItemIndex(0);

  // Call CommitPendingItem() with a valid pending item.
  auto item = std::make_unique<web::NavigationItemImpl>();
  item->SetURL(GURL("http://www.url.com/new"));
  item->SetNavigationInitiationType(
      web::NavigationInitiationType::BROWSER_INITIATED);
  navigation_manager()->CommitPendingItem(std::move(item));

  // Verify navigation manager and navigation item states.
  if (web::GetWebClient()->IsSlimNavigationManagerEnabled()) {
    EXPECT_EQ(1, navigation_manager()->GetPreviousItemIndex());
  } else {
    EXPECT_EQ(0, navigation_manager()->GetPreviousItemIndex());
  }
  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_EQ(0, navigation_manager()->GetPendingItemIndex());
  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_FALSE(
      navigation_manager()->GetLastCommittedItem()->GetTimestamp().is_null());
  EXPECT_EQ(web::NavigationInitiationType::NONE,
            navigation_manager()
                ->GetLastCommittedItemImpl()
                ->NavigationInitiationType());
  ASSERT_EQ(2, navigation_manager()->GetItemCount());
  EXPECT_EQ(navigation_manager()->GetLastCommittedItem(),
            navigation_manager()->GetItemAtIndex(1));
}

TEST_P(NavigationManagerTest, LoadIfNecessary) {
  EXPECT_CALL(navigation_manager_delegate(), LoadIfNecessary()).Times(1);
  navigation_manager()->LoadIfNecessary();
}

// Tests that GetCurrentItemImpl() returns the transient item, pending item or
// last committed item in that precedence order.
TEST_P(NavigationManagerTest, GetCurrentItemImpl) {
  ASSERT_EQ(nullptr, navigation_manager()->GetCurrentItemImpl());

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();
  NavigationItem* last_committed_item =
      navigation_manager()->GetLastCommittedItem();
  ASSERT_NE(last_committed_item, nullptr);
  EXPECT_EQ(last_committed_item, navigation_manager()->GetCurrentItemImpl());

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  NavigationItem* pending_item = navigation_manager()->GetPendingItem();
  ASSERT_NE(pending_item, nullptr);
  EXPECT_EQ(pending_item, navigation_manager()->GetCurrentItemImpl());

  navigation_manager()->AddTransientItem(GURL("http://www.url.com/2"));
  NavigationItem* transient_item = navigation_manager()->GetTransientItem();
  ASSERT_NE(transient_item, nullptr);
  EXPECT_EQ(transient_item, navigation_manager()->GetCurrentItemImpl());
}

TEST_P(NavigationManagerTest, UpdateCurrentItemForReplaceState) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"),
      Referrer(GURL("http://referrer.com"), ReferrerPolicyDefault),
      ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  // Tests that pending item can be replaced.
  GURL replace_page_url("http://www.url.com/replace");
  NSString* state_object = @"{'foo': 1}";

  // Replace current item and check history size and fields of the modified
  // item.
  navigation_manager()->UpdateCurrentItemForReplaceState(replace_page_url,
                                                         state_object);

  EXPECT_EQ(0, navigation_manager()->GetItemCount());
  auto* pending_item =
      static_cast<NavigationItemImpl*>(navigation_manager()->GetPendingItem());
  EXPECT_EQ(replace_page_url, pending_item->GetURL());
  EXPECT_FALSE(pending_item->IsCreatedFromPushState());
  EXPECT_NSEQ(state_object, pending_item->GetSerializedStateObject());
  EXPECT_EQ(GURL("http://referrer.com"), pending_item->GetReferrer().url);

  // Commit pending item and tests that replace updates the committed item.
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  // Replace current item again and check history size and fields.
  GURL replace_page_url2("http://www.url.com/replace2");
  navigation_manager()->UpdateCurrentItemForReplaceState(replace_page_url2,
                                                         nil);

  EXPECT_EQ(1, navigation_manager()->GetItemCount());
  auto* last_committed_item = static_cast<NavigationItemImpl*>(
      navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(replace_page_url2, last_committed_item->GetURL());
  EXPECT_FALSE(last_committed_item->IsCreatedFromPushState());
  EXPECT_NSEQ(nil, last_committed_item->GetSerializedStateObject());
  EXPECT_EQ(GURL("http://referrer.com"),
            last_committed_item->GetReferrer().url);
}

// Tests SetPendingItem() and ReleasePendingItem() methods.
TEST_P(NavigationManagerTest, TransferPendingItem) {
  auto item = std::make_unique<web::NavigationItemImpl>();
  web::NavigationItemImpl* item_ptr = item.get();

  navigation_manager()->SetPendingItem(std::move(item));
  EXPECT_EQ(item_ptr, navigation_manager()->GetPendingItem());

  auto extracted_item = navigation_manager()->ReleasePendingItem();
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(item_ptr, extracted_item.get());
}

INSTANTIATE_TEST_SUITE_P(
    ProgrammaticNavigationManagerTest,
    NavigationManagerTest,
    ::testing::Values(
        NavigationManagerChoice::TEST_LEGACY_NAVIGATION_MANAGER,
        NavigationManagerChoice::TEST_WK_BASED_NAVIGATION_MANAGER));

class NavigationManagerImplUtilTest : public PlatformTest {
 protected:
  web::TestNavigationManager nav_manager_;
};

// Tests that empty navigation manager returns nullptr.
TEST_F(NavigationManagerImplUtilTest, TestLastNonRedirectedItemEmpty) {
  EXPECT_FALSE(
      NavigationManagerImpl::GetLastCommittedNonRedirectedItem(nullptr));
  EXPECT_FALSE(
      NavigationManagerImpl::GetLastCommittedNonRedirectedItem(&nav_manager_));
}

// Tests that typed in URL works correctly.
TEST_F(NavigationManagerImplUtilTest, TestLastNonRedirectedItemTypedUrl) {
  nav_manager_.AddItem(GURL("http://foo.com/page0"), ui::PAGE_TRANSITION_TYPED);
  NavigationItem* item =
      NavigationManagerImpl::GetLastCommittedNonRedirectedItem(&nav_manager_);
  ASSERT_TRUE(item);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      item->GetTransitionType(), ui::PAGE_TRANSITION_TYPED));
}

// Tests that link click works correctly.
TEST_F(NavigationManagerImplUtilTest, TestLastNonRedirectedItemLinkClicked) {
  nav_manager_.AddItem(GURL("http://foo.com/page0"), ui::PAGE_TRANSITION_LINK);
  NavigationItem* item =
      NavigationManagerImpl::GetLastCommittedNonRedirectedItem(&nav_manager_);
  ASSERT_TRUE(item);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      item->GetTransitionType(), ui::PAGE_TRANSITION_LINK));
}

// Tests that redirect items are skipped.
TEST_F(NavigationManagerImplUtilTest,
       TestLastNonRedirectedItemLinkMultiRedirects) {
  nav_manager_.AddItem(GURL("http://foo.com/page0"), ui::PAGE_TRANSITION_LINK);
  nav_manager_.AddItem(GURL("http://bar.com/redir1"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  nav_manager_.AddItem(GURL("http://bar.com/redir2"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  NavigationItem* item =
      NavigationManagerImpl::GetLastCommittedNonRedirectedItem(&nav_manager_);
  ASSERT_TRUE(item);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      item->GetTransitionType(), ui::PAGE_TRANSITION_LINK));
}

// Tests that when all items are redirects, nullptr is returned.
TEST_F(NavigationManagerImplUtilTest, TestLastNonRedirectedItemAllRedirects) {
  nav_manager_.AddItem(GURL("http://bar.com/redir0"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  nav_manager_.AddItem(GURL("http://bar.com/redir1"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  nav_manager_.AddItem(GURL("http://bar.com/redir2"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  NavigationItem* item =
      NavigationManagerImpl::GetLastCommittedNonRedirectedItem(&nav_manager_);
  EXPECT_FALSE(item);
}

// Tests that earlier redirects are not found.
TEST_F(NavigationManagerImplUtilTest, TestLastNonRedirectedItemNotEarliest) {
  nav_manager_.AddItem(GURL("http://foo.com/bookmark"),
                       ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  nav_manager_.AddItem(GURL("http://foo.com/page0"), ui::PAGE_TRANSITION_TYPED);
  nav_manager_.AddItem(GURL("http://bar.com/redir1"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  nav_manager_.AddItem(GURL("http://bar.com/redir2"),
                       ui::PAGE_TRANSITION_CLIENT_REDIRECT);
  NavigationItem* item =
      NavigationManagerImpl::GetLastCommittedNonRedirectedItem(&nav_manager_);
  ASSERT_TRUE(item);
  EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
      item->GetTransitionType(), ui::PAGE_TRANSITION_TYPED));
}

}  // namespace web
