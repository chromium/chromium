// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/shortcuts_view_controller.h"

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/ntp_tile_views/ntp_most_visited_tile_view.h"
#import "ios/chrome/browser/ui/ntp_tile_views/ntp_shortcut_tile_view.h"
#import "ios/chrome/browser/ui/ntp_tile_views/ntp_tile_constants.h"
#import "ios/chrome/browser/ui/ntp_tile_views/ntp_tile_layout_util.h"
#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/collection_shortcut_cell.h"
#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/most_visited_shortcut_cell.h"
#import "ios/chrome/browser/ui/omnibox/popup/shortcuts/shortcuts_view_controller_delegate.h"
#import "ios/chrome/common/favicon/favicon_view.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kTopInset = 10;
}  // namespace

@interface ShortcutsViewController ()

// Latest most visited items. Updated directly from the consumer calls.
@property(nonatomic, strong)
    NSArray<ShortcutsMostVisitedItem*>* latestMostVisitedItems;
// Currently displayed most visited items. Will be set to nil when the view
// disappears, and set to |latestMostVisitedItems| when the view appears. This
// prevents the updates when the user sees the tiles.
@property(nonatomic, strong)
    NSArray<ShortcutsMostVisitedItem*>* displayedMostVisitedItems;
@property(nonatomic, assign) NSInteger readingListBadgeValue;

@end

@implementation ShortcutsViewController

#pragma mark - initializers

- (instancetype)init {
  UICollectionViewFlowLayout* layout =
      [[UICollectionViewFlowLayout alloc] init];
  layout.sectionInset = UIEdgeInsetsMake(kTopInset, 0, 0, 0);
  self = [super initWithCollectionViewLayout:layout];
  if (self) {
    self.collectionView.backgroundColor = [UIColor clearColor];
    [self.collectionView registerClass:[MostVisitedShortcutCell class]
            forCellWithReuseIdentifier:NSStringFromClass(
                                           [MostVisitedShortcutCell class])];
    [self.collectionView registerClass:[CollectionShortcutCell class]
            forCellWithReuseIdentifier:NSStringFromClass(
                                           [CollectionShortcutCell class])];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];

  // Promote the latest most visited items to the displayed ones and reload the
  // collection view data.
  self.displayedMostVisitedItems = self.latestMostVisitedItems;

  [self configureLayout:base::mac::ObjCCastStrict<UICollectionViewFlowLayout>(
                            self.collectionViewLayout)];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];

  [self configureLayout:base::mac::ObjCCastStrict<UICollectionViewFlowLayout>(
                            self.collectionViewLayout)];
}

#pragma mark - ShortcutsConsumer

- (void)mostVisitedShortcutsAvailable:
    (NSArray<ShortcutsMostVisitedItem*>*)items {
  self.latestMostVisitedItems = items;

  // Normally, the most visited tiles should not change when the user sees them.
  // However, in case there were no items, and now they're available, it is
  // better to show something, even if this means reloading the view.
  if (self.displayedMostVisitedItems.count == 0 && self.viewLoaded) {
    self.displayedMostVisitedItems = self.latestMostVisitedItems;
    [self.collectionView reloadData];
  }
}

- (void)faviconChangedForURL:(const GURL&)URL {
  if (!self.viewLoaded) {
    return;
  }

  for (ShortcutsMostVisitedItem* item in self.displayedMostVisitedItems) {
    if (item.URL == URL) {
      NSUInteger i = [self.displayedMostVisitedItems indexOfObject:item];
      NSIndexPath* indexPath = [NSIndexPath indexPathForItem:i inSection:0];
      MostVisitedShortcutCell* cell =
          base::mac::ObjCCastStrict<MostVisitedShortcutCell>(
              [self.collectionView cellForItemAtIndexPath:indexPath]);
      [self configureMostVisitedCell:cell
                            withItem:self.displayedMostVisitedItems[i]];
    }
  }
}

- (void)readingListBadgeUpdatedWithCount:(NSInteger)count {
  self.readingListBadgeValue = count;
  if (!self.viewLoaded) {
    return;
  }

  NSIndexPath* readingListShortcutIndexPath =
      [NSIndexPath indexPathForItem:NTPCollectionShortcutTypeReadingList +
                                    self.displayedMostVisitedItems.count
                          inSection:0];
  [self.collectionView
      reloadItemsAtIndexPaths:@[ readingListShortcutIndexPath ]];
}

#pragma mark - UICollectionViewDataSource

- (NSInteger)numberOfSectionsInCollectionView:
    (UICollectionView*)collectionView {
  return 1;
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  DCHECK(section == 0);
  return self.displayedMostVisitedItems.count + NTPCollectionShortcutTypeCount;
}

// The cell that is returned must be retrieved from a call to
// -dequeueReusableCellWithReuseIdentifier:forIndexPath:
- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  if (static_cast<NSUInteger>(indexPath.row) <
      self.displayedMostVisitedItems.count) {
    MostVisitedShortcutCell* cell = [self.collectionView
        dequeueReusableCellWithReuseIdentifier:
            NSStringFromClass([MostVisitedShortcutCell class])
                                  forIndexPath:indexPath];
    ShortcutsMostVisitedItem* item =
        self.displayedMostVisitedItems[indexPath.item];
    [self configureMostVisitedCell:cell withItem:item];
    cell.accessibilityTraits = UIAccessibilityTraitButton;
    return cell;
  } else {
    CollectionShortcutCell* cell = [self.collectionView
        dequeueReusableCellWithReuseIdentifier:
            NSStringFromClass([CollectionShortcutCell class])
                                  forIndexPath:indexPath];
    DCHECK(static_cast<NSUInteger>(indexPath.row) <
           self.displayedMostVisitedItems.count +
               NTPCollectionShortcutTypeCount)
        << "Only four collection shortcuts described in "
           "NTPCollectionShortcutType are supported";

    NTPCollectionShortcutType type = (NTPCollectionShortcutType)(
        indexPath.item - self.displayedMostVisitedItems.count);
    [self configureCollectionShortcutCell:cell withCollection:type];
    cell.accessibilityTraits = UIAccessibilityTraitButton;
    return cell;
  }

  return nil;
}

- (void)configureMostVisitedCell:(MostVisitedShortcutCell*)cell
                        withItem:(ShortcutsMostVisitedItem*)item {
  [cell.tile.faviconView configureWithAttributes:item.attributes];
  cell.tile.titleLabel.text = item.title;
  cell.accessibilityLabel = cell.tile.titleLabel.text;
}

- (void)configureCollectionShortcutCell:(CollectionShortcutCell*)cell
                         withCollection:(NTPCollectionShortcutType)type {
  cell.tile.titleLabel.text = TitleForCollectionShortcutType(type);
  cell.tile.iconView.image = ImageForCollectionShortcutType(type);
  cell.accessibilityLabel = cell.tile.titleLabel.text;
  if (@available(iOS 13, *)) {
    // The accessibilityUserInputLabel should just be the title, with nothing
    // extra for the reading list tile.
    cell.accessibilityUserInputLabels = @[ cell.tile.titleLabel.text ];
  }

  if (type == NTPCollectionShortcutTypeReadingList) {
    if (self.readingListBadgeValue > 0) {
      cell.tile.countLabel.text = [@(self.readingListBadgeValue) stringValue];
      cell.tile.countContainer.hidden = NO;
      cell.accessibilityLabel = [NSString
          stringWithFormat:@"%@, %@", cell.accessibilityLabel,
                           AccessibilityLabelForReadingListCellWithCount(
                               self.readingListBadgeValue)];
    }
  }
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  if (static_cast<NSUInteger>(indexPath.row) <
      self.displayedMostVisitedItems.count) {
    ShortcutsMostVisitedItem* item =
        self.displayedMostVisitedItems[indexPath.item];
    DCHECK(item);
    [self.commandHandler openMostVisitedItem:item];
    base::RecordAction(
        base::UserMetricsAction("MobileOmniboxShortcutsOpenMostVisitedItem"));
  } else {
    NTPCollectionShortcutType type = (NTPCollectionShortcutType)(
        indexPath.item - self.displayedMostVisitedItems.count);
    switch (type) {
      case NTPCollectionShortcutTypeBookmark:
        [self.commandHandler openBookmarks];
        base::RecordAction(
            base::UserMetricsAction("MobileOmniboxShortcutsOpenBookmarks"));
        break;
      case NTPCollectionShortcutTypeRecentTabs:
        [self.commandHandler openRecentTabs];
        base::RecordAction(
            base::UserMetricsAction("MobileOmniboxShortcutsOpenRecentTabs"));
        break;
      case NTPCollectionShortcutTypeReadingList:
        [self.commandHandler openReadingList];
        base::RecordAction(
            base::UserMetricsAction("MobileOmniboxShortcutsOpenReadingList"));
        break;
      case NTPCollectionShortcutTypeHistory:
        [self.commandHandler openHistory];
        base::RecordAction(
            base::UserMetricsAction("MobileOmniboxShortcutsOpenHistory"));
        break;
      case NTPCollectionShortcutTypeCount:
        NOTREACHED();
        break;
    }
  }
}

#pragma mark - Private

- (void)configureLayout:(UICollectionViewFlowLayout*)layout {
  layout.minimumLineSpacing = kNtpTilesVerticalSpacing;
  layout.minimumInteritemSpacing =
      NtpTilesHorizontalSpacing(self.traitCollection);
  layout.itemSize =
      MostVisitedCellSize(self.traitCollection.preferredContentSizeCategory);
}

@end
