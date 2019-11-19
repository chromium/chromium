// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/payment_request_picker_view_controller.h"

#include "base/logging.h"
#import "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/material_components/chrome_app_bar_view_controller.h"
#import "ios/chrome/browser/ui/material_components/utils.h"
#import "ios/chrome/browser/ui/payments/payment_request_picker_row.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"
#include "third_party/libaddressinput/messages.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kPaymentRequestPickerRowAccessibilityID =
    @"kPaymentRequestPickerRowAccessibilityID";
NSString* const kPaymentRequestPickerViewControllerAccessibilityID =
    @"kPaymentRequestPickerViewControllerAccessibilityID";
NSString* const kPaymentRequestPickerSearchBarAccessibilityID =
    @"kPaymentRequestPickerSearchBarAccessibilityID";

@interface PaymentRequestPickerViewController ()<UISearchResultsUpdating>

// Search controller that contains search bar.
@property(nonatomic, strong) UISearchController* searchController;

// Full data set displayed when tableView is not filtered.
@property(nonatomic, strong) NSArray<PickerRow*>* allRows;

// Displayed rows in the tableView.
@property(nonatomic, strong) NSArray<PickerRow*>* displayedRows;

// Selected row.
@property(nonatomic, strong) PickerRow* selectedRow;

@property(nonatomic, strong)
    NSDictionary<NSString*, NSArray<PickerRow*>*>* sectionTitleToSectionRowsMap;

@end

@implementation PaymentRequestPickerViewController

@synthesize appBarViewController = _appBarViewController;
@synthesize searchController = _searchController;
@synthesize allRows = _allRows;
@synthesize displayedRows = _displayedRows;
@synthesize selectedRow = _selectedRow;
@synthesize sectionTitleToSectionRowsMap = _sectionTitleToSectionRowsMap;
@synthesize delegate = _delegate;

- (instancetype)initWithRows:(NSArray<PickerRow*>*)rows
                    selected:(PickerRow*)selectedRow {
  self = [super initWithStyle:UITableViewStylePlain];
  if (self) {
    self.title =
        l10n_util::GetNSString(IDS_LIBADDRESSINPUT_COUNTRY_OR_REGION_LABEL);
    self.allRows = [rows sortedArrayUsingComparator:^NSComparisonResult(
                             PickerRow* row1, PickerRow* row2) {
      return [row1.label localizedCaseInsensitiveCompare:row2.label];
    }];
    self.selectedRow = selectedRow;
    // Default to displaying all the rows.
    self.displayedRows = self.allRows;

    _appBarViewController = [[ChromeAppBarViewController alloc] init];

    // Set up leading (back) button.
    UIBarButtonItem* backButton =
        [ChromeIcon templateBarButtonItemWithImage:[ChromeIcon backIcon]
                                            target:self
                                            action:@selector(onBack)];
    self.appBarViewController.navigationBar.backItem = backButton;
  }
  return self;
}

- (void)setDisplayedRows:(NSArray<PickerRow*>*)displayedRows {
  _displayedRows = displayedRows;

  // Update the mapping from section titles to rows in that section, for
  // currently displayed rows.
  [self updateSectionTitleToSectionRowsMap];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.delegate = self;

  self.tableView.estimatedRowHeight = MDCCellDefaultOneLineHeight;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.accessibilityIdentifier =
      kPaymentRequestPickerViewControllerAccessibilityID;

  self.tableView.sectionIndexBackgroundColor = [UIColor clearColor];
  self.tableView.sectionIndexTrackingBackgroundColor = [UIColor clearColor];

  self.searchController =
      [[UISearchController alloc] initWithSearchResultsController:nil];
  self.searchController.searchResultsUpdater = self;
  self.searchController.obscuresBackgroundDuringPresentation = NO;
  self.searchController.hidesNavigationBarDuringPresentation = NO;
  self.searchController.searchBar.accessibilityIdentifier =
      kPaymentRequestPickerSearchBarAccessibilityID;
  self.tableView.tableHeaderView = self.searchController.searchBar;

  // Presentation of searchController will walk up the view controller hierarchy
  // until it finds the root view controller or one that defines a presentation
  // context. Make this class the presentation context so that the search
  // controller does not present on top of the navigation controller.
  self.definesPresentationContext = YES;

  [self addChildViewController:self.appBarViewController];
  ConfigureAppBarViewControllerWithCardStyle(self.appBarViewController);
  self.appBarViewController.headerView.trackingScrollView = self.tableView;
  // Match the width of the parent view.
  CGRect frame = self.appBarViewController.view.frame;
  frame.origin.x = 0;
  frame.size.width =
      self.appBarViewController.parentViewController.view.bounds.size.width;
  self.appBarViewController.view.frame = frame;
  [self.view addSubview:self.appBarViewController.view];
  [self.appBarViewController didMoveToParentViewController:self];
}

- (UIViewController*)childViewControllerForStatusBarHidden {
  return self.appBarViewController;
}

- (UIViewController*)childViewControllerForStatusBarStyle {
  return self.appBarViewController;
}

#pragma mark - UITableViewDataSource

- (NSArray<NSString*>*)sectionIndexTitlesForTableView:(UITableView*)tableView {
  return [self sectionTitles];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return [[self sectionTitles] count];
}

- (NSString*)tableView:(UITableView*)tableView
    titleForHeaderInSection:(NSInteger)section {
  return [[self sectionTitles] objectAtIndex:section];
}

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return [[self rowsInSection:section] count];
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [tableView dequeueReusableCellWithIdentifier:@"cell"];
  if (!cell) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleSubtitle
                                  reuseIdentifier:@"cell"];
    cell.isAccessibilityElement = YES;
    cell.accessibilityIdentifier = kPaymentRequestPickerRowAccessibilityID;
  }
  PickerRow* row =
      [[self rowsInSection:indexPath.section] objectAtIndex:indexPath.row];
  cell.textLabel.text = row.label;
  cell.accessoryType = (row == self.selectedRow)
                           ? UITableViewCellAccessoryCheckmark
                           : UITableViewCellAccessoryNone;
  if (row == self.selectedRow)
    cell.accessibilityTraits |= UIAccessibilityTraitSelected;
  else
    cell.accessibilityTraits &= ~UIAccessibilityTraitSelected;

  return cell;
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  MDCFlexibleHeaderView* headerView = self.appBarViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView trackingScrollViewDidScroll];
  }
}

- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView {
  MDCFlexibleHeaderView* headerView = self.appBarViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView trackingScrollViewDidEndDecelerating];
  }
}

- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate {
  MDCFlexibleHeaderView* headerView = self.appBarViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView trackingScrollViewDidEndDraggingWillDecelerate:decelerate];
  }
}

- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset {
  MDCFlexibleHeaderView* headerView = self.appBarViewController.headerView;
  if (scrollView == headerView.trackingScrollView) {
    [headerView
        trackingScrollViewWillEndDraggingWithVelocity:velocity
                                  targetContentOffset:targetContentOffset];
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  if (self.selectedRow) {
    NSIndexPath* oldSelectedIndexPath = [self indexPathForRow:self.selectedRow];
    self.selectedRow = nil;

    // Update the previously selected row if it is displaying.
    if (oldSelectedIndexPath) {
      UITableViewCell* cell =
          [self.tableView cellForRowAtIndexPath:oldSelectedIndexPath];
      cell.accessoryType = UITableViewCellAccessoryNone;
      cell.accessibilityTraits &= ~UIAccessibilityTraitSelected;
    }
  }

  self.selectedRow =
      [[self rowsInSection:indexPath.section] objectAtIndex:indexPath.row];

  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  cell.accessoryType = UITableViewCellAccessoryCheckmark;
  cell.accessibilityTraits |= UIAccessibilityTraitSelected;

  [_delegate paymentRequestPickerViewController:self
                                   didSelectRow:self.selectedRow];
}

#pragma mark - UISearchResultsUpdating

- (void)updateSearchResultsForSearchController:
    (UISearchController*)searchController {
  NSString* searchText = searchController.searchBar.text;

  // Filter |allRows| for |searchText| and reload the tableView. If |searchText|
  // is empty, tableView will be loaded with |allRows|.
  if (searchText.length != 0) {
    // The search is case-insensitive and ignores diacritics.
    NSPredicate* predicate =
        [NSPredicate predicateWithFormat:@"label CONTAINS[cd] %@", searchText];
    self.displayedRows = [self.allRows filteredArrayUsingPredicate:predicate];
  } else {
    self.displayedRows = self.allRows;
  }

  [self.tableView reloadData];
}

#pragma mark - Private

- (void)onBack {
  [self.delegate paymentRequestPickerViewControllerDidFinish:self];
}
// Creates a mapping from section titles to rows in that section, for currently
// displaying rows, and updates |sectionTitleToSectionRowsMap|.
- (void)updateSectionTitleToSectionRowsMap {
  NSMutableDictionary<NSString*, NSArray<PickerRow*>*>*
      sectionTitleToSectionRowsMap = [[NSMutableDictionary alloc] init];

  for (PickerRow* row in self.displayedRows) {
    NSString* sectionTitle = [self sectionTitleForRow:row];
    NSMutableArray<PickerRow*>* sectionRows =
        base::mac::ObjCCastStrict<NSMutableArray<PickerRow*>>(
            sectionTitleToSectionRowsMap[sectionTitle]);
    if (!sectionRows)
      sectionRows = [[NSMutableArray alloc] init];
    [sectionRows addObject:row];
    [sectionTitleToSectionRowsMap setObject:sectionRows forKey:sectionTitle];
  }

  self.sectionTitleToSectionRowsMap = sectionTitleToSectionRowsMap;
}

// Returns the indexPath for |row| by calculating its section and its index
// within the section. Returns nil if the row is not currently displaying.
- (NSIndexPath*)indexPathForRow:(PickerRow*)row {
  NSString* sectionTitle = [self sectionTitleForRow:row];

  NSInteger section = [[self sectionTitles] indexOfObject:sectionTitle];
  if (section == NSNotFound)
    return nil;

  NSInteger indexInSection =
      [self.sectionTitleToSectionRowsMap[sectionTitle] indexOfObject:row];
  if (indexInSection == NSNotFound)
    return nil;

  return [NSIndexPath indexPathForRow:indexInSection inSection:section];
}

// Returns the titles for the displayed sections in the tableView.
- (NSArray<NSString*>*)sectionTitles {
  return [[self.sectionTitleToSectionRowsMap allKeys]
      sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)];
}

// Returns the displayed rows in the given section.
- (NSArray<PickerRow*>*)rowsInSection:(NSInteger)section {
  NSArray<NSString*>* sectionTitles = [self sectionTitles];
  DCHECK(section >= 0 && section < static_cast<NSInteger>(sectionTitles.count));

  NSString* sectionTitle = [sectionTitles objectAtIndex:section];

  return self.sectionTitleToSectionRowsMap[sectionTitle];
}

// Returns the title for the section the given row gets added to. The section
// title for a row is the capitalized first letter of the label for that row.
- (NSString*)sectionTitleForRow:(PickerRow*)row {
  return [[row.label substringToIndex:1] uppercaseString];
}

- (NSString*)description {
  return kPaymentRequestPickerViewControllerAccessibilityID;
}

@end
