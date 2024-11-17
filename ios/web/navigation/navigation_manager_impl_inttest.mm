// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_manager_impl.h"

#import "base/test/metrics/histogram_tester.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_view_content_test_util.h"
#import "ios/web/test/web_int_test.h"
#import "ios/web/web_state/ui/crw_web_controller.h"
#import "ios/web/web_state/web_state_impl.h"
#import "net/test/embedded_test_server/default_handlers.h"
#import "net/test/embedded_test_server/embedded_test_server.h"

namespace web {

// Test fixture for navigation manager tests requiring a real webview (for
// example, those involving session restoration).
class NavigationManagerImplTest : public WebIntTest {
 protected:
  void SetUp() override {
    WebIntTest::SetUp();

    test_server_ = std::make_unique<net::test_server::EmbeddedTestServer>();
    net::test_server::RegisterDefaultHandlers(test_server_.get());
    ASSERT_TRUE(test_server_->Start());
  }

  NavigationManager* navigation_manager() {
    return web_state()->GetNavigationManager();
  }

  NavigationManagerImpl& navigation_manager_impl() {
    return web::WebStateImpl::FromWebState(web_state())
        ->GetNavigationManagerImpl();
  }

  base::HistogramTester histogram_tester_;

 protected:
  // Embedded test server which hosts sample pages.
  std::unique_ptr<net::EmbeddedTestServer> test_server_;
};

// Tests state of an empty navigation manager.
TEST_F(NavigationManagerImplTest, EmptyManager) {
  ASSERT_EQ(0, navigation_manager()->GetItemCount());
}

// Tests state of a single navigation.
TEST_F(NavigationManagerImplTest, SingleNavigation) {
  ASSERT_EQ(0, navigation_manager()->GetItemCount());

  GURL url = test_server_->GetURL("/echo");
  ASSERT_TRUE(LoadUrl(url));

  EXPECT_EQ(1, navigation_manager()->GetItemCount());
}

// Tests state after restoration of a single item.
TEST_F(NavigationManagerImplTest, SingleItemRestore) {
  ASSERT_EQ(0, navigation_manager()->GetItemCount());

  GURL url = test_server_->GetURL("/echo");
  auto item = std::make_unique<NavigationItemImpl>();
  item->SetURL(url);
  item->SetTitle(u"Test Website");

  std::vector<std::unique_ptr<NavigationItem>> items;
  items.push_back(std::move(item));

  navigation_manager()->Restore(/*last_committed_item_index=*/0,
                                std::move(items));

  EXPECT_EQ(1, navigation_manager()->GetItemCount());
  EXPECT_EQ(0, navigation_manager()->GetLastCommittedItemIndex());

  NavigationItem* visible_item = navigation_manager()->GetVisibleItem();
  ASSERT_TRUE(visible_item);
  NavigationItem* last_committed_item =
      navigation_manager()->GetLastCommittedItem();
  ASSERT_TRUE(last_committed_item);
  EXPECT_EQ(visible_item, last_committed_item);
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
}

// Tests state after restoration of multiple items.
TEST_F(NavigationManagerImplTest, MultipleItemRestore) {
  ASSERT_EQ(0, web_state()->GetNavigationManager()->GetItemCount());

  auto item0 = std::make_unique<NavigationItemImpl>();
  item0->SetURL(test_server_->GetURL("/echo?0"));
  item0->SetTitle(u"Test Website 0");
  auto item1 = std::make_unique<NavigationItemImpl>();
  item1->SetURL(test_server_->GetURL("/echo?1"));

  std::vector<std::unique_ptr<NavigationItem>> items;
  items.push_back(std::move(item0));
  items.push_back(std::move(item1));

  navigation_manager()->Restore(1 /* last_committed_item_index */,
                                std::move(items));

  ASSERT_EQ(2, web_state()->GetNavigationManager()->GetItemCount());

  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  ASSERT_FALSE(navigation_manager()->GetPendingItem());
}

// Tests that restoring session replaces existing history in navigation manager.
TEST_F(NavigationManagerImplTest, RestoreSessionResetsHistory) {
  EXPECT_EQ(-1, navigation_manager()->GetPendingItemIndex());
  EXPECT_EQ(-1, navigation_manager()->GetLastCommittedItemIndex());

  ASSERT_TRUE(LoadUrl(test_server_->GetURL("/echo?0")));
  ASSERT_TRUE(LoadUrl(test_server_->GetURL("/echo?1")));
  navigation_manager_impl().AddPendingItem(
      test_server_->GetURL("/echo?0"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  EXPECT_EQ(1, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_TRUE(navigation_manager()->GetPendingItem() != nullptr);

  // Sets up each test case with a session history of 3 items. The middle item
  // is the current item.
  auto item0 = std::make_unique<NavigationItemImpl>();
  GURL url0 = test_server_->GetURL("/echo?restored0");
  item0->SetURL(url0);
  auto item1 = std::make_unique<NavigationItemImpl>();
  GURL url1 = test_server_->GetURL("/echo?restored1");
  item1->SetURL(url1);
  auto item2 = std::make_unique<NavigationItemImpl>();
  GURL url2 = test_server_->GetURL("/echo?restored2");
  item2->SetURL(url2);
  std::vector<std::unique_ptr<NavigationItem>> items;
  items.push_back(std::move(item0));
  items.push_back(std::move(item1));
  items.push_back(std::move(item2));

  navigation_manager()->Restore(2 /* last_committed_item_index */,
                                std::move(items));
  ASSERT_EQ(3, web_state()->GetNavigationManager()->GetItemCount());

  EXPECT_EQ(2, navigation_manager()->GetLastCommittedItemIndex());
  EXPECT_TRUE(navigation_manager()->GetPendingItem() == nullptr);

  //  // Check that cached visible item is returned.
  EXPECT_EQ(url2, navigation_manager()->GetVisibleItem()->GetURL());
}

// Tests that Reload from detached mode restores cached history.
TEST_F(NavigationManagerImplTest, DetachedModeReload) {
  GURL url0 = test_server_->GetURL("/echo?0");
  ASSERT_TRUE(LoadUrl(url0));
  GURL url1 = test_server_->GetURL("/echo?1");
  ASSERT_TRUE(LoadUrl(url1));
  GURL url2 = test_server_->GetURL("/echo?2");
  ASSERT_TRUE(LoadUrl(url2));

  navigation_manager()->GoBack();

  ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::test::ios::kWaitForPageLoadTimeout, ^{
        base::RunLoop().RunUntilIdle();
        return url1 == navigation_manager()->GetVisibleItem()->GetURL();
      }));

  ASSERT_EQ(3, web_state()->GetNavigationManager()->GetItemCount());

  // Clear the webview.
  navigation_manager_impl().DetachFromWebView();
  [web::test::GetWebController(web_state()) removeWebView];

  // Reloading should restore history.
  navigation_manager()->Reload(web::ReloadType::NORMAL,
                               false /* check_for_repost */);
  EXPECT_EQ(url1, navigation_manager()->GetVisibleItem()->GetURL());

  std::vector<NavigationItem*> back_items =
      navigation_manager()->GetBackwardItems();
  ASSERT_EQ(1ul, back_items.size());
  EXPECT_EQ(url0, back_items[0]->GetURL().spec());

  std::vector<NavigationItem*> forward_items =
      navigation_manager()->GetForwardItems();
  ASSERT_EQ(1ul, forward_items.size());
  EXPECT_EQ(url2, forward_items[0]->GetURL().spec());

  histogram_tester_.ExpectTotalCount(kRestoreNavigationItemCount, 1);
  histogram_tester_.ExpectBucketCount(kRestoreNavigationItemCount, 3, 1);
}

// Tests that GoToIndex from detached mode restores cached history with
// updated / current item offset.
TEST_F(NavigationManagerImplTest, DetachedModeGoToIndex) {
  GURL url0 = test_server_->GetURL("/echo?0");
  ASSERT_TRUE(LoadUrl(url0));
  ASSERT_TRUE(LoadUrl(test_server_->GetURL("/echo?1")));
  ASSERT_TRUE(LoadUrl(test_server_->GetURL("/echo?2")));

  navigation_manager_impl().DetachFromWebView();
  [web::test::GetWebController(web_state()) removeWebView];

  navigation_manager()->GoToIndex(0);

  EXPECT_EQ(nullptr, navigation_manager()->GetPendingItem());
  EXPECT_EQ(url0, navigation_manager()->GetVisibleItem()->GetURL());

  histogram_tester_.ExpectTotalCount(kRestoreNavigationItemCount, 1);
  histogram_tester_.ExpectBucketCount(kRestoreNavigationItemCount, 3, 1);
}

// Tests that LoadIfNecessary from detached mode restores cached history.
TEST_F(NavigationManagerImplTest, DetachedModeLoadIfNecessary) {
  ASSERT_TRUE(LoadUrl(test_server_->GetURL("/echo?0")));
  ASSERT_TRUE(LoadUrl(test_server_->GetURL("/echo?1")));
  GURL url2 = test_server_->GetURL("/echo?2");
  ASSERT_TRUE(LoadUrl(url2));

  navigation_manager_impl().DetachFromWebView();
  [web::test::GetWebController(web_state()) removeWebView];

  navigation_manager()->LoadIfNecessary();

  EXPECT_EQ(nullptr, navigation_manager()->GetPendingItem());
  EXPECT_EQ(url2, navigation_manager()->GetVisibleItem()->GetURL());

  histogram_tester_.ExpectTotalCount(kRestoreNavigationItemCount, 1);
  histogram_tester_.ExpectBucketCount(kRestoreNavigationItemCount, 3, 1);
}

// Tests that LoadURLWithParams from detached mode restores backward history and
// adds the new item at the end.
TEST_F(NavigationManagerImplTest, DetachedModeLoadURLWithParams) {
  ASSERT_TRUE(LoadUrl(test_server_->GetURL("/echo?0")));
  ASSERT_TRUE(LoadUrl(test_server_->GetURL("/echo?1")));

  navigation_manager_impl().DetachFromWebView();
  [web::test::GetWebController(web_state()) removeWebView];

  GURL url = test_server_->GetURL("/echo?2");
  NavigationManager::WebLoadParams params(url);
  navigation_manager()->LoadURLWithParams(params);

  EXPECT_EQ(nullptr, navigation_manager()->GetPendingItem());
  EXPECT_EQ(url, navigation_manager()->GetVisibleItem()->GetURL());

  histogram_tester_.ExpectTotalCount(kRestoreNavigationItemCount, 1);
  histogram_tester_.ExpectBucketCount(kRestoreNavigationItemCount, 3, 1);
}

}  // namespace web
