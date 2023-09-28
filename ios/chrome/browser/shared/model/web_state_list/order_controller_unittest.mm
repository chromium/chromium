// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/web_state_list/order_controller.h"

#import "base/check_op.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller_source_from_web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
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
  FakeOrderControllerSource(int pinned_items_count, std::vector<ItemInfo> items)
      : items_(std::move(items)), pinned_items_count_(pinned_items_count) {
    DCHECK_GE(pinned_items_count_, 0);
    DCHECK_LE(pinned_items_count_, static_cast<int>(items_.size()));
  }

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

 private:
  const std::vector<ItemInfo> items_;
  const int pinned_items_count_;
};

}  // namespace

using OrderControllerTest = PlatformTest;

// Tests that DetermineInsertionIndex respects the pinned/regular group
// when the insertion policy is "automatic".
TEST_F(OrderControllerTest, DetermineInsertionIndex_Automatic) {
  FakeOrderControllerSource source(2, {
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
                       OrderController::ItemGroup::kRegular)));

  // Verify that inserting an item with "automatic" policy put the item
  // at the end of the selected group (pinned).
  EXPECT_EQ(2, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::Automatic(
                       OrderController::ItemGroup::kPinned)));
}

// Tests that DetermineInsertionIndex respects the desired index when
// insertion policy is "forced".
TEST_F(OrderControllerTest, DetermineInsertionIndex_ForceIndex) {
  FakeOrderControllerSource source(2, {
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
                       2, OrderController::ItemGroup::kRegular)));

  EXPECT_EQ(3, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       3, OrderController::ItemGroup::kRegular)));

  EXPECT_EQ(4, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       4, OrderController::ItemGroup::kRegular)));

  EXPECT_EQ(0, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       0, OrderController::ItemGroup::kPinned)));

  EXPECT_EQ(1, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       1, OrderController::ItemGroup::kPinned)));

  EXPECT_EQ(2, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       2, OrderController::ItemGroup::kPinned)));

  // Verify that inserting an item with "forced" policy puts the item at
  // the end of the group if the requested position is not in group.
  EXPECT_EQ(4, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       0, OrderController::ItemGroup::kRegular)));

  EXPECT_EQ(4, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       1, OrderController::ItemGroup::kRegular)));

  EXPECT_EQ(2, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       3, OrderController::ItemGroup::kPinned)));

  EXPECT_EQ(2, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::ForceIndex(
                       4, OrderController::ItemGroup::kPinned)));
}

// Tests that DetermineInsertionIndex correctly position an item with an
// opener relative to its parent, siblings and the pinned/regular group.
TEST_F(OrderControllerTest, DetermineInsertionIndex_WithOpener) {
  FakeOrderControllerSource source(6, {
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
                        7, OrderController::ItemGroup::kRegular)));

  EXPECT_EQ(4, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::WithOpener(
                       1, OrderController::ItemGroup::kPinned)));

  // Verify that inserting an item with an opener will position it after
  // the last sibling if there is at least one sibling, even if the last
  // sibling is "before" the opener.
  EXPECT_EQ(8, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::WithOpener(
                       11, OrderController::ItemGroup::kRegular)));

  EXPECT_EQ(2, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::WithOpener(
                       5, OrderController::ItemGroup::kPinned)));

  // Verify that inserting an item with an opener will position it after
  // the parent if there is no sibling.
  EXPECT_EQ(9, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::WithOpener(
                       8, OrderController::ItemGroup::kRegular)));

  EXPECT_EQ(3, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::WithOpener(
                       2, OrderController::ItemGroup::kPinned)));

  // Verify that inserting an item with an opener will force the index
  // in the correct group if the automatically determined position is
  // outside of the group.
  EXPECT_EQ(12, order_controller.DetermineInsertionIndex(
                    OrderController::InsertionParams::WithOpener(
                        0, OrderController::ItemGroup::kRegular)));

  EXPECT_EQ(6, order_controller.DetermineInsertionIndex(
                   OrderController::InsertionParams::WithOpener(
                       6, OrderController::ItemGroup::kPinned)));
}

// Tests that the selection of the next active element when closing tabs
// respects the opener-opened relationship.
TEST_F(OrderControllerTest, DetermineNewActiveIndex) {
  FakeOrderControllerSource source(0, {
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
  // not changed, but the index is updated if the item is after the closed
  // one.
  EXPECT_EQ(4, order_controller.DetermineNewActiveIndex(4, {5}));
  EXPECT_EQ(5, order_controller.DetermineNewActiveIndex(6, {5}));
  EXPECT_EQ(4, order_controller.DetermineNewActiveIndex(4, {5, 7}));
  EXPECT_EQ(5, order_controller.DetermineNewActiveIndex(6, {5, 7}));
  EXPECT_EQ(5, order_controller.DetermineNewActiveIndex(7, {5, 6}));

  // Verify that if closing an item with siblings, the next sibling is
  // selected, even if it is before the active one.
  EXPECT_EQ(4, order_controller.DetermineNewActiveIndex(4, {4}));
  EXPECT_EQ(4, order_controller.DetermineNewActiveIndex(5, {5}));

  // Verify that if closing an item with opener but no sibling, then the
  // opener is selected.
  EXPECT_EQ(1, order_controller.DetermineNewActiveIndex(6, {6}));

  // Verify that if closing an item with children, the first child is
  // selected, even if it is before the active item.
  EXPECT_EQ(5, order_controller.DetermineNewActiveIndex(1, {1}));
  EXPECT_EQ(2, order_controller.DetermineNewActiveIndex(7, {7}));

  // Verify that if closing an item with no child, the next item is
  // selected, or the previous one if the last item was closed.
  EXPECT_EQ(3, order_controller.DetermineNewActiveIndex(3, {3}));
  EXPECT_EQ(7, order_controller.DetermineNewActiveIndex(8, {8}));

  // Verify that if closing an item and its siblings, the opener is
  // selected.
  EXPECT_EQ(0, order_controller.DetermineNewActiveIndex(4, {4, 5}));

  // Verify that if closing an item with children, the first non closed
  // child is selected.
  EXPECT_EQ(3, order_controller.DetermineNewActiveIndex(0, {0, 4}));

  // Verify that if closing an item with children and all its children,
  // the tab after it is selected.
  EXPECT_EQ(0, order_controller.DetermineNewActiveIndex(0, {0, 4, 5}));

  // Verify that if closing an item, all its children and all the item
  // after it, then the tab before it is selected.
  EXPECT_EQ(
      0, order_controller.DetermineNewActiveIndex(1, {1, 2, 3, 4, 5, 6, 7, 8}));
}
