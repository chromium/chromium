// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_manager_impl.h"

#import <array>
#import <string>

#import "base/apple/foundation_util.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/escape.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/test/scoped_feature_list.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/navigation/wk_navigation_util.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/reload_type.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/proto/navigation.pb.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/web_client.h"
#import "ios/web/test/fakes/crw_fake_back_forward_list.h"
#import "ios/web/test/fakes/crw_fake_web_view_navigation_proxy.h"
#import "ios/web/test/test_url_constants.h"
#import "ios/web/web_state/ui/crw_web_view_navigation_proxy.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/base/apple/url_conversions.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "url/scheme_host_port.h"
#import "url/url_util.h"

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {
namespace {

// URL scheme that will be rewritten by UrlRewriter installed in
// NavigationManagerTest fixture. Scheme will be changed to kTestWebUIScheme.
const char kSchemeToRewrite[] = "navigationmanagerschemetorewrite";

// URLs used for session restoration tests.
const char kTestURL1[] = "about://new-tab";
const char kTestURL2[] = "about://version";
const char* const kTestURLs[] = {kTestURL1, kTestURL2};

// Replaces `kSchemeToRewrite` scheme with `kTestWebUIScheme`.
bool UrlRewriter(GURL* url, BrowserState* browser_state) {
  if (url->scheme() == kSchemeToRewrite) {
    GURL::Replacements scheme_replacements;
    scheme_replacements.SetSchemeStr(kTestWebUIScheme);
    *url = url->ReplaceComponents(scheme_replacements);
    return true;
  }
  return false;
}

// Query parameter that will be appended by AppendingUrlRewriter if it is
// installed into NavigationManager by a test case.
const char kRewrittenQueryParam[] = "navigationmanagerrewrittenquery";

// Appends `kRewrittenQueryParam` to `url`.
bool AppendingUrlRewriter(GURL* url, BrowserState* browser_state) {
  GURL::Replacements query_replacements;
  query_replacements.SetQueryStr(kRewrittenQueryParam);
  *url = url->ReplaceComponents(query_replacements);
  return false;
}

// Mock class for NavigationManagerDelegate.
class MockNavigationManagerDelegate : public NavigationManagerDelegate {
 public:
  void SetWKWebView(id web_view) { mock_web_view_ = web_view; }
  void SetWebState(WebState* web_state) { web_state_ = web_state; }
  void RemoveWebView() override {
    // Simulate removing the web view.
    mock_web_view_ = nil;
  }

  MOCK_METHOD0(ClearDialogs, void());
  MOCK_METHOD0(RecordPageStateInNavigationItem, void());
  MOCK_METHOD1(LoadCurrentItem, void(NavigationInitiationType type));
  MOCK_METHOD0(LoadIfNecessary, void());
  MOCK_METHOD0(Reload, void());
  MOCK_METHOD1(OnNavigationItemsPruned, void(size_t));
  MOCK_METHOD1(OnNavigationItemCommitted, void(NavigationItem* item));
  MOCK_METHOD1(SetWebStateUserAgent, void(UserAgentType user_agent_type));
  MOCK_METHOD4(GoToBackForwardListItem,
               void(WKBackForwardListItem*,
                    NavigationItem*,
                    NavigationInitiationType,
                    bool));
  MOCK_METHOD0(GetPendingItem, NavigationItemImpl*());
  MOCK_CONST_METHOD0(GetCurrentURL, GURL());

 private:
  WebState* GetWebState() override { return web_state_; }

  id<CRWWebViewNavigationProxy> GetWebViewNavigationProxy() const override {
    return mock_web_view_;
  }

  id mock_web_view_;
  raw_ptr<WebState> web_state_ = nullptr;
};

// Data holder for the informations to be restored in the items.
struct ItemInfoToBeRestored {
  GURL url;
  GURL virtual_url;
  UserAgentType user_agent;
};

}  // namespace

// Test fixture for NavigationManagerImpl testing.
class NavigationManagerTest : public PlatformTest {
 protected:
  NavigationManagerTest() {
    mock_web_view_ = OCMClassMock([WKWebView class]);
    mock_wk_list_ = [[CRWFakeBackForwardList alloc] init];
    OCMStub([mock_web_view_ backForwardList]).andReturn(mock_wk_list_);
    delegate_.SetWKWebView(mock_web_view_);

    // Setup rewriter.
    BrowserURLRewriter::GetInstance()->AddURLRewriter(UrlRewriter);
    url::AddStandardScheme(kSchemeToRewrite, url::SCHEME_WITH_HOST);

    manager_ =
        std::make_unique<NavigationManagerImpl>(&browser_state_, &delegate_);
  }

  NavigationManagerImpl* navigation_manager() { return manager_.get(); }

  MockNavigationManagerDelegate& navigation_manager_delegate() {
    return delegate_;
  }

  // Manipulates the underlying session state to simulate the effect of
  // GoToIndex() on the navigation manager to facilitate testing of other
  // NavigationManager APIs. NavigationManager::GoToIndex() itself is tested
  // separately by verifying expectations on the delegate method calls.
  void SimulateGoToIndex(int index) {
    [mock_wk_list_ moveCurrentToIndex:index];
  }

  // Makes delegate to return navigation item, which is stored in navigation
  // context in the real app.
  void SimulateReturningPendingItemFromDelegate(web::NavigationItemImpl* item) {
      ON_CALL(navigation_manager_delegate(), GetPendingItem())
          .WillByDefault(testing::Return(item));
  }

  CRWFakeBackForwardList* mock_wk_list_;
  id mock_web_view_;
  base::HistogramTester histogram_tester_;

 protected:
  FakeBrowserState browser_state_;
  FakeWebState web_state_;
  MockNavigationManagerDelegate delegate_;
  base::test::ScopedFeatureList feature_;
  std::unique_ptr<NavigationManagerImpl> manager_;

 private:
  url::ScopedSchemeRegistryForTests scoped_registry_;
};

// Tests state of an empty navigation manager.
TEST_F(NavigationManagerTest, EmptyManager) {
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
  EXPECT_EQ(-1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_EQ(-1, navigation_manager()->GetIndexForOffset(0));
}

// Tests that GetPendingItemIndex() returns -1 if there is no pending entry.
TEST_F(NavigationManagerTest, GetPendingItemIndexWithoutPendingEntry) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
}

// Tests that GetPendingItemIndex() returns -1 if there is a pending item.
TEST_F(NavigationManagerTest, GetPendingItemIndexWithPendingEntry) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
}

// Tests that setting and getting PendingItemIndex.
TEST_F(NavigationManagerTest, SetAndGetPendingItemIndex) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.test"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  [mock_wk_list_ setCurrentURL:@"http://www.url.test"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->SetPendingItemIndex(0);
  EXPECT_EQ(0, navigation_manager()->GetPendingItemIndex());
}

// Tests that GetPendingItemIndex() returns correct index.
TEST_F(NavigationManagerTest, GetPendingItemIndexWithIndexedPendingEntry) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"
                  backListURLs:@[ @"http://www.url.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
}

// Tests that NavigationManagerImpl::GetPendingItem() returns item provided by
// the delegate.
TEST_F(NavigationManagerTest, GetPendingItemFromDelegate) {
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  auto item = std::make_unique<web::NavigationItemImpl>();
  SimulateReturningPendingItemFromDelegate(item.get());
  EXPECT_EQ(item.get(), navigation_manager()->GetPendingItem());
}

// Tests that NavigationManagerImpl::GetPendingItem() ignores item provided by
// the delegate if navigation manager has own pending item.
TEST_F(NavigationManagerTest, GetPendingItemIgnoringDelegate) {
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  auto item = std::make_unique<web::NavigationItemImpl>();
  SimulateReturningPendingItemFromDelegate(item.get());

  GURL url("http://www.url.test");
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  EXPECT_NE(item.get(), navigation_manager()->GetPendingItem());
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(url, navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that GetPendingItem() returns indexed pending item.
TEST_F(NavigationManagerTest, GetPendingItemWithIndexedPendingEntry) {
  GURL url("http://www.url.test");
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  [mock_wk_list_ setCurrentURL:@"http://www.url.test"];
  navigation_manager()->CommitPendingItem();
  navigation_manager()->SetPendingItemIndex(0);

  auto item = std::make_unique<web::NavigationItemImpl>();
  SimulateReturningPendingItemFromDelegate(item.get());

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_NE(item.get(), navigation_manager()->GetPendingItem());
  EXPECT_EQ(url, navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that going back or negative offset is not possible without a committed
// item.
TEST_F(NavigationManagerTest, CanGoBackWithoutCommitedItem) {
  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests that going back or negative offset is not possible if there is ony one
// committed item.
TEST_F(NavigationManagerTest, CanGoBackWithSingleCommitedItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  EXPECT_FALSE(navigation_manager()->CanGoBack());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(-1));
}

// Tests going back possibility with multiple committed items.
TEST_F(NavigationManagerTest, CanGoBackWithMultipleCommitedItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"
                  backListURLs:@[ @"http://www.url.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

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
TEST_F(NavigationManagerTest, CanGoForwardWithoutCommitedItem) {
  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests that going forward or positive offset is not possible if there is ony
// one committed item.
TEST_F(NavigationManagerTest, CanGoForwardWithSingleCommitedItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/"];
  navigation_manager()->CommitPendingItem();

  EXPECT_FALSE(navigation_manager()->CanGoForward());
  EXPECT_FALSE(navigation_manager()->CanGoToOffset(1));
}

// Tests going forward possibility with multiple committed items.
TEST_F(NavigationManagerTest, CanGoForwardWithMultipleCommitedEntries) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"
                  backListURLs:@[ @"http://www.url.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

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
TEST_F(NavigationManagerTest, OffsetsWithoutPendingIndex) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/redirect"), Referrer(),
      ui::PAGE_TRANSITION_CLIENT_REDIRECT,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/redirect"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[
                    @"http://www.url.com/0", @"http://www.url.com/redirect"
                  ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

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
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/redirect"
                  backListURLs:@[
                    @"http://www.url.com/0", @"http://www.url.com/redirect",
                    @"http://www.url.com/1", @"http://www.url.com/2"
                  ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  ASSERT_EQ(5, navigation_manager()->GetItemCount());
  ASSERT_EQ(4, navigation_manager()->GetLastCommittedItemIndex());
}

// Tests that when given a pending item, adding a new pending item replaces the
// existing pending item if their URLs are different.
TEST_F(NavigationManagerTest, ReplacePendingItemIfDifferentURL) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(existing_url, navigation_manager()->GetPendingItem()->GetURL());
  EXPECT_EQ(web::HttpsUpgradeType::kNone,
            navigation_manager()->GetPendingItem()->GetHttpsUpgradeType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  GURL new_url1 = GURL("http://www.new1.com");
  navigation_manager()->AddPendingItem(
      new_url1, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(new_url1, navigation_manager()->GetPendingItem()->GetURL());
  EXPECT_EQ(web::HttpsUpgradeType::kNone,
            navigation_manager()->GetPendingItem()->GetHttpsUpgradeType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  GURL new_url2 = GURL("http://www.new2.com");
  navigation_manager()->AddPendingItem(
      new_url2, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kHttpsOnlyMode);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(new_url2, navigation_manager()->GetPendingItem()->GetURL());
  EXPECT_EQ(web::HttpsUpgradeType::kHttpsOnlyMode,
            navigation_manager()->GetPendingItem()->GetHttpsUpgradeType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL doesn't replace the existing pending item if new pending item is not a
// form submission.
TEST_F(NavigationManagerTest, NotReplaceSameUrlPendingItemIfNotFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(web::HttpsUpgradeType::kNone,
            navigation_manager()->GetPendingItem()->GetHttpsUpgradeType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());

  // NavigationManagerImpl assumes that AddPendingItem() is only called for
  // new navigation, so it always creates a new pending item.
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_LINK));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  // Try again with a pending item that uses https as the default scheme.
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kHttpsOnlyMode);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(web::HttpsUpgradeType::kHttpsOnlyMode,
            navigation_manager()->GetPendingItem()->GetHttpsUpgradeType());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL replaces the existing pending item if new pending item is a form
// submission while existing pending item is not.
TEST_F(NavigationManagerTest, ReplaceSameUrlPendingItemIfFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(web::HttpsUpgradeType::kNone,
            navigation_manager()->GetPendingItem()->GetHttpsUpgradeType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_FORM_SUBMIT,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_FORM_SUBMIT));
  EXPECT_EQ(web::HttpsUpgradeType::kNone,
            navigation_manager()->GetPendingItem()->GetHttpsUpgradeType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  // Try again with a pending item that uses https as the default scheme.
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_FORM_SUBMIT,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kHttpsOnlyMode);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_FORM_SUBMIT));
  EXPECT_EQ(web::HttpsUpgradeType::kHttpsOnlyMode,
            navigation_manager()->GetPendingItem()->GetHttpsUpgradeType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL doesn't replace the existing pending item if the user agent override
// option is INHERIT.
TEST_F(NavigationManagerTest, NotReplaceSameUrlPendingItemIfOverrideInherit) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());

  // NavigationManagerImpl assumes that AddPendingItem() is only called for
  // new navigation, so it always creates a new pending item.
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_LINK));
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL replaces the existing pending item.
TEST_F(NavigationManagerTest, ReplaceSameUrlPendingItem) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(web::HttpsUpgradeType::kNone,
            navigation_manager()->GetPendingItem()->GetHttpsUpgradeType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(web::HttpsUpgradeType::kNone,
            navigation_manager()->GetPendingItem()->GetHttpsUpgradeType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kHttpsOnlyMode);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(web::HttpsUpgradeType::kHttpsOnlyMode,
            navigation_manager()->GetPendingItem()->GetHttpsUpgradeType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when given a pending item, adding a new pending item with the same
// URL replaces the existing pending item.
TEST_F(NavigationManagerTest, ReplaceSameUrlPendingItemFromDesktop) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  navigation_manager()->GetPendingItem()->SetUserAgentType(
      UserAgentType::DESKTOP);
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(web::UserAgentType::DESKTOP,
            navigation_manager()->GetPendingItem()->GetUserAgentType());
  EXPECT_EQ(web::HttpsUpgradeType::kNone,
            navigation_manager()->GetPendingItem()->GetHttpsUpgradeType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(web::HttpsUpgradeType::kNone,
            navigation_manager()->GetPendingItem()->GetHttpsUpgradeType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kHttpsOnlyMode);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(web::HttpsUpgradeType::kHttpsOnlyMode,
            navigation_manager()->GetPendingItem()->GetHttpsUpgradeType());
  EXPECT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item
// succeeds if the new item's URL is different from the last committed item.
TEST_F(NavigationManagerTest, AddPendingItemIfDiffernetURL) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  OCMStub([mock_web_view_ URL])
      .andReturn([NSURL URLWithString:@"http://www.existing.com"]);
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_EQ(existing_url,
            navigation_manager()->GetLastCommittedItem()->GetURL());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  GURL new_url = GURL("http://www.new.com");
  navigation_manager()->AddPendingItem(
      new_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(new_url, navigation_manager()->GetPendingItem()->GetURL());
  EXPECT_EQ(web::HttpsUpgradeType::kNone,
            navigation_manager()->GetPendingItem()->GetHttpsUpgradeType());
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL fails if the new item is not form submission.
TEST_F(NavigationManagerTest, NotAddSameUrlPendingItemIfNotFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  OCMStub([mock_web_view_ URL])
      .andReturn([NSURL URLWithString:@"http://www.existing.com"]);
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
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  // NavigationManagerImpl assumes that AddPendingItem() is only called for
  // new navigation, so it always creates a new pending item.
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(navigation_manager()->GetPendingItem(),
            navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_LINK));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL updates the existing committed item if the form submission isn't
// using POST.
TEST_F(NavigationManagerTest, NotAddSameUrlPendingItemIfGETFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  OCMStub([mock_web_view_ URL])
      .andReturn([NSURL URLWithString:@"http://www.existing.com"]);
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
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(navigation_manager()->GetPendingItem(),
            navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_FORM_SUBMIT));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL creates a new pending item if the form submission is using POST.
TEST_F(NavigationManagerTest, AddSameUrlPendingItemIfPOSTFormSubmission) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  OCMStub([mock_web_view_ URL])
      .andReturn([NSURL URLWithString:@"http://www.existing.com"]);
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
      /*is_post_navigation=*/true, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_NE(navigation_manager()->GetPendingItem(),
            navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_FORM_SUBMIT));
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL fails if the user agent override option is INHERIT.
TEST_F(NavigationManagerTest, NotAddSameUrlPendingItemIfOverrideInherit) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  OCMStub([mock_web_view_ URL])
      .andReturn([NSURL URLWithString:@"http://www.existing.com"]);
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  // NavigationManagerImpl assumes that AddPendingItem() is only called for
  // new navigation, so it always creates a new pending item.
  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(navigation_manager()->GetPendingItem(),
            navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_LINK));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that when the last committed item exists, adding a pending item with
// the same URL succeeds.
TEST_F(NavigationManagerTest, AddSameUrlPendingItem) {
  GURL existing_url = GURL("http://www.existing.com");
  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.existing.com"];
  OCMStub([mock_web_view_ URL])
      .andReturn([NSURL URLWithString:@"http://www.existing.com"]);
  navigation_manager()->CommitPendingItem();

  ASSERT_TRUE(navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetLastCommittedItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_TYPED));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());

  navigation_manager()->AddPendingItem(
      existing_url, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(navigation_manager()->GetPendingItem(),
            navigation_manager()->GetLastCommittedItem());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(
      navigation_manager()->GetPendingItem()->GetTransitionType(),
      ui::PAGE_TRANSITION_RELOAD));
  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests that calling `Reload` with web::ReloadType::NORMAL is no-op when there
// are no pending or committed items.
TEST_F(NavigationManagerTest, ReloadEmptyWithNormalType) {
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(0);
  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);

  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());
}

// Tests that calling `Reload` with web::ReloadType::NORMAL leaves the url of
// the renderer initiated pending item unchanged when there is one.
TEST_F(NavigationManagerTest, ReloadRendererPendingItemWithNormalType) {
  GURL url_before_reload = GURL("http://www.url.com");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(1);
  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(url_before_reload,
            navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that calling `Reload` with web::ReloadType::NORMAL leaves the url of
// the user initiated pending item unchanged when there is one.
TEST_F(NavigationManagerTest, ReloadUserPendingItemWithNormalType) {
  GURL url_before_reload = GURL("http://www.url.com");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(1);
  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(url_before_reload,
            navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that calling `Reload` with web::ReloadType::NORMAL leaves the url of
// the last committed item unchanged when there is no pending item.
TEST_F(NavigationManagerTest, ReloadLastCommittedItemWithNormalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  GURL url_before_reload = GURL("http://www.url.com/1");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

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

// Tests that calling `Reload` with web::ReloadType::NORMAL leaves the url of
// the last committed item unchanged when there is no pending item, but there
// forward items after last committed item.
TEST_F(NavigationManagerTest,
       ReloadLastCommittedItemWithNormalTypeWithForwardItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  GURL url_before_reload = GURL("http://www.url.com/1");
  navigation_manager()->AddPendingItem(
      url_before_reload, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

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

// Tests that calling `Reload` with web::ReloadType::ORIGINAL_REQUEST_URL is
// no-op when there are no pending or committed items.
TEST_F(NavigationManagerTest, ReloadEmptyWithOriginalType) {
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());

  EXPECT_CALL(navigation_manager_delegate(), Reload()).Times(0);
  navigation_manager()->Reload(web::ReloadType::ORIGINAL_REQUEST_URL,
                               false /* check_for_repost */);

  ASSERT_FALSE(navigation_manager()->GetPendingItem());
  ASSERT_FALSE(navigation_manager()->GetLastCommittedItem());
}

// Tests that calling `Reload` with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the renderer initiated pending item's url to its original request url
// when there is one.
TEST_F(NavigationManagerTest, ReloadRendererPendingItemWithOriginalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
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

// Tests that calling `Reload` with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the user initiated pending item's url to its original request url
// when there is one.
TEST_F(NavigationManagerTest, ReloadUserPendingItemWithOriginalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
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

// Tests that calling `Reload` with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the last committed item's url to its original request url when there
// is no pending item.
TEST_F(NavigationManagerTest, ReloadLastCommittedItemWithOriginalType) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
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

// Tests that calling `Reload` with web::ReloadType::ORIGINAL_REQUEST_URL
// changes the last committed item's url to its original request url when there
// is no pending item, but there are forward items after last committed item.
TEST_F(NavigationManagerTest,
       ReloadLastCommittedItemWithOriginalTypeWithForwardItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
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
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

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
TEST_F(NavigationManagerTest, ReloadWithUserAgentType) {
  GURL url("http://www.1.com");
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  GURL virtual_url("http://www.1.com/virtual");
  navigation_manager()->GetPendingItem()->SetVirtualURL(virtual_url);
  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();
  OCMStub([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.1.com"]);

  EXPECT_CALL(navigation_manager_delegate(), RecordPageStateInNavigationItem());
  EXPECT_CALL(navigation_manager_delegate(), ClearDialogs());
  EXPECT_CALL(navigation_manager_delegate(),
              LoadCurrentItem(NavigationInitiationType::BROWSER_INITIATED));

  navigation_manager()->ReloadWithUserAgentType(UserAgentType::DESKTOP);

  NavigationItem* pending_item = navigation_manager()->GetPendingItem();
  EXPECT_EQ(url, pending_item->GetURL());
  EXPECT_EQ(virtual_url, pending_item->GetVirtualURL());
  EXPECT_EQ(UserAgentType::DESKTOP, pending_item->GetUserAgentType());
}

// Tests that ReloadWithUserAgentType reloads on the last committed item before
// the redirect items.
TEST_F(NavigationManagerTest, ReloadWithUserAgentTypeOnRedirect) {
  GURL url("http://www.1.com");
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.redirect.com"), Referrer(),
      ui::PAGE_TRANSITION_CLIENT_REDIRECT,
      NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/redirect"
                  backListURLs:@[ @"http://www.1.com" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->ReloadWithUserAgentType(UserAgentType::DESKTOP);

  NavigationItem* pending_item = navigation_manager()->GetPendingItem();
  EXPECT_EQ(url, pending_item->GetURL());
}

// Tests that ReloadWithUserAgentType reloads on the last committed item if
// there are no item before a redirect (which happens when opening a new tab on
// a redirect).
TEST_F(NavigationManagerTest, ReloadWithUserAgentTypeOnNewTabRedirect) {
  GURL url("http://www.1.com");
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_CLIENT_REDIRECT,
      NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  [mock_wk_list_ setCurrentURL:@"http://www.1.com"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->ReloadWithUserAgentType(UserAgentType::DESKTOP);

  NavigationItem* pending_item = navigation_manager()->GetPendingItem();
  EXPECT_EQ(url, pending_item->GetURL());
}

// Tests that app-specific URLs are not rewritten for renderer-initiated loads
// or reloads unless requested by a page with app-specific url.
TEST_F(NavigationManagerTest, RewritingAppSpecificUrls) {
  // URL should not be rewritten as there is no committed URL.
  GURL url1(url::SchemeHostPort(kSchemeToRewrite, "test", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url1, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::RENDERER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  EXPECT_EQ(url1, navigation_manager()->GetPendingItem()->GetURL());

  // URL should not be rewritten because last committed URL is not app-specific.
  [mock_wk_list_ setCurrentURL:base::SysUTF8ToNSString(url1.spec())];
  navigation_manager()->CommitPendingItem();

  GURL url2(url::SchemeHostPort(kSchemeToRewrite, "test2", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url2, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::RENDERER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  EXPECT_EQ(url2, navigation_manager()->GetPendingItem()->GetURL());

  // URL should not be rewritten for user initiated reload navigations.
  GURL url_reload(
      url::SchemeHostPort(kSchemeToRewrite, "test-reload", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url_reload, Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  EXPECT_EQ(url_reload, navigation_manager()->GetPendingItem()->GetURL());

  // URL should be rewritten for user initiated navigations.
  GURL url3(url::SchemeHostPort(kSchemeToRewrite, "test3", 0).Serialize());
  navigation_manager()->AddPendingItem(
      url3, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
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
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  GURL rewritten_url4(
      url::SchemeHostPort(kTestWebUIScheme, "test4", 0).Serialize());
  EXPECT_EQ(rewritten_url4, navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that transient URLRewriters are applied for pending items.
TEST_F(NavigationManagerTest, ApplyTransientRewriters) {
  navigation_manager()->AddTransientURLRewriter(&AppendingUrlRewriter);
  navigation_manager()->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  NavigationItem* pending_item = navigation_manager()->GetPendingItem();
  EXPECT_EQ(kRewrittenQueryParam, pending_item->GetURL().query());

  // Now that the transient rewriters are consumed, the next URL should not be
  // changed.
  GURL url("http://www.1.com");
  navigation_manager()->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  EXPECT_EQ(url, navigation_manager()->GetPendingItem()->GetURL());
}

// Tests that GetIndexOfItem() returns the correct values.
TEST_F(NavigationManagerTest, GetIndexOfItem) {
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
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  mock_wk_list_.currentItem = wk_item0;
  navigation_manager()->CommitPendingItem();

  NavigationItem* item0 = navigation_manager()->GetLastCommittedItem();
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

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
TEST_F(NavigationManagerTest, TestBackwardForwardItems) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_
        setCurrentURL:@"http://www.url.com/2"
         backListURLs:@[ @"http://www.url.com/0", @"http://www.url.com/1" ]
      forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_EQ(2, navigation_manager()->GetLastCommittedItemIndex());
  std::vector<NavigationItem*> back_items =
      navigation_manager()->GetBackwardItems();
  EXPECT_EQ(2U, back_items.size());
  EXPECT_EQ("http://www.url.com/1", back_items[0]->GetURL().spec());
  EXPECT_EQ("http://www.url.com/0", back_items[1]->GetURL().spec());
  EXPECT_TRUE(navigation_manager()->GetForwardItems().empty());

  SimulateGoToIndex(1);
  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  back_items = navigation_manager()->GetBackwardItems();
  EXPECT_EQ(1U, back_items.size());
  EXPECT_EQ("http://www.url.com/0", back_items[0]->GetURL().spec());
  std::vector<NavigationItem*> forward_items =
      navigation_manager()->GetForwardItems();
  EXPECT_EQ(1U, forward_items.size());
  EXPECT_EQ("http://www.url.com/2", forward_items[0]->GetURL().spec());
}

// Tests that pending item is not considered part of session history so that
// GetBackwardItems returns the second last committed item even if there is a
// pendign item.
TEST_F(NavigationManagerTest, NewPendingItemIsHiddenFromHistory) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_TRUE(navigation_manager()->GetPendingItem());

  std::vector<NavigationItem*> back_items =
      navigation_manager()->GetBackwardItems();
  EXPECT_EQ(1U, back_items.size());
  EXPECT_EQ("http://www.url.com/0", back_items[0]->GetURL().spec());
}

TEST_F(NavigationManagerTest, PendingItemIsVisibleIfNewAndUserInitiated) {
  delegate_.SetWebState(&web_state_);
  web_state_.SetLoading(true);

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

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
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  EXPECT_EQ("http://www.url.com/1",
            navigation_manager()->GetVisibleItem()->GetURL().spec());
}

TEST_F(NavigationManagerTest, PendingItemIsNotVisibleIfNotUserInitiated) {
  delegate_.SetWebState(&web_state_);
  web_state_.SetLoading(true);

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  EXPECT_EQ(nullptr, navigation_manager()->GetVisibleItem());
}

TEST_F(NavigationManagerTest, PendingItemIsNotVisibleIfNotNewNavigation) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  // Move pending item back to index 0.
  OCMStub([mock_web_view_ URL])
      .andReturn([NSURL URLWithString:@"http://www.url.com/0"]);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"
                  backListURLs:nil
               forwardListURLs:@[ @"http://www.url.com/1" ]];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(),
      ui::PAGE_TRANSITION_FORWARD_BACK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  ASSERT_EQ(0, navigation_manager()->GetPendingItemIndex());

  delegate_.SetWebState(&web_state_);
  web_state_.SetLoading(true);
  OCMStub([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.url.com/0"]);
  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  OCMStub([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.url.com/0"]);
  EXPECT_EQ("http://www.url.com/1",
            navigation_manager()->GetVisibleItem()->GetURL().spec());
}

TEST_F(NavigationManagerTest, VisibleItemDefaultsToLastCommittedItem) {
  delegate_.SetWebState(&web_state_);
  web_state_.SetLoading(true);

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::RENDERER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  OCMStub([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.url.com/0"]);
  ASSERT_TRUE(navigation_manager()->GetVisibleItem());
  OCMStub([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.url.com/0"]);
  EXPECT_EQ("http://www.url.com/0",
            navigation_manager()->GetVisibleItem()->GetURL().spec());
}

// Tests that `extra_headers` and `post_data` from WebLoadParams are added to
// the new navigation item if they are present.
TEST_F(NavigationManagerTest, LoadURLWithParamsWithExtraHeadersAndPostData) {
  NavigationManager::WebLoadParams params(GURL("http://www.url.com/0"));
  params.transition_type = ui::PAGE_TRANSITION_TYPED;
  params.extra_headers = @{@"Content-Type" : @"text/plain"};
  params.post_data = [NSData data];

  EXPECT_CALL(navigation_manager_delegate(), RecordPageStateInNavigationItem())
      .Times(1);
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
TEST_F(NavigationManagerTest, LoadURLWithParamsSavesStateOnCurrentItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  NavigationManager::WebLoadParams params(GURL("http://www.url.com/1"));
  params.transition_type = ui::PAGE_TRANSITION_TYPED;

  EXPECT_CALL(navigation_manager_delegate(), RecordPageStateInNavigationItem())
      .Times(1);
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
  EXPECT_EQ(web::HttpsUpgradeType::kNone, pending_item->GetHttpsUpgradeType());
}

TEST_F(NavigationManagerTest, UpdatePendingItemWithoutPendingItem) {
  navigation_manager()->UpdatePendingItemUrl(GURL("http://another.url.com"));
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
}

TEST_F(NavigationManagerTest, UpdatePendingItemWithPendingItem) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  navigation_manager()->UpdatePendingItemUrl(GURL("http://another.url.com"));

  ASSERT_TRUE(navigation_manager()->GetPendingItem());
  EXPECT_EQ("http://another.url.com/",
            navigation_manager()->GetPendingItem()->GetURL().spec());
}

TEST_F(NavigationManagerTest,
       UpdatePendingItemWithPendingItemAlreadyCommitted) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com"];
  navigation_manager()->CommitPendingItem();
  navigation_manager()->UpdatePendingItemUrl(GURL("http://another.url.com"));

  ASSERT_EQ(1, navigation_manager()->GetItemCount());
  EXPECT_EQ("http://www.url.com/",
            navigation_manager()->GetItemAtIndex(0)->GetURL().spec());
}

// Tests that LoadCurrentItem() is exercised when going to a different page.
TEST_F(NavigationManagerTest, GoToIndexDifferentDocument) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  navigation_manager()->CommitPendingItem();

  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_FALSE(navigation_manager()->GetItemAtIndex(0)->GetTransitionType() &
               ui::PAGE_TRANSITION_FORWARD_BACK);

  EXPECT_CALL(navigation_manager_delegate(), RecordPageStateInNavigationItem());
  EXPECT_CALL(navigation_manager_delegate(), ClearDialogs());

  navigation_manager()->GoToIndex(0);
  EXPECT_TRUE(navigation_manager()->GetItemAtIndex(0)->GetTransitionType() &
              ui::PAGE_TRANSITION_FORWARD_BACK);
}

// Tests that LoadCurrentItem() is not exercised for same-document navigation.
TEST_F(NavigationManagerTest, GoToIndexSameDocument) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0#hash"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
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
  EXPECT_CALL(navigation_manager_delegate(), ClearDialogs());

  navigation_manager()->GoToIndex(0);
  EXPECT_TRUE(navigation_manager()->GetItemAtIndex(0)->GetTransitionType() &
              ui::PAGE_TRANSITION_FORWARD_BACK);
}

// Tests that NavigationManagerImpl::CommitPendingItem() is no-op when called
// with null.
TEST_F(NavigationManagerTest, CommitNilPendingItem) {
  ASSERT_EQ(0, navigation_manager()->GetItemCount());
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

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
TEST_F(NavigationManagerTest, CommitEmptyPendingItem) {
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:nil
               forwardListURLs:nil];

  // Call CommitPendingItem() with a valid pending item.
  auto item = std::make_unique<web::NavigationItemImpl>();
  item->SetURL(GURL());
  navigation_manager()->CommitPendingItem(std::move(item));
}

// Tests NavigationManagerImpl::CommitPendingItem() with a valid pending item.
TEST_F(NavigationManagerTest, CommitNonNilPendingItem) {
  // Create navigation manager with a single forward item and no back items.
  [mock_wk_list_ setCurrentURL:@"http://www.url.test"
                  backListURLs:@[
                    @"www.url.test/0",
                  ]
               forwardListURLs:nil];
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.test/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  navigation_manager()->CommitPendingItem();
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.test/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  navigation_manager()->CommitPendingItem();
  SimulateGoToIndex(0);
  mock_wk_list_.backList = @[ mock_wk_list_.currentItem ];
  mock_wk_list_.currentItem =
      [CRWFakeBackForwardList itemWithURLString:@"http://www.url.com/new"];
  mock_wk_list_.forwardList = nil;
  ASSERT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
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

TEST_F(NavigationManagerTest, LoadIfNecessary) {
  EXPECT_CALL(navigation_manager_delegate(), LoadIfNecessary()).Times(1);
  navigation_manager()->LoadIfNecessary();
}

// Tests that GetCurrentItemImpl() returns the pending item or last committed
// item in that precedence order.
TEST_F(NavigationManagerTest, GetCurrentItemImpl) {
  ASSERT_EQ(nullptr, navigation_manager()->GetCurrentItemImpl());

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  navigation_manager()->CommitPendingItem();
  NavigationItem* last_committed_item =
      navigation_manager()->GetLastCommittedItem();
  ASSERT_NE(last_committed_item, nullptr);
  EXPECT_EQ(last_committed_item, navigation_manager()->GetCurrentItemImpl());

  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  NavigationItem* pending_item = navigation_manager()->GetPendingItem();
  ASSERT_NE(pending_item, nullptr);
  EXPECT_EQ(pending_item, navigation_manager()->GetCurrentItemImpl());
}

TEST_F(NavigationManagerTest, UpdateCurrentItemForReplaceState) {
  navigation_manager()->AddPendingItem(
      GURL("http://www.url.com/0"),
      Referrer(GURL("http://referrer.com"), ReferrerPolicyDefault),
      ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

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
  EXPECT_NSEQ(nil, last_committed_item->GetSerializedStateObject());
  EXPECT_EQ(GURL("http://referrer.com"),
            last_committed_item->GetReferrer().url);
}

// Tests SetPendingItem() and ReleasePendingItem() methods.
TEST_F(NavigationManagerTest, TransferPendingItem) {
  auto item = std::make_unique<web::NavigationItemImpl>();
  web::NavigationItemImpl* item_ptr = item.get();

  navigation_manager()->SetPendingItem(std::move(item));
  EXPECT_EQ(item_ptr, navigation_manager()->GetPendingItem());

  auto extracted_item = navigation_manager()->ReleasePendingItem();
  EXPECT_FALSE(navigation_manager()->GetPendingItem());
  EXPECT_EQ(item_ptr, extracted_item.get());
}

// Tests that GetItemAtIndex() on an empty manager will sync navigation items to
// WKBackForwardList using default properties.
TEST_F(NavigationManagerTest, SyncAfterItemAtIndex) {
  EXPECT_EQ(0, manager_->GetItemCount());
  EXPECT_EQ(nullptr, manager_->GetItemAtIndex(0));

  [mock_wk_list_ setCurrentURL:@"http://www.0.com"];
  EXPECT_EQ(1, manager_->GetItemCount());
  EXPECT_EQ(0, manager_->GetLastCommittedItemIndex());

  NavigationItem* item = manager_->GetItemAtIndex(0);
  ASSERT_NE(item, nullptr);
  EXPECT_EQ(GURL("http://www.0.com"), item->GetURL());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK,
                                           item->GetTransitionType()));
  EXPECT_EQ(UserAgentType::NONE, item->GetUserAgentType());
  EXPECT_FALSE(item->GetTimestamp().is_null());
  EXPECT_EQ(web::HttpsUpgradeType::kNone, item->GetHttpsUpgradeType());
}

// Tests that Referrer is inferred from the previous WKBackForwardListItem.
TEST_F(NavigationManagerTest, SyncAfterItemAtIndexWithPreviousItem) {
  [mock_wk_list_ setCurrentURL:@"http://www.1.com"
                  backListURLs:@[ @"http://www.0.com" ]
               forwardListURLs:@[ @"http://www.2.com" ]];
  EXPECT_EQ(3, manager_->GetItemCount());
  EXPECT_EQ(1, manager_->GetLastCommittedItemIndex());

  // The out-of-order access is intentionall to test that syncing doesn't rely
  // on the previous WKBackForwardListItem having an associated NavigationItem.
  NavigationItem* item2 = manager_->GetItemAtIndex(2);
  ASSERT_NE(item2, nullptr);
  EXPECT_EQ(GURL("http://www.2.com"), item2->GetURL());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK,
                                           item2->GetTransitionType()));
  EXPECT_EQ(UserAgentType::NONE, item2->GetUserAgentType());
  EXPECT_EQ(GURL("http://www.1.com"), item2->GetReferrer().url);
  EXPECT_FALSE(item2->GetTimestamp().is_null());

  NavigationItem* item1 = manager_->GetItemAtIndex(1);
  ASSERT_NE(item1, nullptr);
  EXPECT_EQ(GURL("http://www.1.com"), item1->GetURL());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK,
                                           item1->GetTransitionType()));
  EXPECT_EQ(UserAgentType::NONE, item1->GetUserAgentType());
  EXPECT_EQ(GURL("http://www.0.com"), item1->GetReferrer().url);
  EXPECT_FALSE(item1->GetTimestamp().is_null());

  NavigationItem* item0 = manager_->GetItemAtIndex(0);
  ASSERT_NE(item0, nullptr);
  EXPECT_EQ(GURL("http://www.0.com"), item0->GetURL());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK,
                                           item0->GetTransitionType()));
  EXPECT_EQ(UserAgentType::NONE, item0->GetUserAgentType());
  EXPECT_FALSE(item0->GetTimestamp().is_null());
}

// Tests that GetLastCommittedItem() creates a default NavigationItem when the
// last committed item in WKWebView does not have a linked entry.
TEST_F(NavigationManagerTest, SyncInGetLastCommittedItem) {
  [mock_wk_list_ setCurrentURL:@"http://www.0.com"];
  EXPECT_EQ(1, manager_->GetItemCount());

  NavigationItem* item = manager_->GetLastCommittedItem();
  ASSERT_NE(item, nullptr);
  EXPECT_EQ("http://www.0.com/", item->GetURL().spec());
  EXPECT_FALSE(item->GetTimestamp().is_null());
}

// Tests that GetLastCommittedItem() creates a default NavigationItem when the
// last committed item in WKWebView is an app-specific URL.
TEST_F(NavigationManagerTest, SyncInGetLastCommittedItemForAppSpecificURL) {
  GURL url(url::SchemeHostPort(kSchemeToRewrite, "test", 0).Serialize());

  // Verifies that the test URL is rewritten into an app-specific URL.
  manager_->AddPendingItem(url, Referrer(), ui::PAGE_TRANSITION_TYPED,
                           web::NavigationInitiationType::BROWSER_INITIATED,
                           /*is_post_navigation=*/false,
                           /*is_error_navigation=*/false,
                           web::HttpsUpgradeType::kNone);
  NavigationItem* pending_item = manager_->GetPendingItem();
  ASSERT_TRUE(pending_item);
  ASSERT_TRUE(web::GetWebClient()->IsAppSpecificURL(pending_item->GetURL()));

  [mock_wk_list_ setCurrentURL:base::SysUTF8ToNSString(url.spec())];
  NavigationItem* item = manager_->GetLastCommittedItem();

  ASSERT_NE(item, nullptr);
  EXPECT_EQ(url, item->GetURL());
  EXPECT_EQ(1, manager_->GetItemCount());
}

// Tests that CommitPendingItem() will sync navigation items to
// WKBackForwardList and the pending item NavigationItemImpl will be used.
TEST_F(NavigationManagerTest, GetItemAtIndexAfterCommitPending) {
  // Simulate a main frame navigation.
  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  NavigationItem* pending_item0 = manager_->GetPendingItem();

  [mock_wk_list_ setCurrentURL:@"http://www.0.com"];
  manager_->CommitPendingItem();

  EXPECT_EQ(1, manager_->GetItemCount());
  NavigationItem* item = manager_->GetLastCommittedItem();
  EXPECT_EQ(pending_item0, item);
  EXPECT_EQ(GURL("http://www.0.com"), item->GetURL());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_TYPED,
                                           item->GetTransitionType()));

  // Simulate a second main frame navigation.
  manager_->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  NavigationItem* pending_item2 = manager_->GetPendingItem();

  // Simulate an iframe navigation between the two main frame navigations.
  [mock_wk_list_ setCurrentURL:@"http://www.2.com"
                  backListURLs:@[ @"http://www.0.com", @"http://www.1.com" ]
               forwardListURLs:nil];
  manager_->CommitPendingItem();

  EXPECT_EQ(3, manager_->GetItemCount());
  EXPECT_EQ(2, manager_->GetLastCommittedItemIndex());

  // This item is created by syncing.
  NavigationItem* item1 = manager_->GetItemAtIndex(1);
  EXPECT_EQ(GURL("http://www.1.com"), item1->GetURL());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK,
                                           item1->GetTransitionType()));
  EXPECT_EQ(GURL("http://www.0.com"), item1->GetReferrer().url);

  // This item is created by CommitPendingItem.
  NavigationItem* item2 = manager_->GetItemAtIndex(2);
  EXPECT_EQ(pending_item2, item2);
  EXPECT_EQ(GURL("http://www.2.com"), item2->GetURL());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_TYPED,
                                           item2->GetTransitionType()));
  EXPECT_EQ(GURL(""), item2->GetReferrer().url);
}

// Tests that AddPendingItem does not create a new NavigationItem if the new
// pending item is a back forward navigation or when reloading a redirect page.
TEST_F(NavigationManagerTest, ReusePendingItemForHistoryNavigation) {
  // Simulate two regular navigations.
  [mock_wk_list_ setCurrentURL:@"http://www.1.com"
                  backListURLs:@[ @"http://www.0.com" ]
               forwardListURLs:nil];

  // Force sync NavigationItems.
  NavigationItem* original_item0 = manager_->GetItemAtIndex(0);
  manager_->GetItemAtIndex(1);

  // Simulate a back-forward navigation. Manually shuffle the objects in
  // mock_wk_list_ to avoid creating new WKBackForwardListItem mocks and
  // preserve existing NavigationItem associations.
  WKBackForwardListItem* wk_item0 = [mock_wk_list_ itemAtIndex:-1];
  WKBackForwardListItem* wk_item1 = [mock_wk_list_ itemAtIndex:0];
  mock_wk_list_.currentItem = wk_item0;
  mock_wk_list_.backList = nil;
  mock_wk_list_.forwardList = @[ wk_item1 ];
  OCMStub([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.0.com"]);
  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  EXPECT_EQ(original_item0, manager_->GetPendingItem());
}

// Tests that transient URL rewriters are only applied to a new pending item.
TEST_F(NavigationManagerTest, TransientURLRewritersOnlyUsedForPendingItem) {
  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  // Install transient URL rewriters.
  manager_->AddTransientURLRewriter(&AppendingUrlRewriter);
  [mock_wk_list_ setCurrentURL:@"http://www.0.com"];

  // Transient URL rewriters do not apply to lazily synced items.
  NavigationItem* item0 = manager_->GetItemAtIndex(0);
  EXPECT_EQ(GURL("http://www.0.com"), item0->GetURL());

  // Transient URL rewriters are applied to a new pending item.
  manager_->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  EXPECT_EQ(kRewrittenQueryParam, manager_->GetPendingItem()->GetURL().query());
}

// Tests DiscardNonCommittedItems discards pending items.
TEST_F(NavigationManagerTest, DiscardNonCommittedItems) {
  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  EXPECT_NE(nullptr, manager_->GetPendingItem());

  manager_->DiscardNonCommittedItems();
  EXPECT_EQ(nullptr, manager_->GetPendingItem());
}

// Tests that going back is delegated to the underlying WKWebView.
TEST_F(NavigationManagerTest, GoBack) {
  ASSERT_FALSE(manager_->CanGoBack());

  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  [mock_wk_list_ setCurrentURL:@"http://www.0.com"];

  manager_->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  [mock_wk_list_ setCurrentURL:@"http://www.1.com"
                  backListURLs:@[ @"http://www.0.com" ]
               forwardListURLs:nil];

  ASSERT_TRUE(manager_->CanGoBack());

  EXPECT_CALL(delegate_,
              GoToBackForwardListItem(
                  mock_wk_list_.backList[0], manager_->GetItemAtIndex(0),
                  NavigationInitiationType::BROWSER_INITIATED,
                  /*has_user_gesture=*/true));
  manager_->GoBack();
  [mock_web_view_ verify];
}

// Tests that going forward is always delegated to the underlying WKWebView
// without any sanity checks such as whether any forward history exists.
TEST_F(NavigationManagerTest, GoForward) {
  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  [mock_wk_list_ setCurrentURL:@"http://www.0.com"];
  manager_->CommitPendingItem();

  manager_->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  [mock_wk_list_ setCurrentURL:@"http://www.1.com"
                  backListURLs:@[ @"http://www.0.com" ]
               forwardListURLs:nil];

  [mock_wk_list_ moveCurrentToIndex:0];
  ASSERT_TRUE(manager_->CanGoForward());

  EXPECT_CALL(delegate_,
              GoToBackForwardListItem(
                  mock_wk_list_.forwardList[0], manager_->GetItemAtIndex(1),
                  NavigationInitiationType::BROWSER_INITIATED,
                  /*has_user_gesture=*/true));
  manager_->GoForward();
  [mock_web_view_ verify];
}

// Tests that going forward clears uncommitted items.
TEST_F(NavigationManagerTest, GoForwardShouldDiscardsUncommittedItems) {
  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  [mock_wk_list_ setCurrentURL:@"http://www.0.com"];
  manager_->CommitPendingItem();

  manager_->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  [mock_wk_list_ setCurrentURL:@"http://www.1.com"
                  backListURLs:@[ @"http://www.0.com" ]
               forwardListURLs:nil];

  [mock_wk_list_ moveCurrentToIndex:0];
  ASSERT_TRUE(manager_->CanGoForward());

  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  EXPECT_NE(nullptr, manager_->GetPendingItem());

  EXPECT_CALL(delegate_,
              GoToBackForwardListItem(
                  mock_wk_list_.forwardList[0], manager_->GetItemAtIndex(1),
                  NavigationInitiationType::BROWSER_INITIATED,
                  /*has_user_gesture=*/true));
  manager_->GoForward();
  [mock_web_view_ verify];

  EXPECT_EQ(nullptr, manager_->GetPendingItem());
}

// Tests CanGoToOffset API for positive, negative and zero delta.
TEST_F(NavigationManagerTest, CanGoToOffset) {
  manager_->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  manager_->CommitPendingItem();

  manager_->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  manager_->CommitPendingItem();

  manager_->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  [mock_wk_list_
        setCurrentURL:@"http://www.url.com/2"
         backListURLs:@[ @"http://www.url.com/0", @"http://www.url.com/1" ]
      forwardListURLs:nil];
  manager_->CommitPendingItem();

  ASSERT_EQ(3, manager_->GetItemCount());
  ASSERT_EQ(2, manager_->GetLastCommittedItemIndex());

  // Go to entry at index 1 and test API from that state.
  [mock_wk_list_ moveCurrentToIndex:1];
  ASSERT_EQ(1, manager_->GetLastCommittedItemIndex());
  ASSERT_EQ(-1, manager_->GetPendingItemIndex());
  EXPECT_TRUE(manager_->CanGoToOffset(-1));
  EXPECT_EQ(0, manager_->GetIndexForOffset(-1));
  EXPECT_FALSE(manager_->CanGoToOffset(-2));
  EXPECT_TRUE(manager_->CanGoToOffset(1));
  EXPECT_EQ(2, manager_->GetIndexForOffset(1));
  EXPECT_FALSE(manager_->CanGoToOffset(2));
  // Test with large values
  EXPECT_FALSE(manager_->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(manager_->CanGoToOffset(INT_MIN));

  // Go to entry at index 0 and test API from that state.
  [mock_wk_list_ moveCurrentToIndex:0];
  ASSERT_EQ(0, manager_->GetLastCommittedItemIndex());
  ASSERT_EQ(-1, manager_->GetPendingItemIndex());
  EXPECT_FALSE(manager_->CanGoToOffset(-1));
  EXPECT_TRUE(manager_->CanGoToOffset(1));
  EXPECT_EQ(1, manager_->GetIndexForOffset(1));
  EXPECT_TRUE(manager_->CanGoToOffset(2));
  EXPECT_EQ(2, manager_->GetIndexForOffset(2));
  EXPECT_FALSE(manager_->CanGoToOffset(3));
  // Test with large values
  EXPECT_FALSE(manager_->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(manager_->CanGoToOffset(INT_MIN));

  // Go to entry at index 2 and test API from that state.
  [mock_wk_list_ moveCurrentToIndex:2];
  ASSERT_EQ(2, manager_->GetLastCommittedItemIndex());
  ASSERT_EQ(-1, manager_->GetPendingItemIndex());
  EXPECT_TRUE(manager_->CanGoToOffset(-1));
  EXPECT_EQ(1, manager_->GetIndexForOffset(-1));
  EXPECT_TRUE(manager_->CanGoToOffset(-2));
  EXPECT_EQ(0, manager_->GetIndexForOffset(-2));
  EXPECT_FALSE(manager_->CanGoToOffset(1));
  // Test with large values
  EXPECT_FALSE(manager_->CanGoToOffset(INT_MAX));
  EXPECT_FALSE(manager_->CanGoToOffset(INT_MIN));

  // Simulate a history navigation pending item.
  [mock_wk_list_ moveCurrentToIndex:1];
  OCMExpect([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.url.com/1"]);
  manager_->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  EXPECT_EQ(3, manager_->GetItemCount());
  EXPECT_EQ(2, manager_->GetLastCommittedItemIndex());
  EXPECT_EQ(1, manager_->GetPendingItemIndex());
  EXPECT_TRUE(manager_->CanGoToOffset(-1));
  EXPECT_EQ(0, manager_->GetIndexForOffset(-1));
  EXPECT_FALSE(manager_->CanGoToOffset(-2));
  EXPECT_TRUE(manager_->CanGoToOffset(1));
  EXPECT_EQ(2, manager_->GetIndexForOffset(1));
  EXPECT_FALSE(manager_->CanGoToOffset(2));
}

// Tests that Restore() accepts empty session history and performs no-op.
TEST_F(NavigationManagerTest, RestoreSessionWithEmptyHistory) {
  manager_->Restore(-1 /* last_committed_item_index */,
                    std::vector<std::unique_ptr<NavigationItem>>());

  ASSERT_EQ(nullptr, manager_->GetPendingItem());
}

// Tests that all NavigationManager APIs return reasonable values in the Empty
// Window Open Navigation edge case. See comments in header file for details.
TEST_F(NavigationManagerTest, EmptyWindowOpenNavigation) {
  // Set up the precondition for an empty window open item.
  OCMExpect([mock_web_view_ URL])
      .andReturn(net::NSURLWithGURL(GURL(url::kAboutBlankURL)));
  mock_wk_list_.currentItem = nil;

  manager_->AddPendingItem(
      GURL(url::kAboutBlankURL), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::RENDERER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  const NavigationItem* pending_item = manager_->GetPendingItem();
  ASSERT_TRUE(pending_item);
  EXPECT_EQ(-1, manager_->GetPendingItemIndex());
  EXPECT_EQ(url::kAboutBlankURL, pending_item->GetURL().spec());

  manager_->CommitPendingItem();

  const NavigationItem* last_committed_item = manager_->GetLastCommittedItem();
  ASSERT_EQ(pending_item, last_committed_item);
  EXPECT_EQ(last_committed_item, manager_->GetVisibleItem());

  EXPECT_EQ(0, manager_->GetIndexForOffset(0));
  EXPECT_EQ(1, manager_->GetIndexForOffset(1));
  EXPECT_EQ(-1, manager_->GetIndexForOffset(-1));

  EXPECT_EQ(1, manager_->GetItemCount());
  EXPECT_EQ(last_committed_item, manager_->GetItemAtIndex(0));
  EXPECT_FALSE(manager_->GetItemAtIndex(1));

  EXPECT_EQ(0, manager_->GetIndexOfItem(last_committed_item));
  EXPECT_EQ(-1, manager_->GetPendingItemIndex());
  EXPECT_EQ(0, manager_->GetLastCommittedItemIndex());

  EXPECT_FALSE(manager_->CanGoBack());
  EXPECT_FALSE(manager_->CanGoForward());
  EXPECT_TRUE(manager_->CanGoToOffset(0));
  EXPECT_FALSE(manager_->CanGoToOffset(-1));
  EXPECT_FALSE(manager_->CanGoToOffset(1));

  // This is allowed on an empty window open item.
  manager_->GoToIndex(0);

  // Add another navigation and verify that it replaces the empty window open
  // item.
  manager_->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);

  const NavigationItem* pending_item_2 = manager_->GetPendingItem();
  ASSERT_TRUE(pending_item_2);
  EXPECT_EQ("http://www.2.com/", pending_item_2->GetURL().spec());

  [mock_wk_list_ setCurrentURL:@"http://www.2.com"];
  manager_->CommitPendingItem();
  OCMExpect([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.2.com"]);

  const NavigationItem* last_committed_item_2 =
      manager_->GetLastCommittedItem();
  ASSERT_EQ(pending_item_2, last_committed_item_2);
  EXPECT_EQ(last_committed_item_2, manager_->GetVisibleItem());

  EXPECT_EQ(0, manager_->GetIndexForOffset(0));
  EXPECT_EQ(1, manager_->GetIndexForOffset(1));
  EXPECT_EQ(-1, manager_->GetIndexForOffset(-1));

  EXPECT_EQ(1, manager_->GetItemCount());
  EXPECT_EQ(last_committed_item_2, manager_->GetItemAtIndex(0));
  EXPECT_FALSE(manager_->GetItemAtIndex(1));

  EXPECT_EQ(-1, manager_->GetIndexOfItem(last_committed_item));
  EXPECT_EQ(0, manager_->GetIndexOfItem(last_committed_item_2));
  EXPECT_EQ(-1, manager_->GetPendingItemIndex());
  EXPECT_EQ(0, manager_->GetLastCommittedItemIndex());

  EXPECT_FALSE(manager_->CanGoBack());
  EXPECT_FALSE(manager_->CanGoForward());
  EXPECT_TRUE(manager_->CanGoToOffset(0));
  EXPECT_FALSE(manager_->CanGoToOffset(-1));
  EXPECT_FALSE(manager_->CanGoToOffset(1));

  // This is still allowed on a length-1 navigation history.
  manager_->GoToIndex(0);
}

// Test fixture for detach from web view mode for NavigationManagerImpl.
class NavigationManagerDetachedModeTest : public NavigationManagerTest {
 protected:
  void SetUp() override {
    // Sets up each test case with a session history of 3 items. The middle item
    // is the current item.
    url0_ = GURL("http://www.0.com");
    url1_ = GURL("http://www.1.com");
    url2_ = GURL("http://www.2.com");

    [mock_wk_list_ setCurrentURL:@"http://www.1.com"
                    backListURLs:@[ @"http://www.0.com" ]
                 forwardListURLs:@[ @"http://www.2.com" ]];

    ASSERT_EQ(url0_, manager_->GetItemAtIndex(0)->GetURL());
    ASSERT_EQ(url1_, manager_->GetItemAtIndex(1)->GetURL());
    ASSERT_EQ(url2_, manager_->GetItemAtIndex(2)->GetURL());
  }

  GURL url0_;
  GURL url1_;
  GURL url2_;
};

// Tests that all getters return the expected value in detached mode.
TEST_F(NavigationManagerDetachedModeTest, CachedSessionHistory) {
  manager_->DetachFromWebView();
  delegate_.RemoveWebView();

  EXPECT_EQ(url1_, manager_->GetVisibleItem()->GetURL());
  EXPECT_EQ(3, manager_->GetItemCount());

  EXPECT_EQ(url0_, manager_->GetItemAtIndex(0)->GetURL());
  EXPECT_EQ(url1_, manager_->GetItemAtIndex(1)->GetURL());
  EXPECT_EQ(url2_, manager_->GetItemAtIndex(2)->GetURL());

  EXPECT_EQ(0, manager_->GetIndexOfItem(manager_->GetItemAtIndex(0)));
  EXPECT_EQ(1, manager_->GetIndexOfItem(manager_->GetItemAtIndex(1)));
  EXPECT_EQ(2, manager_->GetIndexOfItem(manager_->GetItemAtIndex(2)));

  EXPECT_EQ(-1, manager_->GetPendingItemIndex());
  EXPECT_EQ(nullptr, manager_->GetPendingItem());

  EXPECT_EQ(1, manager_->GetLastCommittedItemIndex());
  EXPECT_EQ(url1_, manager_->GetLastCommittedItem()->GetURL());

  EXPECT_TRUE(manager_->CanGoBack());
  EXPECT_TRUE(manager_->CanGoForward());
  EXPECT_TRUE(manager_->CanGoToOffset(0));
  EXPECT_TRUE(manager_->CanGoToOffset(-1));
  EXPECT_TRUE(manager_->CanGoToOffset(1));

  EXPECT_EQ(0, manager_->GetIndexForOffset(-1));
  EXPECT_EQ(1, manager_->GetIndexForOffset(0));
  EXPECT_EQ(2, manager_->GetIndexForOffset(1));

  std::vector<NavigationItem*> backward_items = manager_->GetBackwardItems();
  EXPECT_EQ(1UL, backward_items.size());
  EXPECT_EQ(url0_, backward_items[0]->GetURL());

  std::vector<NavigationItem*> forward_items = manager_->GetForwardItems();
  EXPECT_EQ(1UL, forward_items.size());
  EXPECT_EQ(url2_, forward_items[0]->GetURL());
}

// Tests that detaching from an empty WKWebView works.
TEST_F(NavigationManagerDetachedModeTest, NothingToCache) {
  delegate_.RemoveWebView();
  manager_->DetachFromWebView();

  EXPECT_EQ(0, manager_->GetItemCount());
  EXPECT_EQ(nullptr, manager_->GetVisibleItem());
  EXPECT_EQ(nullptr, manager_->GetItemAtIndex(0));
  EXPECT_EQ(nullptr, manager_->GetPendingItem());
  EXPECT_EQ(-1, manager_->GetLastCommittedItemIndex());

  manager_->Reload(web::ReloadType::NORMAL, false /* check_for_repost */);
  EXPECT_EQ(nullptr, manager_->GetPendingItem());
}

// Tests that pending item is set to serializable when appropriate.
TEST_F(NavigationManagerDetachedModeTest, NotSerializable) {
  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  EXPECT_FALSE(manager_->GetPendingItemImpl()->ShouldSkipSerialization());

  manager_->SetWKWebViewNextPendingUrlNotSerializable(GURL("http://www.1.com"));
  manager_->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  EXPECT_TRUE(manager_->GetPendingItemImpl()->ShouldSkipSerialization());

  manager_->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  EXPECT_FALSE(manager_->GetPendingItemImpl()->ShouldSkipSerialization());
}

// Tests that GetVisibleWebViewURL() returns a cached GURL.
TEST_F(NavigationManagerTest, TestGetVisibleWebViewOriginURLCache) {
  NavigationManagerImpl manager(&browser_state_, &delegate_);
  NavigationManagerImpl::WKWebViewCache& cache = manager.web_view_cache_;

  GURL gurl("http://www.existing.com");
  __block NSURL* nsurl = [NSURL URLWithString:@"http://www.existing.com"];
  OCMStub([mock_web_view_ URL]).andDo(^(NSInvocation* invocation) {
    [invocation setReturnValue:&nsurl];
  });
  EXPECT_EQ(gurl, cache.GetVisibleWebViewOriginURL());

  // Change mock_web_view_'s URL.
  nsurl = [NSURL URLWithString:@"http://www.anotherexisting.com"];
  EXPECT_NE(gurl, cache.GetVisibleWebViewOriginURL());
  EXPECT_EQ(GURL("http://www.anotherexisting.com"),
            cache.GetVisibleWebViewOriginURL());
}

class NavigationManagerSerialisationTest : public PlatformTest {
 public:
  NavigationManagerSerialisationTest() {
    web_state_ = WebStateImpl::CreateWithFakeWebViewNavigationProxyForTesting(
        WebState::CreateParams(&browser_state_),
        [[CRWFakeWebViewNavigationProxy alloc] init]);
  }

  ~NavigationManagerSerialisationTest() override {}

  CRWFakeWebViewNavigationProxy* fake_web_view() {
    return base::apple::ObjCCastStrict<CRWFakeWebViewNavigationProxy>(
        web_state_->GetWebViewNavigationProxy());
  }

  WebStateImpl* web_state() { return web_state_.get(); }

  BrowserState* browser_state() { return &browser_state_; }

 private:
  WebTaskEnvironment task_environment_;
  FakeBrowserState browser_state_;
  std::unique_ptr<WebStateImpl> web_state_;
};

// Tests serializing NavigationManagerImpl state for a session that is
// longer than kMaxSessionSize with the last committed item at the end
// of the session.
TEST_F(NavigationManagerSerialisationTest, LargeSession) {
  // Populate the WebState with more than kMaxSessionSize navigation items.
  NSMutableArray<NSString*>* back_urls = [NSMutableArray array];
  for (int i = 0; i < 3 * wk_navigation_util::kMaxSessionSize / 2; ++i) {
    [back_urls addObject:[NSString stringWithFormat:@"http://%d.test", i]];
  }
  [fake_web_view() setCurrentURL:@"http://current.test"
                    backListURLs:back_urls
                 forwardListURLs:nil];
  const int original_item_count = web_state()->GetNavigationItemCount();
  EXPECT_GT(original_item_count, wk_navigation_util::kMaxSessionSize);

  // Verify that the serialised state only contains kMaxSessionSize items.
  proto::NavigationStorage storage;
  web_state()->GetNavigationManagerImpl().SerializeToProto(storage);
  const int storage_item_count = storage.items_size();
  ASSERT_EQ(storage_item_count, wk_navigation_util::kMaxSessionSize);
  const int offset = original_item_count - storage_item_count;

  // Verify that the serialised items URLs match the URLs in original storage.
  NavigationManager* navigation_manager = web_state()->GetNavigationManager();
  for (int i = 0; i < wk_navigation_util::kMaxSessionSize; ++i) {
    NavigationItem* item = navigation_manager->GetItemAtIndex(i + offset);
    const proto::NavigationItemStorage& item_storage = storage.items(i);
    EXPECT_EQ(item->GetURL(), GURL(item_storage.url()));
  }
}

// Tests serializing NavigationManagerImpl state for a session that contain
// items with ShouldSkipSerialization flag.
TEST_F(NavigationManagerSerialisationTest, ShouldSkipSerializationItems) {
  // Number of items to skip.
  const int kCountOfItemsToSkip = 9;

  // Number of items to insert in the session so that there is more than
  // kMaxSessionSize items after dropping the skipped items.
  const int kCountOfItemsToInsert =
      wk_navigation_util::kMaxSessionSize + kCountOfItemsToSkip;

  // Populate the WebState with more than kMaxSessionSize navigation items.
  NSMutableArray<NSString*>* back_urls = [NSMutableArray array];
  for (int i = 0; i < kCountOfItemsToInsert; ++i) {
    [back_urls addObject:[NSString stringWithFormat:@"http://%d.test", i]];
  }
  [fake_web_view() setCurrentURL:@"http://current.test"
                    backListURLs:back_urls
                 forwardListURLs:nil];
  const int original_item_count = web_state()->GetNavigationItemCount();

  web::NavigationManagerImpl& navigation_manager =
      web_state()->GetNavigationManagerImpl();

  // Skip the items just before the last committed item.
  const int skipped_item_begin =
      navigation_manager.GetLastCommittedItemIndex() - 1 - kCountOfItemsToSkip;
  const int skipped_item_end = skipped_item_begin + kCountOfItemsToSkip;
  for (int index = skipped_item_begin; index < skipped_item_end; ++index) {
    navigation_manager.GetNavigationItemImplAtIndex(index)
        ->SetShouldSkipSerialization(true);
  }

  // Verify that the serialised state only contains kMaxSessionSize items.
  proto::NavigationStorage storage;
  navigation_manager.SerializeToProto(storage);
  const int storage_item_count = storage.items_size();
  ASSERT_EQ(storage_item_count, wk_navigation_util::kMaxSessionSize);
  EXPECT_LT(storage.last_committed_item_index(), storage_item_count);
  const int offset = original_item_count - storage_item_count;

  // Verify that URLs in the storage match original URLs without skipped item.
  for (int i = 0; i < wk_navigation_util::kMaxSessionSize; ++i) {
    int item_index = i + offset;
    if (item_index < skipped_item_end) {
      item_index -= kCountOfItemsToSkip;
    }

    NavigationItem* item = navigation_manager.GetItemAtIndex(item_index);
    const proto::NavigationItemStorage& item_storage = storage.items(i);
    EXPECT_EQ(item->GetURL(), GURL(item_storage.url()));
  }
}

// Tests serializing NavigationManagerImpl state for a session that contain
// items with extra long URL.
TEST_F(NavigationManagerSerialisationTest, ExtraLongURL) {
  // Create extra long URL.
  NSString* long_url =
      [@"http://" stringByPaddingToLength:(url::kMaxURLChars + 1)
                               withString:@"a"
                          startingAtIndex:0];
  [fake_web_view() setCurrentURL:@"http://current.test"
                    backListURLs:@[ long_url ]
                 forwardListURLs:nil];
  const int original_item_count = web_state()->GetNavigationItemCount();
  EXPECT_EQ(original_item_count, 2);

  NavigationItem* item = web_state()->GetNavigationManager()->GetItemAtIndex(1);
  ASSERT_EQ(item->GetReferrer().url, GURL(base::SysNSStringToUTF8(long_url)));

  // Verify that the serialised state only contains one item, and that the
  // item does not have a referrer.
  proto::NavigationStorage storage;
  web_state()->GetNavigationManagerImpl().SerializeToProto(storage);
  const int storage_item_count = storage.items_size();
  ASSERT_EQ(storage_item_count, 1);
  EXPECT_EQ(GURL(storage.items(0).referrer().url()), GURL());
}

// Tests serializing NavigationManagerImpl state for a session that contain
// items with extra long URL as the last committed item.
TEST_F(NavigationManagerSerialisationTest, ExtraLongURLLastCommittedItem) {
  // Create extra long URL.
  NSString* long_url =
      [@"http://" stringByPaddingToLength:(url::kMaxURLChars + 1)
                               withString:@"a"
                          startingAtIndex:0];
  [fake_web_view() setCurrentURL:long_url
                    backListURLs:@[ @"http://current.test" ]
                 forwardListURLs:nil];
  const int original_item_count = web_state()->GetNavigationItemCount();
  EXPECT_EQ(original_item_count, 2);

  // Verify that the serialised state last committed item is correct.
  proto::NavigationStorage storage;
  web_state()->GetNavigationManagerImpl().SerializeToProto(storage);
  const int storage_item_count = storage.items_size();
  ASSERT_EQ(storage_item_count, 1);
  ASSERT_EQ(storage.last_committed_item_index(), 0);

  NavigationItem* item = web_state()->GetNavigationManager()->GetItemAtIndex(0);
  EXPECT_EQ(item->GetURL(), GURL(storage.items(0).url()));
}

// Tests that restoring a session works correctly.
TEST_F(NavigationManagerSerialisationTest, RestoreFromProto) {
  proto::NavigationStorage storage;
  storage.set_last_committed_item_index(0);
  for (const char* url : kTestURLs) {
    storage.add_items()->set_virtual_url(url);
  }
  storage.set_last_committed_item_index(storage.items_size() - 1);

  // Create a WebState with a real navigation proxy as this is required to
  // perform a session restore and access the view to force instantiation
  // of the WKWebView.
  std::unique_ptr<web::WebStateImpl> web_state =
      std::make_unique<web::WebStateImpl>(
          web::WebState::CreateParams(browser_state()));
  std::ignore = web_state->GetView();

  NavigationManagerImpl& navigation_manager =
      web_state->GetNavigationManagerImpl();

  navigation_manager.RestoreFromProto(storage);

  const int urls_count = static_cast<int>(std::size(kTestURLs));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return navigation_manager.GetItemCount() == urls_count;
  }));

  EXPECT_EQ(navigation_manager.GetLastCommittedItemIndex(), urls_count - 1);

  for (int index = 0; index < urls_count; ++index) {
    EXPECT_EQ(navigation_manager.GetItemAtIndex(index)->GetURL(),
              GURL(kTestURLs[index]));
  }
}

// Tests that restoring a empty session works correctly.
TEST_F(NavigationManagerSerialisationTest, RestoreFromProto_Empty) {
  proto::NavigationStorage storage;
  storage.set_last_committed_item_index(storage.items_size() - 1);

  // Create a WebState with a real navigation proxy as this is required to
  // perform a session restore and access the view to force instantiation
  // of the WKWebView.
  std::unique_ptr<web::WebStateImpl> web_state =
      std::make_unique<web::WebStateImpl>(
          web::WebState::CreateParams(browser_state()));
  std::ignore = web_state->GetView();

  NavigationManagerImpl& navigation_manager =
      web_state->GetNavigationManagerImpl();

  navigation_manager.RestoreFromProto(storage);

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return navigation_manager.GetItemCount() == 0;
  }));
  EXPECT_EQ(navigation_manager.GetLastCommittedItemIndex(), -1);
}

// Tests that restoring a session works correctly and respect the index
// of the last committed item.
TEST_F(NavigationManagerSerialisationTest, RestoreFromProto_LastItemIndex) {
  proto::NavigationStorage storage;
  storage.set_last_committed_item_index(0);
  for (const char* url : kTestURLs) {
    storage.add_items()->set_virtual_url(url);
  }
  storage.set_last_committed_item_index(0);

  // Create a WebState with a real navigation proxy as this is required to
  // perform a session restore and access the view to force instantiation
  // of the WKWebView.
  std::unique_ptr<web::WebStateImpl> web_state =
      std::make_unique<web::WebStateImpl>(
          web::WebState::CreateParams(browser_state()));
  std::ignore = web_state->GetView();

  NavigationManagerImpl& navigation_manager =
      web_state->GetNavigationManagerImpl();

  navigation_manager.RestoreFromProto(storage);

  const int urls_count = static_cast<int>(std::size(kTestURLs));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return navigation_manager.GetItemCount() == urls_count;
  }));
  EXPECT_EQ(navigation_manager.GetLastCommittedItemIndex(), 0);

  for (int index = 0; index < urls_count; ++index) {
    EXPECT_EQ(navigation_manager.GetItemAtIndex(index)->GetURL(),
              GURL(kTestURLs[index]));
  }
}

// Tests that restoring a session works correctly even if the index of the
// last committed item is invalid (a bug in M117 caused the application to
// write sessions with an index past the end of items).
TEST_F(NavigationManagerSerialisationTest, RestoreFromProto_IndexOutOfBound) {
  // The code to fix the out-of-bound index is in the code that deserialize
  // the CRWSessionStorage, so we have to serialize/deserialize the object.
  CRWSessionStorage* session_storage = nil;
  {
    proto::WebStateStorage storage;
    proto::NavigationStorage* navigation_storage = storage.mutable_navigation();
    for (const char* url : kTestURLs) {
      navigation_storage->add_items()->set_virtual_url(url);
    }
    // Set an out-of-bound value for last committed item index.
    navigation_storage->set_last_committed_item_index(std::size(kTestURLs));
    session_storage =
        [[CRWSessionStorage alloc] initWithProto:storage
                                uniqueIdentifier:web::WebStateID::NewUnique()
                                stableIdentifier:[[NSUUID UUID] UUIDString]];
  }

  NSError* error = nil;
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:session_storage
                                       requiringSecureCoding:NO
                                                       error:&error];
  ASSERT_FALSE(error);

  NSKeyedUnarchiver* unarchiver =
      [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:&error];
  unarchiver.requiresSecureCoding = NO;
  ASSERT_FALSE(error);

  session_storage = base::apple::ObjCCast<CRWSessionStorage>(
      [unarchiver decodeObjectForKey:@"root"]);
  ASSERT_TRUE(session_storage);

  proto::WebStateStorage storage;
  [session_storage serializeToProto:storage];

  // Create a WebState with a real navigation proxy as this is required to
  // perform a session restore and access the view to force instantiation
  // of the WKWebView.
  std::unique_ptr<web::WebStateImpl> web_state =
      std::make_unique<web::WebStateImpl>(
          web::WebState::CreateParams(browser_state()));
  std::ignore = web_state->GetView();

  NavigationManagerImpl& navigation_manager =
      web_state->GetNavigationManagerImpl();

  navigation_manager.RestoreFromProto(storage.navigation());

  const int urls_count = static_cast<int>(std::size(kTestURLs));
  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^{
    return navigation_manager.GetItemCount() == urls_count;
  }));
  EXPECT_EQ(navigation_manager.GetLastCommittedItemIndex(), urls_count - 1);

  for (int index = 0; index < urls_count; ++index) {
    NavigationItem* item = navigation_manager.GetItemAtIndex(index);
    EXPECT_EQ(item->GetURL(), GURL(kTestURLs[index]));
  }
}

}  // namespace web
