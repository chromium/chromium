// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/order_controller_source_from_web_state_list.h"

#import "ios/chrome/browser/shared/model/web_state_list/test/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

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
  int GetLastCommittedItemIndex() const override { return index_; }

  // Setter for GetLastCommittedItemIndex().
  void SetLastCommittedItemIndex(int index) { index_ = index; }

 private:
  int index_ = 0;
};

}  // anonymous namespace

class OrderControllerSourceFromWebStateListTest : public PlatformTest {
 public:
  OrderControllerSourceFromWebStateListTest()
      : web_state_list_(&web_state_list_delegate_) {}

  void InsertNewWebState(int index, WebStateOpener opener) {
    auto test_web_state = std::make_unique<web::FakeWebState>();
    test_web_state->SetCurrentURL(GURL(kURL));
    test_web_state->SetNavigationManager(
        std::make_unique<FakeNavigationManager>());
    web_state_list_.InsertWebState(
        std::move(test_web_state),
        WebStateList::InsertionParams::AtIndex(index).WithOpener(opener));
  }

  WebStateList& web_state_list() { return web_state_list_; }

 private:
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
};

// Tests that GetCount() returns the correct value.
TEST_F(OrderControllerSourceFromWebStateListTest, GetCount) {
  OrderControllerSourceFromWebStateList source(web_state_list());

  // Test that GetCount() returns the list count.
  EXPECT_EQ(0, source.GetCount());

  // Test that GetCount() returns the list count when items are
  // inserted.
  for (int index = 0; index < 10; ++index) {
    InsertNewWebState(index, WebStateOpener());
    EXPECT_EQ(index + 1, source.GetCount());
  }

  // Test that GetCount() returns the list count when items are
  // removed.
  CloseAllWebStates(web_state_list(), WebStateList::CLOSE_NO_FLAGS);
  EXPECT_EQ(0, source.GetCount());
}

// Tests that GetPinnedCount() returns the correct value.
TEST_F(OrderControllerSourceFromWebStateListTest, GetPinnedCount) {
  OrderControllerSourceFromWebStateList source(web_state_list());

  // Insert a few tabs in the WebStateList.
  for (int index = 0; index < 10; ++index) {
    InsertNewWebState(index, WebStateOpener());
  }

  // Test that GetPinnedCount() returns the number of pinned tabs.
  EXPECT_EQ(0, source.GetPinnedCount());

  // Test that GetPinnedCount() returns the number of pinned tabs
  // when new tabs are pinned.
  for (int index = 0; index < 5; ++index) {
    web_state_list().SetWebStatePinnedAt(index, true);
    EXPECT_EQ(index + 1, source.GetPinnedCount());
  }

  // Test that GetPinnedCount() returns the number of pinned tabs
  // when tabs are unpinned.
  for (int index = 0; index < 5; ++index) {
    web_state_list().SetWebStatePinnedAt(0, false);
    EXPECT_EQ(5 - (index + 1), source.GetPinnedCount());
  }
}

// Tests that GetOpenerOfItemAt() returns the correct value.
TEST_F(OrderControllerSourceFromWebStateListTest, GetOpenerOfItemAt) {
  OrderControllerSourceFromWebStateList source(web_state_list());

  // Insert a few tabs in the WebStateList, some with openers.
  InsertNewWebState(0, WebStateOpener());
  InsertNewWebState(1, WebStateOpener());
  InsertNewWebState(2, WebStateOpener(web_state_list().GetWebStateAt(0)));
  InsertNewWebState(0, WebStateOpener(web_state_list().GetWebStateAt(1)));

  // Check that GetOpenerOfItemAt() returns the correct value.
  EXPECT_EQ(source.GetOpenerOfItemAt(0), 2);
  EXPECT_EQ(source.GetOpenerOfItemAt(1), WebStateList::kInvalidIndex);
  EXPECT_EQ(source.GetOpenerOfItemAt(2), WebStateList::kInvalidIndex);
  EXPECT_EQ(source.GetOpenerOfItemAt(3), 1);
}

// Tests that IsOpenerOfItemAt() returns the correct value.
TEST_F(OrderControllerSourceFromWebStateListTest, IsOpenerOfItemAt) {
  OrderControllerSourceFromWebStateList source(web_state_list());

  // Insert a few tabs in the WebStateList, some with openers.
  InsertNewWebState(0, WebStateOpener());
  InsertNewWebState(1, WebStateOpener(web_state_list().GetWebStateAt(0)));
  InsertNewWebState(2, WebStateOpener(web_state_list().GetWebStateAt(1)));

  // Change the last committed index of the WebState at index 0 to check
  // that IsOpenerOfItemAt() respects `check_navigation_index`.
  static_cast<FakeNavigationManager*>(
      web_state_list().GetWebStateAt(0)->GetNavigationManager())
      ->SetLastCommittedItemIndex(1);

  // Check that IsOpenerOfItemAt() returns the correct value.
  EXPECT_EQ(source.IsOpenerOfItemAt(0, 1, true), false);
  EXPECT_EQ(source.IsOpenerOfItemAt(0, 2, true), false);
  EXPECT_EQ(source.IsOpenerOfItemAt(0, 1, false), false);
  EXPECT_EQ(source.IsOpenerOfItemAt(0, 2, false), false);

  EXPECT_EQ(source.IsOpenerOfItemAt(1, 0, true), false);
  EXPECT_EQ(source.IsOpenerOfItemAt(1, 2, true), false);
  EXPECT_EQ(source.IsOpenerOfItemAt(1, 0, false), true);
  EXPECT_EQ(source.IsOpenerOfItemAt(1, 2, false), false);

  EXPECT_EQ(source.IsOpenerOfItemAt(2, 0, true), false);
  EXPECT_EQ(source.IsOpenerOfItemAt(2, 1, true), true);
  EXPECT_EQ(source.IsOpenerOfItemAt(2, 0, false), false);
  EXPECT_EQ(source.IsOpenerOfItemAt(2, 1, false), true);
}
