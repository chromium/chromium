// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/navigation_manager_util.h"

#import <WebKit/WebKit.h>

#import "base/memory/ptr_util.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/test/fakes/crw_fake_back_forward_list.h"
#import "ios/web/test/fakes/fake_navigation_manager_delegate.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace web {

// Testing fixture for navigation_manager_util.h functions.
class NavigationManagerUtilTest : public PlatformTest {
 protected:
  NavigationManagerUtilTest() {
    WKWebView* mock_web_view = OCMClassMock([WKWebView class]);
    mock_wk_list_ = [[CRWFakeBackForwardList alloc] init];
    OCMStub([mock_web_view backForwardList]).andReturn(mock_wk_list_);
    delegate_.SetWebViewNavigationProxy(mock_web_view);
    manager_ =
        std::make_unique<NavigationManagerImpl>(&browser_state_, &delegate_);
  }

  std::unique_ptr<NavigationManagerImpl> manager_;
  web::FakeNavigationManagerDelegate delegate_;
  CRWFakeBackForwardList* mock_wk_list_ = nil;

 private:
  FakeBrowserState browser_state_;
};

// Tests GetCommittedItemWithUniqueID, GetCommittedItemIndexWithUniqueID and
// GetItemWithUniqueID functions.
TEST_F(NavigationManagerUtilTest, GetCommittedItemWithUniqueID) {
  // Start with NavigationManager that only has a pending item.
  std::unique_ptr<NavigationContextImpl> context =
      NavigationContextImpl::CreateNavigationContext(
          /*web_state=*/nullptr, GURL(),
          /*has_user_gesture=*/false, ui::PAGE_TRANSITION_TYPED,
          /*is_renderer_initiated=*/false);
  manager_->AddPendingItem(
      GURL("http://chromium.org"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      /*is_post_navigation=*/false, /*is_error_navigation=*/false,
      web::HttpsUpgradeType::kNone);
  NavigationItem* item = manager_->GetPendingItem();
  int unique_id = item->GetUniqueID();
  context->SetNavigationItemUniqueID(item->GetUniqueID());
  EXPECT_FALSE(GetCommittedItemWithUniqueID(manager_.get(), unique_id));
  EXPECT_EQ(item, GetItemWithUniqueID(manager_.get(), context.get()));
  EXPECT_EQ(-1, GetCommittedItemIndexWithUniqueID(manager_.get(), unique_id));

  // Commit that pending item.
  [mock_wk_list_ setCurrentURL:@"http://chromium.org"];
  manager_->CommitPendingItem();
  EXPECT_EQ(item, GetCommittedItemWithUniqueID(manager_.get(), unique_id));
  EXPECT_EQ(item, GetItemWithUniqueID(manager_.get(), context.get()));
  EXPECT_EQ(0, GetCommittedItemIndexWithUniqueID(manager_.get(), unique_id));

  // Add item to NavigationContextImpl.
  auto context_item = std::make_unique<NavigationItemImpl>();
  context->SetNavigationItemUniqueID(context_item->GetUniqueID());
  context->SetItem(std::move(context_item));
  EXPECT_EQ(context->GetItem(),
            GetItemWithUniqueID(manager_.get(), context.get()));
}

}  // namespace web
