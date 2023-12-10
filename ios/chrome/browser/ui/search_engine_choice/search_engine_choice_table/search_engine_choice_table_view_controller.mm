// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/search_engine_choice_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/cells/snippet_search_engine_cell.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/cells/snippet_search_engine_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "url/gurl.h"

namespace {

constexpr CGFloat kTableViewSeparatorLeadingInset = 56;

}  // namespace

@implementation SearchEngineChoiceTableViewController {
  // Search engine item chosen by the user.
  SnippetSearchEngineItem* _chosenSearchEngineItem;
}

@synthesize searchEngines = _searchEngines;

- (void)scrollToBottom {
  TableViewModel* model = self.tableViewModel;
  NSInteger lastSectionIndex = [model numberOfSections] - 1;
  NSInteger lastRowIndex = [model numberOfItemsInSection:lastSectionIndex] - 1;
  NSIndexPath* lastRowIndexPath =
      [NSIndexPath indexPathForRow:lastRowIndex inSection:lastSectionIndex];
  [self.tableView scrollToRowAtIndexPath:lastRowIndexPath
                        atScrollPosition:UITableViewScrollPositionBottom
                                animated:YES];
  // Make sure the delegate receives the bottom reach event, so if the scroll
  // as an offset of one pixel, the delegate will be called.
  [self bottomReached];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  UITableView* tableView = self.tableView;
  tableView.accessibilityIdentifier = kSearchEngineTableViewIdentifier;
  // With no header on first appearance, UITableView adds a 35 points space at
  // the beginning of the table view. This space remains after this table view
  // reloads with headers. Setting a small tableHeaderView avoids this.
  tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  tableView.separatorInset =
      UIEdgeInsetsMake(0, kTableViewSeparatorLeadingInset, 0, 0);
  tableView.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.styler.cellBackgroundColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  tableView.separatorColor = [UIColor colorNamed:kGrey300Color];

  [self loadModel];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self updateDidReachBottomFlag];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;

  // Add search engines.
  if (_searchEngines.count > 0) {
    [model addSectionWithIdentifier:kSectionIdentifierEnumZero];

    for (SnippetSearchEngineItem* item : _searchEngines) {
      [model addItem:item toSectionWithIdentifier:kSectionIdentifierEnumZero];
    }
  }
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  // Deselects the cell, to clear the background color.
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
  NSInteger selectedRow = indexPath.row;
  TableViewModel* model = self.tableViewModel;
  // Show the checkmark on the new default engine.
  SnippetSearchEngineItem* newDefaultEngine =
      base::apple::ObjCCastStrict<SnippetSearchEngineItem>(
          [model itemAtIndexPath:indexPath]);
  if (newDefaultEngine == _chosenSearchEngineItem) {
    return;
  }
  SnippetSearchEngineItem* previousDefaultSearchEngine =
      _chosenSearchEngineItem;
  previousDefaultSearchEngine.checked = NO;
  _chosenSearchEngineItem = newDefaultEngine;
  _chosenSearchEngineItem.checked = YES;
  NSArray<SnippetSearchEngineItem*>* items = nil;
  if (previousDefaultSearchEngine) {
    items = @[ previousDefaultSearchEngine, newDefaultEngine ];
  } else {
    items = @[ newDefaultEngine ];
  }
  [self reconfigureCellsForItems:items];
  CHECK(self.delegate);
  [self.delegate selectSearchEngineAtRow:selectedRow];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self updateDidReachBottomFlag];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  SnippetSearchEngineItem* engineItem =
      base::apple::ObjCCastStrict<SnippetSearchEngineItem>(item);
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  SnippetSearchEngineCell* URLCell =
      base::apple::ObjCCastStrict<SnippetSearchEngineCell>(cell);
  __weak __typeof(self) weakSelf = self;
  URLCell.chevronToggledBlock = ^(SnippetState snippet_state) {
    engineItem.snippetState = snippet_state;
    [weakSelf.tableView reconfigureRowsAtIndexPaths:@[ indexPath ]];
  };
  return cell;
}

#pragma mark - SearchEngineChoiceTableConsumer

- (void)reloadData {
  [self loadModel];
  [self.tableView reloadData];
}

- (void)faviconAttributesUpdatedForItem:(SnippetSearchEngineItem*)item {
  [self reconfigureCellsForItems:@[ item ]];
}

#pragma mark - Private

// Checks if the the bottom has been reached.
- (void)updateDidReachBottomFlag {
  if (self.didReachBottom) {
    // Don't update the value if the bottom was reached at least once.
    return;
  }
  CGFloat scrollPosition =
      self.tableView.contentOffset.y + self.tableView.frame.size.height;
  CGFloat scrollLimit =
      self.tableView.contentSize.height + self.tableView.contentInset.bottom;
  if (scrollPosition >= scrollLimit) {
    [self bottomReached];
  }
}

// Updates `-SearchEngineChoiceTableViewController.didReachBottom`, and calls
// the delegate.
- (void)bottomReached {
  self.didReachBottom = YES;
  [self.delegate didReachBottom];
}

@end
