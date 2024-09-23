// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_view_controller.h"

#import <utility>

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_tabs_search_suggested_history_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"
#import "ios/chrome/browser/ui/tab_switcher/tab_grid/suggested_actions/suggested_actions_delegate.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

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
// YES, if all the cells were loaded at least once.
@property(nonatomic, assign) BOOL allCellsLoaded;

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
  self.allCellsLoaded = NO;
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

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  self.allCellsLoaded = YES;
}

#pragma mark - TableViewModel

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:kSectionIdentifierSuggestedActions];
  UIColor* actionsTextColor = [UIColor colorNamed:kBlueColor];
  TableViewImageItem* searchWebItem = [[TableViewImageItem alloc]
      initWithType:ItemTypeSuggestedActionSearchWeb];
  searchWebItem.title =
      l10n_util::GetNSString(IDS_IOS_TABS_SEARCH_SUGGESTED_ACTION_SEARCH_WEB);
  searchWebItem.image = [[UIImage imageNamed:@"suggested_action_web"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  searchWebItem.textColor = actionsTextColor;
  [model addItem:searchWebItem
      toSectionWithIdentifier:kSectionIdentifierSuggestedActions];

  if (!IsTabGroupSyncEnabled()) {
    TableViewImageItem* searchRecentTabsItem = [[TableViewImageItem alloc]
        initWithType:ItemTypeSuggestedActionSearchRecentTabs];
    searchRecentTabsItem.title = l10n_util::GetNSString(
        IDS_IOS_TABS_SEARCH_SUGGESTED_ACTION_SEARCH_RECENT_TABS);
    searchRecentTabsItem.image =
        [[UIImage imageNamed:@"suggested_action_recent_tabs"]
            imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    searchRecentTabsItem.textColor = actionsTextColor;
    [model addItem:searchRecentTabsItem
        toSectionWithIdentifier:kSectionIdentifierSuggestedActions];
  }

  TableViewTabsSearchSuggestedHistoryItem* searchHistoryItem =
      [[TableViewTabsSearchSuggestedHistoryItem alloc]
          initWithType:ItemTypeSuggestedActionSearchHistory];
  searchHistoryItem.textColor = actionsTextColor;
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
  if (itemType == ItemTypeSuggestedActionSearchHistory &&
      self.searchText.length) {
    __weak TableViewTabsSearchSuggestedHistoryCell* weakCell =
        base::apple::ObjCCastStrict<TableViewTabsSearchSuggestedHistoryCell>(
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
  if (!self.allCellsLoaded) {
    // If all the cells have not been loaded at least once, load them so this
    // method can return an accurate height.
    int rowsCount = [self.tableView numberOfRowsInSection:0];
    for (int row = 0; row < rowsCount; row++) {
      [self.tableView cellForRowAtIndexPath:[NSIndexPath indexPathForItem:row
                                                                inSection:0]];
    }
    self.allCellsLoaded = YES;
  }
  return self.tableView.contentSize.height;
}

@end
