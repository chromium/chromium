// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_view_controller.h"

#import "base/allocator/partition_allocator/partition_alloc.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_view_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

static NSString* const kReuseIdentifier = @"TabView";
NSIndexPath* CreateIndexPath(NSInteger index) {
  return [NSIndexPath indexPathForItem:index inSection:0];
}

// The size of the new tab button.
const CGFloat kNewTabButtonWidth = 44;
// Default image insets for the new tab button.
const CGFloat kNewTabButtonLeadingImageInset = -10.0;
const CGFloat kNewTabButtonBottomImageInset = -2.0;

}  // namespace

@interface TabStripViewController () <TabStripCellDelegate>

@property(nonatomic, strong) UIButton* buttonNewTab;
// The local model backing the collection view.
@property(nonatomic, strong) NSMutableArray<TabSwitcherItem*>* items;
// Identifier of the selected item. This value is disregarded if |self.items| is
// empty.
@property(nonatomic, copy) NSString* selectedItemID;
// Index of the selected item in |items|.
@property(nonatomic, readonly) NSUInteger selectedIndex;

@end

@implementation TabStripViewController

- (instancetype)init {
  TabStripViewLayout* layout = [[TabStripViewLayout alloc] init];
  if (self = [super initWithCollectionViewLayout:layout]) {
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.translatesAutoresizingMaskIntoConstraints = NO;
  self.collectionView.alwaysBounceHorizontal = YES;
  [self.collectionView registerClass:[TabStripCell class]
          forCellWithReuseIdentifier:kReuseIdentifier];

  self.buttonNewTab = [[UIButton alloc] init];
  self.buttonNewTab.translatesAutoresizingMaskIntoConstraints = NO;
  self.buttonNewTab.imageView.contentMode = UIViewContentModeCenter;
  UIImage* buttonNewTabImage = [[UIImage imageNamed:@"tabstrip_new_tab"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [self.buttonNewTab setImage:buttonNewTabImage forState:UIControlStateNormal];
  [self.buttonNewTab.imageView setTintColor:[UIColor colorNamed:kGrey500Color]];
  UIEdgeInsets imageInsets = UIEdgeInsetsMake(0, kNewTabButtonLeadingImageInset,
                                              kNewTabButtonBottomImageInset, 0);
  self.buttonNewTab.imageEdgeInsets = imageInsets;
  [self.view addSubview:self.buttonNewTab];
  [NSLayoutConstraint activateConstraints:@[
    [self.buttonNewTab.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor],
    [self.buttonNewTab.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [self.buttonNewTab.heightAnchor
        constraintEqualToAnchor:self.view.heightAnchor],
    [self.buttonNewTab.widthAnchor
        constraintEqualToConstant:kNewTabButtonWidth],
  ]];

  [self.buttonNewTab addTarget:self
                        action:@selector(sendNewTabCommand)
              forControlEvents:UIControlEventTouchUpInside];
}

- (NSInteger)numberOfSectionsInCollectionView:
    (UICollectionView*)collectionView {
  return 1;
}

- (NSInteger)collectionView:(UICollectionView*)collectionView
     numberOfItemsInSection:(NSInteger)section {
  return _items.count;
}

- (UICollectionViewCell*)collectionView:(UICollectionView*)collectionView
                 cellForItemAtIndexPath:(NSIndexPath*)indexPath {
  NSUInteger itemIndex = base::checked_cast<NSUInteger>(indexPath.item);
  if (itemIndex >= self.items.count)
    itemIndex = self.items.count - 1;

  TabSwitcherItem* item = self.items[itemIndex];
  TabStripCell* cell = (TabStripCell*)[collectionView
      dequeueReusableCellWithReuseIdentifier:kReuseIdentifier
                                forIndexPath:indexPath];

  [self configureCell:cell withItem:item];
  return cell;
}

#pragma mark - TabStripConsumer

- (void)populateItems:(NSArray<TabSwitcherItem*>*)items
       selectedItemID:(NSString*)selectedItemID {
#ifndef NDEBUG
  // Consistency check: ensure no IDs are duplicated.
  NSMutableSet<NSString*>* identifiers = [[NSMutableSet alloc] init];
  for (TabSwitcherItem* item in items) {
    [identifiers addObject:item.identifier];
  }
  CHECK_EQ(identifiers.count, items.count);
#endif

  self.items = [items mutableCopy];
  self.selectedItemID = selectedItemID;
  [self.collectionView reloadData];
}

- (void)replaceItemID:(NSString*)itemID withItem:(TabSwitcherItem*)item {
  if ([self indexOfItemWithID:itemID] == NSNotFound)
    return;
  // Consistency check: |item|'s ID is either |itemID| or not in |items|.
  DCHECK([item.identifier isEqualToString:itemID] ||
         [self indexOfItemWithID:item.identifier] == NSNotFound);
  NSUInteger index = [self indexOfItemWithID:itemID];
  self.items[index] = item;
  TabStripCell* cell = (TabStripCell*)[self.collectionView
      cellForItemAtIndexPath:CreateIndexPath(index)];
  // |cell| may be nil if it is scrolled offscreen.
  if (cell)
    [self configureCell:cell withItem:item];
}

#pragma mark - Private

// Configures |cell|'s title synchronously, and favicon asynchronously with
// information from |item|. Updates the |cell|'s theme to this view controller's
// theme.
- (void)configureCell:(TabStripCell*)cell withItem:(TabSwitcherItem*)item {
  if (item) {
    cell.delegate = self;
    cell.itemIdentifier = item.identifier;
    cell.titleLabel.text = item.title;
    NSString* itemIdentifier = item.identifier;
    [self.faviconDataSource
        faviconForIdentifier:itemIdentifier
                  completion:^(UIImage* icon) {
                    // Only update the icon if the cell is not
                    // already reused for another item.
                    if (cell.itemIdentifier == itemIdentifier)
                      cell.faviconView.image = icon;
                  }];
  }
}

// Returns the index in |self.items| of the first item whose identifier is
// |identifier|.
- (NSUInteger)indexOfItemWithID:(NSString*)identifier {
  auto selectedTest =
      ^BOOL(TabSwitcherItem* item, NSUInteger index, BOOL* stop) {
        return [item.identifier isEqualToString:identifier];
      };
  return [self.items indexOfObjectPassingTest:selectedTest];
}

- (void)sendNewTabCommand {
  [self.delegate addNewItem];
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  int index = indexPath.item;
  [self.delegate selectTab:index];
}

#pragma mark - TabStripCellDelegate

- (void)closeButtonTappedForCell:(TabStripCell*)cell {
  [self.delegate closeItemWithID:cell.itemIdentifier];
}

@end
