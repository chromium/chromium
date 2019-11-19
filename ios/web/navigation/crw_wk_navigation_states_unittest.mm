// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_wk_navigation_states.h"

#import <WebKit/WebKit.h>

#import "ios/web/navigation/navigation_context_impl.h"
#import "ios/web/navigation/navigation_item_impl.h"
#include "net/http/http_response_headers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kTestUrl1[] = "https://test1.test/";
const char kTestUrl2[] = "https://test2.test/";
}

namespace web {

// Test fixture for CRWWKNavigationStates testing.
class CRWWKNavigationStatesTest : public PlatformTest {
 protected:
  CRWWKNavigationStatesTest()
      : navigation1_(static_cast<WKNavigation*>([[NSObject alloc] init])),
        navigation2_(static_cast<WKNavigation*>([[NSObject alloc] init])),
        navigation3_(static_cast<WKNavigation*>([[NSObject alloc] init])),
        states_([[CRWWKNavigationStates alloc] init]) {}

 protected:
  WKNavigation* navigation1_;
  WKNavigation* navigation2_;
  WKNavigation* navigation3_;
  CRWWKNavigationStates* states_;
};

// Tests |removeNavigation:| method.
TEST_F(CRWWKNavigationStatesTest, RemovingNavigation) {
  // navigation_1 is the only navigation and it is the latest.
  [states_ setState:WKNavigationState::REQUESTED forNavigation:navigation1_];
  ASSERT_EQ(WKNavigationState::REQUESTED,
            [states_ stateForNavigation:navigation1_]);
  ASSERT_EQ(navigation1_, [states_ lastAddedNavigation]);
  [states_ removeNavigation:navigation1_];
  EXPECT_FALSE([states_ lastAddedNavigation]);
  EXPECT_EQ(WKNavigationState::NONE, [states_ stateForNavigation:navigation1_]);
}

// Tests |lastAddedNavigation| method.
TEST_F(CRWWKNavigationStatesTest, LastAddedNavigation) {
  // navigation_1 is the only navigation and it is the latest.
  [states_ setState:WKNavigationState::REQUESTED forNavigation:navigation1_];
  EXPECT_EQ(WKNavigationState::REQUESTED,
            [states_ stateForNavigation:navigation1_]);
  EXPECT_EQ(navigation1_, [states_ lastAddedNavigation]);
  EXPECT_EQ(WKNavigationState::REQUESTED, [states_ lastAddedNavigationState]);

  // navigation_2 is added later and hence the latest.
  [states_ setState:WKNavigationState::REQUESTED forNavigation:navigation2_];
  EXPECT_EQ(WKNavigationState::REQUESTED,
            [states_ stateForNavigation:navigation2_]);
  EXPECT_EQ(navigation2_, [states_ lastAddedNavigation]);
  EXPECT_EQ(WKNavigationState::REQUESTED, [states_ lastAddedNavigationState]);

  // Updating state for existing navigation does not make it the latest.
  [states_ setState:WKNavigationState::STARTED forNavigation:navigation1_];
  EXPECT_EQ(WKNavigationState::STARTED,
            [states_ stateForNavigation:navigation1_]);
  EXPECT_EQ(navigation2_, [states_ lastAddedNavigation]);
  EXPECT_EQ(WKNavigationState::REQUESTED, [states_ lastAddedNavigationState]);

  // navigation_2 is still the latest.
  [states_ setState:WKNavigationState::STARTED forNavigation:navigation2_];
  EXPECT_EQ(WKNavigationState::STARTED,
            [states_ stateForNavigation:navigation2_]);
  EXPECT_EQ(navigation2_, [states_ lastAddedNavigation]);
  EXPECT_EQ(WKNavigationState::STARTED, [states_ lastAddedNavigationState]);

  // navigation_3 is added later and hence the latest.
  std::unique_ptr<web::NavigationContextImpl> context =
      NavigationContextImpl::CreateNavigationContext(
          nullptr /*web_state*/, GURL(kTestUrl1), /*has_user_gesture=*/false,
          ui::PageTransition::PAGE_TRANSITION_SERVER_REDIRECT,
          /*is_renderer_initiated=*/true);
  [states_ setContext:std::move(context) forNavigation:navigation3_];
  EXPECT_EQ(navigation3_, [states_ lastAddedNavigation]);
  EXPECT_EQ(WKNavigationState::NONE, [states_ lastAddedNavigationState]);
}

// Tests |lastNavigationWithPendingItemInNavigationContext| method.
TEST_F(CRWWKNavigationStatesTest,
       LastNavigationWithPendingItemInNavigationContext) {
  // Empty state.
  EXPECT_FALSE([states_ lastNavigationWithPendingItemInNavigationContext]);

  // Navigation without context.
  [states_ setState:WKNavigationState::REQUESTED forNavigation:navigation1_];
  EXPECT_FALSE([states_ lastNavigationWithPendingItemInNavigationContext]);

  // Navigation with context that does not have pending item.
  std::unique_ptr<web::NavigationContextImpl> context =
      NavigationContextImpl::CreateNavigationContext(
          nullptr /*web_state*/, GURL(kTestUrl1), /*has_user_gesture=*/false,
          ui::PageTransition::PAGE_TRANSITION_SERVER_REDIRECT,
          /*is_renderer_initiated=*/true);
  web::NavigationContextImpl* context_ptr = context.get();
  [states_ setContext:std::move(context) forNavigation:navigation1_];
  EXPECT_FALSE([states_ lastNavigationWithPendingItemInNavigationContext]);

  // Navigation with context that has pending item.
  auto item = std::make_unique<NavigationItemImpl>();
  context_ptr->SetNavigationItemUniqueID(item->GetUniqueID());
  context_ptr->SetItem(std::move(item));
  EXPECT_EQ(navigation1_,
            [states_ lastNavigationWithPendingItemInNavigationContext]);

  // Newest context does not have pending item.
  std::unique_ptr<web::NavigationContextImpl> context2 =
      NavigationContextImpl::CreateNavigationContext(
          nullptr /*web_state*/, GURL(kTestUrl1), /*has_user_gesture=*/false,
          ui::PageTransition::PAGE_TRANSITION_SERVER_REDIRECT,
          /*is_renderer_initiated=*/true);
  web::NavigationContextImpl* context_ptr2 = context2.get();
  [states_ setState:WKNavigationState::REQUESTED forNavigation:navigation2_];
  [states_ setContext:std::move(context2) forNavigation:navigation2_];
  EXPECT_EQ(navigation1_,
            [states_ lastNavigationWithPendingItemInNavigationContext]);

  // Navigation with newest context that has pending item.
  auto item2 = std::make_unique<NavigationItemImpl>();
  context_ptr2->SetNavigationItemUniqueID(item2->GetUniqueID());
  context_ptr2->SetItem(std::move(item2));
  EXPECT_EQ(navigation2_,
            [states_ lastNavigationWithPendingItemInNavigationContext]);
}

// Tests |setContext:forNavigation:| and |contextForNavigation:| methods.
TEST_F(CRWWKNavigationStatesTest, Context) {
  EXPECT_FALSE([states_ contextForNavigation:navigation1_]);
  EXPECT_FALSE([states_ contextForNavigation:navigation2_]);
  EXPECT_FALSE([states_ contextForNavigation:navigation3_]);

  // Add first context.
  std::unique_ptr<web::NavigationContextImpl> context1 =
      NavigationContextImpl::CreateNavigationContext(
          nullptr /*web_state*/, GURL(kTestUrl1), /*has_user_gesture=*/false,
          ui::PageTransition::PAGE_TRANSITION_RELOAD,
          /*is_renderer_initiated=*/false);
  context1->SetIsSameDocument(true);
  [states_ setContext:std::move(context1) forNavigation:navigation1_];
  EXPECT_FALSE([states_ contextForNavigation:navigation2_]);
  EXPECT_FALSE([states_ contextForNavigation:navigation3_]);
  ASSERT_TRUE([states_ contextForNavigation:navigation1_]);
  EXPECT_EQ(GURL(kTestUrl1),
            [states_ contextForNavigation:navigation1_] -> GetUrl());
  EXPECT_TRUE([states_ contextForNavigation:navigation1_] -> IsSameDocument());
  EXPECT_FALSE([states_ contextForNavigation:navigation1_] -> GetError());
  EXPECT_FALSE(
      [states_ contextForNavigation:navigation1_] -> IsRendererInitiated());

  // Replace existing context.
  std::unique_ptr<web::NavigationContextImpl> context2 =
      NavigationContextImpl::CreateNavigationContext(
          nullptr /*web_state*/, GURL(kTestUrl2), /*has_user_gesture=*/false,
          ui::PageTransition::PAGE_TRANSITION_GENERATED,
          /*is_renderer_initiated=*/true);
  NSError* error = [[NSError alloc] initWithDomain:@"" code:0 userInfo:nil];
  context2->SetError(error);
  [states_ setContext:std::move(context2) forNavigation:navigation1_];
  EXPECT_FALSE([states_ contextForNavigation:navigation2_]);
  EXPECT_FALSE([states_ contextForNavigation:navigation3_]);
  ASSERT_TRUE([states_ contextForNavigation:navigation1_]);
  EXPECT_EQ(GURL(kTestUrl2),
            [states_ contextForNavigation:navigation1_] -> GetUrl());
  EXPECT_FALSE([states_ contextForNavigation:navigation1_] -> IsSameDocument());
  EXPECT_EQ(error, [states_ contextForNavigation:navigation1_] -> GetError());
  EXPECT_TRUE(
      [states_ contextForNavigation:navigation1_] -> IsRendererInitiated());

  // Extract existing context.
  std::unique_ptr<web::NavigationContextImpl> extractedContext =
      [states_ removeNavigation:navigation1_];
  EXPECT_EQ(GURL(kTestUrl2), extractedContext->GetUrl());
  EXPECT_FALSE(extractedContext->IsSameDocument());
  EXPECT_EQ(error, extractedContext->GetError());
  EXPECT_TRUE(extractedContext->IsRendererInitiated());
}

// Tests null WKNavigation object.
TEST_F(CRWWKNavigationStatesTest, NullNavigation) {
  // navigation_1 is the only navigation and it is the latest.
  [states_ setState:WKNavigationState::REQUESTED forNavigation:navigation1_];
  EXPECT_EQ(WKNavigationState::REQUESTED,
            [states_ stateForNavigation:navigation1_]);
  ASSERT_EQ(navigation1_, [states_ lastAddedNavigation]);
  EXPECT_EQ(WKNavigationState::REQUESTED, [states_ lastAddedNavigationState]);

  // null navigation is added later and hence the latest.
  [states_ setState:WKNavigationState::STARTED forNavigation:nil];
  EXPECT_EQ(WKNavigationState::STARTED, [states_ stateForNavigation:nil]);
  EXPECT_FALSE([states_ lastAddedNavigation]);
  EXPECT_EQ(WKNavigationState::STARTED, [states_ lastAddedNavigationState]);

  // navigation_1 is the latest again after removing null navigation.
  [states_ removeNavigation:nil];
  ASSERT_EQ(navigation1_, [states_ lastAddedNavigation]);
  EXPECT_EQ(WKNavigationState::REQUESTED, [states_ lastAddedNavigationState]);
}

// Tests -[CRWWKNavigationStates pendingNavigations].
TEST_F(CRWWKNavigationStatesTest, PendingNavigations) {
  ASSERT_EQ(0U, [states_ pendingNavigations].count);

  // Add pending navigation_1.
  [states_ setState:WKNavigationState::REQUESTED forNavigation:navigation1_];
  ASSERT_EQ(WKNavigationState::REQUESTED,
            [states_ stateForNavigation:navigation1_]);
  ASSERT_EQ(1U, [states_ pendingNavigations].count);
  EXPECT_TRUE([[states_ pendingNavigations] containsObject:navigation1_]);

  // Add pending navigation_2.
  [states_ setState:WKNavigationState::STARTED forNavigation:navigation2_];
  ASSERT_EQ(WKNavigationState::STARTED,
            [states_ stateForNavigation:navigation2_]);
  ASSERT_EQ(2U, [states_ pendingNavigations].count);
  EXPECT_TRUE([[states_ pendingNavigations] containsObject:navigation1_]);
  EXPECT_TRUE([[states_ pendingNavigations] containsObject:navigation2_]);

  // Add pending navigation_3.
  [states_ setState:WKNavigationState::STARTED forNavigation:navigation3_];
  ASSERT_EQ(WKNavigationState::STARTED,
            [states_ stateForNavigation:navigation3_]);
  ASSERT_EQ(3U, [states_ pendingNavigations].count);
  EXPECT_TRUE([[states_ pendingNavigations] containsObject:navigation1_]);
  EXPECT_TRUE([[states_ pendingNavigations] containsObject:navigation2_]);
  EXPECT_TRUE([[states_ pendingNavigations] containsObject:navigation3_]);

  // Add pending null navigation.
  [states_ setState:WKNavigationState::STARTED forNavigation:nil];
  ASSERT_EQ(WKNavigationState::STARTED, [states_ stateForNavigation:nil]);
  ASSERT_EQ(4U, [states_ pendingNavigations].count);
  EXPECT_TRUE([[states_ pendingNavigations] containsObject:navigation1_]);
  EXPECT_TRUE([[states_ pendingNavigations] containsObject:navigation2_]);
  EXPECT_TRUE([[states_ pendingNavigations] containsObject:navigation3_]);
  EXPECT_TRUE([[states_ pendingNavigations] containsObject:[NSNull null]]);

  // Provisionally fail null navigation.
  [states_ setState:WKNavigationState::PROVISIONALY_FAILED forNavigation:nil];
  ASSERT_EQ(WKNavigationState::PROVISIONALY_FAILED,
            [states_ stateForNavigation:nil]);
  ASSERT_EQ(3U, [states_ pendingNavigations].count);
  EXPECT_TRUE([[states_ pendingNavigations] containsObject:navigation2_]);
  EXPECT_TRUE([[states_ pendingNavigations] containsObject:navigation3_]);
  EXPECT_TRUE([[states_ pendingNavigations] containsObject:navigation3_]);

  // Commit navigation_1.
  EXPECT_FALSE([states_ isCommittedNavigation:navigation1_]);
  [states_ setState:WKNavigationState::COMMITTED forNavigation:navigation1_];
  ASSERT_EQ(WKNavigationState::COMMITTED,
            [states_ stateForNavigation:navigation1_]);
  ASSERT_EQ(2U, [states_ pendingNavigations].count);
  EXPECT_TRUE([[states_ pendingNavigations] containsObject:navigation2_]);
  EXPECT_TRUE([[states_ pendingNavigations] containsObject:navigation3_]);
  EXPECT_TRUE([states_ isCommittedNavigation:navigation1_]);

  // Finish navigation_1.
  [states_ setState:WKNavigationState::FINISHED forNavigation:navigation1_];
  ASSERT_EQ(WKNavigationState::FINISHED,
            [states_ stateForNavigation:navigation1_]);
  ASSERT_EQ(2U, [states_ pendingNavigations].count);
  EXPECT_TRUE([[states_ pendingNavigations] containsObject:navigation2_]);
  EXPECT_TRUE([[states_ pendingNavigations] containsObject:navigation3_]);

  // Remove navigation_2.
  [states_ removeNavigation:navigation2_];
  ASSERT_EQ(1U, [states_ pendingNavigations].count);
  EXPECT_TRUE([[states_ pendingNavigations] containsObject:navigation3_]);

  // Fail navigation_3.
  [states_ setState:WKNavigationState::FAILED forNavigation:navigation3_];
  ASSERT_EQ(WKNavigationState::FAILED,
            [states_ stateForNavigation:navigation3_]);
  ASSERT_EQ(0U, [states_ pendingNavigations].count);
}

}  // namespace web
