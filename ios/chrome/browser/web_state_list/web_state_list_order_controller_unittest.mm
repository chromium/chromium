// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_list_order_controller.h"

#include <memory>

#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_removing_indexes.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char kURL[] = "https://chromium.org/";

// A fake NavigationManager used to test opener-opened relationship in the
// WebStateList.
class FakeNavigationManager : public web::FakeNavigationManager {
 public:
  FakeNavigationManager() = default;

  FakeNavigationManager(const FakeNavigationManager&) = delete;
  FakeNavigationManager& operator=(const FakeNavigationManager&) = delete;

  // web::NavigationManager implementation.
  int GetLastCommittedItemIndex() const override { return 0; }
};

}  // namespace

class WebStateListOrderControllerTest : public PlatformTest {
 public:
  WebStateListOrderControllerTest()
      : web_state_list_(&web_state_list_delegate_),
        order_controller_(web_state_list_) {}

 protected:
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
  WebStateListOrderController order_controller_;

  void InsertNewWebState(int index, WebStateOpener opener) {
    auto test_web_state = std::make_unique<web::FakeWebState>();
    test_web_state->SetCurrentURL(GURL(kURL));
    test_web_state->SetNavigationManager(
        std::make_unique<FakeNavigationManager>());
    web_state_list_.InsertWebState(index, std::move(test_web_state),
                                   WebStateList::INSERT_FORCE_INDEX, opener);
  }
};

TEST_F(WebStateListOrderControllerTest, DetermineInsertionIndex) {
  InsertNewWebState(0, WebStateOpener());
  InsertNewWebState(1, WebStateOpener());
  web::WebState* opener = web_state_list_.GetWebStateAt(0);

  // Verify that first child WebState is inserted after |opener| if there are
  // no other children.
  EXPECT_EQ(1, order_controller_.DetermineInsertionIndex(opener));

  // Verify that  WebState is inserted at the end if it has no opener.
  EXPECT_EQ(2, order_controller_.DetermineInsertionIndex(nullptr));

  // Add a child WebState to |opener|, and verify that a second child would be
  // inserted after the first.
  InsertNewWebState(2, WebStateOpener(opener));

  EXPECT_EQ(3, order_controller_.DetermineInsertionIndex(opener));

  // Add a grand-child to |opener|, and verify that adding another child to
  // |opener| would be inserted before the grand-child.
  InsertNewWebState(3, WebStateOpener(web_state_list_.GetWebStateAt(1)));

  EXPECT_EQ(3, order_controller_.DetermineInsertionIndex(opener));
}

// Test that the selection of the next tab to show when closing a tab respect
// the opener-opened relationship (note: the index returned always omit the
// would be closed WebState, so there is a - 1 offset).
TEST_F(WebStateListOrderControllerTest, DetermineNewActiveIndex) {
  InsertNewWebState(0, WebStateOpener());

  // Verify that if closing the last WebState, no WebState is selected.
  EXPECT_EQ(WebStateList::kInvalidIndex,
            order_controller_.DetermineNewActiveIndex(0, {0}));

  InsertNewWebState(1, WebStateOpener());
  InsertNewWebState(2, WebStateOpener());
  InsertNewWebState(3, WebStateOpener(web_state_list_.GetWebStateAt(0)));
  InsertNewWebState(4, WebStateOpener(web_state_list_.GetWebStateAt(0)));
  InsertNewWebState(5, WebStateOpener(web_state_list_.GetWebStateAt(1)));
  InsertNewWebState(6, WebStateOpener());

  // Verify that if there is no active WebState, no WebState will be activated.
  EXPECT_EQ(WebStateList::kInvalidIndex,
            order_controller_.DetermineNewActiveIndex(
                WebStateList::kInvalidIndex, {0}));

  // Verify that if closing a WebState that is not active, the active item
  // is not changed, but the index is updated only if active WebState is
  // after the closed WebState.
  EXPECT_EQ(3, order_controller_.DetermineNewActiveIndex(3, {4}));
  EXPECT_EQ(4, order_controller_.DetermineNewActiveIndex(5, {4}));

  // Verify that if closing a WebState with siblings, the next sibling is
  // selected.
  EXPECT_EQ(3, order_controller_.DetermineNewActiveIndex(3, {3}));

  // Verify that if closing a WebState with siblings, the next sibling is
  // selected even if it is before the active WebState.
  EXPECT_EQ(3, order_controller_.DetermineNewActiveIndex(4, {4}));

  // Verify that if closing a WebState with no sibling, the opener is selected.
  EXPECT_EQ(1, order_controller_.DetermineNewActiveIndex(5, {5}));

  // Verify that if closing a WebState with children, the first child is
  // selected.
  EXPECT_EQ(4, order_controller_.DetermineNewActiveIndex(1, {1}));

  // Verify that if closing a WebState with children, the first child is
  // selected, even if it is before the active WebState.
  EXPECT_EQ(4, order_controller_.DetermineNewActiveIndex(1, {1}));

  // Verify that if closing a WebState with no child, the next WebState is
  // selected, or the previous one if the last WebState was closed.
  EXPECT_EQ(2, order_controller_.DetermineNewActiveIndex(2, {2}));
  EXPECT_EQ(5, order_controller_.DetermineNewActiveIndex(6, {6}));
}

// Test that the selection of the next tab to show when closing multiple
// tabs respect the opener-opened relationship (note: the index returned
// always omit the would be closed WebStates).
TEST_F(WebStateListOrderControllerTest,
       DetermineNewActiveIndexClosingMultipleTabs) {
  InsertNewWebState(0, WebStateOpener());

  // Verify that if closing the last WebState, no WebState is selected.
  EXPECT_EQ(WebStateList::kInvalidIndex,
            order_controller_.DetermineNewActiveIndex(0, {0}));

  InsertNewWebState(1, WebStateOpener());
  InsertNewWebState(2, WebStateOpener());
  InsertNewWebState(3, WebStateOpener(web_state_list_.GetWebStateAt(0)));
  InsertNewWebState(4, WebStateOpener(web_state_list_.GetWebStateAt(0)));
  InsertNewWebState(5, WebStateOpener(web_state_list_.GetWebStateAt(1)));
  InsertNewWebState(6, WebStateOpener());

  // Verify that if closing all WebStates, no WebState is selected.
  EXPECT_EQ(
      WebStateList::kInvalidIndex,
      order_controller_.DetermineNewActiveIndex(0, {0, 1, 2, 3, 4, 5, 6}));

  // Verify that if there is no active WebState, no WebState will be activated.
  EXPECT_EQ(WebStateList::kInvalidIndex,
            order_controller_.DetermineNewActiveIndex(
                WebStateList::kInvalidIndex, {0}));

  // Verify that if closing a WebState that is not active, the active item
  // is not changed, but the index is updated only if active WebState is
  // after the closed WebState.
  EXPECT_EQ(3, order_controller_.DetermineNewActiveIndex(3, {4, 6}));
  EXPECT_EQ(4, order_controller_.DetermineNewActiveIndex(5, {4, 6}));
  EXPECT_EQ(4, order_controller_.DetermineNewActiveIndex(6, {4, 5}));

  // Verify that if closing a WebState and its siblings, the opener is
  // selected.
  EXPECT_EQ(0, order_controller_.DetermineNewActiveIndex(3, {3, 4}));

  // Verify that if closing a WebState with children, the first non closed
  // child is selected.
  EXPECT_EQ(2, order_controller_.DetermineNewActiveIndex(0, {0, 3}));

  // Verify that if closing a WebState with children and all its children,
  // the tab after it is selected.
  EXPECT_EQ(0, order_controller_.DetermineNewActiveIndex(0, {0, 3, 4}));

  // Verify that if closing a WebState, all its children and all the tab
  // after it, then the tab before it is selected.
  EXPECT_EQ(1, order_controller_.DetermineNewActiveIndex(2, {2, 3, 4, 5, 6}));
}
