// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_session_controller.h"

#import <Foundation/Foundation.h>

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "ios/web/common/features.h"
#import "ios/web/navigation/crw_session_controller+private_constructors.h"
#import "ios/web/navigation/legacy_navigation_manager_impl.h"
#import "ios/web/navigation/navigation_item_impl.h"
#import "ios/web/navigation/navigation_manager_impl.h"
#include "ios/web/public/navigation/referrer.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#import "ios/web/test/fakes/crw_fake_session_controller_delegate.h"
#include "ios/web/test/fakes/fake_navigation_manager_delegate.h"
#import "net/base/mac/url_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using UserAgentOverrideOption = web::NavigationManager::UserAgentOverrideOption;

@interface CRWSessionController (Testing)
- (const GURL&)URLForItemAtIndex:(size_t)index;
- (const GURL&)currentURL;
@end

@implementation CRWSessionController (Testing)
- (const GURL&)URLForItemAtIndex:(size_t)index {
  if (index < self.items.size())
    return self.items[index]->GetURL();
  return GURL::EmptyGURL();
}

- (const GURL&)currentURL {
  return self.currentItem ? self.currentItem->GetURL() : GURL::EmptyGURL();
}
@end

namespace {

class CRWSessionControllerTest : public PlatformTest {
 protected:
  void SetUp() override {
    session_controller_ =
        [[CRWSessionController alloc] initWithBrowserState:&browser_state_];
    session_controller_delegate_ =
        [[CRWFakeSessionControllerDelegate alloc] init];
    session_controller_.delegate = session_controller_delegate_;
    CreateNavigationManagerForSessionController(session_controller_);
  }

  // Creates a new LegacyNavigationManagerImpl object and links it with
  // session_controller. This method should be called for each newly created
  // CRWSessionController because they rely on a non-null NavigationManagerImpl
  // to create NavigationItemImpl objects. The life time of the created
  // navigation managers are managed by the test fixture class.
  void CreateNavigationManagerForSessionController(
      CRWSessionController* session_controller) {
    auto navigation_manager =
        std::make_unique<web::LegacyNavigationManagerImpl>();
    navigation_manager->SetBrowserState(&browser_state_);
    navigation_manager->SetDelegate(&navigation_manager_delegate_);
    navigation_manager->SetSessionController(session_controller);
    navigation_managers_.push_back(std::move(navigation_manager));

    navigation_manager_delegate_.SetWebState(&web_state_);
  }

  web::Referrer MakeReferrer(const std::string& url) {
    return web::Referrer(GURL(url), web::ReferrerPolicyDefault);
  }

  base::test::ScopedFeatureList feature_list_;
  web::WebTaskEnvironment task_environment_;
  web::TestBrowserState browser_state_;
  web::TestWebState web_state_;
  web::FakeNavigationManagerDelegate navigation_manager_delegate_;
  CRWSessionController* session_controller_;
  CRWFakeSessionControllerDelegate* session_controller_delegate_ = nil;
  // Implements RAII pattern for navigation manager objects created by
  // CreateNavigationManagerForSessionController.
  std::vector<std::unique_ptr<web::LegacyNavigationManagerImpl>>
      navigation_managers_;
};

TEST_F(CRWSessionControllerTest, Init) {
  EXPECT_TRUE([session_controller_ items].empty());
  EXPECT_FALSE([session_controller_ currentItem]);
}

// Tests that [session_controller_ pendingItem] returns item provided by the
// delegate.
TEST_F(CRWSessionControllerTest, GetPendingItemFromDelegate) {
  feature_list_.InitAndDisableFeature(web::features::kSlimNavigationManager);

  ASSERT_FALSE([session_controller_ pendingItem]);
  auto item = std::make_unique<web::NavigationItemImpl>();
  session_controller_delegate_.pendingItem = item.get();
  EXPECT_EQ(item.get(), [session_controller_ pendingItem]);
}

// Tests that [session_controller_ pendingItem] ignores item provided by
// the delegate if session controller has own pending item.
TEST_F(CRWSessionControllerTest, GetPendingItemIgnoringDelegate) {
  ASSERT_FALSE([session_controller_ pendingItem]);
  auto item = std::make_unique<web::NavigationItemImpl>();
  session_controller_delegate_.pendingItem = item.get();

  GURL url("http://www.url.test");
  [session_controller_
               addPendingItem:url
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];

  EXPECT_NE(item.get(), [session_controller_ pendingItem]);
  ASSERT_TRUE([session_controller_ pendingItem]);
  EXPECT_EQ(url, [session_controller_ pendingItem] -> GetURL());
}

// Tests session controller state after setting a pending index.
TEST_F(CRWSessionControllerTest, SetPendingIndex) {
  auto delegate_item = std::make_unique<web::NavigationItemImpl>();
  session_controller_delegate_.pendingItem = delegate_item.get();

  [session_controller_
               addPendingItem:GURL("http://www.example.com")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  EXPECT_EQ(-1, [session_controller_ pendingItemIndex]);
  [session_controller_ setPendingItemIndex:0];
  EXPECT_EQ(0, [session_controller_ pendingItemIndex]);
  EXPECT_EQ([session_controller_ items].back().get(),
            [session_controller_ pendingItem]);
}

TEST_F(CRWSessionControllerTest, addPendingItem) {
  [session_controller_
               addPendingItem:GURL("http://www.url.com")
                     referrer:MakeReferrer("http://www.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];

  EXPECT_TRUE([session_controller_ items].empty());
  EXPECT_EQ(
      GURL("http://www.url.com/"),
      [session_controller_ currentURL]);
}

TEST_F(CRWSessionControllerTest, addPendingItemWithCommittedItems) {
  [session_controller_
               addPendingItem:GURL("http://www.committed.url.com")
                     referrer:MakeReferrer("http://www.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  [session_controller_
               addPendingItem:GURL("http://www.url.com")
                     referrer:MakeReferrer("http://www.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];

  EXPECT_EQ(1U, [session_controller_ items].size());
  EXPECT_EQ(GURL("http://www.committed.url.com/"),
            [session_controller_ URLForItemAtIndex:0U]);
  EXPECT_EQ(
      GURL("http://www.url.com/"),
      [session_controller_ currentURL]);
}

// Tests that adding a pending item resets pending item index.
TEST_F(CRWSessionControllerTest, addPendingItemWithExistingPendingItemIndex) {
  [session_controller_
               addPendingItem:GURL("http://www.example.com")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.com/0")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  // Set 0 as pending item index.
  [session_controller_ setPendingItemIndex:0];
  EXPECT_EQ(GURL("http://www.example.com/"),
            [session_controller_ pendingItem]->GetURL());
  EXPECT_EQ(0, [session_controller_ pendingItemIndex]);

  // Add a pending item, which should drop pending navigation index.
  [session_controller_
               addPendingItem:GURL("http://www.example.com/1")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  EXPECT_EQ(GURL("http://www.example.com/1"),
            [session_controller_ pendingItem]->GetURL());
  EXPECT_EQ(-1, [session_controller_ pendingItemIndex]);
}

TEST_F(CRWSessionControllerTest, addPendingItemOverriding) {
  [session_controller_
               addPendingItem:GURL("http://www.url.com")
                     referrer:MakeReferrer("http://www.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_
               addPendingItem:GURL("http://www.another.url.com")
                     referrer:MakeReferrer("http://www.another.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];

  EXPECT_TRUE([session_controller_ items].empty());
  EXPECT_EQ(
      GURL("http://www.another.url.com/"),
      [session_controller_ currentURL]);
}

TEST_F(CRWSessionControllerTest, addPendingItemAndCommit) {
  [session_controller_
               addPendingItem:GURL("http://www.url.com")
                     referrer:MakeReferrer("http://www.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  EXPECT_EQ(1U, [session_controller_ items].size());
  EXPECT_EQ(GURL("http://www.url.com/"),
            [session_controller_ URLForItemAtIndex:0U]);
  EXPECT_EQ([session_controller_ items].front().get(),
            [session_controller_ currentItem]);
}

TEST_F(CRWSessionControllerTest, addPendingItemOverridingAndCommit) {
  [session_controller_
               addPendingItem:GURL("http://www.url.com")
                     referrer:MakeReferrer("http://www.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_
               addPendingItem:GURL("http://www.another.url.com")
                     referrer:MakeReferrer("http://www.another.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  EXPECT_EQ(1U, [session_controller_ items].size());
  EXPECT_EQ(GURL("http://www.another.url.com/"),
            [session_controller_ URLForItemAtIndex:0U]);
  EXPECT_EQ([session_controller_ items].front().get(),
            [session_controller_ currentItem]);
}

TEST_F(CRWSessionControllerTest, addPendingItemAndCommitMultiple) {
  [session_controller_
               addPendingItem:GURL("http://www.url.com")
                     referrer:MakeReferrer("http://www.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  [session_controller_
               addPendingItem:GURL("http://www.another.url.com")
                     referrer:MakeReferrer("http://www.another.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  EXPECT_EQ(2U, [session_controller_ items].size());
  EXPECT_EQ(GURL("http://www.url.com/"),
            [session_controller_ URLForItemAtIndex:0U]);
  EXPECT_EQ(GURL("http://www.another.url.com/"),
            [session_controller_ URLForItemAtIndex:1U]);
  EXPECT_EQ([session_controller_ items][1U].get(),
            [session_controller_ currentItem]);
}

TEST_F(CRWSessionControllerTest, addPendingItemAndDiscard) {
  [session_controller_
               addPendingItem:GURL("http://www.url.com")
                     referrer:MakeReferrer("http://www.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ discardNonCommittedItems];

  EXPECT_TRUE([session_controller_ items].empty());
  EXPECT_FALSE([session_controller_ currentItem]);
}

// Tests discarding pending item added via |setPendingItemIndex:| call.
TEST_F(CRWSessionControllerTest, setPendingItemIndexAndDiscard) {
  [session_controller_
               addPendingItem:GURL("http://www.example.com")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  [session_controller_ setPendingItemIndex:0];
  EXPECT_TRUE([session_controller_ pendingItem]);
  EXPECT_EQ(0, [session_controller_ pendingItemIndex]);

  [session_controller_ discardNonCommittedItems];
  EXPECT_FALSE([session_controller_ pendingItem]);
  EXPECT_EQ(-1, [session_controller_ pendingItemIndex]);
}

TEST_F(CRWSessionControllerTest, addPendingItemAndDiscardAndAddAndCommit) {
  [session_controller_
               addPendingItem:GURL("http://www.url.com")
                     referrer:MakeReferrer("http://www.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ discardNonCommittedItems];

  [session_controller_
               addPendingItem:GURL("http://www.another.url.com")
                     referrer:MakeReferrer("http://www.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  EXPECT_EQ(1U, [session_controller_ items].size());
  EXPECT_EQ(GURL("http://www.another.url.com/"),
            [session_controller_ URLForItemAtIndex:0U]);
  EXPECT_EQ([session_controller_ items].front().get(),
            [session_controller_ currentItem]);
}

TEST_F(CRWSessionControllerTest, addPendingItemAndCommitAndAddAndDiscard) {
  [session_controller_
               addPendingItem:GURL("http://www.url.com")
                     referrer:MakeReferrer("http://www.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  [session_controller_
               addPendingItem:GURL("http://www.another.url.com")
                     referrer:MakeReferrer("http://www.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ discardNonCommittedItems];

  EXPECT_EQ(1U, [session_controller_ items].size());
  EXPECT_EQ(GURL("http://www.url.com/"),
            [session_controller_ URLForItemAtIndex:0U]);
  EXPECT_EQ([session_controller_ items].front().get(),
            [session_controller_ currentItem]);
}

TEST_F(CRWSessionControllerTest,
       commitPendingItemWithoutPendingOrCommittedItem) {
  [session_controller_ commitPendingItem];

  EXPECT_TRUE([session_controller_ items].empty());
  EXPECT_FALSE([session_controller_ currentItem]);
}

TEST_F(CRWSessionControllerTest,
       commitPendingItemWithoutPendingItemWithCommittedItem) {
  // Setup committed item.
  [session_controller_
               addPendingItem:GURL("http://www.url.com/")
                     referrer:MakeReferrer("http://www.referrer.com/")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  // Commit pending item when there is no such one
  [session_controller_ commitPendingItem];

  EXPECT_EQ(1U, [session_controller_ items].size());
  EXPECT_EQ([session_controller_ items].front().get(),
            [session_controller_ currentItem]);
}

// Tests that forward items are discarded after navigation item is committed.
TEST_F(CRWSessionControllerTest, commitPendingItemWithExistingForwardItems) {
  // Make 3 items.
  [session_controller_
               addPendingItem:GURL("http://www.example.com/0")
                     referrer:MakeReferrer("http://www.example.com/a")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::RENDERER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.com/1")
                     referrer:MakeReferrer("http://www.example.com/b")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::RENDERER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.com/2")
                     referrer:MakeReferrer("http://www.example.com/c")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::RENDERER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  // Go back to the first item.
  [session_controller_ goToItemAtIndex:0 discardNonCommittedItems:NO];

  // Create and commit a new pending item.
  [session_controller_
               addPendingItem:GURL("http://www.example.com/2")
                     referrer:MakeReferrer("http://www.example.com/c")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::RENDERER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  // All forward items should go away.
  EXPECT_EQ(2U, [session_controller_ items].size());
  EXPECT_EQ(0U, [session_controller_ forwardItems].size());
  ASSERT_EQ(1, [session_controller_ lastCommittedItemIndex]);
  ASSERT_EQ(0, [session_controller_ previousItemIndex]);
}

// Tests committing pending item index from the middle.
TEST_F(CRWSessionControllerTest, commitPendingItemIndex) {
  [session_controller_
               addPendingItem:GURL("http://www.example.com/0")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.com/1")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.com/2")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  ASSERT_EQ(3U, [session_controller_ items].size());

  // Go to the middle, and commit first pending item index.
  [session_controller_ goToItemAtIndex:1 discardNonCommittedItems:NO];
  [session_controller_ setPendingItemIndex:0];
  ASSERT_EQ(0, [session_controller_ pendingItemIndex]);
  web::NavigationItem* pending_item = [session_controller_ pendingItem];
  ASSERT_TRUE(pending_item);
  ASSERT_EQ(1, [session_controller_ lastCommittedItemIndex]);
  EXPECT_EQ(2, [session_controller_ previousItemIndex]);
  [session_controller_ commitPendingItem];

  // Verify that pending item has been committed and current and previous item
  // indices updated.
  EXPECT_EQ(pending_item, [session_controller_ lastCommittedItem]);
  EXPECT_EQ(-1, [session_controller_ pendingItemIndex]);
  EXPECT_FALSE([session_controller_ pendingItem]);
  EXPECT_EQ(0, [session_controller_ lastCommittedItemIndex]);
  EXPECT_EQ(1, [session_controller_ previousItemIndex]);
  EXPECT_EQ(3U, [session_controller_ items].size());
}

// Tests that -[CRWSessionController commitPendingItem:] is no-op when called
// with null.
TEST_F(CRWSessionControllerTest, CommitNilPendingItem) {
  ASSERT_TRUE([session_controller_ items].empty());
  [session_controller_ commitPendingItem:nil];
  EXPECT_TRUE([session_controller_ items].empty());
}

// Tests -[CRWSessionController commitPendingItem:] with a valid pending item.
TEST_F(CRWSessionControllerTest, CommitNonNilPendingItem) {
  // Create session controller with a single forward item and no back items.
  [session_controller_
               addPendingItem:GURL("http://www.example.test/0")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.test/1")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_ goToItemAtIndex:0 discardNonCommittedItems:NO];
  ASSERT_EQ(2U, session_controller_.items.size());
  ASSERT_EQ(0, session_controller_.lastCommittedItemIndex);

  // Call commitPendingItem: with a valid pending item.
  auto item = std::make_unique<web::NavigationItemImpl>();
  item->SetNavigationInitiationType(
      web::NavigationInitiationType::BROWSER_INITIATED);
  [session_controller_ commitPendingItem:std::move(item)];

  // Verify session controller and navigation item states.
  EXPECT_EQ(0, session_controller_.previousItemIndex);
  EXPECT_EQ(1, session_controller_.lastCommittedItemIndex);
  EXPECT_EQ(-1, session_controller_.pendingItemIndex);
  ASSERT_TRUE(session_controller_.lastCommittedItem);
  EXPECT_FALSE(session_controller_.lastCommittedItem->GetTimestamp().is_null());
  EXPECT_EQ(web::NavigationInitiationType::NONE,
            session_controller_.lastCommittedItem->NavigationInitiationType());
  ASSERT_EQ(2U, session_controller_.items.size());
  EXPECT_EQ(session_controller_.lastCommittedItem,
            [session_controller_ itemAtIndex:1]);
}

TEST_F(CRWSessionControllerTest,
       DiscardPendingItemWithoutPendingOrCommittedItem) {
  [session_controller_ discardNonCommittedItems];

  EXPECT_TRUE([session_controller_ items].empty());
  EXPECT_FALSE([session_controller_ currentItem]);
}

TEST_F(CRWSessionControllerTest,
       DiscardPendingItemWithoutPendingItemWithCommittedItem) {
  // Setup committed item
  [session_controller_
               addPendingItem:GURL("http://www.url.com")
                     referrer:MakeReferrer("http://www.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  // Discard noncommitted items when there is no such one
  [session_controller_ discardNonCommittedItems];

  EXPECT_EQ(1U, [session_controller_ items].size());
  EXPECT_EQ([session_controller_ items].front().get(),
            [session_controller_ currentItem]);
}

// Tests inserting session controller state.
TEST_F(CRWSessionControllerTest, CopyState) {
  // Add 1 committed and 1 pending item to target controller.
  [session_controller_
               addPendingItem:GURL("http://www.url.com/2")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.url.com/3")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];

  // Create source session controller with 1 committed item.
  CRWSessionController* other_session_controller =
      [[CRWSessionController alloc] initWithBrowserState:&browser_state_];
  CreateNavigationManagerForSessionController(other_session_controller);
  [other_session_controller
               addPendingItem:GURL("http://www.url.com/0")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [other_session_controller commitPendingItem];
  [other_session_controller
               addPendingItem:GURL("http://www.url.com/1")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];

  // Insert and verify the state of target session controller.
  EXPECT_TRUE([session_controller_ canPruneAllButLastCommittedItem]);
  [session_controller_
      copyStateFromSessionControllerAndPrune:other_session_controller];

  EXPECT_EQ(2U, [session_controller_ items].size());
  EXPECT_EQ(1, [session_controller_ lastCommittedItemIndex]);
  EXPECT_EQ(-1, [session_controller_ previousItemIndex]);
  EXPECT_EQ(-1, [session_controller_ pendingItemIndex]);

  EXPECT_EQ(GURL("http://www.url.com/0"),
            [session_controller_ URLForItemAtIndex:0]);
  EXPECT_EQ(GURL("http://www.url.com/2"),
            [session_controller_ URLForItemAtIndex:1]);
  EXPECT_EQ(GURL("http://www.url.com/3"),
            [session_controller_ pendingItem]->GetURL());
}

// Tests inserting session controller state from empty session controller.
TEST_F(CRWSessionControllerTest, CopyStateFromEmptySessionController) {
  // Add 2 committed items to target controller.
  [session_controller_
               addPendingItem:GURL("http://www.url.com/0")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.url.com/1")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  // Create empty source session controller.
  CRWSessionController* other_session_controller =
      [[CRWSessionController alloc] initWithBrowserState:&browser_state_];
  CreateNavigationManagerForSessionController(other_session_controller);

  // Insert and verify the state of target session controller.
  EXPECT_TRUE([session_controller_ canPruneAllButLastCommittedItem]);
  [session_controller_
      copyStateFromSessionControllerAndPrune:other_session_controller];
  EXPECT_EQ(2U, [session_controller_ items].size());
  EXPECT_EQ(1, [session_controller_ lastCommittedItemIndex]);
  EXPECT_EQ(0, [session_controller_ previousItemIndex]);
  EXPECT_EQ(-1, [session_controller_ pendingItemIndex]);
  EXPECT_FALSE([session_controller_ pendingItem]);
  EXPECT_EQ(GURL("http://www.url.com/0"),
            [session_controller_ URLForItemAtIndex:0]);
  EXPECT_EQ(GURL("http://www.url.com/1"),
            [session_controller_ URLForItemAtIndex:1]);
}

// Tests that |-copyStateFromSessionControllerAndPrune:| is a no-op when the
// receiver has no last committed item.
TEST_F(CRWSessionControllerTest, CopyStateToEmptySessionController) {
  EXPECT_FALSE([session_controller_ canPruneAllButLastCommittedItem]);

  // Create source session controller with 1 committed item.
  CRWSessionController* other_session_controller =
      [[CRWSessionController alloc] initWithBrowserState:&browser_state_];
  CreateNavigationManagerForSessionController(other_session_controller);
  [other_session_controller
               addPendingItem:GURL("http://www.url.com/0")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [other_session_controller commitPendingItem];
  [other_session_controller
               addPendingItem:GURL("http://www.url.com/1")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];

  // Attempt to copy |other_session_controller|'s state and verify that
  // |session_controller_| is unchanged.
  [session_controller_
      copyStateFromSessionControllerAndPrune:other_session_controller];
  EXPECT_TRUE([session_controller_ items].empty());
  EXPECT_EQ(-1, [session_controller_ lastCommittedItemIndex]);
  EXPECT_EQ(-1, [session_controller_ previousItemIndex]);
  EXPECT_FALSE([session_controller_ currentItem]);
  EXPECT_FALSE([session_controller_ pendingItem]);
  EXPECT_EQ(-1, [session_controller_ pendingItemIndex]);
}

// Tests that |-copyStateFromSessionControllerAndPrune:| is a no-op during a
// pending history navigation.
TEST_F(CRWSessionControllerTest, CopyStateDuringPendingHistoryNavigation) {
  // Add 1 committed and 1 pending item to target controller.
  [session_controller_
               addPendingItem:GURL("http://www.url.com/1")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.url.com/2")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  // Create source session controller with 1 committed item.
  CRWSessionController* other_session_controller =
      [[CRWSessionController alloc] initWithBrowserState:&browser_state_];
  CreateNavigationManagerForSessionController(other_session_controller);
  [other_session_controller
               addPendingItem:GURL("http://www.url.com/0")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [other_session_controller commitPendingItem];
  [other_session_controller
               addPendingItem:GURL("http://www.url.com/1")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];

  // Set the pending item index to the first item.
  [session_controller_ setPendingItemIndex:0];
  EXPECT_FALSE([session_controller_ canPruneAllButLastCommittedItem]);

  // Attempt to copy |other_session_controller|'s state and verify that
  // |session_controller_| is unchanged.
  [session_controller_
      copyStateFromSessionControllerAndPrune:other_session_controller];
  EXPECT_EQ(2U, [session_controller_ items].size());
  EXPECT_EQ(1, [session_controller_ lastCommittedItemIndex]);
  EXPECT_EQ(0, [session_controller_ previousItemIndex]);
  EXPECT_EQ(0, [session_controller_ pendingItemIndex]);
  EXPECT_TRUE([session_controller_ pendingItem]);
  EXPECT_EQ([session_controller_ previousItem],
            [session_controller_ pendingItem]);
}

// Tests that |-copyStateFromSessionControllerAndPrune:| is a when a transient
// NavigationItem exists.
TEST_F(CRWSessionControllerTest, CopyStateWithTransientItem) {
  // Add 1 committed and 1 pending item to target controller.
  [session_controller_
               addPendingItem:GURL("http://www.url.com/1")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  GURL second_url = GURL("http://www.url.com/2");
  [session_controller_
               addPendingItem:second_url
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ addTransientItemWithURL:second_url];

  // Create source session controller with 1 committed item.
  CRWSessionController* other_session_controller =
      [[CRWSessionController alloc] initWithBrowserState:&browser_state_];
  CreateNavigationManagerForSessionController(other_session_controller);
  [other_session_controller
               addPendingItem:GURL("http://www.url.com/0")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [other_session_controller commitPendingItem];
  [other_session_controller
               addPendingItem:GURL("http://www.url.com/1")
                     referrer:web::Referrer()
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];

  // Attempt to copy |other_session_controller|'s state and verify that
  // |session_controller_| is unchanged.
  EXPECT_FALSE([session_controller_ canPruneAllButLastCommittedItem]);
  [session_controller_
      copyStateFromSessionControllerAndPrune:other_session_controller];
  EXPECT_EQ(1U, [session_controller_ items].size());
  EXPECT_EQ(0, [session_controller_ lastCommittedItemIndex]);
  EXPECT_EQ(-1, [session_controller_ previousItemIndex]);
  EXPECT_EQ(-1, [session_controller_ pendingItemIndex]);
  EXPECT_TRUE([session_controller_ pendingItem]);
  EXPECT_TRUE([session_controller_ transientItem]);
  EXPECT_EQ([session_controller_ transientItem],
            [session_controller_ currentItem]);
}

// Tests state of an empty session controller.
TEST_F(CRWSessionControllerTest, EmptyController) {
  EXPECT_TRUE([session_controller_ items].empty());
  EXPECT_EQ(-1, [session_controller_ lastCommittedItemIndex]);
  EXPECT_EQ(-1, [session_controller_ previousItemIndex]);
  EXPECT_FALSE([session_controller_ currentItem]);
  EXPECT_FALSE([session_controller_ pendingItem]);
  EXPECT_EQ(-1, [session_controller_ pendingItemIndex]);
}

// Helper to create a NavigationItem.
std::unique_ptr<web::NavigationItem> CreateNavigationItem(
    const std::string& url,
    const std::string& referrer,
    NSString* title) {
  web::Referrer referrer_object(GURL(referrer),
                                web::ReferrerPolicyDefault);
  std::unique_ptr<web::NavigationItemImpl> navigation_item =
      std::make_unique<web::NavigationItemImpl>();
  navigation_item->SetURL(GURL(url));
  navigation_item->SetReferrer(referrer_object);
  navigation_item->SetTitle(base::SysNSStringToUTF16(title));
  navigation_item->SetTransitionType(ui::PAGE_TRANSITION_TYPED);

  // XCode clang doesn't correctly handle implicit unique_ptr casting for
  // returned values, so manually cast here.
  std::unique_ptr<web::NavigationItem> item(
      static_cast<web::NavigationItem*>(navigation_item.release()));
  return item;
}

TEST_F(CRWSessionControllerTest, CreateWithEmptyNavigations) {
  std::vector<std::unique_ptr<web::NavigationItem>> items;
  CRWSessionController* controller =
      [[CRWSessionController alloc] initWithBrowserState:&browser_state_
                                         navigationItems:std::move(items)
                                  lastCommittedItemIndex:0];
  CreateNavigationManagerForSessionController(controller);
  EXPECT_TRUE(controller.items.empty());
  EXPECT_EQ(controller.lastCommittedItemIndex, -1);
  EXPECT_EQ(controller.previousItemIndex, -1);
  EXPECT_FALSE(controller.currentItem);
}

TEST_F(CRWSessionControllerTest, CreateWithNavList) {
  std::vector<std::unique_ptr<web::NavigationItem>> items;
  items.push_back(CreateNavigationItem("http://www.google.com",
                                       "http://www.referrer.com", @"Google"));
  items.push_back(CreateNavigationItem("http://www.yahoo.com",
                                       "http://www.google.com", @"Yahoo"));
  items.push_back(CreateNavigationItem("http://www.espn.com",
                                       "http://www.nothing.com", @"ESPN"));
  CRWSessionController* controller =
      [[CRWSessionController alloc] initWithBrowserState:&browser_state_
                                         navigationItems:std::move(items)
                                  lastCommittedItemIndex:1];
  CreateNavigationManagerForSessionController(controller);

  EXPECT_EQ(controller.items.size(), 3U);
  EXPECT_EQ(controller.lastCommittedItemIndex, 1);
  EXPECT_EQ(controller.previousItemIndex, -1);
  // Sanity check the current item, the NavigationItem unit test will ensure
  // the entire object is created properly.
  EXPECT_EQ([controller currentItem]->GetURL(), GURL("http://www.yahoo.com"));
}

// Tests index of previous navigation item.
TEST_F(CRWSessionControllerTest, PreviousNavigationItem) {
  EXPECT_EQ(session_controller_.previousItemIndex, -1);
  [session_controller_
               addPendingItem:GURL("http://www.url.com")
                     referrer:MakeReferrer("http://www.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  EXPECT_EQ(session_controller_.previousItemIndex, -1);
  [session_controller_
               addPendingItem:GURL("http://www.url1.com")
                     referrer:MakeReferrer("http://www.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  EXPECT_EQ(session_controller_.previousItemIndex, 0);
  [session_controller_
               addPendingItem:GURL("http://www.url2.com")
                     referrer:MakeReferrer("http://www.referer.com")
                   transition:ui::PAGE_TRANSITION_TYPED
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  EXPECT_EQ(session_controller_.previousItemIndex, 1);

  [session_controller_ goToItemAtIndex:1 discardNonCommittedItems:NO];
  EXPECT_EQ(session_controller_.previousItemIndex, 2);

  [session_controller_ goToItemAtIndex:0 discardNonCommittedItems:NO];
  EXPECT_EQ(session_controller_.previousItemIndex, 1);

  [session_controller_ goToItemAtIndex:1 discardNonCommittedItems:NO];
  EXPECT_EQ(session_controller_.previousItemIndex, 0);

  [session_controller_ goToItemAtIndex:2 discardNonCommittedItems:NO];
  EXPECT_EQ(session_controller_.previousItemIndex, 1);
}

TEST_F(CRWSessionControllerTest, PushNewItem) {
  std::vector<std::unique_ptr<web::NavigationItem>> items;
  items.push_back(CreateNavigationItem("http://www.firstpage.com",
                                       "http://www.starturl.com", @"First"));
  items.push_back(CreateNavigationItem("http://www.secondpage.com",
                                       "http://www.firstpage.com", @"Second"));
  items.push_back(CreateNavigationItem("http://www.thirdpage.com",
                                       "http://www.secondpage.com", @"Third"));
  CRWSessionController* controller =
      [[CRWSessionController alloc] initWithBrowserState:&browser_state_
                                         navigationItems:std::move(items)
                                  lastCommittedItemIndex:0];
  CreateNavigationManagerForSessionController(controller);

  GURL pushPageGurl1("http://www.firstpage.com/#push1");
  NSString* stateObject1 = @"{'foo': 1}";
  [controller pushNewItemWithURL:pushPageGurl1
                     stateObject:stateObject1
                      transition:ui::PAGE_TRANSITION_LINK];
  web::NavigationItemImpl* pushedItem = [controller currentItem];
  NSUInteger expectedCount = 2;
  EXPECT_EQ(expectedCount, controller.items.size());
  EXPECT_EQ(pushPageGurl1, pushedItem->GetURL());
  EXPECT_TRUE(pushedItem->IsCreatedFromPushState());
  EXPECT_NSEQ(stateObject1, pushedItem->GetSerializedStateObject());
  EXPECT_EQ(GURL("http://www.firstpage.com/"), pushedItem->GetReferrer().url);

  // Add another new item and check size and fields again.
  GURL pushPageGurl2("http://www.firstpage.com/push2");
  [controller pushNewItemWithURL:pushPageGurl2
                     stateObject:nil
                      transition:ui::PAGE_TRANSITION_LINK];
  pushedItem = [controller currentItem];
  expectedCount = 3;
  EXPECT_EQ(expectedCount, controller.items.size());
  EXPECT_EQ(pushPageGurl2, pushedItem->GetURL());
  EXPECT_TRUE(pushedItem->IsCreatedFromPushState());
  EXPECT_EQ(nil, pushedItem->GetSerializedStateObject());
  EXPECT_EQ(pushPageGurl1, pushedItem->GetReferrer().url);
}

TEST_F(CRWSessionControllerTest, IsSameDocumentNavigation) {
  std::vector<std::unique_ptr<web::NavigationItem>> items;
  items.push_back(
      CreateNavigationItem("http://foo.com", "http://google.com", @"First"));
  // Push state navigation.
  items.push_back(
      CreateNavigationItem("http://foo.com#bar", "http://foo.com", @"Second"));
  items.push_back(CreateNavigationItem("http://google.com",
                                       "http://foo.com#bar", @"Third"));
  items.push_back(
      CreateNavigationItem("http://foo.com", "http://google.com", @"Fourth"));
  // Push state navigation.
  items.push_back(
      CreateNavigationItem("http://foo.com/bar", "http://foo.com", @"Fifth"));
  // Push state navigation.
  items.push_back(CreateNavigationItem("http://foo.com/bar#bar",
                                       "http://foo.com/bar", @"Sixth"));
  items.push_back(CreateNavigationItem("http://fooz.com/bar#bar",
                                       "http://fooz.com/bar", @"Seventh"));
  CRWSessionController* controller =
      [[CRWSessionController alloc] initWithBrowserState:&browser_state_
                                         navigationItems:std::move(items)
                                  lastCommittedItemIndex:0];
  CreateNavigationManagerForSessionController(controller);
  web::NavigationItemImpl* item0 = [controller items][0].get();
  web::NavigationItemImpl* item1 = [controller items][1].get();
  web::NavigationItemImpl* item2 = [controller items][2].get();
  web::NavigationItemImpl* item3 = [controller items][3].get();
  web::NavigationItemImpl* item4 = [controller items][4].get();
  web::NavigationItemImpl* item5 = [controller items][5].get();
  web::NavigationItemImpl* item6 = [controller items][6].get();
  item1->SetIsCreatedFromPushState(true);
  item4->SetIsCreatedFromHashChange(true);
  item5->SetIsCreatedFromPushState(true);
  item6->SetIsCreatedFromHashChange(true);

  EXPECT_FALSE(
      [controller isSameDocumentNavigationBetweenItem:item0 andItem:item0]);
  EXPECT_TRUE(
      [controller isSameDocumentNavigationBetweenItem:item0 andItem:item1]);
  EXPECT_TRUE(
      [controller isSameDocumentNavigationBetweenItem:item5 andItem:item3]);
  EXPECT_TRUE(
      [controller isSameDocumentNavigationBetweenItem:item4 andItem:item3]);
  EXPECT_FALSE(
      [controller isSameDocumentNavigationBetweenItem:item1 andItem:item2]);
  EXPECT_FALSE(
      [controller isSameDocumentNavigationBetweenItem:item0 andItem:item5]);
  EXPECT_FALSE(
      [controller isSameDocumentNavigationBetweenItem:item2 andItem:item4]);
  EXPECT_FALSE(
      [controller isSameDocumentNavigationBetweenItem:item6 andItem:item5]);
}

TEST_F(CRWSessionControllerTest, TestBackwardForwardItems) {
  [session_controller_
               addPendingItem:GURL("http://www.example.com/0")
                     referrer:MakeReferrer("http://www.example.com/a")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.com/1")
                     referrer:MakeReferrer("http://www.example.com/b")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.com/redirect")
                     referrer:MakeReferrer("http://www.example.com/r")
                   transition:ui::PAGE_TRANSITION_CLIENT_REDIRECT
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.com/2")
                     referrer:MakeReferrer("http://www.example.com/c")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  EXPECT_EQ(3, session_controller_.lastCommittedItemIndex);
  web::NavigationItemList back_items = [session_controller_ backwardItems];
  EXPECT_EQ(2U, back_items.size());
  EXPECT_TRUE([session_controller_ forwardItems].empty());
  EXPECT_EQ("http://www.example.com/redirect", back_items[0]->GetURL().spec());

  [session_controller_ goToItemAtIndex:1 discardNonCommittedItems:NO];
  EXPECT_EQ(1U, [session_controller_ backwardItems].size());
  EXPECT_EQ(1U, [session_controller_ forwardItems].size());

  [session_controller_ goToItemAtIndex:0 discardNonCommittedItems:NO];
  web::NavigationItemList forwardItems = [session_controller_ forwardItems];
  EXPECT_EQ(0U, [session_controller_ backwardItems].size());
  EXPECT_EQ(2U, forwardItems.size());
  EXPECT_EQ("http://www.example.com/2", forwardItems[1]->GetURL().spec());
}

// Tests going to items with existing and non-existing indices.
TEST_F(CRWSessionControllerTest, GoToItemAtIndex) {
  [session_controller_
               addPendingItem:GURL("http://www.example.com/0")
                     referrer:MakeReferrer("http://www.example.com/a")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.com/1")
                     referrer:MakeReferrer("http://www.example.com/b")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.com/redirect")
                     referrer:MakeReferrer("http://www.example.com/r")
                   transition:ui::PAGE_TRANSITION_CLIENT_REDIRECT
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.com/2")
                     referrer:MakeReferrer("http://www.example.com/c")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.com/3")
                     referrer:MakeReferrer("http://www.example.com/d")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ addTransientItemWithURL:GURL("http://www.example.com")];
  EXPECT_EQ(3, session_controller_.lastCommittedItemIndex);
  EXPECT_EQ(2, session_controller_.previousItemIndex);
  EXPECT_TRUE([session_controller_ pendingItem]);
  EXPECT_TRUE([session_controller_ transientItem]);

  // Going back and forth without discaring transient and pending items.
  [session_controller_ goToItemAtIndex:1 discardNonCommittedItems:NO];
  EXPECT_EQ(1, session_controller_.lastCommittedItemIndex);
  EXPECT_EQ(3, session_controller_.previousItemIndex);
  EXPECT_TRUE(session_controller_.pendingItem);
  EXPECT_TRUE(session_controller_.transientItem);
  [session_controller_ goToItemAtIndex:3 discardNonCommittedItems:NO];
  EXPECT_EQ(3, session_controller_.lastCommittedItemIndex);
  EXPECT_EQ(1, session_controller_.previousItemIndex);
  EXPECT_TRUE(session_controller_.pendingItem);
  EXPECT_TRUE(session_controller_.transientItem);

  // Going back should discard transient and pending items.
  [session_controller_ goToItemAtIndex:1 discardNonCommittedItems:YES];
  EXPECT_EQ(1, session_controller_.lastCommittedItemIndex);
  EXPECT_EQ(3, session_controller_.previousItemIndex);
  EXPECT_FALSE(session_controller_.pendingItem);
  EXPECT_FALSE(session_controller_.transientItem);

  // Going forward should discard transient item.
  [session_controller_ addTransientItemWithURL:GURL("http://www.example.com")];
  EXPECT_TRUE(session_controller_.transientItem);
  [session_controller_ goToItemAtIndex:2 discardNonCommittedItems:YES];
  EXPECT_EQ(2, session_controller_.lastCommittedItemIndex);
  EXPECT_EQ(1, session_controller_.previousItemIndex);
  EXPECT_FALSE(session_controller_.transientItem);

  // Out of bounds navigations should be no-op.
  [session_controller_ goToItemAtIndex:-1 discardNonCommittedItems:NO];
  EXPECT_EQ(2, session_controller_.lastCommittedItemIndex);
  EXPECT_EQ(1, session_controller_.previousItemIndex);
  [session_controller_ goToItemAtIndex:NSIntegerMax
              discardNonCommittedItems:NO];
  EXPECT_EQ(2, session_controller_.lastCommittedItemIndex);
  EXPECT_EQ(1, session_controller_.previousItemIndex);

  // Going to current index should not change the previous index.
  [session_controller_ goToItemAtIndex:2 discardNonCommittedItems:NO];
  EXPECT_EQ(2, session_controller_.lastCommittedItemIndex);
  EXPECT_EQ(1, session_controller_.previousItemIndex);
}

// Tests that visible URL is the same as transient URL if there are no committed
// items.
TEST_F(CRWSessionControllerTest, VisibleItemWithSingleTransientItem) {
  [session_controller_ addTransientItemWithURL:GURL("http://www.example.com")];
  web::NavigationItem* visible_item = [session_controller_ visibleItem];
  ASSERT_TRUE(visible_item);
  EXPECT_EQ("http://www.example.com/", visible_item->GetURL().spec());
}

// Tests that visible URL is the same as transient URL if there is a committed
// item.
TEST_F(CRWSessionControllerTest, VisibleItemWithCommittedAndTransientItems) {
  [session_controller_
               addPendingItem:GURL("http://www.example.com/0")
                     referrer:MakeReferrer("http://www.example.com/a")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_ addTransientItemWithURL:GURL("http://www.example.com")];
  web::NavigationItem* visible_item = [session_controller_ visibleItem];
  ASSERT_TRUE(visible_item);
  EXPECT_EQ("http://www.example.com/", visible_item->GetURL().spec());
}

// Tests that visible URL is the same as pending URL if it was user-initiated.
TEST_F(CRWSessionControllerTest,
       VisibleItemWithSingleUserInitiatedPendingItem) {
  web_state_.SetLoading(true);
  [session_controller_
               addPendingItem:GURL("http://www.example.com/0")
                     referrer:MakeReferrer("http://www.example.com/a")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  web::NavigationItem* visible_item = [session_controller_ visibleItem];
  ASSERT_TRUE(visible_item);
  EXPECT_EQ("http://www.example.com/0", visible_item->GetURL().spec());
}

// Tests that visible URL is the same as pending URL if it was user-initiated
// and there is a committed item.
TEST_F(CRWSessionControllerTest,
       VisibleItemWithCommittedAndUserInitiatedPendingItem) {
  web_state_.SetLoading(true);
  [session_controller_
               addPendingItem:GURL("http://www.example.com")
                     referrer:MakeReferrer("http://www.example.com/a")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.com/0")
                     referrer:MakeReferrer("http://www.example.com/b")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  web::NavigationItem* visible_item = [session_controller_ visibleItem];
  ASSERT_TRUE(visible_item);
  EXPECT_EQ("http://www.example.com/0", visible_item->GetURL().spec());
}

// Tests that visible URL is not the same as pending URL if it was
// renderer-initiated.
TEST_F(CRWSessionControllerTest,
       VisibleItemWithSingleRendererInitiatedPendingItem) {
  [session_controller_
               addPendingItem:GURL("http://www.example.com/0")
                     referrer:MakeReferrer("http://www.example.com/a")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::RENDERER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  web::NavigationItem* visible_item = [session_controller_ visibleItem];
  ASSERT_FALSE(visible_item);
}

// Tests that visible URL is not the same as pending URL if it was
// renderer-initiated and there is a committed item.
TEST_F(CRWSessionControllerTest,
       VisibleItemWithCommittedAndRendererInitiatedPendingItem) {
  [session_controller_
               addPendingItem:GURL("http://www.example.com")
                     referrer:MakeReferrer("http://www.example.com/a")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::RENDERER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.com/0")
                     referrer:MakeReferrer("http://www.example.com/b")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::RENDERER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  web::NavigationItem* visible_item = [session_controller_ visibleItem];
  ASSERT_TRUE(visible_item);
  EXPECT_EQ("http://www.example.com/", visible_item->GetURL().spec());
}

// Tests that visible URL is not the same as pending URL created via pending
// navigation index.
TEST_F(CRWSessionControllerTest, VisibleItemWithPendingNavigationIndex) {
  [session_controller_
               addPendingItem:GURL("http://www.example.com")
                     referrer:MakeReferrer("http://www.example.com/a")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.com/0")
                     referrer:MakeReferrer("http://www.example.com/b")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];

  [session_controller_ setPendingItemIndex:0];

  web::NavigationItem* visible_item = [session_controller_ visibleItem];
  ASSERT_TRUE(visible_item);
  EXPECT_EQ("http://www.example.com/0", visible_item->GetURL().spec());
}

// Tests that |-backwardItems| is empty if all preceding items are
// redirects.
TEST_F(CRWSessionControllerTest, BackwardItemsForAllRedirects) {
  [session_controller_
               addPendingItem:GURL("http://www.example.com")
                     referrer:MakeReferrer("http://www.example.com/a")
                   transition:ui::PAGE_TRANSITION_CLIENT_REDIRECT
               initiationType:web::NavigationInitiationType::RENDERER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.com/0")
                     referrer:MakeReferrer("http://www.example.com/b")
                   transition:ui::PAGE_TRANSITION_CLIENT_REDIRECT
               initiationType:web::NavigationInitiationType::RENDERER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  EXPECT_EQ(0U, [session_controller_ backwardItems].size());
}

// Tests that |-pendingItem| is not considered part of session history so that
// |-backwardItems| returns the second last committed item even if there is a
// pendign item.
TEST_F(CRWSessionControllerTest, NewPendingItemIsHiddenFromHistory) {
  [session_controller_
               addPendingItem:GURL("http://www.example.com/0")
                     referrer:MakeReferrer("http://www.example.com/a")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.com/1")
                     referrer:MakeReferrer("http://www.example.com/b")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
               addPendingItem:GURL("http://www.example.com/2")
                     referrer:MakeReferrer("http://www.example.com/c")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];

  EXPECT_EQ(1, session_controller_.lastCommittedItemIndex);
  EXPECT_TRUE([session_controller_ pendingItem]);
  EXPECT_EQ(-1, [session_controller_ pendingItemIndex]);
  EXPECT_EQ(GURL("http://www.example.com/2"), [session_controller_ currentURL]);

  web::NavigationItemList back_items = [session_controller_ backwardItems];
  ASSERT_EQ(1U, back_items.size());
  EXPECT_EQ("http://www.example.com/0", back_items[0]->GetURL().spec());
}

// Tests that |-backwardItems| returns all committed items if there is a
// transient item. This can happen if an interstitial was loaded for SSL error.
// See crbug.com/691311.
TEST_F(CRWSessionControllerTest,
       BackwardItemsShouldContainAllCommittedIfCurrentIsTransient) {
  [session_controller_
               addPendingItem:GURL("http://www.example.com/0")
                     referrer:MakeReferrer("http://www.example.com/a")
                   transition:ui::PAGE_TRANSITION_LINK
               initiationType:web::NavigationInitiationType::BROWSER_INITIATED
      userAgentOverrideOption:UserAgentOverrideOption::INHERIT];
  [session_controller_ commitPendingItem];
  [session_controller_
      addTransientItemWithURL:GURL("http://www.example.com/1")];

  EXPECT_EQ(0, session_controller_.lastCommittedItemIndex);
  EXPECT_TRUE([session_controller_ transientItem]);
  EXPECT_EQ(GURL("http://www.example.com/1"), [session_controller_ currentURL]);

  web::NavigationItemList back_items = [session_controller_ backwardItems];
  ASSERT_EQ(1U, back_items.size());
  EXPECT_EQ("http://www.example.com/0", back_items[0]->GetURL().spec());
}

// Tests that |-backwardItems| works as expected when the transient item is the
// only item in history.
TEST_F(CRWSessionControllerTest, BackwardItemsShouldBeEmptyIfFirstIsTransient) {
  [session_controller_
      addTransientItemWithURL:GURL("http://www.example.com/1")];

  EXPECT_EQ(-1, session_controller_.lastCommittedItemIndex);
  EXPECT_TRUE([session_controller_ transientItem]);
  EXPECT_EQ(GURL("http://www.example.com/1"), [session_controller_ currentURL]);

  web::NavigationItemList back_items = [session_controller_ backwardItems];
  EXPECT_TRUE(back_items.empty());
}

// Tests setPendingItem: and releasePendingItem methods.
TEST_F(CRWSessionControllerTest, TransferPendingItem) {
  auto item = std::make_unique<web::NavigationItemImpl>();
  web::NavigationItemImpl* item_ptr = item.get();

  [session_controller_ setPendingItem:std::move(item)];
  EXPECT_EQ(item_ptr, [session_controller_ pendingItem]);

  auto extracted_item = [session_controller_ releasePendingItem];
  EXPECT_FALSE([session_controller_ pendingItem]);
  EXPECT_EQ(item_ptr, extracted_item.get());
}

}  // anonymous namespace
