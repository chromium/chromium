// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/price_notifications/price_notifications_table_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_controller_test.h"
#import "ios/chrome/browser/ui/price_notifications/cells/price_notifications_table_view_item.h"
#import "ios/chrome/browser/ui/price_notifications/price_notifications_consumer.h"
#import "ios/chrome/browser/ui/price_notifications/test_price_notifications_mutator.h"
#import "ios/chrome/grit/ios_strings.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// PriceNotificationsTableViewController SectionIdentifier values.
NSUInteger SectionIdentifierTrackableItemsOnCurrentSite = 10;
NSUInteger SectionIdentifierTrackedItems = 11;
NSUInteger SectionIdentifierTableViewHeader = 12;

// PriceNotificaitonsTableViewController ListItem values.
NSUInteger ItemTypeListItem = 102;

template <typename T>
// Returns the TableViewHeaderFooterItem `T` from `section_id`.
T* GetHeaderItemFromSection(LegacyChromeTableViewController* controller,
                            NSUInteger section_id) {
  return base::apple::ObjCCastStrict<T>([controller.tableViewModel
      headerForSectionIndex:[controller.tableViewModel
                                sectionForSectionIdentifier:section_id]]);
}

// Returns an array of PriceNotificationTableViewItems contained in
// `section_id`.
NSArray<PriceNotificationsTableViewItem*>* GetItemsFromSection(
    TableViewModel* model,
    NSUInteger section_id) {
  NSArray<NSIndexPath*>* paths = [model indexPathsForItemType:ItemTypeListItem
                                            sectionIdentifier:section_id];
  NSMutableArray* items = [[NSMutableArray alloc] initWithCapacity:paths.count];
  for (NSIndexPath* path in paths) {
    [items addObject:[model itemAtIndexPath:path]];
  }

  return items;
}
}  // namespace

class PriceNotificationsTableViewControllerTest
    : public LegacyChromeTableViewControllerTest {
 public:
  LegacyChromeTableViewController* InstantiateController() override {
    return [[PriceNotificationsTableViewController alloc]
        initWithStyle:UITableViewStylePlain];
  }
};

// Tests the Trackable Item is in the loading state, which displays a
// placeholder view, on the creation of the TableViewController.
TEST_F(PriceNotificationsTableViewControllerTest,
       DisplayTrackableItemLoadingScreenWhenThereIsNoData) {
  TableViewModel* model = controller().tableViewModel;
  NSIndexPath* trackableItemPlaceholderIndexPath =
      [model indexPathForItemType:ItemTypeListItem
                sectionIdentifier:SectionIdentifierTrackableItemsOnCurrentSite];
  PriceNotificationsTableViewItem* trackableItemPlaceholder =
      base::apple::ObjCCast<PriceNotificationsTableViewItem>(
          [model itemAtIndexPath:trackableItemPlaceholderIndexPath]);

  EXPECT_EQ(trackableItemPlaceholder.loading, true);
}

// Tests the two tracked items are in the loading state, which displays a
// placeholder view, on the creation of the TableViewController.
TEST_F(PriceNotificationsTableViewControllerTest,
       DisplayTrackedItemsLoadingScreenWhenThereIsNoData) {
  TableViewModel* model = controller().tableViewModel;
  NSArray<NSIndexPath*>* placeholders =
      [model indexPathsForItemType:ItemTypeListItem
                 sectionIdentifier:SectionIdentifierTrackedItems];
  PriceNotificationsTableViewItem* firstTrackedItemPlacholder =
      base::apple::ObjCCast<PriceNotificationsTableViewItem>(
          [model itemAtIndexPath:placeholders[0]]);
  PriceNotificationsTableViewItem* secondTrackedItemPlaceholder =
      base::apple::ObjCCast<PriceNotificationsTableViewItem>(
          [model itemAtIndexPath:placeholders[1]]);

  EXPECT_EQ(placeholders.count, 2u);
  EXPECT_TRUE(firstTrackedItemPlacholder.loading);
  EXPECT_TRUE(secondTrackedItemPlaceholder.loading);
}

// Simulates receiving no data from the mediator and checks that the
// correct messages are displayed.
TEST_F(PriceNotificationsTableViewControllerTest,
       DisplayTrackableSectionEmptyStateWhenProductPageIsNotTrackable) {
  id<PriceNotificationsConsumer> consumer =
      base::apple::ObjCCast<PriceNotificationsTableViewController>(
          controller());

  [consumer setTrackableItem:nil currentlyTracking:NO];
  TableViewTextHeaderFooterItem* item =
      GetHeaderItemFromSection<TableViewTextHeaderFooterItem>(
          controller(), SectionIdentifierTableViewHeader);
  NSString* tableHeadingText = item.subtitle;
  TableViewTextHeaderFooterItem* trackableHeaderItem =
      GetHeaderItemFromSection<TableViewTextHeaderFooterItem>(
          controller(), SectionIdentifierTrackableItemsOnCurrentSite);
  TableViewTextHeaderFooterItem* trackedHeaderItem =
      GetHeaderItemFromSection<TableViewTextHeaderFooterItem>(
          controller(), SectionIdentifierTrackedItems);

  EXPECT_NSEQ(
      tableHeadingText,
      l10n_util::GetNSString(
          IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_DESCRIPTION_EMPTY_STATE));
  EXPECT_FALSE([controller().tableViewModel
      hasItemForItemType:ItemTypeListItem
       sectionIdentifier:SectionIdentifierTrackableItemsOnCurrentSite]);
  EXPECT_NSEQ(
      trackableHeaderItem.text,
      l10n_util::GetNSString(
          IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TRACKABLE_SECTION_HEADER));
  EXPECT_NSEQ(
      trackableHeaderItem.subtitle,
      l10n_util::GetNSString(
          IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TRACKABLE_EMPTY_LIST));
  EXPECT_FALSE([controller().tableViewModel
      hasItemForItemType:ItemTypeListItem
       sectionIdentifier:SectionIdentifierTrackedItems]);
  EXPECT_NSEQ(
      trackedHeaderItem.text,
      l10n_util::GetNSString(
          IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TRACKED_SECTION_HEADER));
  EXPECT_NSEQ(trackedHeaderItem.subtitle,
              l10n_util::GetNSString(
                  IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_TRACKING_EMPTY_LIST));
}

// Simulates that a trackable item exists and is properly displayed.
TEST_F(PriceNotificationsTableViewControllerTest,
       DisplayTrackableItemWhenAvailable) {
  id<PriceNotificationsConsumer> consumer =
      base::apple::ObjCCast<PriceNotificationsTableViewController>(
          controller());
  PriceNotificationsTableViewItem* item =
      [[PriceNotificationsTableViewItem alloc] initWithType:ItemTypeListItem];
  item.title = @"Test Title";

  [consumer setTrackableItem:item currentlyTracking:NO];
  NSArray<PriceNotificationsTableViewItem*>* items =
      GetItemsFromSection(controller().tableViewModel,
                          SectionIdentifierTrackableItemsOnCurrentSite);

  EXPECT_TRUE([controller().tableViewModel
      hasItemForItemType:ItemTypeListItem
       sectionIdentifier:SectionIdentifierTrackableItemsOnCurrentSite]);
  EXPECT_EQ(items.count, 1u);
  EXPECT_NSEQ(items[0].title, item.title);
}

// Simulates that a tracked item exists and is displayed
TEST_F(PriceNotificationsTableViewControllerTest, DisplayUsersTrackedItems) {
  id<PriceNotificationsConsumer> consumer =
      base::apple::ObjCCast<PriceNotificationsTableViewController>(
          controller());
  PriceNotificationsTableViewItem* item =
      [[PriceNotificationsTableViewItem alloc] initWithType:ItemTypeListItem];
  item.title = @"Test Title";

  [consumer addTrackedItem:item toBeginning:NO];
  NSArray<PriceNotificationsTableViewItem*>* items = GetItemsFromSection(
      controller().tableViewModel, SectionIdentifierTrackedItems);

  EXPECT_TRUE([controller().tableViewModel
      hasItemForItemType:ItemTypeListItem
       sectionIdentifier:SectionIdentifierTrackedItems]);
  EXPECT_EQ(items.count, 1u);
  EXPECT_NSEQ(items[0].title, item.title);
}

// Simulates that a tracked item exists and is displayed when the user is
// on that item's webpage.
TEST_F(PriceNotificationsTableViewControllerTest,
       DisplayUsersTrackedItemsWhenViewingTrackedItemWebpage) {
  PriceNotificationsTableViewController* tableViewController =
      base::apple::ObjCCastStrict<PriceNotificationsTableViewController>(
          controller());
  PriceNotificationsTableViewItem* item =
      [[PriceNotificationsTableViewItem alloc] initWithType:ItemTypeListItem];
  item.title = @"Test Title";

  [tableViewController setTrackableItem:nil currentlyTracking:YES];
  [tableViewController addTrackedItem:item toBeginning:NO];
  NSArray<PriceNotificationsTableViewItem*>* items = GetItemsFromSection(
      tableViewController.tableViewModel, SectionIdentifierTrackedItems);
  TableViewTextHeaderFooterItem* trackableHeaderItem =
      GetHeaderItemFromSection<TableViewTextHeaderFooterItem>(
          tableViewController, SectionIdentifierTrackableItemsOnCurrentSite);

  EXPECT_TRUE([tableViewController.tableViewModel
      hasItemForItemType:ItemTypeListItem
       sectionIdentifier:SectionIdentifierTrackedItems]);
  EXPECT_EQ(items.count, 1u);
  EXPECT_NSEQ(
      trackableHeaderItem.subtitle,
      l10n_util::GetNSString(
          IDS_IOS_PRICE_NOTIFICAITONS_PRICE_TRACK_TRACKABLE_ITEM_IS_TRACKED));
}

// Simulates that a trackable item exists, has been selected to be tracked,
// and the item is moved to the tracked section
TEST_F(PriceNotificationsTableViewControllerTest,
       TrackableItemMovedToTrackedSectionOnStartTracking) {
  id<PriceNotificationsConsumer> consumer =
      base::apple::ObjCCast<PriceNotificationsTableViewController>(
          controller());
  PriceNotificationsTableViewItem* item =
      [[PriceNotificationsTableViewItem alloc] initWithType:ItemTypeListItem];
  TableViewModel* model = controller().tableViewModel;

  [consumer setTrackableItem:item currentlyTracking:NO];
  NSArray<PriceNotificationsTableViewItem*>* items =
      GetItemsFromSection(model, SectionIdentifierTrackableItemsOnCurrentSite);
  NSUInteger trackableItemCountBeforeStartTracking = items.count;
  items = GetItemsFromSection(model, SectionIdentifierTrackedItems);
  NSUInteger trackedItemCountBeforeStartTracking = items.count;
  [consumer didStartPriceTrackingForItem:item];
  TableViewTextHeaderFooterItem* trackableHeaderItem =
      GetHeaderItemFromSection<TableViewTextHeaderFooterItem>(
          controller(), SectionIdentifierTrackableItemsOnCurrentSite);
  items =
      GetItemsFromSection(model, SectionIdentifierTrackableItemsOnCurrentSite);
  NSUInteger trackableItemCountAfterStartTracking = items.count;
  items = GetItemsFromSection(model, SectionIdentifierTrackedItems);
  NSUInteger trackedItemCountAfterStartTracking = items.count;

  EXPECT_EQ(trackableItemCountBeforeStartTracking, 1u);
  EXPECT_EQ(trackedItemCountBeforeStartTracking, 0u);
  EXPECT_EQ(trackableItemCountAfterStartTracking, 0u);
  EXPECT_EQ(trackedItemCountAfterStartTracking, 1u);
  EXPECT_NSEQ(
      trackableHeaderItem.subtitle,
      l10n_util::GetNSString(
          IDS_IOS_PRICE_NOTIFICATIONS_PRICE_TRACK_DESCRIPTION_FOR_TRACKED_ITEM));
}

// Simulates the user tapping on a tracked item and being redirected to
// that page.
TEST_F(PriceNotificationsTableViewControllerTest,
       RedirectToTrackedItemsWebpageOnSelection) {
  PriceNotificationsTableViewController* tableViewController =
      base::apple::ObjCCastStrict<PriceNotificationsTableViewController>(
          controller());
  PriceNotificationsTableViewItem* item =
      [[PriceNotificationsTableViewItem alloc] initWithType:ItemTypeListItem];
  item.tracking = YES;
  TableViewModel* model = tableViewController.tableViewModel;
  TestPriceNotificationsMutator* mutator =
      [[TestPriceNotificationsMutator alloc] init];
  tableViewController.mutator = mutator;

  [tableViewController setTrackableItem:item currentlyTracking:NO];
  [tableViewController didStartPriceTrackingForItem:item];
  NSIndexPath* itemIndexPath =
      [model indexPathForItemType:ItemTypeListItem
                sectionIdentifier:SectionIdentifierTrackedItems];

  EXPECT_EQ(itemIndexPath, [tableViewController tableView:controller().tableView
                                 willSelectRowAtIndexPath:itemIndexPath]);
  [tableViewController tableView:tableViewController.tableView
         didSelectRowAtIndexPath:itemIndexPath];
  EXPECT_FALSE(mutator.didNavigateToItemPage);
  EXPECT_TRUE([tableViewController tableView:tableViewController.tableView
      canPerformPrimaryActionForRowAtIndexPath:itemIndexPath]);
  [tableViewController tableView:tableViewController.tableView
      performPrimaryActionForRowAtIndexPath:itemIndexPath];
  EXPECT_TRUE(mutator.didNavigateToItemPage);
  return;
}

// Simulates untracking an item when the user is viewing a page that is not
// price trackable
TEST_F(PriceNotificationsTableViewControllerTest,
       UntrackItemWhenTrackableItemSectionIsEmpty) {
  id<PriceNotificationsConsumer> consumer =
      base::apple::ObjCCast<PriceNotificationsTableViewController>(
          controller());
  PriceNotificationsTableViewItem* item =
      [[PriceNotificationsTableViewItem alloc] initWithType:ItemTypeListItem];

  [consumer addTrackedItem:item toBeginning:YES];
  [consumer didStopPriceTrackingItem:item onCurrentSite:NO];
  NSArray<PriceNotificationsTableViewItem*>* trackable_section_items =
      GetItemsFromSection(controller().tableViewModel,
                          SectionIdentifierTrackableItemsOnCurrentSite);
  NSArray<PriceNotificationsTableViewItem*>* tracked_section_items =
      GetItemsFromSection(controller().tableViewModel,
                          SectionIdentifierTrackedItems);

  EXPECT_EQ(trackable_section_items.count, 0u);
  EXPECT_EQ(tracked_section_items.count, 0u);
}

// Simulates untracking an item when the user is viewing that item's
// webpage.
TEST_F(PriceNotificationsTableViewControllerTest,
       UntrackItemFromCurrentlyViewedWebpageWhenTrackableItemSectionIsEmpty) {
  id<PriceNotificationsConsumer> consumer =
      base::apple::ObjCCast<PriceNotificationsTableViewController>(
          controller());
  PriceNotificationsTableViewItem* item =
      [[PriceNotificationsTableViewItem alloc] initWithType:ItemTypeListItem];

  [consumer addTrackedItem:item toBeginning:YES];
  [consumer didStopPriceTrackingItem:item onCurrentSite:YES];
  NSArray<PriceNotificationsTableViewItem*>* trackable_section_items =
      GetItemsFromSection(controller().tableViewModel,
                          SectionIdentifierTrackableItemsOnCurrentSite);
  NSArray<PriceNotificationsTableViewItem*>* tracked_section_items =
      GetItemsFromSection(controller().tableViewModel,
                          SectionIdentifierTrackedItems);

  EXPECT_EQ(trackable_section_items.count, 1u);
  EXPECT_EQ(tracked_section_items.count, 0u);
}

// Simulates untracking an item when the user is viewing a page that is
// price trackable and the number of tracked items is one.
TEST_F(PriceNotificationsTableViewControllerTest,
       UntrackItemRemainingTrackedItemWhenTrackableItemSectionIsNotEmpty) {
  id<PriceNotificationsConsumer> consumer =
      base::apple::ObjCCast<PriceNotificationsTableViewController>(
          controller());
  PriceNotificationsTableViewItem* trackable_item =
      [[PriceNotificationsTableViewItem alloc] initWithType:ItemTypeListItem];
  PriceNotificationsTableViewItem* tracked_item =
      [[PriceNotificationsTableViewItem alloc] initWithType:ItemTypeListItem];

  [consumer setTrackableItem:trackable_item currentlyTracking:NO];
  [consumer addTrackedItem:tracked_item toBeginning:YES];
  [consumer didStopPriceTrackingItem:tracked_item onCurrentSite:NO];
  NSArray<PriceNotificationsTableViewItem*>* trackable_section_items =
      GetItemsFromSection(controller().tableViewModel,
                          SectionIdentifierTrackableItemsOnCurrentSite);
  NSArray<PriceNotificationsTableViewItem*>* tracked_section_items =
      GetItemsFromSection(controller().tableViewModel,
                          SectionIdentifierTrackedItems);

  EXPECT_EQ(trackable_section_items.count, 1u);
  EXPECT_EQ(tracked_section_items.count, 0u);
}

// Simulates untracking an item when the user is viewing a page that is
// price trackable and the number of tracked items is greater than 1.
TEST_F(
    PriceNotificationsTableViewControllerTest,
    UntrackItemWithMultipleTrackedItemsRemainingWhenTrackableItemSectionIsNotEmpty) {
  id<PriceNotificationsConsumer> consumer =
      base::apple::ObjCCast<PriceNotificationsTableViewController>(
          controller());
  PriceNotificationsTableViewItem* trackable_item =
      [[PriceNotificationsTableViewItem alloc] initWithType:ItemTypeListItem];
  PriceNotificationsTableViewItem* tracked_item;

  [consumer setTrackableItem:trackable_item currentlyTracking:NO];
  for (size_t i = 0; i < 5; i++) {
    tracked_item =
        [[PriceNotificationsTableViewItem alloc] initWithType:ItemTypeListItem];
    [consumer addTrackedItem:tracked_item toBeginning:NO];
  }
  [consumer didStopPriceTrackingItem:tracked_item onCurrentSite:NO];
  NSArray<PriceNotificationsTableViewItem*>* trackable_section_items =
      GetItemsFromSection(controller().tableViewModel,
                          SectionIdentifierTrackableItemsOnCurrentSite);
  NSArray<PriceNotificationsTableViewItem*>* tracked_section_items =
      GetItemsFromSection(controller().tableViewModel,
                          SectionIdentifierTrackedItems);

  EXPECT_EQ(trackable_section_items.count, 1u);
  EXPECT_EQ(tracked_section_items.count, 4u);
}

// Simulates untracking a product that is visible on the current site but the
// site's product is not tracked.
TEST_F(PriceNotificationsTableViewControllerTest,
       UntrackCrossMerchantItemWithItemOnCurrentPageNotTracked) {
  id<PriceNotificationsConsumer> consumer =
      base::apple::ObjCCast<PriceNotificationsTableViewController>(
          controller());
  PriceNotificationsTableViewItem* trackable_item =
      [[PriceNotificationsTableViewItem alloc] initWithType:ItemTypeListItem];
  [consumer setTrackableItem:trackable_item currentlyTracking:NO];
  PriceNotificationsTableViewItem* tracked_item =
      [[PriceNotificationsTableViewItem alloc] initWithType:ItemTypeListItem];
  [consumer addTrackedItem:tracked_item toBeginning:NO];

  [consumer didStopPriceTrackingItem:tracked_item onCurrentSite:YES];
  NSArray<PriceNotificationsTableViewItem*>* trackable_section_items =
      GetItemsFromSection(controller().tableViewModel,
                          SectionIdentifierTrackableItemsOnCurrentSite);
  NSArray<PriceNotificationsTableViewItem*>* tracked_section_items =
      GetItemsFromSection(controller().tableViewModel,
                          SectionIdentifierTrackedItems);

  EXPECT_EQ(trackable_section_items.count, 1u);
  EXPECT_EQ(tracked_section_items.count, 0u);
}
