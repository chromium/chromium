// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_view_controller.h"

#import <utility>

#include "base/check_op.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_delegate.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_tabs_search_suggested_history_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kEstimatedRowMaxHeight = 150;
NSString* const kSuggestedActionsViewControllerAccessibilityIdentifier =
    @"search_suggestions_view_controller";
const int kSectionIdentifierSuggestedActions = kSectionIdentifierEnumZero + 1;

}  // namespace

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSuggestedActionSearchRecentTabs = kItemTypeEnumZero,
  ItemTypeSuggestedActionSearchWeb,
  ItemTypeSuggestedActionSearchHistory,
};

@interface SuggestedActionsViewController ()

// Delegate to handle the execution of the suggested actions.
@property(nonatomic, weak) id<SuggestedActionsDelegate>
    suggestedActionsDelegate;

@end

@implementation SuggestedActionsViewController

- (instancetype)initWithDelegate:
    (id<SuggestedActionsViewControllerDelegate>)delegate {
  self = [super initWithStyle:UITableViewStyleGrouped];

  if (self) {
    _delegate = delegate;
    self.styler.tableViewBackgroundColor =
        [UIColor colorNamed:kGridBackgroundColor];
  }
  return self;
}

#pragma mark - ViewController Lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.accessibilityIdentifier =
      kSuggestedActionsViewControllerAccessibilityIdentifier;
  self.tableView.cellLayoutMarginsFollowReadableWidth = YES;
  self.tableView.estimatedRowHeight = kEstimatedRowMaxHeight;
  self.tableView.estimatedSectionHeaderHeight = 0.0;
  self.tableView.rowHeight = UITableViewAutomaticDimension;
  self.tableView.sectionFooterHeight = 0.0;
  self.tableView.alwaysBounceVertical = NO;
  self.tableView.scrollEnabled = NO;
  // The TableView header and footer are set to some default size when they are
  // set to nil, and that will result on empty space on the top and the bottom
  // of the table. To workaround that an empty frame is created and set on both
  // the header and the footer of the TableView.
  CGRect frame = CGRectZero;
  frame.size.height = CGFLOAT_MIN;
  [self.tableView setTableHeaderView:[[UIView alloc] initWithFrame:frame]];
  [self.tableView setTableFooterView:[[UIView alloc] initWithFrame:frame]];

  self.tableView.layer.cornerRadius = kGridCellCornerRadius;
  [self loadModel];
  [self.tableView reloadData];
  [self.tableView layoutIfNeeded];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self loadModel];
  [self.tableView reloadData];
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:kSectionIdentifierSuggestedActions];
  TableViewImageItem* searchWebItem = [[TableViewImageItem alloc]
      initWithType:ItemTypeSuggestedActionSearchWeb];
  searchWebItem.title =
      l10n_util::GetNSString(IDS_IOS_TABS_SEARCH_SUGGESTED_ACTION_SEARCH_WEB);
  searchWebItem.image = [[UIImage imageNamed:@"popup_menu_web"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [model addItem:searchWebItem
      toSectionWithIdentifier:kSectionIdentifierSuggestedActions];

  TableViewImageItem* searchRecentTabsItem = [[TableViewImageItem alloc]
      initWithType:ItemTypeSuggestedActionSearchRecentTabs];
  searchRecentTabsItem.title = l10n_util::GetNSString(
      IDS_IOS_TABS_SEARCH_SUGGESTED_ACTION_SEARCH_RECENT_TABS);
  searchRecentTabsItem.image = [[UIImage imageNamed:@"popup_menu_recent_tabs"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [model addItem:searchRecentTabsItem
      toSectionWithIdentifier:kSectionIdentifierSuggestedActions];

  TableViewTabsSearchSuggestedHistoryItem* searchHistoryItem =
      [[TableViewTabsSearchSuggestedHistoryItem alloc]
          initWithType:ItemTypeSuggestedActionSearchHistory];
  [model addItem:searchHistoryItem
      toSectionWithIdentifier:kSectionIdentifierSuggestedActions];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);

  // Update the history search result count once available.
  if (itemType == ItemTypeSuggestedActionSearchHistory) {
    __weak TableViewTabsSearchSuggestedHistoryCell* weakCell =
        base::mac::ObjCCastStrict<TableViewTabsSearchSuggestedHistoryCell>(
            cell);
    NSString* currentSearchText = self.searchText;
    weakCell.searchTerm = currentSearchText;
    [self.delegate suggestedActionsViewController:self
           fetchHistoryResultsCountWithCompletion:^(size_t resultCount) {
             if ([weakCell.searchTerm isEqualToString:currentSearchText]) {
               [weakCell updateHistoryResultsCount:resultCount];
             }
           }];
  }
  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
  NSInteger itemTypeSelected =
      [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemTypeSelected) {
    case ItemTypeSuggestedActionSearchRecentTabs:
      [self.delegate
          didSelectSearchRecentTabsInSuggestedActionsViewController:self];
      break;
    case ItemTypeSuggestedActionSearchWeb:
      [self.delegate didSelectSearchWebInSuggestedActionsViewController:self];
      break;
    case ItemTypeSuggestedActionSearchHistory:
      [self.delegate
          didSelectSearchHistoryInSuggestedActionsViewController:self];
      break;
  }
}

#pragma mark - Public

- (void)setSearchText:(NSString*)searchText {
  _searchText = searchText;
  [self.tableView reloadData];
}

- (CGFloat)contentHeight {
  return self.tableView.contentSize.height;
}

@end
