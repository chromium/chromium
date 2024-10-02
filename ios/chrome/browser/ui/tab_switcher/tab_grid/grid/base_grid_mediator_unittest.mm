// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/base_grid_mediator.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/containers/contains.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/metrics/histogram_tester.h"
#import "base/time/time.h"
#import "components/commerce/core/commerce_feature_list.h"
#import "components/saved_tab_groups/public/saved_tab_group.h"
#import "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#import "components/tab_groups/tab_group_id.h"
#import "components/tab_groups/tab_group_visual_data.h"
#import "ios/chrome/browser/commerce/model/shopping_persisted_data_tab_helper.h"
#import "ios/chrome/browser/drag_and_drop/model/drag_item_util.h"
#import "ios/chrome/browser/main/model/browser_web_state_list_delegate.h"
#import "ios/chrome/browser/saved_tab_groups/model/tab_group_sync_service_factory.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/test/test_browser.h"
#import "ios/chrome/browser/shared/model/profile/test/test_profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group.h"
#import "ios/chrome/browser/shared/model/web_state_list/test/web_state_list_builder_from_description.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_opener.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_collection_drag_drop_metrics.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item_identifier.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_mediator_test.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/incognito/incognito_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/regular/regular_grid_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_grid_mode_holder.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/test/fake_tab_grid_toolbars_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_group_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/browser/ui/tab_switcher/test/fake_tab_collection_consumer.h"
#import "ios/chrome/browser/ui/tab_switcher/web_state_tab_switcher_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_id.h"
#import "testing/gtest_mac.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

using tab_groups::TabGroupId;
using testing::_;

namespace {

// URL to be used for drag and drop tests.
const char kDraggedUrl[] = "https://dragged_url.com";

// BaseGridMediatorTest is parameterized on this enum to test all children
// mediator.
enum GridMediatorType { TEST_REGULAR_MEDIATOR, TEST_INCOGNITO_MEDIATOR };

const char kHasPriceDropUserAction[] = "Commerce.TabGridSwitched.HasPriceDrop";
const char kHasNoPriceDropUserAction[] = "Commerce.TabGridSwitched.NoPriceDrop";

// Returns a test `SavedTabGroup`.
tab_groups::SavedTabGroup TestSavedGroup() {
  tab_groups::SavedTabGroup saved_group(
      u"Test title", tab_groups::TabGroupColorId::kBlue, {}, std::nullopt,
      base::Uuid::GenerateRandomV4(), tab_groups::TabGroupId::GenerateNew());
  return saved_group;
}

}  // namespace

class BaseGridMediatorTest
    : public GridMediatorTestClass,
      public ::testing::WithParamInterface<GridMediatorType> {
 public:
  BaseGridMediatorTest() {}
  ~BaseGridMediatorTest() override {}

  void SetUp() override {
    GridMediatorTestClass::SetUp();
    mode_holder_ = [[TabGridModeHolder alloc] init];
    if (GetParam() == TEST_INCOGNITO_MEDIATOR) {
      mediator_ =
          [[IncognitoGridMediator alloc] initWithModeHolder:mode_holder_];
    } else {
      mediator_ = [[RegularGridMediator alloc] initWithModeHolder:mode_holder_];
    }
    mediator_.consumer = consumer_;
    mediator_.browser = browser_.get();
    mediator_.toolbarsMutator = fake_toolbars_mediator_;
    [mediator_ currentlySelectedGrid:YES];
  }

  void TearDown() override {
    // Forces the IncognitoGridMediator to removes its Observer from
    // WebStateList before the Browser is destroyed.
    mediator_.browser = nullptr;
    mediator_ = nil;
    GridMediatorTestClass::TearDown();
  }

  // Checks that the drag item origin metric is logged in UMA.
  void ExpectThatDragItemOriginMetricLogged(DragItemOrigin origin,
                                            int count = 1) {
    histogram_tester_.ExpectUniqueSample(kUmaGridViewDragOrigin, origin, count);
  }

 protected:
  BaseGridMediator* mediator_;
  base::HistogramTester histogram_tester_;
  TabGridModeHolder* mode_holder_;
};

// Variation on BaseGridMediatorTest which enable PriceDropIndicatorsFlag.
class BaseGridMediatorWithPriceDropIndicatorsTest
    : public BaseGridMediatorTest {
 protected:

  void SetFakePriceDrop(web::WebState* web_state) {
    ShoppingPersistedDataTabHelper::PriceDrop price_drop;
    price_drop.current_price = @"$5";
    price_drop.previous_price = @"$10";
    price_drop.url = web_state->GetLastCommittedURL();
    price_drop.timestamp = base::Time::Now();
    ShoppingPersistedDataTabHelper::FromWebState(web_state)
        ->SetPriceDropForTesting(
            std::make_unique<ShoppingPersistedDataTabHelper::PriceDrop>(
                std::move(price_drop)));
  }
};

#pragma mark - Consumer tests

// Tests drag and dropping an item that has been closed.
TEST_P(BaseGridMediatorTest, DragAndDropClosedItem) {
  std::unique_ptr<web::FakeWebState> web_state =
      CreateFakeWebStateWithURL(GURL("https://google.com"));
  web::WebState* web_state_ptr = web_state.get();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state), WebStateList::InsertionParams::AtIndex(1));

  mode_holder_.mode = TabGridMode::kSelection;
  [mediator_
      addToSelectionItemID:[GridItemIdentifier tabIdentifier:web_state_ptr]];

  browser_->GetWebStateList()->CloseWebStateAt(1,
                                               WebStateList::CLOSE_USER_ACTION);
  EXPECT_EQ(0UL, [mediator_ allSelectedDragItems].count);
}

// Tests that the consumer is populated after the tab model is set on the
// mediator.
TEST_P(BaseGridMediatorTest, ConsumerPopulateItems) {
  EXPECT_EQ(3UL, consumer_.items.size());
  EXPECT_EQ(original_selected_identifier_,
            consumer_.selectedItem.tabSwitcherItem.identifier);
}

// Tests that the consumer is notified when a web state is inserted.
TEST_P(BaseGridMediatorTest, ConsumerInsertItem) {
  ASSERT_EQ(3UL, consumer_.items.size());
  std::unique_ptr<web::FakeWebState> web_state =
      CreateFakeWebStateWithURL(GURL());
  web::WebStateID item_identifier = web_state.get()->GetUniqueIdentifier();
  browser_->GetWebStateList()->InsertWebState(
      std::move(web_state), WebStateList::InsertionParams::AtIndex(1));
  EXPECT_EQ(4UL, consumer_.items.size());
  // The same ID should be selected after the insertion, since the new web state
  // wasn't selected.
  EXPECT_EQ(original_selected_identifier_,
            consumer_.selectedItem.tabSwitcherItem.identifier);
  EXPECT_EQ(item_identifier, consumer_.items[1]);
  EXPECT_FALSE(base::Contains(original_identifiers_, item_identifier));
}

// Tests that the consumer is notified when a web state is removed.
// The selected web state at index 1 is removed. The web state originally
// at index 2 should be the new selected item.
TEST_P(BaseGridMediatorTest, ConsumerRemoveItem) {
  browser_->GetWebStateList()->CloseWebStateAt(1, WebStateList::CLOSE_NO_FLAGS);
  EXPECT_EQ(2UL, consumer_.items.size());
  // Expect that a different web state is selected now.
  EXPECT_NE(original_selected_identifier_,
            consumer_.selectedItem.tabSwitcherItem.identifier);
}

// Tests that the consumer is notified when the active web state is changed.
TEST_P(BaseGridMediatorTest, ConsumerUpdateSelectedItem) {
  EXPECT_EQ(original_selected_identifier_,
            consumer_.selectedItem.tabSwitcherItem.identifier);
  browser_->GetWebStateList()->ActivateWebStateAt(2);
  EXPECT_EQ(
      browser_->GetWebStateList()->GetWebStateAt(2)->GetUniqueIdentifier(),
      consumer_.selectedItem.tabSwitcherItem.identifier);
}

// Tests that the consumer is notified when a web state is replaced.
// The selected item is replaced, so the new selected item id should be the
// id of the new item.
TEST_P(BaseGridMediatorTest, ConsumerReplaceItem) {
  std::unique_ptr<web::FakeWebState> new_web_state =
      CreateFakeWebStateWithURL(GURL());
  web::WebStateID new_item_identifier = new_web_state->GetUniqueIdentifier();
  @autoreleasepool {
    browser_->GetWebStateList()->ReplaceWebStateAt(1, std::move(new_web_state));
  }
  EXPECT_EQ(3UL, consumer_.items.size());
  EXPECT_EQ(new_item_identifier,
            consumer_.selectedItem.tabSwitcherItem.identifier);
  EXPECT_EQ(new_item_identifier, consumer_.items[1]);
  EXPECT_FALSE(base::Contains(original_identifiers_, new_item_identifier));
}

// Tests that the consumer is notified when a web state is moved.
TEST_P(BaseGridMediatorTest, ConsumerMoveItem) {
  web::WebStateID item1 = consumer_.items[1];
  web::WebStateID item2 = consumer_.items[2];
  browser_->GetWebStateList()->MoveWebStateAt(1, 2);
  EXPECT_EQ(item1, consumer_.items[2]);
  EXPECT_EQ(item2, consumer_.items[1]);
}

#pragma mark - Command tests

// Tests that the active index is updated when `-selectItemWithID:` is called.
// Tests that the consumer's selected index is updated.
TEST_P(BaseGridMediatorTest, SelectItemCommand) {
  // Previous selected index is 1.
  web::WebStateID identifier =
      browser_->GetWebStateList()->GetWebStateAt(2)->GetUniqueIdentifier();
  [mediator_ selectItemWithID:identifier pinned:NO isFirstActionOnTabGrid:NO];
  EXPECT_EQ(2, browser_->GetWebStateList()->active_index());
  EXPECT_EQ(identifier, consumer_.selectedItem.tabSwitcherItem.identifier);
}

// Tests that the active index is updated when `-selectItemWithID:` is called.
// Tests that the consumer's selected index is updated with pinned state.
TEST_P(BaseGridMediatorTest, SelectPinnedItemCommand) {
  if (GetParam() == TEST_INCOGNITO_MEDIATOR || !IsPinnedTabsEnabled()) {
    // Test only available in non-incognito when pinned tabs are enabled.
    return;
  }
  WebStateList* web_state_list = browser_->GetWebStateList();
  web::WebStateID identifier_0 =
      web_state_list->GetWebStateAt(0)->GetUniqueIdentifier();
  web::WebStateID identifier_1 =
      web_state_list->GetWebStateAt(1)->GetUniqueIdentifier();
  web::WebStateID identifier_2 =
      web_state_list->GetWebStateAt(2)->GetUniqueIdentifier();
  [mediator_ setPinState:YES forItemWithID:identifier_0];
  ASSERT_EQ(1, browser_->GetWebStateList()->active_index());
  ASSERT_EQ(identifier_1, consumer_.selectedItem.tabSwitcherItem.identifier);

  [mediator_ selectItemWithID:identifier_0
                       pinned:YES
       isFirstActionOnTabGrid:NO];

  EXPECT_EQ(0, browser_->GetWebStateList()->active_index());
  EXPECT_EQ(identifier_0, consumer_.selectedItem.tabSwitcherItem.identifier);

  [mediator_ selectItemWithID:identifier_2 pinned:NO isFirstActionOnTabGrid:NO];

  EXPECT_EQ(2, browser_->GetWebStateList()->active_index());
  EXPECT_EQ(identifier_2, consumer_.selectedItem.tabSwitcherItem.identifier);

  // Selecting the pinned one with pinned = NO fails.
  [mediator_ selectItemWithID:identifier_0 pinned:NO isFirstActionOnTabGrid:NO];

  EXPECT_EQ(2, browser_->GetWebStateList()->active_index());
  EXPECT_EQ(identifier_2, consumer_.selectedItem.tabSwitcherItem.identifier);
}

// Tests the pinned tab command.
TEST_P(BaseGridMediatorTest, PinItemCommand) {
  if (GetParam() == TEST_INCOGNITO_MEDIATOR || !IsPinnedTabsEnabled()) {
    // Test only available in non-incognito when pinned tabs are enabled.
    return;
  }
  WebStateList* web_state_list = browser_->GetWebStateList();
  // At first the second web state is active.
  ASSERT_EQ(1, web_state_list->active_index());
  ASSERT_EQ(0, web_state_list->pinned_tabs_count());

  web::WebStateID selected_identifier =
      web_state_list->GetWebStateAt(1)->GetUniqueIdentifier();
  web::WebStateID identifier =
      web_state_list->GetWebStateAt(2)->GetUniqueIdentifier();

  [mediator_ setPinState:YES forItemWithID:identifier];

  // The pinned web state moved to the first position, moving the others.
  EXPECT_EQ(1, web_state_list->pinned_tabs_count());
  EXPECT_EQ(2, web_state_list->active_index());
  EXPECT_EQ(selected_identifier,
            consumer_.selectedItem.tabSwitcherItem.identifier);

  [mediator_ setPinState:NO forItemWithID:identifier];

  // The pinned web state moves back to the end of the WebStateList.
  EXPECT_EQ(0, web_state_list->pinned_tabs_count());
  EXPECT_EQ(1, web_state_list->active_index());
  EXPECT_EQ(selected_identifier,
            consumer_.selectedItem.tabSwitcherItem.identifier);
}

// Tests that the WebStateList count is decremented when
// `-closeItemWithID:` is called.
// Tests that the consumer's item count is also decremented.
TEST_P(BaseGridMediatorTest, CloseItemCommand) {
  // Previously there were 3 items.
  web::WebStateID identifier =
      browser_->GetWebStateList()->GetWebStateAt(0)->GetUniqueIdentifier();
  [mediator_ closeItemWithID:identifier];
  EXPECT_EQ(2, browser_->GetWebStateList()->count());
  EXPECT_EQ(2UL, consumer_.items.size());
}

// Tests that when `-addNewItem` is called, the WebStateList count is
// incremented, the `active_index` is at the end of WebStateList, the new
// web state has no opener, and the URL is the New Tab Page.
// Tests that the consumer has added an item with the correct identifier.
TEST_P(BaseGridMediatorTest, AddNewItemAtEndCommand) {
  // Previously there were 3 items and the selected index was 1.
  [mediator_ addNewItem];
  EXPECT_EQ(4, browser_->GetWebStateList()->count());
  EXPECT_EQ(3, browser_->GetWebStateList()->active_index());
  web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(3);
  ASSERT_TRUE(web_state);
  EXPECT_EQ(web_state->GetBrowserState(), profile_.get());
  EXPECT_FALSE(web_state->HasOpener());
  // The URL of pending item (i.e. kChromeUINewTabURL) will not be returned
  // here because WebState doesn't load the URL until it's visible and
  // NavigationManager::GetVisibleURL requires WebState::IsLoading to be true
  // to return pending item's URL.
  EXPECT_EQ("", web_state->GetVisibleURL().spec());
  web::WebStateID identifier = web_state->GetUniqueIdentifier();
  EXPECT_FALSE(base::Contains(original_identifiers_, identifier));
  // Consumer checks.
  EXPECT_EQ(4UL, consumer_.items.size());
  EXPECT_EQ(identifier, consumer_.selectedItem.tabSwitcherItem.identifier);
  EXPECT_EQ(identifier, consumer_.items[3]);
}

// Tests that `-addNewItem` is a no-op if the mediator's browser
// is nullptr.
TEST_P(BaseGridMediatorTest, AddNewItemWithNoBrowserCommand) {
  mediator_.browser = nullptr;
  ASSERT_EQ(3, browser_->GetWebStateList()->count());
  ASSERT_EQ(1, browser_->GetWebStateList()->active_index());
  [mediator_ addNewItem];
  EXPECT_EQ(3, browser_->GetWebStateList()->count());
  EXPECT_EQ(1, browser_->GetWebStateList()->active_index());
}

// Tests that when `-searchItemsWithText:` is called, there is no change in the
// items in WebStateList and the correct items are populated by the consumer.
TEST_P(BaseGridMediatorTest, SearchItemsWithTextCommand) {
  // Capture ordered original IDs.
  std::vector<web::WebStateID> pre_search_ids;
  for (int i = 0; i < 3; i++) {
    web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(i);
    pre_search_ids.push_back(web_state->GetUniqueIdentifier());
  }
  web::WebStateID expected_result_identifier =
      browser_->GetWebStateList()->GetWebStateAt(2)->GetUniqueIdentifier();

  [mediator_ searchItemsWithText:@"hello"];

  // Only one result should be found.
  EXPECT_TRUE(WaitForConsumerUpdates(1UL));
  EXPECT_EQ(expected_result_identifier, consumer_.items[0]);

  // Web states count should not change.
  EXPECT_EQ(3, browser_->GetWebStateList()->count());
  // Active index should not change.
  EXPECT_EQ(1, browser_->GetWebStateList()->active_index());
  // The order of the items should be the same.
  for (int i = 0; i < 3; i++) {
    web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(i);
    ASSERT_TRUE(web_state);
    web::WebStateID identifier = web_state->GetUniqueIdentifier();
    EXPECT_EQ(identifier, pre_search_ids[i]);
  }
}

// Tests that when `-resetToAllItems:` is called, the consumer gets all the
// items from items in WebStateList and correct item selected.
TEST_P(BaseGridMediatorTest, resetToAllItems) {
  ASSERT_EQ(3, browser_->GetWebStateList()->count());
  ASSERT_EQ(3UL, consumer_.items.size());

  [mediator_ searchItemsWithText:@"hello"];
  // Only 1 result is in the consumer after the search is done.
  ASSERT_TRUE(WaitForConsumerUpdates(1UL));

  [mediator_ resetToAllItems];
  // consumer should revert back to have the items from the webstate list.
  ASSERT_TRUE(WaitForConsumerUpdates(3UL));
  // Active index should not change.
  EXPECT_EQ(original_selected_identifier_,
            consumer_.selectedItem.tabSwitcherItem.identifier);

  // The order of the items on the consumer be the exact same order as the in
  // WebStateList.
  for (int i = 0; i < 3; i++) {
    web::WebState* web_state = browser_->GetWebStateList()->GetWebStateAt(i);
    ASSERT_TRUE(web_state);
    web::WebStateID identifier = web_state->GetUniqueIdentifier();
    EXPECT_EQ(identifier, consumer_.items[i]);
  }
}

TEST_P(BaseGridMediatorWithPriceDropIndicatorsTest,
       TestSelectItemWithNoPriceDrop) {
  web::WebState* web_state_to_select =
      browser_->GetWebStateList()->GetWebStateAt(2);
  // No need to set a null price drop - it will be null by default.
  [mediator_ selectItemWithID:web_state_to_select->GetUniqueIdentifier()
                       pinned:NO
       isFirstActionOnTabGrid:NO];
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kHasNoPriceDropUserAction));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(kHasPriceDropUserAction));
}

TEST_P(BaseGridMediatorWithPriceDropIndicatorsTest,
       TestSelectItemWithPriceDrop) {
  web::WebState* web_state_to_select =
      browser_->GetWebStateList()->GetWebStateAt(2);
  // Add a fake price drop.
  SetFakePriceDrop(web_state_to_select);
  [mediator_ selectItemWithID:web_state_to_select->GetUniqueIdentifier()
                       pinned:NO
       isFirstActionOnTabGrid:NO];
  EXPECT_EQ(1, user_action_tester_.GetActionCount(kHasPriceDropUserAction));
  EXPECT_EQ(0, user_action_tester_.GetActionCount(kHasNoPriceDropUserAction));
}

// Ensures that when there is web states in normal mode, the toolbar
// configuration is correct.
TEST_P(BaseGridMediatorTest, TestToolbarsNormalModeWithWebstates) {
  EXPECT_EQ(3UL, consumer_.items.size());
  // Force the toolbar configuration by setting the view as currently selected.
  [mediator_ currentlySelectedGrid:YES];
  EXPECT_TRUE(fake_toolbars_mediator_.configuration.closeAllButton);
  EXPECT_TRUE(fake_toolbars_mediator_.configuration.doneButton);
  EXPECT_TRUE(fake_toolbars_mediator_.configuration.newTabButton);
  EXPECT_TRUE(fake_toolbars_mediator_.configuration.searchButton);
  EXPECT_TRUE(fake_toolbars_mediator_.configuration.selectTabsButton);

  EXPECT_FALSE(fake_toolbars_mediator_.configuration.undoButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.deselectAllButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.selectAllButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.addToButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.closeSelectedTabsButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.shareButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.cancelSearchButton);
}

// Ensures that selection button are correctly enabled when pushing select tab
// button.
TEST_P(BaseGridMediatorTest, TestToolbarsSelectionModeWithoutSelection) {
  EXPECT_EQ(3UL, consumer_.items.size());
  [mediator_ selectTabsButtonTapped:nil];

  EXPECT_TRUE(fake_toolbars_mediator_.configuration.selectAllButton);
  EXPECT_TRUE(fake_toolbars_mediator_.configuration.doneButton);
  EXPECT_EQ(0u, fake_toolbars_mediator_.configuration.selectedItemsCount);

  EXPECT_FALSE(fake_toolbars_mediator_.configuration.closeAllButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.newTabButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.searchButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.selectTabsButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.undoButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.deselectAllButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.addToButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.closeSelectedTabsButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.shareButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.cancelSearchButton);
}

// Ensures that selection button are correctly enabled when pushing select tab
// button and the user selected one tab.
TEST_P(BaseGridMediatorTest, TestToolbarsSelectionModeWithSelection) {
  EXPECT_EQ(3UL, consumer_.items.size());
  [mediator_ selectTabsButtonTapped:nil];

  // Simulate a user who tapped on a tab.
  [mediator_ userTappedOnItemID:[GridItemIdentifier
                                    tabIdentifier:browser_->GetWebStateList()
                                                      ->GetWebStateAt(1)]];

  EXPECT_TRUE(fake_toolbars_mediator_.configuration.selectAllButton);
  EXPECT_TRUE(fake_toolbars_mediator_.configuration.doneButton);
  EXPECT_EQ(1u, fake_toolbars_mediator_.configuration.selectedItemsCount);
  EXPECT_TRUE(fake_toolbars_mediator_.configuration.closeSelectedTabsButton);
  EXPECT_TRUE(fake_toolbars_mediator_.configuration.shareButton);
  EXPECT_TRUE(fake_toolbars_mediator_.configuration.addToButton);

  EXPECT_FALSE(fake_toolbars_mediator_.configuration.closeAllButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.newTabButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.searchButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.selectTabsButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.undoButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.deselectAllButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.cancelSearchButton);
}

// Tests that no updates to the toolbars happen when the mediator is not
// selected.
TEST_P(BaseGridMediatorTest, NoToolbarUpdateNotSelected) {
  EXPECT_EQ(3UL, consumer_.items.size());
  [mediator_ selectTabsButtonTapped:nil];

  EXPECT_TRUE(fake_toolbars_mediator_.configuration.selectAllButton);
  EXPECT_TRUE(fake_toolbars_mediator_.configuration.doneButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.closeSelectedTabsButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.shareButton);

  [mediator_ currentlySelectedGrid:NO];

  // Simulate a user who tapped on a tab.
  [mediator_ userTappedOnItemID:[GridItemIdentifier
                                    tabIdentifier:browser_->GetWebStateList()
                                                      ->GetWebStateAt(1)]];

  // No update on the configuration as the mediator is no longer selected.
  EXPECT_TRUE(fake_toolbars_mediator_.configuration.selectAllButton);
  EXPECT_TRUE(fake_toolbars_mediator_.configuration.doneButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.closeSelectedTabsButton);
  EXPECT_FALSE(fake_toolbars_mediator_.configuration.shareButton);
}

// Tests selecting a NTP with no existing groups. The option to add to a group
// should be presented, the others would be disabled.
TEST_P(BaseGridMediatorTest, NTPSelectedWithoutGroup) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kTabGroupsIPad, kModernTabStrip}, {});

  ASSERT_EQ(3UL, consumer_.items.size());
  browser_->GetWebStateList()->InsertWebState(
      CreateFakeWebStateWithURL(GURL("about:newtab")));
  ASSERT_EQ(4UL, consumer_.items.size());

  [mediator_ selectTabsButtonTapped:nil];

  // Simulate a user who tapped on the NTP.
  [mediator_ userTappedOnItemID:[GridItemIdentifier
                                    tabIdentifier:browser_->GetWebStateList()
                                                      ->GetWebStateAt(3)]];

  TabGridToolbarsConfiguration* configuration =
      fake_toolbars_mediator_.configuration;
  EXPECT_TRUE(configuration.selectAllButton);
  EXPECT_TRUE(configuration.doneButton);
  EXPECT_EQ(1u, configuration.selectedItemsCount);
  EXPECT_TRUE(configuration.closeSelectedTabsButton);
  EXPECT_TRUE(configuration.addToButton);

  EXPECT_FALSE(configuration.shareButton);
  EXPECT_FALSE(configuration.closeAllButton);
  EXPECT_FALSE(configuration.newTabButton);
  EXPECT_FALSE(configuration.searchButton);
  EXPECT_FALSE(configuration.selectTabsButton);
  EXPECT_FALSE(configuration.undoButton);
  EXPECT_FALSE(configuration.deselectAllButton);
  EXPECT_FALSE(configuration.cancelSearchButton);

  ASSERT_EQ(3u, configuration.addToButtonMenu.children.count);
  // Add to bookmark/reading list are disabled as it is a NTP.
  UIMenuElement* addToBookmark = configuration.addToButtonMenu.children[1];
  EXPECT_EQ(UIMenuElementAttributesDisabled,
            base::apple::ObjCCast<UIAction>(addToBookmark).attributes);
  UIMenuElement* addToReadingList = configuration.addToButtonMenu.children[1];
  EXPECT_EQ(UIMenuElementAttributesDisabled,
            base::apple::ObjCCast<UIAction>(addToReadingList).attributes);

  // Even if there is a single option, it is displayed as an inlined menu.
  UIMenuElement* addToGroupElement = configuration.addToButtonMenu.children[0];
  ASSERT_TRUE([addToGroupElement isKindOfClass:UIMenu.class]);

  UIMenu* addToGroupMenu = base::apple::ObjCCast<UIMenu>(addToGroupElement);
  ASSERT_EQ(1u, addToGroupMenu.children.count);

  UIMenuElement* addToGroup = addToGroupMenu.children[0];
  EXPECT_TRUE([addToGroup isKindOfClass:UIAction.class]);

  EXPECT_NSEQ(l10n_util::GetPluralNSStringF(
                  IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP, 1),
              addToGroup.title);
}

// Tests selecting a tab with one existing group.
TEST_P(BaseGridMediatorTest, SelectedTabWithGroup) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kTabGroupsIPad, kModernTabStrip}, {});

  EXPECT_EQ(3UL, consumer_.items.size());

  [mediator_ selectTabsButtonTapped:nil];
  browser_->GetWebStateList()->CreateGroup(
      {1},
      tab_groups::TabGroupVisualData(u"My group",
                                     tab_groups::TabGroupColorId::kBlue),
      TabGroupId::GenerateNew());

  // Simulate a user who tapped on a tab.
  [mediator_ userTappedOnItemID:[GridItemIdentifier
                                    tabIdentifier:browser_->GetWebStateList()
                                                      ->GetWebStateAt(2)]];

  TabGridToolbarsConfiguration* configuration =
      fake_toolbars_mediator_.configuration;
  EXPECT_TRUE(configuration.selectAllButton);
  EXPECT_TRUE(configuration.doneButton);
  EXPECT_EQ(1u, configuration.selectedItemsCount);
  EXPECT_TRUE(configuration.closeSelectedTabsButton);
  EXPECT_TRUE(configuration.addToButton);
  EXPECT_TRUE(configuration.shareButton);

  EXPECT_FALSE(configuration.closeAllButton);
  EXPECT_FALSE(configuration.newTabButton);
  EXPECT_FALSE(configuration.searchButton);
  EXPECT_FALSE(configuration.selectTabsButton);
  EXPECT_FALSE(configuration.undoButton);
  EXPECT_FALSE(configuration.deselectAllButton);
  EXPECT_FALSE(configuration.cancelSearchButton);

  ASSERT_EQ(3u, configuration.addToButtonMenu.children.count);
  // Even if there is a single option, it is displayed as an inlined menu.
  UIMenuElement* addToGroupElement = configuration.addToButtonMenu.children[0];
  ASSERT_TRUE([addToGroupElement isKindOfClass:UIMenu.class]);

  UIMenu* addToGroupMenu = base::apple::ObjCCast<UIMenu>(addToGroupElement);
  ASSERT_EQ(1u, addToGroupMenu.children.count);

  UIMenuElement* addToGroupSubmenuElement = addToGroupMenu.children[0];
  ASSERT_TRUE([addToGroupSubmenuElement isKindOfClass:UIMenu.class]);
  UIMenu* addToGroupSubmenu =
      base::apple::ObjCCast<UIMenu>(addToGroupSubmenuElement);
  EXPECT_NSEQ(l10n_util::GetPluralNSStringF(
                  IDS_IOS_CONTENT_CONTEXT_ADDTABTOTABGROUP, 1),
              addToGroupSubmenu.title);
  ASSERT_EQ(2u, addToGroupSubmenu.children.count);

  UIMenuElement* addToGroup = addToGroupSubmenu.children[0];
  EXPECT_TRUE([addToGroup isKindOfClass:UIAction.class]);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP_SUBMENU),
              addToGroup.title);

  UIMenuElement* groupsElement = addToGroupSubmenu.children[1];
  ASSERT_TRUE([groupsElement isKindOfClass:UIMenu.class]);
  UIMenu* groups = base::apple::ObjCCast<UIMenu>(groupsElement);
  EXPECT_EQ(1u, groups.children.count);
  EXPECT_NSEQ(@"My group", groups.children[0].title);
}

// Tests that closing all tabs then adding a tab to the WebStateList removes the
// undo.
TEST_P(BaseGridMediatorTest, CloseAllThenAddWebState) {
  EXPECT_EQ(3UL, consumer_.items.size());
  [mediator_ closeAllButtonTapped:nil];

  TabGridToolbarsConfiguration* configuration =
      fake_toolbars_mediator_.configuration;
  EXPECT_TRUE(configuration.newTabButton);
  EXPECT_TRUE(configuration.searchButton);
  if (GetParam() == TEST_REGULAR_MEDIATOR) {
    // Undo is only available in regular.
    EXPECT_TRUE(configuration.undoButton);
  } else {
    EXPECT_FALSE(configuration.undoButton);
  }

  EXPECT_FALSE(configuration.selectAllButton);
  EXPECT_FALSE(configuration.doneButton);
  EXPECT_EQ(0u, configuration.selectedItemsCount);
  EXPECT_FALSE(configuration.closeSelectedTabsButton);
  EXPECT_FALSE(configuration.addToButton);
  EXPECT_FALSE(configuration.shareButton);
  EXPECT_FALSE(configuration.closeAllButton);
  EXPECT_FALSE(configuration.selectTabsButton);
  EXPECT_FALSE(configuration.deselectAllButton);
  EXPECT_FALSE(configuration.cancelSearchButton);

  // Insert a WebState, the undo should be gone.
  std::unique_ptr<web::FakeWebState> web_state =
      CreateFakeWebStateWithURL(GURL("http://google.com"));
  browser_->GetWebStateList()->InsertWebState(std::move(web_state));

  configuration = fake_toolbars_mediator_.configuration;
  EXPECT_TRUE(configuration.closeAllButton);
  EXPECT_TRUE(configuration.doneButton);
  EXPECT_TRUE(configuration.newTabButton);
  EXPECT_TRUE(configuration.searchButton);
  EXPECT_TRUE(configuration.selectTabsButton);

  EXPECT_FALSE(configuration.undoButton);
  EXPECT_FALSE(configuration.deselectAllButton);
  EXPECT_FALSE(configuration.selectAllButton);
  EXPECT_FALSE(configuration.addToButton);
  EXPECT_FALSE(configuration.closeSelectedTabsButton);
  EXPECT_FALSE(configuration.shareButton);
  EXPECT_FALSE(configuration.cancelSearchButton);
}

// Tests selecting a tab and a group with one existing group.
TEST_P(BaseGridMediatorTest, SelectedTabAndGroupWithGroup) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kTabGroupsIPad, kModernTabStrip}, {});

  EXPECT_EQ(3UL, consumer_.items.size());

  [mediator_ selectTabsButtonTapped:nil];
  browser_->GetWebStateList()->CreateGroup(
      {1},
      tab_groups::TabGroupVisualData(u"My group",
                                     tab_groups::TabGroupColorId::kBlue),
      TabGroupId::GenerateNew());

  // Simulate a user who tapped on a tab and on the group.
  [mediator_ userTappedOnItemID:[GridItemIdentifier
                                    tabIdentifier:browser_->GetWebStateList()
                                                      ->GetWebStateAt(2)]];
  [mediator_ userTappedOnItemID:[GridItemIdentifier
                                    tabIdentifier:browser_->GetWebStateList()
                                                      ->GetWebStateAt(1)]];

  TabGridToolbarsConfiguration* configuration =
      fake_toolbars_mediator_.configuration;
  EXPECT_TRUE(configuration.selectAllButton);
  EXPECT_TRUE(configuration.doneButton);
  EXPECT_EQ(2u, configuration.selectedItemsCount);
  EXPECT_TRUE(configuration.closeSelectedTabsButton);
  EXPECT_TRUE(configuration.addToButton);
  EXPECT_TRUE(configuration.shareButton);

  EXPECT_FALSE(configuration.closeAllButton);
  EXPECT_FALSE(configuration.newTabButton);
  EXPECT_FALSE(configuration.searchButton);
  EXPECT_FALSE(configuration.selectTabsButton);
  EXPECT_FALSE(configuration.undoButton);
  EXPECT_FALSE(configuration.deselectAllButton);
  EXPECT_FALSE(configuration.cancelSearchButton);

  ASSERT_EQ(3u, configuration.addToButtonMenu.children.count);
  // Even if there is a single option, it is displayed as an inlined menu.
  UIMenuElement* addToGroupElement = configuration.addToButtonMenu.children[0];
  ASSERT_TRUE([addToGroupElement isKindOfClass:UIMenu.class]);

  UIMenu* addToGroupMenu = base::apple::ObjCCast<UIMenu>(addToGroupElement);
  ASSERT_EQ(1u, addToGroupMenu.children.count);

  UIMenuElement* addToGroupSubmenuElement = addToGroupMenu.children[0];
  ASSERT_TRUE([addToGroupSubmenuElement isKindOfClass:UIMenu.class]);
  UIMenu* addToGroupSubmenu =
      base::apple::ObjCCast<UIMenu>(addToGroupSubmenuElement);
  EXPECT_NSEQ(l10n_util::GetPluralNSStringF(
                  IDS_IOS_CONTENT_CONTEXT_ADDTABTOTABGROUP, 2),
              addToGroupSubmenu.title);
  ASSERT_EQ(2u, addToGroupSubmenu.children.count);

  UIMenuElement* addToGroup = addToGroupSubmenu.children[0];
  EXPECT_TRUE([addToGroup isKindOfClass:UIAction.class]);
  EXPECT_NSEQ(l10n_util::GetNSString(
                  IDS_IOS_CONTENT_CONTEXT_ADDTABTONEWTABGROUP_SUBMENU),
              addToGroup.title);

  UIMenuElement* groupsElement = addToGroupSubmenu.children[1];
  ASSERT_TRUE([groupsElement isKindOfClass:UIMenu.class]);
  UIMenu* groups = base::apple::ObjCCast<UIMenu>(groupsElement);
  EXPECT_EQ(1u, groups.children.count);
  EXPECT_NSEQ(@"My group", groups.children[0].title);
}

// Tests that ungrouping a group correctly deletes the group.
TEST_P(BaseGridMediatorTest, UnGroup) {
  scoped_feature_list_.InitWithFeatures(
      {kTabGroupsIPad, kModernTabStrip, kTabGroupSync}, {});

  tab_groups::MockTabGroupSyncService* mock_service =
      static_cast<tab_groups::MockTabGroupSyncService*>(
          tab_groups::TabGroupSyncServiceFactory::GetForProfile(
              profile_.get()));

  WebStateList* web_state_list = browser_->GetWebStateList();
  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  web_state_list->CreateGroup({1}, {}, tab_group_id);
  const TabGroup* group = web_state_list->GetGroupOfWebStateAt(1);
  EXPECT_EQ(1u, web_state_list->GetGroups().size());
  EXPECT_EQ(3, web_state_list->count());

  EXPECT_CALL(*mock_service, RemoveLocalTabGroupMapping(tab_group_id, _))
      .Times(0);

  [mediator_ ungroupTabGroup:group];
  EXPECT_EQ(0u, web_state_list->GetGroups().size());
  EXPECT_EQ(3, web_state_list->count());
}

// Tests that ungrouping a group from another browser (e.g from Search)
// correctly deletes the group.
TEST_P(BaseGridMediatorTest, UnGroupFromAnotherBrowser) {
  scoped_feature_list_.InitWithFeatures(
      {kTabGroupsIPad, kModernTabStrip, kTabGroupSync}, {});
  mode_holder_.mode = TabGridMode::kSearch;

  WebStateList* other_web_state_list = other_browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(other_web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "| a b c d e f g", other_browser_->GetProfile()));

  tab_groups::MockTabGroupSyncService* mock_service =
      static_cast<tab_groups::MockTabGroupSyncService*>(
          tab_groups::TabGroupSyncServiceFactory::GetForProfile(
              profile_.get()));
  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  other_web_state_list->CreateGroup({1}, {}, tab_group_id);
  const TabGroup* group = other_web_state_list->GetGroupOfWebStateAt(1);
  EXPECT_EQ(1u, other_web_state_list->GetGroups().size());
  EXPECT_EQ(7, other_web_state_list->count());

  EXPECT_CALL(*mock_service, RemoveLocalTabGroupMapping(tab_group_id, _))
      .Times(0);

  [mediator_ ungroupTabGroup:group];
  EXPECT_EQ(0u, other_web_state_list->GetGroups().size());
  EXPECT_EQ(7, other_web_state_list->count());
}

// Tests that closing the last tab of a selected group clears the selection.
TEST_P(BaseGridMediatorTest, CloseSelectedGroup) {
  scoped_feature_list_.InitWithFeatures(
      {kTabGroupsIPad, kModernTabStrip, kTabGroupSync}, {});

  tab_groups::MockTabGroupSyncService* mock_service =
      static_cast<tab_groups::MockTabGroupSyncService*>(
          tab_groups::TabGroupSyncServiceFactory::GetForProfile(
              profile_.get()));

  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  WebStateList* web_state_list = browser_->GetWebStateList();
  const TabGroup* group = web_state_list->CreateGroup({1}, {}, tab_group_id);
  mode_holder_.mode = TabGridMode::kSelection;
  [mediator_
      addToSelectionItemID:[GridItemIdentifier groupIdentifier:group
                                              withWebStateList:web_state_list]];
  EXPECT_EQ(1UL, [mediator_ allSelectedDragItems].count);

  EXPECT_CALL(*mock_service, GetGroup(tab_group_id))
      .WillOnce(testing::Return(TestSavedGroup()));
  EXPECT_CALL(*mock_service, RemoveLocalTabGroupMapping(tab_group_id, _));
  EXPECT_CALL(*mock_service, RemoveGroup(tab_group_id)).Times(0);

  [mediator_ closeItemsWithTabIDs:{} groupIDs:{tab_group_id} tabCount:1];

  EXPECT_EQ(0UL, [mediator_ allSelectedDragItems].count);
}

// Tests that closing a group locally removes the mapping from the sync service.
TEST_P(BaseGridMediatorTest, CloseGroupLocally) {
  scoped_feature_list_.InitWithFeatures(
      {kTabGroupsIPad, kModernTabStrip, kTabGroupSync}, {});

  tab_groups::MockTabGroupSyncService* mock_service =
      static_cast<tab_groups::MockTabGroupSyncService*>(
          tab_groups::TabGroupSyncServiceFactory::GetForProfile(
              profile_.get()));

  WebStateList* web_state_list = browser_->GetWebStateList();
  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  web_state_list->CreateGroup({1}, {}, tab_group_id);
  const TabGroup* group = web_state_list->GetGroupOfWebStateAt(1);
  EXPECT_EQ(1u, web_state_list->GetGroups().size());

  EXPECT_CALL(*mock_service, GetGroup(tab_group_id))
      .WillOnce(testing::Return(TestSavedGroup()));
  EXPECT_CALL(*mock_service, RemoveLocalTabGroupMapping(tab_group_id, _));
  EXPECT_CALL(*mock_service, RemoveGroup(tab_group_id)).Times(0);

  [mediator_ closeItemWithIdentifier:[GridItemIdentifier
                                          groupIdentifier:group
                                         withWebStateList:web_state_list]];
  EXPECT_EQ(0u, web_state_list->GetGroups().size());
}

// Tests that closing a group locally from another browser (e.g from Search)
// correctly closes the group and removes the mapping from the sync service.
TEST_P(BaseGridMediatorTest, CloseGroupFromAnotherBrowser) {
  scoped_feature_list_.InitWithFeatures(
      {kTabGroupsIPad, kModernTabStrip, kTabGroupSync}, {});
  mode_holder_.mode = TabGridMode::kSearch;

  WebStateList* other_web_state_list = other_browser_->GetWebStateList();
  WebStateListBuilderFromDescription builder(other_web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "| a b c d e f g", other_browser_->GetProfile()));

  tab_groups::MockTabGroupSyncService* mock_service =
      static_cast<tab_groups::MockTabGroupSyncService*>(
          tab_groups::TabGroupSyncServiceFactory::GetForProfile(
              profile_.get()));
  TabGroupId tab_group_id = TabGroupId::GenerateNew();
  const TabGroup* group =
      other_web_state_list->CreateGroup({1}, {}, tab_group_id);
  EXPECT_EQ(1u, other_web_state_list->GetGroups().size());

  EXPECT_CALL(*mock_service, GetGroup(tab_group_id))
      .WillOnce(testing::Return(TestSavedGroup()));
  EXPECT_CALL(*mock_service, RemoveLocalTabGroupMapping(tab_group_id, _));
  EXPECT_CALL(*mock_service, RemoveGroup(tab_group_id)).Times(0);

  [mediator_
      closeItemWithIdentifier:[GridItemIdentifier
                                   groupIdentifier:group
                                  withWebStateList:other_web_state_list]];
  EXPECT_EQ(0u, other_web_state_list->GetGroups().size());
}

// Tests that closing multiple selected items doesn't delete saved groups.
TEST_P(BaseGridMediatorTest, CloseSelectedTabsAndGroups) {
  scoped_feature_list_.InitWithFeatures(
      {kTabGroupsIPad, kModernTabStrip, kTabGroupSync}, {});

  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "| a b c [ 1 d e ] [ 2 f g ] h", browser_->GetProfile()));

  tab_groups::MockTabGroupSyncService* mock_service =
      static_cast<tab_groups::MockTabGroupSyncService*>(
          tab_groups::TabGroupSyncServiceFactory::GetForProfile(
              profile_.get()));

  const TabGroup* group_1 = builder.GetTabGroupForIdentifier('1');
  const TabGroup* group_2 = builder.GetTabGroupForIdentifier('2');
  TabGroupId tab_group_id_1 = group_1->tab_group_id();
  TabGroupId tab_group_id_2 = group_2->tab_group_id();
  web::WebState* web_state_a = builder.GetWebStateForIdentifier('a');
  web::WebState* web_state_b = builder.GetWebStateForIdentifier('b');

  mode_holder_.mode = TabGridMode::kSelection;
  [mediator_
      addToSelectionItemID:[GridItemIdentifier tabIdentifier:web_state_a]];
  [mediator_
      addToSelectionItemID:[GridItemIdentifier tabIdentifier:web_state_b]];
  [mediator_
      addToSelectionItemID:[GridItemIdentifier groupIdentifier:group_1
                                              withWebStateList:web_state_list]];
  [mediator_
      addToSelectionItemID:[GridItemIdentifier groupIdentifier:group_2
                                              withWebStateList:web_state_list]];

  // 2 tabs, 2 tab groups.
  EXPECT_EQ(4UL, [mediator_ allSelectedDragItems].count);

  EXPECT_CALL(*mock_service, GetGroup(tab_group_id_1))
      .WillOnce(testing::Return(TestSavedGroup()));
  EXPECT_CALL(*mock_service, GetGroup(tab_group_id_2))
      .WillOnce(testing::Return(TestSavedGroup()));
  EXPECT_CALL(*mock_service, RemoveLocalTabGroupMapping(tab_group_id_1, _));
  EXPECT_CALL(*mock_service, RemoveLocalTabGroupMapping(tab_group_id_2, _));
  EXPECT_CALL(*mock_service, RemoveGroup(tab_group_id_1)).Times(0);
  EXPECT_CALL(*mock_service, RemoveGroup(tab_group_id_2)).Times(0);

  [mediator_ closeItemsWithTabIDs:{web_state_a->GetUniqueIdentifier(),
                                   web_state_b->GetUniqueIdentifier()}
                         groupIDs:{tab_group_id_1, tab_group_id_2}
                         tabCount:6];

  ASSERT_EQ("| c h", builder.GetWebStateListDescription());
  EXPECT_EQ(0UL, [mediator_ allSelectedDragItems].count);
}

// Tests that closing the last tab of a selected group in a batch operation
// clears the selection.
TEST_P(BaseGridMediatorTest, CloseSelectedGroupInBatch) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  web_state_list->CreateGroup({1}, {}, TabGroupId::GenerateNew());
  const TabGroup* group = web_state_list->GetGroupOfWebStateAt(1);
  mode_holder_.mode = TabGridMode::kSelection;
  [mediator_
      addToSelectionItemID:[GridItemIdentifier groupIdentifier:group
                                              withWebStateList:web_state_list]];
  EXPECT_EQ(1UL, [mediator_ allSelectedDragItems].count);

  {
    WebStateList::ScopedBatchOperation lock =
        browser_->GetWebStateList()->StartBatchOperation();
    browser_->GetWebStateList()->CloseWebStateAt(
        1, WebStateList::CLOSE_USER_ACTION);
  }

  EXPECT_EQ(0UL, [mediator_ allSelectedDragItems].count);
}

// Tests that moving the selected tab from one group to another correctly
// updates the selected element of the Grid, whether the tab itself is moving in
// the web state list or not.
TEST_P(BaseGridMediatorTest, SelectionAfterChangingGroup) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kTabGroupsIPad, kModernTabStrip}, {});

  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| [ 1 a* b ] [ 2 c ]",
                                                       browser_->GetProfile()));
  const TabGroup* group1 = builder.GetTabGroupForIdentifier('1');
  const TabGroup* group2 = builder.GetTabGroupForIdentifier('2');

  ASSERT_EQ(group1, consumer_.selectedItem.tabGroupItem.tabGroup);

  // Check after a move.
  web_state_list->MoveToGroup({0}, group2);
  ASSERT_EQ("| [ 1 b ] [ 2 c a* ]", builder.GetWebStateListDescription());
  EXPECT_EQ(group2, consumer_.selectedItem.tabGroupItem.tabGroup);

  // Check after a status-only change.
  web_state_list->ActivateWebStateAt(1);
  web_state_list->MoveToGroup({1}, group1);
  ASSERT_EQ("| [ 1 b c* ] [ 2 a ]", builder.GetWebStateListDescription());
  EXPECT_EQ(group1, consumer_.selectedItem.tabGroupItem.tabGroup);
}

// Tests dropping a local tab (e.g. drag from same window) in the grid.
TEST_P(BaseGridMediatorTest, DropLocalTab) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);

  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* b c ",
                                                       browser_->GetProfile()));

  web::WebStateID web_state_id =
      web_state_list->GetWebStateAt(2)->GetUniqueIdentifier();

  id local_object = [[TabInfo alloc] initWithTabID:web_state_id
                                           profile:browser_->GetProfile()];
  NSItemProvider* item_provider = [[NSItemProvider alloc] init];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;

  // Drop item.
  [mediator_ dropItem:drag_item toIndex:0 fromSameCollection:YES];

  EXPECT_EQ("| c a* b", builder.GetWebStateListDescription());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kSameCollection);
}

// Tests dropping a tabs from the tab group view in the grid.
TEST_P(BaseGridMediatorTest, DropLocalTabFromTabGroup) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kTabGroupsIPad, kModernTabStrip}, {});

  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);

  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* b c [ 0 d e f ] g",
                                                       browser_->GetProfile()));

  // Drop "D" (in a group) after "A".
  web::WebStateID web_state_id =
      web_state_list->GetWebStateAt(3)->GetUniqueIdentifier();
  id local_object = [[TabInfo alloc] initWithTabID:web_state_id
                                           profile:browser_->GetProfile()];
  NSItemProvider* item_provider = [[NSItemProvider alloc] init];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;
  [mediator_ dropItem:drag_item toIndex:1 fromSameCollection:NO];
  EXPECT_EQ("| a* d b c [ 0 e f ] g", builder.GetWebStateListDescription());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kSameBrowser, 1);

  // Drop "E" (in a group) before "G".
  web_state_id = web_state_list->GetWebStateAt(4)->GetUniqueIdentifier();
  local_object = [[TabInfo alloc] initWithTabID:web_state_id
                                        profile:browser_->GetProfile()];
  item_provider = [[NSItemProvider alloc] init];
  drag_item = [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;
  [mediator_ dropItem:drag_item toIndex:5 fromSameCollection:NO];
  EXPECT_EQ("| a* d b c [ 0 f ] e g", builder.GetWebStateListDescription());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kSameBrowser, 2);
}

// Tests dropping a tab from another browser (e.g. drag from another window) in
// the grid.
TEST_P(BaseGridMediatorTest, DropCrossWindowTab) {
  auto other_browser = std::make_unique<TestBrowser>(
      profile_.get(), scene_state_,
      std::make_unique<BrowserWebStateListDelegate>());
  SnapshotBrowserAgent::CreateForBrowser(other_browser.get());

  browser_list_->AddBrowser(other_browser.get());

  GURL url_to_load = GURL(kDraggedUrl);
  std::unique_ptr<web::FakeWebState> other_web_state =
      CreateFakeWebStateWithURL(url_to_load);
  web::WebStateID other_id = other_web_state->GetUniqueIdentifier();
  other_browser->GetWebStateList()->InsertWebState(std::move(other_web_state));

  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);

  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* b c ",
                                                       browser_->GetProfile()));

  id local_object = [[TabInfo alloc] initWithTabID:other_id
                                           profile:browser_->GetProfile()];
  NSItemProvider* item_provider = [[NSItemProvider alloc] init];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;

  // Drop item.
  [mediator_ dropItem:drag_item toIndex:1 fromSameCollection:NO];

  EXPECT_EQ(4, web_state_list->count());
  EXPECT_EQ(0, other_browser->GetWebStateList()->count());
  EXPECT_EQ(url_to_load, web_state_list->GetWebStateAt(1)->GetVisibleURL());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kOtherBrowser);
}

// Tests dropping a local Tab Group (i.e. from the same window).
TEST_P(BaseGridMediatorTest, DropLocalTabGroup) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kTabGroupsIPad, kModernTabStrip}, {});

  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);

  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "| [ 1 a* b ] c [ 2 d e ]", browser_->GetProfile()));

  const TabGroup* tab_group = web_state_list->GetGroupOfWebStateAt(4);

  id local_object = [[TabGroupInfo alloc] initWithTabGroup:tab_group
                                                   profile:profile_.get()];
  NSItemProvider* item_provider = [[NSItemProvider alloc] init];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;

  // Drop item.
  [mediator_ dropItem:drag_item toIndex:1 fromSameCollection:YES];

  EXPECT_EQ("| [ 1 a* b ] [ 2 d e ] c", builder.GetWebStateListDescription());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kSameCollection);
}

// Tests dropping a Tab Group from another browser (i.e. from the same window).
TEST_P(BaseGridMediatorTest, DropCrossBrowserTabGroup) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kTabGroupsIPad, kModernTabStrip}, {});

  // Prepare the web state list in which the group will be dropped.
  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);

  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription(
      "| [ 1 a* b ] c [ 2 d e ]", browser_->GetProfile()));

  // Prepare the other web state list, from which the group will be dragged.
  auto other_browser = std::make_unique<TestBrowser>(
      profile_.get(), scene_state_,
      std::make_unique<BrowserWebStateListDelegate>());
  SnapshotBrowserAgent::CreateForBrowser(other_browser.get());

  browser_list_->AddBrowser(other_browser.get());

  GURL url_to_load = GURL(kDraggedUrl);
  other_browser->GetWebStateList()->InsertWebState(
      CreateFakeWebStateWithURL(url_to_load));
  other_browser->GetWebStateList()->CreateGroup({0}, {},
                                                TabGroupId::GenerateNew());

  const TabGroup* other_tab_group =
      other_browser->GetWebStateList()->GetGroupOfWebStateAt(0);
  tab_groups::TabGroupId tab_group_id = other_tab_group->tab_group_id();

  id local_object = [[TabGroupInfo alloc] initWithTabGroup:other_tab_group
                                                   profile:profile_.get()];
  NSItemProvider* item_provider = [[NSItemProvider alloc] init];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;

  // Drop item.
  [mediator_ dropItem:drag_item toIndex:2 fromSameCollection:NO];

  EXPECT_EQ(nullptr, web_state_list->GetGroupOfWebStateAt(2));
  EXPECT_EQ(tab_group_id,
            web_state_list->GetGroupOfWebStateAt(3)->tab_group_id());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kOtherBrowser);
}

// Tests dropping an interal URL (e.g. drag from omnibox) in the grid.
TEST_P(BaseGridMediatorTest, DropInternalURL) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* b c ",
                                                       browser_->GetProfile()));

  GURL url_to_load = GURL(kDraggedUrl);
  id local_object = [[URLInfo alloc] initWithURL:url_to_load title:@"My title"];
  NSItemProvider* item_provider = [[NSItemProvider alloc] init];
  UIDragItem* drag_item =
      [[UIDragItem alloc] initWithItemProvider:item_provider];
  drag_item.localObject = local_object;

  // Drop item.
  [mediator_ dropItem:drag_item toIndex:1 fromSameCollection:YES];

  EXPECT_EQ(4, web_state_list->count());
  web::WebState* web_state = web_state_list->GetWebStateAt(1);
  EXPECT_EQ(url_to_load,
            web_state->GetNavigationManager()->GetPendingItem()->GetURL());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kOther);
}

// Tests dropping an external URL in the grid.
TEST_P(BaseGridMediatorTest, DropExternalURL) {
  WebStateList* web_state_list = browser_->GetWebStateList();
  CloseAllWebStates(*web_state_list, WebStateList::CLOSE_NO_FLAGS);
  WebStateListBuilderFromDescription builder(web_state_list);
  ASSERT_TRUE(builder.BuildWebStateListFromDescription("| a* b c ",
                                                       browser_->GetProfile()));

  NSItemProvider* item_provider = [[NSItemProvider alloc]
      initWithContentsOfURL:[NSURL URLWithString:base::SysUTF8ToNSString(
                                                     kDraggedUrl)]];

  // Drop item.
  [mediator_ dropItemFromProvider:item_provider
                          toIndex:1
               placeholderContext:nil];

  EXPECT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
      base::Seconds(1), ^bool(void) {
        return web_state_list->count() == 4;
      }));
  web::WebState* web_state = web_state_list->GetWebStateAt(1);
  EXPECT_EQ(GURL(kDraggedUrl),
            web_state->GetNavigationManager()->GetPendingItem()->GetURL());
  ExpectThatDragItemOriginMetricLogged(DragItemOrigin::kOther);
}

INSTANTIATE_TEST_SUITE_P(
    /* No InstantiationName */,
    BaseGridMediatorTest,
    ::testing::Values(GridMediatorType::TEST_REGULAR_MEDIATOR,
                      GridMediatorType::TEST_INCOGNITO_MEDIATOR));

INSTANTIATE_TEST_SUITE_P(
    /* No InstantiationName */,
    BaseGridMediatorWithPriceDropIndicatorsTest,
    ::testing::Values(GridMediatorType::TEST_REGULAR_MEDIATOR,
                      GridMediatorType::TEST_INCOGNITO_MEDIATOR));
