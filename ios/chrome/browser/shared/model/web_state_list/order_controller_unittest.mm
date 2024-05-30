// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/order_controller.h"

#import "base/check_op.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller_source_from_web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace {

struct ItemInfo {
  int opener_index = -1;
  int opener_navigation_index = -1;
  int last_committed_navigation_index = 0;
};

class FakeOrderControllerSource final : public OrderControllerSource {
 public:
  FakeOrderControllerSource(int pinned_items_count,
                            TabGroupRange tab_group_range,
                            std::vector<ItemInfo> items)
      : collapsed_indexes_(),
        items_(std::move(items)),
        pinned_items_count_(pinned_items_count),
        tab_group_range_(tab_group_range) {
    DCHECK_GE(pinned_items_count_, 0);
    DCHECK_LE(pinned_items_count_, static_cast<int>(items_.size()));
  }

  // Returns a range corresponding to all pinned tabs.
  OrderController::Range PinnedTabsRange() const {
    return OrderController::Range{
        .begin = 0,
        .end = GetPinnedCount(),
    };
  }

  // Returns a range corresponding to all regular tabs.
  OrderController::Range RegularTabsRange() const {
    return OrderController::Range{
        .begin = GetPinnedCount(),
        .end = GetCount(),
    };
  }

  // Collapses the group at `tab_group_range_`.
  void CollapseTabGroup() { collapsed_indexes_ = tab_group_range_.AsSet(); }

  // OrderControllerSource implementation.
  int GetCount() const final { return static_cast<int>(items_.size()); }

  int GetPinnedCount() const final { return pinned_items_count_; }

  int GetOpenerOfItemAt(int index) const final {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, static_cast<int>(items_.size()));
    return items_[index].opener_index;
  }

  bool IsOpenerOfItemAt(int index,
                        int opener_index,
                        bool check_navigation_index) const final {
    DCHECK_GE(index, 0);
    DCHECK_LT(index, static_cast<int>(items_.size()));
    const ItemInfo& item = items_[index];
    if (opener_index != item.opener_index) {
      return false;
    }

    if (!check_navigation_index) {
      return true;
    }

    DCHECK_GE(opener_index, 0);
    DCHECK_LT(opener_index, static_cast<int>(items_.size()));
    return item.opener_navigation_index ==
           items_[opener_index].last_committed_navigation_index;
  }

  TabGroupRange GetGroupRangeOfItemAt(int index) const final {
    if (tab_group_range_.contains(index)) {
      return tab_group_range_;
    } else {
      return TabGroupRange::InvalidRange();
    }
  }

  std::set<int> GetCollapsedGroupIndexes() const final {
    return collapsed_indexes_;
  }

 private:
  std::set<int> collapsed_indexes_;
  const std::vector<ItemInfo> items_;
  const int pinned_items_count_;
  TabGroupRange tab_group_range_;
};

}  // namespace

using OrderControllerTest = PlatformTest;

// Tests that DetermineInsertionIndex respects the pinned/regular group
// when the insertion policy is "automatic".
TEST_F(OrderControllerTest, DetermineInsertionIndex_Automatic) {
  FakeOrderControllerSource source(2, TabGroupRange::InvalidRange(),
                                   {
                                       // Pinned items
                                       ItemInfo{},
                                       ItemInfo{},

                                       // Regular items
                                       ItemInfo{},
                                       ItemInfo{},
                                   });
  OrderController order_controller(source);

  // Verify that inserting an item with "automatic" policy put the item
  // at the end of the selected group (regular).
  EXPECT_EQ(4, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::Automatic(
                       source.RegularTabsRange())));

  // Verify that inserting an item with "automatic" policy put the item
  // at the end of the selected group (pinned).
  EXPECT_EQ(2, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::Automatic(
                       source.PinnedTabsRange())));
}

// Tests that DetermineInsertionIndex respects the desired index when
// insertion policy is "forced".
TEST_F(OrderControllerTest, DetermineInsertionIndex_ForceIndex) {
  FakeOrderControllerSource source(2, TabGroupRange::InvalidRange(),
                                   {
                                       // Pinned items
                                       ItemInfo{},
                                       ItemInfo{},

                                       // Regular items
                                       ItemInfo{},
                                       ItemInfo{},
                                   });
  OrderController order_controller(source);

  // Verify that inserting an item with "forced" policy puts the item at
  // the requested position.
  EXPECT_EQ(2, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       2, source.RegularTabsRange())));

  EXPECT_EQ(3, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       3, source.RegularTabsRange())));

  EXPECT_EQ(4, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       4, source.RegularTabsRange())));

  EXPECT_EQ(0, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       0, source.PinnedTabsRange())));

  EXPECT_EQ(1, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       1, source.PinnedTabsRange())));

  EXPECT_EQ(2, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       2, source.PinnedTabsRange())));

  // Verify that inserting an item with "forced" policy puts the item at
  // the end of the group if the requested position is not in group.
  EXPECT_EQ(4, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       0, source.RegularTabsRange())));

  EXPECT_EQ(4, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       1, source.RegularTabsRange())));

  EXPECT_EQ(2, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       3, source.PinnedTabsRange())));

  EXPECT_EQ(2, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       4, source.PinnedTabsRange())));
}

// Tests that DetermineInsertionIndex correctly position an item with an
// opener relative to its parent, siblings and the pinned/regular group.
TEST_F(OrderControllerTest, DetermineInsertionIndex_WithOpener) {
  FakeOrderControllerSource source(6, TabGroupRange::InvalidRange(),
                                   {
                                       // Pinned items
                                       ItemInfo{},
                                       ItemInfo{
                                           .opener_index = 5,
                                           .opener_navigation_index = 0,
                                       },
                                       ItemInfo{
                                           .opener_index = 1,
                                           .opener_navigation_index = 0,
                                       },
                                       ItemInfo{
                                           .opener_index = 1,
                                           .opener_navigation_index = 0,
                                       },
                                       ItemInfo{
                                           .opener_index = 1,
                                           .opener_navigation_index = 1,
                                       },
                                       ItemInfo{},

                                       // Regular items
                                       ItemInfo{},
                                       ItemInfo{
                                           .opener_index = 11,
                                           .opener_navigation_index = 0,
                                       },
                                       ItemInfo{
                                           .opener_index = 7,
                                           .opener_navigation_index = 0,
                                       },
                                       ItemInfo{
                                           .opener_index = 7,
                                           .opener_navigation_index = 0,
                                       },
                                       ItemInfo{
                                           .opener_index = 7,
                                           .opener_navigation_index = 1,
                                       },
                                       ItemInfo{},
                                   });
  OrderController order_controller(source);

  // Verify that inserting an item with an opener will position it after
  // the last sibling if there is at least one sibling.
  EXPECT_EQ(10, order_controller.DetermineInsertionIndex(
                    OrderController::InsertionParams::WithOpener(
                        7, source.RegularTabsRange())));

  EXPECT_EQ(4, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::WithOpener(
                       1, source.PinnedTabsRange())));

  // Verify that inserting an item with an opener will position it after
  // the last sibling if there is at least one sibling, even if the last
  // sibling is "before" the opener.
  EXPECT_EQ(8, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::WithOpener(
                       11, source.RegularTabsRange())));

  EXPECT_EQ(2, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::WithOpener(
                       5, source.PinnedTabsRange())));

  // Verify that inserting an item with an opener will position it after
  // the parent if there is no sibling.
  EXPECT_EQ(9, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::WithOpener(
                       8, source.RegularTabsRange())));

  EXPECT_EQ(3, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::WithOpener(
                       2, source.PinnedTabsRange())));

  // Verify that inserting an item with an opener will force the index
  // in the correct group if the automatically determined position is
  // outside of the group.
  EXPECT_EQ(12, order_controller.DetermineInsertionIndex(
                    OrderController::InsertionParams::WithOpener(
                        0, source.RegularTabsRange())));

  EXPECT_EQ(6, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::WithOpener(
                       6, source.PinnedTabsRange())));
}

// Tests that the selection of the next active element when closing tabs
// respects the opener-opened relationship.
TEST_F(OrderControllerTest, DetermineNewActiveIndex) {
  FakeOrderControllerSource source(0, TabGroupRange::InvalidRange(),
                                   {
                                       ItemInfo{},
                                       ItemInfo{},
                                       ItemInfo{.opener_index = 7},
                                       ItemInfo{},
                                       ItemInfo{.opener_index = 0},
                                       ItemInfo{.opener_index = 0},
                                       ItemInfo{.opener_index = 1},
                                       ItemInfo{},
                                       ItemInfo{},
                                   });
  OrderController order_controller(source);

  // Verify that if closing all the items, no item is selected.
  EXPECT_EQ(
      WebStateList::kInvalidIndex,
      order_controller.DetermineNewActiveIndex(0, {0, 1, 2, 3, 4, 5, 6, 7, 8}));

  // Verify that if there is no active item, no active item will be activated.
  EXPECT_EQ(WebStateList::kInvalidIndex,
            order_controller.DetermineNewActiveIndex(
                WebStateList::kInvalidIndex, {}));

  // Verify that if closing an item that is not active, the active item is
  // not changed. The returned index is the index before closing the other
  // items, so it should not change.
  EXPECT_EQ(4, order_controller.DetermineNewActiveIndex(4, {5}));
  EXPECT_EQ(6, order_controller.DetermineNewActiveIndex(6, {5}));
  EXPECT_EQ(4, order_controller.DetermineNewActiveIndex(4, {5, 7}));
  EXPECT_EQ(6, order_controller.DetermineNewActiveIndex(6, {5, 7}));
  EXPECT_EQ(7, order_controller.DetermineNewActiveIndex(7, {5, 6}));

  // Verify that if closing an item with siblings, the next sibling is
  // selected, even if it is before the active one.
  EXPECT_EQ(5, order_controller.DetermineNewActiveIndex(4, {4}));
  EXPECT_EQ(4, order_controller.DetermineNewActiveIndex(5, {5}));

  // Verify that if closing an item with opener but no sibling, then the
  // opener is selected.
  EXPECT_EQ(1, order_controller.DetermineNewActiveIndex(6, {6}));

  // Verify that if closing an item with children, the first child is
  // selected, even if it is before the active item.
  EXPECT_EQ(6, order_controller.DetermineNewActiveIndex(1, {1}));
  EXPECT_EQ(2, order_controller.DetermineNewActiveIndex(7, {7}));

  // Veriffy that closing an item with multiple children, the first
  // one is selected.
  EXPECT_EQ(4, order_controller.DetermineNewActiveIndex(0, {0}));

  // Verify that if closing an item with no child, the next item is
  // selected, or the previous one if the last item was closed.
  EXPECT_EQ(4, order_controller.DetermineNewActiveIndex(3, {3}));
  EXPECT_EQ(7, order_controller.DetermineNewActiveIndex(8, {8}));

  // Verify that if closing an item and its siblings, the opener is
  // selected.
  EXPECT_EQ(0, order_controller.DetermineNewActiveIndex(4, {4, 5}));

  // Verify that if closing an item with children, the first non closed
  // child is selected.
  EXPECT_EQ(5, order_controller.DetermineNewActiveIndex(0, {0, 4}));

  // Verify that if closing an item with children and all its children,
  // the tab after it is selected.
  EXPECT_EQ(1, order_controller.DetermineNewActiveIndex(0, {0, 4, 5}));

  // Verify that if closing an item, all its children and all the item
  // after it, then the tab before it is selected.
  EXPECT_EQ(
      0, order_controller.DetermineNewActiveIndex(1, {1, 2, 3, 4, 5, 6, 7, 8}));
}

// Tests that when closing tabs from a group, the selection of the next active
// tab respects the opener-opened order.
TEST_F(OrderControllerTest, DetermineNewActiveIndex_TabGroup) {
  FakeOrderControllerSource source(0, TabGroupRange(0, 3),
                                   {
                                       // Grouped items
                                       ItemInfo{},
                                       ItemInfo{},
                                       ItemInfo{},
                                       // Regular items
                                       ItemInfo{},
                                       ItemInfo{},
                                       ItemInfo{},
                                   });
  OrderController order_controller(source);

  // Closing a non-active tab correctly keeps the active tab index.
  EXPECT_EQ(0, order_controller.DetermineNewActiveIndex(0, {1}));
  EXPECT_EQ(3, order_controller.DetermineNewActiveIndex(3, {4, 5}));
  EXPECT_EQ(2, order_controller.DetermineNewActiveIndex(2, {1, 3, 4}));

  // Closing an active tab within a group selects the next available tab in the
  // group.
  EXPECT_EQ(2, order_controller.DetermineNewActiveIndex(1, {1}));
  EXPECT_EQ(0, order_controller.DetermineNewActiveIndex(0, {1}));

  // Closing the active last tab in a group selects the closest preceding tab in
  // the group.
  EXPECT_EQ(1, order_controller.DetermineNewActiveIndex(2, {2}));
  EXPECT_EQ(0, order_controller.DetermineNewActiveIndex(2, {1, 2}));

  // Closing all tabs in a group selects a tab outside the group.
  EXPECT_EQ(3, order_controller.DetermineNewActiveIndex(1, {0, 1, 2}));

  // Closing an active tab in a group and tabs outside the group selects the
  // next available tab outside the group.
  EXPECT_EQ(3, order_controller.DetermineNewActiveIndex(2, {2, 4}));
}

// Tests that when closing a tab, the next active tab is not in a collapsed
// group.
TEST_F(OrderControllerTest, DetermineNewActiveIndex_CollapsedTabs) {
  FakeOrderControllerSource source(0, TabGroupRange(1, 1),
                                   {
                                       // Regular item
                                       ItemInfo{},
                                       // Grouped item
                                       ItemInfo{},
                                       // Regular item
                                       ItemInfo{},
                                   });
  OrderController order_controller(source);

  // Closing the active tab with no collapsed tabs selects the closest preceding
  // tab.
  EXPECT_EQ(1, order_controller.DetermineNewActiveIndex(2, {2}));

  // Closing the active tab with collapsed tabs selects the closest non
  // collasped preceding tab.
  source.CollapseTabGroup();
  EXPECT_EQ(0, order_controller.DetermineNewActiveIndex(2, {2}));
}
