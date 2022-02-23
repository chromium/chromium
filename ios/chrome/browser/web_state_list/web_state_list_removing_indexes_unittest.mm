// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web_state_list/web_state_list_removing_indexes.h"

#import "ios/chrome/browser/web_state_list/fake_web_state_list_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_opener.h"
#import "ios/web/public/test/fakes/fake_navigation_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

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

class WebStateListRemovingIndexesTest : public PlatformTest {
 public:
  WebStateListRemovingIndexesTest()
      : web_state_list_(&web_state_list_delegate_) {}

  web::WebState* InsertNewWebState(int index, WebStateOpener opener) {
    auto test_web_state = std::make_unique<web::FakeWebState>();
    test_web_state->SetCurrentURL(GURL(kURL));
    test_web_state->SetNavigationManager(
        std::make_unique<FakeNavigationManager>());
    const int insertion_index = web_state_list_.InsertWebState(
        index, std::move(test_web_state), WebStateList::INSERT_FORCE_INDEX,
        opener);
    return web_state_list_.GetWebStateAt(insertion_index);
  }

 protected:
  FakeWebStateListDelegate web_state_list_delegate_;
  WebStateList web_state_list_;
};

// Tests that WebStateListRemovingIndexes reports the correct count of
// closed tabs (as a tab cannot be closed multiple times, duplicates
// should not be counted).
TEST_F(WebStateListRemovingIndexesTest, Count) {
  EXPECT_EQ(WebStateListRemovingIndexes({}).count(), 0);
  EXPECT_EQ(WebStateListRemovingIndexes({1}).count(), 1);
  EXPECT_EQ(WebStateListRemovingIndexes({1, 1}).count(), 1);
  EXPECT_EQ(WebStateListRemovingIndexes({1, 2}).count(), 2);
  EXPECT_EQ(WebStateListRemovingIndexes({2, 1, 2, 1}).count(), 2);
}

// Tests that WebStateListRemovingIndexes correctly returns the correct
// updated value when asked for index once tabs have been removed.
TEST_F(WebStateListRemovingIndexesTest, IndexAfterRemoval) {
  WebStateListRemovingIndexes removing_indexes({1, 3, 7});
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(0), 0);  // no removal before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(1), WebStateList::kInvalidIndex);
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(2), 1);  // one removals before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(3), WebStateList::kInvalidIndex);
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(4), 2);  // two removals before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(5), 3);  // two removals before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(6), 4);  // two removals before
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(7), WebStateList::kInvalidIndex);
  EXPECT_EQ(removing_indexes.IndexAfterRemoval(8), 5);  // three removals before
}

// Tests that WebStateListRemovingIndexes correctly find a child index after
// removal.
TEST_F(WebStateListRemovingIndexesTest,
       FindIndexOfNextNonRemovedWebStateOpenedBy) {
  // Create a WebStateList with 6 WebStates, 5 of them children of the
  // WebState at index 2 (so the WebState at index 2 has two children
  // before itself and three children after).
  web::WebState* opener = InsertNewWebState(0, WebStateOpener());
  InsertNewWebState(0, WebStateOpener(opener));
  InsertNewWebState(0, WebStateOpener(opener));
  InsertNewWebState(3, WebStateOpener(opener));
  InsertNewWebState(4, WebStateOpener(opener));
  InsertNewWebState(5, WebStateOpener(opener));

  // If no indexes are removed, FindIndexOfNextNonRemovedWebStateOpenedBy()
  // should behave as GetIndexOfNextWebStateOpenedBy().
  WebStateListRemovingIndexes removing_no_children({});
  EXPECT_EQ(removing_no_children.FindIndexOfNextNonRemovedWebStateOpenedBy(
                web_state_list_, opener, 2),
            3);
  EXPECT_EQ(removing_no_children.FindIndexOfNextNonRemovedWebStateOpenedBy(
                web_state_list_, opener, 3),
            4);
  EXPECT_EQ(removing_no_children.FindIndexOfNextNonRemovedWebStateOpenedBy(
                web_state_list_, opener, 4),
            5);
  EXPECT_EQ(removing_no_children.FindIndexOfNextNonRemovedWebStateOpenedBy(
                web_state_list_, opener, 5),
            0);
  EXPECT_EQ(removing_no_children.FindIndexOfNextNonRemovedWebStateOpenedBy(
                web_state_list_, opener, 0),
            1);
  EXPECT_EQ(removing_no_children.FindIndexOfNextNonRemovedWebStateOpenedBy(
                web_state_list_, opener, 1),
            3);
  EXPECT_EQ(removing_no_children.FindIndexOfNextNonRemovedWebStateOpenedBy(
                web_state_list_, web_state_list_.GetWebStateAt(0), 0),
            WebStateList::kInvalidIndex);

  // If some child are removed, FindIndexOfNextNonRemovedWebStateOpenedBy()
  // correctly skips them.
  WebStateListRemovingIndexes removing_some_children({1, 3});
  EXPECT_EQ(removing_some_children.FindIndexOfNextNonRemovedWebStateOpenedBy(
                web_state_list_, opener, 2),
            2);
  EXPECT_EQ(removing_some_children.FindIndexOfNextNonRemovedWebStateOpenedBy(
                web_state_list_, opener, 3),
            2);
  EXPECT_EQ(removing_some_children.FindIndexOfNextNonRemovedWebStateOpenedBy(
                web_state_list_, opener, 4),
            3);
  EXPECT_EQ(removing_some_children.FindIndexOfNextNonRemovedWebStateOpenedBy(
                web_state_list_, opener, 5),
            0);
  EXPECT_EQ(removing_some_children.FindIndexOfNextNonRemovedWebStateOpenedBy(
                web_state_list_, opener, 0),
            2);
  EXPECT_EQ(removing_some_children.FindIndexOfNextNonRemovedWebStateOpenedBy(
                web_state_list_, opener, 1),
            2);
  EXPECT_EQ(removing_some_children.FindIndexOfNextNonRemovedWebStateOpenedBy(
                web_state_list_, web_state_list_.GetWebStateAt(0), 0),
            WebStateList::kInvalidIndex);

  // If some child are removed, FindIndexOfNextNonRemovedWebStateOpenedBy()
  // correctly reports there is no possible index.
  WebStateListRemovingIndexes removing_all_children({0, 1, 3, 4, 5});
  EXPECT_EQ(removing_all_children.FindIndexOfNextNonRemovedWebStateOpenedBy(
                web_state_list_, opener, 2),
            WebStateList::kInvalidIndex);
}
