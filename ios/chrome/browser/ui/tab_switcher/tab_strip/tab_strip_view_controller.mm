// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_view_controller.h"

#import "base/allocator/partition_allocator/partition_alloc.h"
#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/numerics/safe_conversions.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_cell.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_mediator.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_strip/tab_strip_view_layout.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_switcher_item.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_utils.h"
#import "ios/chrome/common/button_configuration_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/web/public/web_state_id.h"

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

const CGFloat kSymbolSize = 18;

}  // namespace

@interface TabStripViewController () <TabStripCellDelegate>

@property(nonatomic, strong) UIButton* buttonNewTab;
// The local model backing the collection view.
@property(nonatomic, strong) NSMutableArray<TabSwitcherItem*>* items;
// Identifier of the selected item. This value is disregarded if `self.items` is
// empty.
@property(nonatomic, assign) web::WebStateID selectedItemID;
// Index of the selected item in `items`.
@property(nonatomic, readonly) NSUInteger selectedIndex;
// Constraints that are used when the button needs to be kept next to the last
// cell.
@property NSLayoutConstraint* lastCellConstraint;

@end

@implementation TabStripViewController

@synthesize isOffTheRecord = _isOffTheRecord;

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
  [self orderBySelectedTab];

  self.buttonNewTab = [[UIButton alloc] init];
  self.buttonNewTab.translatesAutoresizingMaskIntoConstraints = NO;
  UIImage* buttonNewTabImage =
      DefaultSymbolWithPointSize(kPlusSymbol, kSymbolSize);

  if (IsUIButtonConfigurationEnabled()) {
    UIButtonConfiguration* buttonConfiguration =
        [UIButtonConfiguration plainButtonConfiguration];
    buttonConfiguration.contentInsets = NSDirectionalEdgeInsetsMake(
        0, kNewTabButtonLeadingImageInset, kNewTabButtonBottomImageInset, 0);
    buttonConfiguration.image = buttonNewTabImage;
    buttonConfiguration.baseForegroundColor =
        [UIColor colorNamed:kGrey500Color];
    self.buttonNewTab.configuration = buttonConfiguration;
    self.buttonNewTab.configurationUpdateHandler = ^(UIButton* incomingButton) {
      UIButtonConfiguration* updatedConfig = incomingButton.configuration;
      switch (incomingButton.state) {
        case UIControlStateHighlighted: {
          updatedConfig.baseForegroundColor =
              [UIColor colorNamed:kGrey900Color];
          break;
        }
        case UIControlStateNormal:
          updatedConfig.baseForegroundColor =
              [UIColor colorNamed:kGrey500Color];
          break;
        default:
          break;
      }
      incomingButton.configuration = updatedConfig;
    };
  } else {
    self.buttonNewTab.imageView.contentMode = UIViewContentModeCenter;
    [self.buttonNewTab setImage:buttonNewTabImage
                       forState:UIControlStateNormal];
    [self.buttonNewTab.imageView
        setTintColor:[UIColor colorNamed:kGrey500Color]];
    UIEdgeInsets imageInsets = UIEdgeInsetsMake(
        0, kNewTabButtonLeadingImageInset, kNewTabButtonBottomImageInset, 0);
    SetImageEdgeInsets(self.buttonNewTab, imageInsets);
  }

  [self.view addSubview:self.buttonNewTab];
  [NSLayoutConstraint activateConstraints:@[
    [self.buttonNewTab.trailingAnchor
        constraintLessThanOrEqualToAnchor:self.view.trailingAnchor],
    [self.buttonNewTab.topAnchor constraintEqualToAnchor:self.view.topAnchor],
    [self.buttonNewTab.heightAnchor
        constraintEqualToAnchor:self.view.heightAnchor],
    [self.buttonNewTab.widthAnchor
        constraintEqualToConstant:kNewTabButtonWidth],
  ]];

  self.lastCellConstraint = [self.buttonNewTab.leadingAnchor
      constraintEqualToAnchor:self.view.trailingAnchor];
  self.lastCellConstraint.priority = UILayoutPriorityDefaultHigh;
  self.lastCellConstraint.active = YES;

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
  TabStripCell* cell = base::apple::ObjCCastStrict<TabStripCell>([collectionView
      dequeueReusableCellWithReuseIdentifier:kReuseIdentifier
                                forIndexPath:indexPath]);

  [self configureCell:cell withItem:item];
  cell.selected = cell.itemIdentifier == self.selectedItemID;
  return cell;
}

- (void)collectionView:(UICollectionView*)collectionView
       willDisplayCell:(UICollectionViewCell*)cell
    forItemAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger numberOfItems = [self collectionView:collectionView
                          numberOfItemsInSection:indexPath.section];
  if (indexPath.row == numberOfItems - 1) {
    // Adding a constant to the button's contraints to keep it next to the last
    // cell using the given collectionView and cell.
    CGFloat newConstant = -(collectionView.bounds.size.width -
                            (cell.frame.origin.x + cell.frame.size.width));
    self.lastCellConstraint.constant = newConstant;
  }
}

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  // Adding a constant to the button's contraints to keep it next to the last
  // cell while scrolling with given scrollView.
  CGFloat newConstant = scrollView.contentSize.width -
                        scrollView.contentOffset.x -
                        scrollView.bounds.size.width;
  self.lastCellConstraint.constant = newConstant;
}

#pragma mark - TabStripConsumer

- (void)populateItems:(NSArray<TabSwitcherItem*>*)items
       selectedItemID:(web::WebStateID)selectedItemID {
  // Note: Keep as a DCHECK, as this can be costly.
  DCHECK(!HasDuplicateIdentifiers(items));

  self.items = [items mutableCopy];
  self.selectedItemID = selectedItemID;
  [self.collectionView reloadData];
  [self.collectionView
      selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                   animated:YES
             scrollPosition:UICollectionViewScrollPositionNone];
}

- (void)replaceItemID:(web::WebStateID)itemID withItem:(TabSwitcherItem*)item {
  if ([self indexOfItemWithID:itemID] == NSNotFound) {
    return;
  }
  // Consistency check: `item`'s ID is either `itemID` or not in `items`.
  DCHECK(item.identifier == itemID ||
         [self indexOfItemWithID:item.identifier] == NSNotFound);
  NSUInteger index = [self indexOfItemWithID:itemID];
  self.items[index] = item;
  TabStripCell* cell = (TabStripCell*)[self.collectionView
      cellForItemAtIndexPath:CreateIndexPath(index)];
  // `cell` may be nil if it is scrolled offscreen.
  if (cell)
    [self configureCell:cell withItem:item];
}

- (void)selectItemWithID:(web::WebStateID)selectedItemID {
  if (self.selectedItemID == selectedItemID) {
    return;
  }

  [self.collectionView
      deselectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                     animated:YES];
  UICollectionViewCell* cell = [self.collectionView
      cellForItemAtIndexPath:CreateIndexPath(self.selectedIndex)];
  cell.selected = NO;

  self.selectedItemID = selectedItemID;

  [self.collectionView
      selectItemAtIndexPath:CreateIndexPath(self.selectedIndex)
                   animated:YES
             scrollPosition:UICollectionViewScrollPositionNone];
  cell = [self.collectionView
      cellForItemAtIndexPath:CreateIndexPath(self.selectedIndex)];
  cell.selected = YES;
  [self orderBySelectedTab];
}

#pragma mark - Private

// Configures `cell`'s title synchronously, and favicon asynchronously with
// information from `item`. Updates the `cell`'s theme to this view controller's
// theme.
- (void)configureCell:(TabStripCell*)cell withItem:(TabSwitcherItem*)item {
  if (item) {
    cell.delegate = self;
    cell.itemIdentifier = item.identifier;
    cell.titleLabel.text = item.title;
    [item fetchFavicon:^(TabSwitcherItem* innerItem, UIImage* icon) {
      // Only update the icon if the cell is not
      // already reused for another item.
      if (cell.itemIdentifier == innerItem.identifier) {
        cell.faviconView.image = icon;
      }
    }];
    cell.selected = cell.itemIdentifier == self.selectedItemID;
  }
}

// Returns the index in `self.items` of the first item whose identifier is
// `identifier`.
- (NSUInteger)indexOfItemWithID:(web::WebStateID)identifier {
  auto selectedTest =
      ^BOOL(TabSwitcherItem* item, NSUInteger index, BOOL* stop) {
        return item.identifier == identifier;
      };
  return [self.items indexOfObjectPassingTest:selectedTest];
}

- (void)sendNewTabCommand {
  [self.delegate addNewItem];
}

// Puts the tabs into the right positioning, i.e. the selected tab is in the
// foreground, and the farther tabs are from the selected tab, the lower
// zPositioning is.
- (void)orderBySelectedTab {
  for (TabSwitcherItem* item in self.items) {
    NSUInteger index = [self indexOfItemWithID:item.identifier];
    UICollectionViewCell* cell =
        [self.collectionView cellForItemAtIndexPath:CreateIndexPath(index)];
    // Calculating how "far" is the current tab from the selected one.
    cell.layer.zPosition =
        (int)(self.selectedIndex - abs(int(self.selectedIndex - index)));
  }
}

#pragma mark - Private properties

- (NSUInteger)selectedIndex {
  return [self indexOfItemWithID:self.selectedItemID];
}

#pragma mark - UICollectionViewDelegate

- (void)collectionView:(UICollectionView*)collectionView
    didSelectItemAtIndexPath:(NSIndexPath*)indexPath {
  int index = indexPath.item;
  [self.delegate selectTab:index];
  [self orderBySelectedTab];
}

#pragma mark - TabStripCellDelegate

- (void)closeButtonTappedForCell:(TabStripCell*)cell {
  [self.delegate closeItemWithID:cell.itemIdentifier];
}

@end
