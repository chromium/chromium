// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/wk_based_navigation_manager_impl.h"

#include <WebKit/WebKit.h>
#include <memory>

#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "ios/web/common/features.h"
#import "ios/web/navigation/navigation_manager_delegate.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/navigation/wk_navigation_util.h"
#include "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/navigation/reload_type.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/public/web_client.h"
#import "ios/web/test/fakes/crw_fake_back_forward_list.h"
#include "ios/web/test/test_url_constants.h"
#include "net/base/escape.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "ui/base/page_transition_types.h"
#include "url/scheme_host_port.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// Query parameter that will be appended by AppendingUrlRewriter if it is
// installed into NavigationManager by a test case.
const char kRewrittenQueryParam[] = "wknavigationmanagerrewrittenquery";

// Appends |kRewrittenQueryParam| to |url|.
bool AppendingUrlRewriter(GURL* url, BrowserState* browser_state) {
  GURL::Replacements query_replacements;
  query_replacements.SetQueryStr(kRewrittenQueryParam);
  *url = url->ReplaceComponents(query_replacements);
  return false;
}

// URL scheme that will be rewritten by WebUIUrlRewriter.
const char kSchemeToRewrite[] = "wknavigationmanagerschemetorewrite";

// Replaces |kSchemeToRewrite| scheme with |kTestWebUIScheme|.
bool WebUIUrlRewriter(GURL* url, BrowserState* browser_state) {
  if (url->scheme() == kSchemeToRewrite) {
    GURL::Replacements scheme_replacements;
    scheme_replacements.SetSchemeStr(kTestWebUIScheme);
    *url = url->ReplaceComponents(scheme_replacements);
    return true;
  }
  return false;
}

class MockNavigationManagerDelegate : public NavigationManagerDelegate {
 public:
  void SetWebViewNavigationProxy(id web_view) { mock_web_view_ = web_view; }
  void RemoveWebView() override {
    // Simulate removing the web view.
    mock_web_view_ = nil;
  }

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
  MOCK_METHOD4(GoToBackForwardListItem,
               void(WKBackForwardListItem*,
                    NavigationItem*,
                    NavigationInitiationType,
                    bool));
  MOCK_METHOD0(GetPendingItem, NavigationItemImpl*());

 private:
  WebState* GetWebState() override { return nullptr; }

  id<CRWWebViewNavigationProxy> GetWebViewNavigationProxy() const override {
    return mock_web_view_;
  }

  id mock_web_view_;
};

// Test fixture for WKBasedNavigationManagerImpl.
class WKBasedNavigationManagerTest : public PlatformTest {
 protected:
  WKBasedNavigationManagerTest() : manager_(new WKBasedNavigationManagerImpl) {
    mock_web_view_ = OCMClassMock([WKWebView class]);
    mock_wk_list_ = [[CRWFakeBackForwardList alloc] init];
    OCMStub([mock_web_view_ backForwardList]).andReturn(mock_wk_list_);
    delegate_.SetWebViewNavigationProxy(mock_web_view_);

    manager_->SetDelegate(&delegate_);
    manager_->SetBrowserState(&browser_state_);

    BrowserURLRewriter::GetInstance()->AddURLRewriter(WebUIUrlRewriter);
    url::AddStandardScheme(kSchemeToRewrite, url::SCHEME_WITH_HOST);
  }

  // Returns the value of the "#session=" URL hash component from |url|.
  static std::string ExtractRestoredSession(const GURL& url) {
    std::string decoded = net::UnescapeBinaryURLComponent(url.ref());
    return decoded.substr(
        strlen(wk_navigation_util::kRestoreSessionSessionHashPrefix));
  }

  std::unique_ptr<NavigationManagerImpl> manager_;
  CRWFakeBackForwardList* mock_wk_list_;
  id mock_web_view_;
  MockNavigationManagerDelegate delegate_;
  base::HistogramTester histogram_tester_;

 private:
  TestBrowserState browser_state_;
};

// Tests that GetItemAtIndex() on an empty manager will sync navigation items to
// WKBackForwardList using default properties.
TEST_F(WKBasedNavigationManagerTest, SyncAfterItemAtIndex) {
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
  EXPECT_EQ(UserAgentType::MOBILE, item->GetUserAgentType());
  EXPECT_FALSE(item->GetTimestamp().is_null());
}

// Tests that Referrer is inferred from the previous WKBackForwardListItem.
TEST_F(WKBasedNavigationManagerTest, SyncAfterItemAtIndexWithPreviousItem) {
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
  EXPECT_EQ(UserAgentType::MOBILE, item2->GetUserAgentType());
  EXPECT_EQ(GURL("http://www.1.com"), item2->GetReferrer().url);
  EXPECT_FALSE(item2->GetTimestamp().is_null());

  NavigationItem* item1 = manager_->GetItemAtIndex(1);
  ASSERT_NE(item1, nullptr);
  EXPECT_EQ(GURL("http://www.1.com"), item1->GetURL());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK,
                                           item1->GetTransitionType()));
  EXPECT_EQ(UserAgentType::MOBILE, item1->GetUserAgentType());
  EXPECT_EQ(GURL("http://www.0.com"), item1->GetReferrer().url);
  EXPECT_FALSE(item1->GetTimestamp().is_null());

  NavigationItem* item0 = manager_->GetItemAtIndex(0);
  ASSERT_NE(item0, nullptr);
  EXPECT_EQ(GURL("http://www.0.com"), item0->GetURL());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_LINK,
                                           item0->GetTransitionType()));
  EXPECT_EQ(UserAgentType::MOBILE, item0->GetUserAgentType());
  EXPECT_FALSE(item0->GetTimestamp().is_null());
}

// Tests that GetLastCommittedItem() creates a default NavigationItem when the
// last committed item in WKWebView does not have a linked entry.
TEST_F(WKBasedNavigationManagerTest, SyncInGetLastCommittedItem) {
  [mock_wk_list_ setCurrentURL:@"http://www.0.com"];
  EXPECT_EQ(1, manager_->GetItemCount());

  NavigationItem* item = manager_->GetLastCommittedItem();
  ASSERT_NE(item, nullptr);
  EXPECT_EQ("http://www.0.com/", item->GetURL().spec());
  EXPECT_FALSE(item->GetTimestamp().is_null());
}

// Tests that GetLastCommittedItem() creates a default NavigationItem when the
// last committed item in WKWebView is an app-specific URL.
TEST_F(WKBasedNavigationManagerTest,
       SyncInGetLastCommittedItemForAppSpecificURL) {
  GURL url(url::SchemeHostPort(kSchemeToRewrite, "test", 0).Serialize());

  // Verifies that the test URL is rewritten into an app-specific URL.
  manager_->AddPendingItem(
      url, Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
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
TEST_F(WKBasedNavigationManagerTest, GetItemAtIndexAfterCommitPending) {
  // Simulate a main frame navigation.
  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  NavigationItem* pending_item0 = manager_->GetPendingItem();

  [mock_wk_list_ setCurrentURL:@"http://www.0.com"];
  manager_->CommitPendingItem();

  EXPECT_EQ(1, manager_->GetItemCount());
  NavigationItem* item = manager_->GetLastCommittedItem();
  EXPECT_EQ(pending_item0, item);
  EXPECT_EQ(GURL("http://www.0.com"), item->GetURL());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_TYPED,
                                           item->GetTransitionType()));
  EXPECT_EQ(UserAgentType::DESKTOP, item->GetUserAgentType());

  // Simulate a second main frame navigation.
  manager_->AddPendingItem(
      GURL("http://www.2.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
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
  EXPECT_EQ(UserAgentType::MOBILE, item1->GetUserAgentType());
  EXPECT_EQ(GURL("http://www.0.com"), item1->GetReferrer().url);

  // This item is created by CommitPendingItem.
  NavigationItem* item2 = manager_->GetItemAtIndex(2);
  EXPECT_EQ(pending_item2, item2);
  EXPECT_EQ(GURL("http://www.2.com"), item2->GetURL());
  EXPECT_TRUE(ui::PageTransitionCoreTypeIs(ui::PAGE_TRANSITION_TYPED,
                                           item2->GetTransitionType()));
  EXPECT_EQ(UserAgentType::DESKTOP, item2->GetUserAgentType());
  EXPECT_EQ(GURL(""), item2->GetReferrer().url);
}

// Tests that AddPendingItem does not create a new NavigationItem if the new
// pending item is a back forward navigation or when reloading a redirect page.
TEST_F(WKBasedNavigationManagerTest, ReusePendingItemForHistoryNavigation) {
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
      web::NavigationManager::UserAgentOverrideOption::MOBILE);

  EXPECT_EQ(original_item0, manager_->GetPendingItem());

  // Simulate reloading a redirect url.  This happens when one restores while
  // offline.
  GURL redirect_url = wk_navigation_util::CreateRedirectUrl(
      manager_->GetPendingItem()->GetURL());
  [mock_wk_list_ setCurrentURL:base::SysUTF8ToNSString(redirect_url.spec())
                  backListURLs:nil
               forwardListURLs:nil];
  original_item0 = manager_->GetItemAtIndex(0);
  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::MOBILE);
  EXPECT_EQ(original_item0, manager_->GetPendingItem());
}

// Tests that AddPendingItem does not create a new NavigationItem if the new
// pending item is a reload of app-specific URL.
TEST_F(WKBasedNavigationManagerTest, ReusePendingItemForReloadAppSpecificURL) {
  // Simulate a previous app-specific navigation.
  NSString* url = @"about:blank?for=chrome%3A%2F%2Fnewtab";
  [mock_wk_list_ setCurrentURL:url];
  NavigationItem* original_item = manager_->GetItemAtIndex(0);

  OCMExpect([mock_web_view_ URL]).andReturn([[NSURL alloc] initWithString:url]);

  manager_->AddPendingItem(
      GURL("chrome://newtab"), Referrer(), ui::PAGE_TRANSITION_RELOAD,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  EXPECT_EQ(original_item, manager_->GetPendingItem());
}

// Tests that transient URL rewriters are only applied to a new pending item.
TEST_F(WKBasedNavigationManagerTest,
       TransientURLRewritersOnlyUsedForPendingItem) {
  manager_->AddPendingItem(GURL("http://www.0.com"), Referrer(),
                           ui::PAGE_TRANSITION_TYPED,
                           NavigationInitiationType::BROWSER_INITIATED,
                           NavigationManager::UserAgentOverrideOption::INHERIT);

  // Install transient URL rewriters.
  manager_->AddTransientURLRewriter(&AppendingUrlRewriter);
  [mock_wk_list_ setCurrentURL:@"http://www.0.com"];

  // Transient URL rewriters do not apply to lazily synced items.
  NavigationItem* item0 = manager_->GetItemAtIndex(0);
  EXPECT_EQ(GURL("http://www.0.com"), item0->GetURL());

  // Transient URL rewriters do not apply to transient items.
  manager_->AddTransientItem(GURL("http://www.1.com"));
  EXPECT_EQ(GURL("http://www.1.com"), manager_->GetTransientItem()->GetURL());

  // Transient URL rewriters are applied to a new pending item.
  manager_->AddPendingItem(GURL("http://www.2.com"), Referrer(),
                           ui::PAGE_TRANSITION_TYPED,
                           NavigationInitiationType::BROWSER_INITIATED,
                           NavigationManager::UserAgentOverrideOption::INHERIT);
  EXPECT_EQ(kRewrittenQueryParam, manager_->GetPendingItem()->GetURL().query());
}

// Tests DiscardNonCommittedItems discards both pending and transient items.
TEST_F(WKBasedNavigationManagerTest, DiscardNonCommittedItems) {
  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  manager_->AddTransientItem(GURL("http://www.1.com"));

  EXPECT_NE(nullptr, manager_->GetPendingItem());
  EXPECT_NE(nullptr, manager_->GetTransientItem());

  manager_->DiscardNonCommittedItems();
  EXPECT_EQ(nullptr, manager_->GetPendingItem());
  EXPECT_EQ(nullptr, manager_->GetTransientItem());
}

// Tests that in the absence of a transient item, going back is delegated to the
// underlying WKWebView.
TEST_F(WKBasedNavigationManagerTest, GoBackWithoutTransientItem) {
  ASSERT_FALSE(manager_->CanGoBack());

  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  [mock_wk_list_ setCurrentURL:@"http://www.0.com"];

  manager_->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
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

// Tests that going back from a transient item will discard the transient item
// and the pending item associated with it.
TEST_F(WKBasedNavigationManagerTest, GoBackFromTransientItem) {
  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  [mock_wk_list_ setCurrentURL:@"http://www.0.com"];
  manager_->CommitPendingItem();

  manager_->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  manager_->AddTransientItem(GURL("http://www.1.com/transient"));

  ASSERT_TRUE(manager_->CanGoBack());
  EXPECT_CALL(delegate_,
              GoToBackForwardListItem(
                  mock_wk_list_.currentItem, manager_->GetItemAtIndex(0),
                  NavigationInitiationType::BROWSER_INITIATED,
                  /*has_user_gesture=*/true));
  manager_->GoBack();
  [mock_web_view_ verify];

  EXPECT_EQ(nullptr, manager_->GetPendingItem());
  EXPECT_EQ(nullptr, manager_->GetTransientItem());
}

// Tests that going forward is always delegated to the underlying WKWebView
// without any sanity checks such as whether any forward history exists.
TEST_F(WKBasedNavigationManagerTest, GoForward) {
  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  [mock_wk_list_ setCurrentURL:@"http://www.0.com"];
  manager_->CommitPendingItem();

  manager_->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
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
TEST_F(WKBasedNavigationManagerTest, GoForwardShouldDiscardsUncommittedItems) {
  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  [mock_wk_list_ setCurrentURL:@"http://www.0.com"];
  manager_->CommitPendingItem();

  manager_->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  [mock_wk_list_ setCurrentURL:@"http://www.1.com"
                  backListURLs:@[ @"http://www.0.com" ]
               forwardListURLs:nil];

  [mock_wk_list_ moveCurrentToIndex:0];
  ASSERT_TRUE(manager_->CanGoForward());

  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  manager_->AddTransientItem(GURL("http://www.1.com"));

  EXPECT_NE(nullptr, manager_->GetPendingItem());
  EXPECT_NE(nullptr, manager_->GetTransientItem());

  EXPECT_CALL(delegate_,
              GoToBackForwardListItem(
                  mock_wk_list_.forwardList[0], manager_->GetItemAtIndex(1),
                  NavigationInitiationType::BROWSER_INITIATED,
                  /*has_user_gesture=*/true));
  manager_->GoForward();
  [mock_web_view_ verify];

  EXPECT_EQ(nullptr, manager_->GetPendingItem());
  EXPECT_EQ(nullptr, manager_->GetTransientItem());
}

// Tests CanGoToOffset API for positive, negative and zero delta.
TEST_F(WKBasedNavigationManagerTest, CanGoToOffset) {
  manager_->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  manager_->CommitPendingItem();

  manager_->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  manager_->CommitPendingItem();

  manager_->AddPendingItem(
      GURL("http://www.url.com/2"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

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

  // Test with transient entry.
  manager_->AddPendingItem(
      GURL("http://www.url.com/3"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  manager_->AddTransientItem(GURL("http://www.url.com/3"));
  ASSERT_EQ(3, manager_->GetItemCount());
  ASSERT_EQ(2, manager_->GetLastCommittedItemIndex());
  EXPECT_TRUE(manager_->CanGoToOffset(-1));
  EXPECT_EQ(2, manager_->GetIndexForOffset(-1));
  EXPECT_TRUE(manager_->CanGoToOffset(-3));
  EXPECT_EQ(0, manager_->GetIndexForOffset(-3));
  EXPECT_FALSE(manager_->CanGoToOffset(-4));
  EXPECT_FALSE(manager_->CanGoToOffset(1));

  // Simulate a history navigation pending item.
  [mock_wk_list_ moveCurrentToIndex:1];
  OCMExpect([mock_web_view_ URL])
      .andReturn([[NSURL alloc] initWithString:@"http://www.url.com/1"]);
  manager_->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

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

// Tests that non-empty session history can be restored, and are re-written if
// necessary.
TEST_F(WKBasedNavigationManagerTest, RestoreSessionWithHistory) {
  manager_->AddTransientURLRewriter(&WebUIUrlRewriter);
  auto item0 = std::make_unique<NavigationItemImpl>();
  GURL url(url::SchemeHostPort(kSchemeToRewrite, "test", 0).Serialize());
  item0->SetURL(url);
  item0->SetTitle(base::ASCIIToUTF16("Test Website 0"));
  auto item1 = std::make_unique<NavigationItemImpl>();
  item1->SetURL(GURL("http://www.1.com"));

  std::vector<std::unique_ptr<NavigationItem>> items;
  items.push_back(std::move(item0));
  items.push_back(std::move(item1));

  ASSERT_FALSE(manager_->IsRestoreSessionInProgress());
  manager_->Restore(1 /* last_committed_item_index */, std::move(items));
  EXPECT_TRUE(manager_->IsRestoreSessionInProgress());

  ASSERT_FALSE(manager_->GetPendingItem());
  NavigationItem* pending_item =
      manager_->GetPendingItemInCurrentOrRestoredSession();
  ASSERT_TRUE(pending_item);
  GURL pending_url = pending_item->GetURL();
  EXPECT_TRUE(pending_url.SchemeIsFile());
  EXPECT_EQ("restore_session.html", pending_url.ExtractFileName());
  EXPECT_EQ(url.spec(), pending_item->GetVirtualURL());
  EXPECT_EQ("Test Website 0", base::UTF16ToUTF8(pending_item->GetTitle()));

  EXPECT_EQ("{\"offset\":0,\"titles\":[\"Test Website 0\",\"\"],"
            "\"urls\":[\"testwebui://test/\","
            "\"http://www.1.com/\"]}",
            ExtractRestoredSession(pending_url));

  // Check that cached visible item is returned.
  EXPECT_EQ("http://www.1.com/", manager_->GetVisibleItem()->GetURL());

  histogram_tester_.ExpectTotalCount(kRestoreNavigationItemCount, 1);
  histogram_tester_.ExpectBucketCount(kRestoreNavigationItemCount, 2, 1);
}

// Tests that restoring session replaces existing history in navigation manager.
TEST_F(WKBasedNavigationManagerTest, RestoreSessionResetsHistory) {
  EXPECT_EQ(-1, manager_->GetPendingItemIndex());
  EXPECT_EQ(-1, manager_->GetPreviousItemIndex());
  EXPECT_EQ(-1, manager_->GetLastCommittedItemIndex());

  // Sets up the navigation history with 2 entries, and a pending back-forward
  // navigation so that last_committed_item_index is 1, pending_item_index is 0,
  // and previous_item_index is 0. Basically, none of them is -1.
  manager_->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/0"];
  manager_->CommitPendingItem();

  manager_->AddPendingItem(
      GURL("http://www.url.com/1"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  [mock_wk_list_ setCurrentURL:@"http://www.url.com/1"
                  backListURLs:@[ @"http://www.url.com/0" ]
               forwardListURLs:nil];
  manager_->CommitPendingItem();

  [mock_wk_list_ moveCurrentToIndex:0];
  OCMStub([mock_web_view_ URL])
      .andReturn([NSURL URLWithString:@"http://www.url.com/0"]);
  manager_->AddPendingItem(
      GURL("http://www.url.com/0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

  EXPECT_EQ(1, manager_->GetLastCommittedItemIndex());
  EXPECT_EQ(0, manager_->GetPreviousItemIndex());
  EXPECT_EQ(0, manager_->GetPendingItemIndex());
  EXPECT_TRUE(manager_->GetPendingItem() != nullptr);

  // Restores a fake session.
  auto restored_item = std::make_unique<NavigationItemImpl>();
  restored_item->SetURL(GURL("http://restored.com"));
  std::vector<std::unique_ptr<NavigationItem>> items;
  items.push_back(std::move(restored_item));
  ASSERT_FALSE(manager_->IsRestoreSessionInProgress());
  manager_->Restore(0 /* last_committed_item_index */, std::move(items));
  EXPECT_TRUE(manager_->IsRestoreSessionInProgress());

  // Check that last_committed_index, previous_item_index and pending_item_index
  // are all reset to -1. Note that last_committed_item_index will change to the
  // value in the restored session (i.e. 0) once restore_session.html finishes
  // loading in the web view. This is not tested here because this test doesn't
  // use real WKWebView.
  EXPECT_EQ(-1, manager_->GetLastCommittedItemIndex());
  EXPECT_EQ(-1, manager_->GetPreviousItemIndex());
  EXPECT_EQ(-1, manager_->GetPendingItemIndex());

  // Check that the only pending item is restore_session.html.
  ASSERT_FALSE(manager_->GetPendingItem());
  NavigationItem* pending_item =
      manager_->GetPendingItemInCurrentOrRestoredSession();
  ASSERT_TRUE(pending_item != nullptr);
  GURL pending_url = pending_item->GetURL();
  EXPECT_TRUE(pending_url.SchemeIsFile());
  EXPECT_EQ("restore_session.html", pending_url.ExtractFileName());

  // Check that cached visible item is returned.
  EXPECT_EQ("http://restored.com/", manager_->GetVisibleItem()->GetURL());
}

// Tests that Restore() accepts empty session history and performs no-op.
TEST_F(WKBasedNavigationManagerTest, RestoreSessionWithEmptyHistory) {
  manager_->Restore(-1 /* last_committed_item_index */,
                    std::vector<std::unique_ptr<NavigationItem>>());

  ASSERT_EQ(nullptr, manager_->GetPendingItem());
}

// Tests that the virtual URL of a restore_session redirect item is updated to
// the target URL.
TEST_F(WKBasedNavigationManagerTest, HideInternalRedirectUrl) {
  GURL target_url = GURL("http://www.1.com?query=special%26chars");
  GURL url = wk_navigation_util::CreateRedirectUrl(target_url);
  NSString* url_spec = base::SysUTF8ToNSString(url.spec());
  [mock_wk_list_ setCurrentURL:url_spec];
  NavigationItem* item = manager_->GetItemAtIndex(0);
  ASSERT_TRUE(item);
  EXPECT_EQ(target_url, item->GetVirtualURL());
  EXPECT_EQ(url, item->GetURL());
}

// Tests that the virtual URL of a placeholder item is updated to the original
// URL.
TEST_F(WKBasedNavigationManagerTest, HideInternalPlaceholderUrl) {
  GURL original_url = GURL("http://www.1.com?query=special%26chars");
  GURL url = wk_navigation_util::CreatePlaceholderUrlForUrl(original_url);
  NSString* url_spec = base::SysUTF8ToNSString(url.spec());
  [mock_wk_list_ setCurrentURL:url_spec];
  NavigationItem* item = manager_->GetItemAtIndex(0);
  ASSERT_TRUE(item);
  EXPECT_EQ(original_url, item->GetVirtualURL());
  EXPECT_EQ(url, item->GetURL());
}

// Tests that all NavigationManager APIs return reasonable values in the Empty
// Window Open Navigation edge case. See comments in header file for details.
TEST_F(WKBasedNavigationManagerTest, EmptyWindowOpenNavigation) {
  // Set up the precondition for an empty window open item.
  OCMExpect([mock_web_view_ URL])
      .andReturn(net::NSURLWithGURL(GURL(url::kAboutBlankURL)));
  mock_wk_list_.currentItem = nil;

  manager_->AddPendingItem(
      GURL(url::kAboutBlankURL), Referrer(), ui::PAGE_TRANSITION_LINK,
      web::NavigationInitiationType::RENDERER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

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

  EXPECT_EQ(-1, manager_->GetPreviousItemIndex());
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
      web::NavigationManager::UserAgentOverrideOption::INHERIT);

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

  EXPECT_EQ(-1, manager_->GetPreviousItemIndex());
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

// Test fixture for detach from web view mode for WKBasedNavigationManagerImpl.
class WKBasedNavigationManagerDetachedModeTest
    : public WKBasedNavigationManagerTest {
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

  NSString* CreateRedirectUrlForWKList(GURL url) {
    GURL redirect_url = wk_navigation_util::CreateRedirectUrl(url);
    return base::SysUTF8ToNSString(redirect_url.spec());
  }

  GURL url0_;
  GURL url1_;
  GURL url2_;
};

// Tests that all getters return the expected value in detached mode.
TEST_F(WKBasedNavigationManagerDetachedModeTest, CachedSessionHistory) {
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

  NavigationItemList backward_items = manager_->GetBackwardItems();
  EXPECT_EQ(1UL, backward_items.size());
  EXPECT_EQ(url0_, backward_items[0]->GetURL());

  NavigationItemList forward_items = manager_->GetForwardItems();
  EXPECT_EQ(1UL, forward_items.size());
  EXPECT_EQ(url2_, forward_items[0]->GetURL());
}

// Tests that detaching from an empty WKWebView works.
TEST_F(WKBasedNavigationManagerDetachedModeTest, NothingToCache) {
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

// Tests that Reload from detached mode restores cached history.
TEST_F(WKBasedNavigationManagerDetachedModeTest, Reload) {
  manager_->DetachFromWebView();
  delegate_.RemoveWebView();

  manager_->Reload(web::ReloadType::NORMAL, false /* check_for_repost */);
  NavigationItem* pending_item =
      manager_->GetPendingItemInCurrentOrRestoredSession();
  EXPECT_EQ(
      "{\"offset\":-1,\"titles\":[\"\",\"\",\"\"],\"urls\":[\"http://www.0.com/"
      "\",\"http://www.1.com/\",\"http://www.2.com/\"]}",
      ExtractRestoredSession(pending_item->GetURL()));

  EXPECT_EQ(url0_, pending_item->GetVirtualURL());
  EXPECT_EQ(url1_, manager_->GetVisibleItem()->GetURL());

  histogram_tester_.ExpectTotalCount(kRestoreNavigationItemCount, 1);
  histogram_tester_.ExpectBucketCount(kRestoreNavigationItemCount, 3, 1);
}

// Tests that GoToIndex from detached mode restores cached history with updated
// current item offset.
TEST_F(WKBasedNavigationManagerDetachedModeTest, GoToIndex) {
  manager_->DetachFromWebView();
  delegate_.RemoveWebView();

  manager_->GoToIndex(0);
  NavigationItem* pending_item =
      manager_->GetPendingItemInCurrentOrRestoredSession();

  EXPECT_EQ(
      "{\"offset\":-2,\"titles\":[\"\",\"\",\"\"],\"urls\":[\"http://www.0.com/"
      "\",\"http://www.1.com/\",\"http://www.2.com/\"]}",
      ExtractRestoredSession(pending_item->GetURL()));
  EXPECT_EQ(url0_, pending_item->GetVirtualURL());
  EXPECT_EQ(url0_, manager_->GetVisibleItem()->GetURL());

  histogram_tester_.ExpectTotalCount(kRestoreNavigationItemCount, 1);
  histogram_tester_.ExpectBucketCount(kRestoreNavigationItemCount, 3, 1);
}

// Tests that LoadIfNecessary from detached mode restores cached history.
TEST_F(WKBasedNavigationManagerDetachedModeTest, LoadIfNecessary) {
  manager_->DetachFromWebView();
  delegate_.RemoveWebView();

  manager_->LoadIfNecessary();
  NavigationItem* pending_item =
      manager_->GetPendingItemInCurrentOrRestoredSession();

  EXPECT_EQ(
      "{\"offset\":-1,\"titles\":[\"\",\"\",\"\"],\"urls\":[\"http://www.0.com/"
      "\",\"http://www.1.com/\",\"http://www.2.com/\"]}",
      ExtractRestoredSession(pending_item->GetURL()));
  EXPECT_EQ(url0_, pending_item->GetVirtualURL());
  EXPECT_EQ(url1_, manager_->GetVisibleItem()->GetURL());

  histogram_tester_.ExpectTotalCount(kRestoreNavigationItemCount, 1);
  histogram_tester_.ExpectBucketCount(kRestoreNavigationItemCount, 3, 1);
}

// Tests that LoadURLWithParams from detached mode restores backward history and
// adds the new item at the end.
TEST_F(WKBasedNavigationManagerDetachedModeTest, LoadURLWithParams) {
  manager_->DetachFromWebView();
  delegate_.RemoveWebView();

  GURL url("http://www.3.com");
  NavigationManager::WebLoadParams params(url);
  manager_->LoadURLWithParams(params);
  NavigationItem* pending_item =
      manager_->GetPendingItemInCurrentOrRestoredSession();
  EXPECT_EQ(
      "{\"offset\":0,\"titles\":[\"\",\"\",\"\"],\"urls\":[\"http://www.0.com/"
      "\",\"http://www.1.com/\",\"http://www.3.com/\"]}",
      ExtractRestoredSession(pending_item->GetURL()));
  EXPECT_EQ(url0_, pending_item->GetVirtualURL());
  EXPECT_EQ(url, manager_->GetVisibleItem()->GetURL());

  histogram_tester_.ExpectTotalCount(kRestoreNavigationItemCount, 1);
  histogram_tester_.ExpectBucketCount(kRestoreNavigationItemCount, 3, 1);
}

// Tests that detaching placeholder urls are cleaned before being cached.
TEST_F(WKBasedNavigationManagerDetachedModeTest, CachedPlaceholders) {
  [mock_wk_list_ setCurrentURL:CreateRedirectUrlForWKList(url1_)
                  backListURLs:@[ CreateRedirectUrlForWKList(url0_) ]
               forwardListURLs:@[ CreateRedirectUrlForWKList(url2_) ]];
  manager_->DetachFromWebView();

  EXPECT_EQ(url0_, manager_->GetNavigationItemImplAtIndex(0)->GetURL());
  EXPECT_EQ(url1_, manager_->GetNavigationItemImplAtIndex(1)->GetURL());
  EXPECT_EQ(url2_, manager_->GetNavigationItemImplAtIndex(2)->GetURL());
}

// Tests that pending item is set to serializable when appropriate.
TEST_F(WKBasedNavigationManagerDetachedModeTest, NotSerializable) {
  manager_->AddPendingItem(
      GURL("http://www.0.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  EXPECT_FALSE(manager_->GetPendingItemInCurrentOrRestoredSession()
                   ->ShouldSkipSerialization());

  manager_->SetWKWebViewNextPendingUrlNotSerializable(GURL("http://www.1.com"));
  manager_->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  EXPECT_TRUE(manager_->GetPendingItemInCurrentOrRestoredSession()
                  ->ShouldSkipSerialization());

  manager_->AddPendingItem(
      GURL("http://www.1.com"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::DESKTOP);
  EXPECT_FALSE(manager_->GetPendingItemInCurrentOrRestoredSession()
                   ->ShouldSkipSerialization());
}

}  // namespace web
