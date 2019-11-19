// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/navigation/navigation_manager_util.h"

#import <WebKit/WebKit.h>

#include "base/memory/ptr_util.h"
#import "ios/web/navigation/crw_session_controller+private_constructors.h"
#import "ios/web/navigation/crw_session_controller.h"
#import "ios/web/navigation/legacy_navigation_manager_impl.h"
#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/wk_based_navigation_manager_impl.h"
#import "ios/web/public/navigation/navigation_item.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/test/fakes/crw_fake_back_forward_list.h"
#import "ios/web/test/fakes/fake_navigation_manager_delegate.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

// Parameterized fixture testing navigation_manager_util.h functions.
// GetParam() chooses whether to run the tests on LegacyNavigationManagerImpl
// or (the soon-to-be-added) WKBasedNavigationManagerImpl.
// TODO(crbug.com/734150): cleanup LegacyNavigationManagerImpl use case.
class NavigationManagerUtilTest : public PlatformTest,
                                  public ::testing::WithParamInterface<bool> {
 protected:
  NavigationManagerUtilTest() {
    bool test_legacy_navigation_manager = GetParam();
    if (test_legacy_navigation_manager) {
      controller_ =
          [[CRWSessionController alloc] initWithBrowserState:&browser_state_];
      manager_ = std::make_unique<LegacyNavigationManagerImpl>();
      manager_->SetBrowserState(&browser_state_);
      manager_->SetSessionController(controller_);
    } else {
      manager_ = std::make_unique<WKBasedNavigationManagerImpl>();
      manager_->SetBrowserState(&browser_state_);
      WKWebView* mock_web_view = OCMClassMock([WKWebView class]);
      mock_wk_list_ = [[CRWFakeBackForwardList alloc] init];
      OCMStub([mock_web_view backForwardList]).andReturn(mock_wk_list_);
      delegate_.SetWebViewNavigationProxy(mock_web_view);
    }
    manager_->SetDelegate(&delegate_);
  }

  std::unique_ptr<NavigationManagerImpl> manager_;
  web::FakeNavigationManagerDelegate delegate_;
  CRWSessionController* controller_ = nil;
  CRWFakeBackForwardList* mock_wk_list_ = nil;

 private:
  TestBrowserState browser_state_;
};

// Tests GetCommittedItemWithUniqueID, GetCommittedItemIndexWithUniqueID and
// GetItemWithUniqueID functions.
TEST_P(NavigationManagerUtilTest, GetCommittedItemWithUniqueID) {
  // Start with NavigationManager that only has a pending item.
  std::unique_ptr<NavigationContextImpl> context =
      NavigationContextImpl::CreateNavigationContext(
          /*web_state=*/nullptr, GURL::EmptyGURL(),
          /*has_user_gesture=*/false, ui::PAGE_TRANSITION_TYPED,
          /*is_renderer_initiated=*/false);
  manager_->AddPendingItem(
      GURL("http://chromium.org"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
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

  // Commit another navigation so that the current item is updated.  This allows
  // for removing the item with |unique_id|.
  manager_->AddPendingItem(
      GURL("http://test.org"), Referrer(), ui::PAGE_TRANSITION_TYPED,
      web::NavigationInitiationType::BROWSER_INITIATED,
      web::NavigationManager::UserAgentOverrideOption::INHERIT);
  manager_->CommitPendingItem();
  manager_->RemoveItemAtIndex(0);
  EXPECT_FALSE(GetCommittedItemWithUniqueID(manager_.get(), unique_id));
  EXPECT_FALSE(GetItemWithUniqueID(manager_.get(), context.get()));
  EXPECT_EQ(-1, GetCommittedItemIndexWithUniqueID(manager_.get(), unique_id));

  // Add transient item.
  manager_->AddTransientItem(GURL("http://chromium.org"));
  item = manager_->GetTransientItem();
  unique_id = item->GetUniqueID();
  context->SetNavigationItemUniqueID(unique_id);
  EXPECT_FALSE(GetCommittedItemWithUniqueID(manager_.get(), unique_id));
  EXPECT_EQ(item, GetItemWithUniqueID(manager_.get(), context.get()));
  EXPECT_EQ(-1, GetCommittedItemIndexWithUniqueID(manager_.get(), unique_id));

  // Add item to NavigationContextImpl.
  auto context_item = std::make_unique<NavigationItemImpl>();
  context->SetNavigationItemUniqueID(context_item->GetUniqueID());
  context->SetItem(std::move(context_item));
  EXPECT_EQ(context->GetItem(),
            GetItemWithUniqueID(manager_.get(), context.get()));
}

INSTANTIATE_TEST_SUITE_P(
    ProgrammaticNavigationManagerUtilTest,
    NavigationManagerUtilTest,
    ::testing::Values(/*test_legacy_navigation_manager=*/true,
                      /*test_legacy_navigation_manager=*/false));

}  // namespace web
