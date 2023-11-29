// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/search_engine_choice_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_constants.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/cells/snippet_search_engine_cell.h"
#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/cells/snippet_search_engine_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_constants.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "url/gurl.h"

namespace {

const CGFloat kTableViewSeparatorLeadingInset = 56;
// The size of the radio button at the side of each cell.
const CGFloat kRadioButtonSize = 22.;

UIImageView* CreateEmptyCircle() {
  UIImageView* circleView =
      [[UIImageView alloc] initWithImage:DefaultSymbolWithPointSize(
                                             kCircleSymbol, kRadioButtonSize)];
  [circleView setTintColor:[UIColor colorNamed:kGrey700Color]];
  return circleView;
}

UIImageView* CreateCheckedCircle() {
  return [[UIImageView alloc]
      initWithImage:DefaultSymbolWithPointSize(kCheckmarkCircleFillSymbol,
                                               kRadioButtonSize)];
}

}  // namespace

@implementation SearchEngineChoiceTableViewController {
  // FaviconLoader is a keyed service that uses LargeIconService to retrieve
  // favicon images.
  FaviconLoader* _faviconLoader;
  // Index of the selected row, if there is one.
  NSInteger _selectedRow;
}

@synthesize searchEngines = _searchEngines;

- (instancetype)initWithFaviconLoader:(FaviconLoader*)faviconLoader {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    _faviconLoader = faviconLoader;
    _selectedRow = -1;
  }
  return self;
}

- (void)choiceScreenWillDisappear {
  _faviconLoader = nullptr;
}

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
      [UIColor colorNamed:kTertiaryBackgroundColor];
  tableView.separatorColor = [UIColor colorNamed:kGrey300Color];

  [self loadModel];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self updateDidReachBottomFlag];
}

- (void)viewWillDisappear:(BOOL)animated {
  _faviconLoader = nullptr;
  [super viewWillDisappear:animated];
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
  _selectedRow = indexPath.row;
  TableViewModel* model = self.tableViewModel;

  // Iterate through the engines and remove the checkmark from any that have it.
  for (TableViewItem* item in
       [model itemsInSectionWithIdentifier:kSectionIdentifierEnumZero]) {
    SnippetSearchEngineItem* textItem =
        base::apple::ObjCCastStrict<SnippetSearchEngineItem>(item);
    if (textItem.accessoryType == UITableViewCellAccessoryCheckmark) {
      textItem.accessoryType = UITableViewCellAccessoryNone;
      UITableViewCell* cell =
          [tableView cellForRowAtIndexPath:[model indexPathForItem:item]];
      SnippetSearchEngineCell* urlCell =
          base::apple::ObjCCastStrict<SnippetSearchEngineCell>(cell);
      UIImageView* circleView = CreateEmptyCircle();
      [urlCell setAccessoryView:circleView];
      urlCell.snippetLabel.numberOfLines = 1;
    }
  }

  // Show the checkmark on the new default engine.
  SnippetSearchEngineItem* newDefaultEngine =
      base::apple::ObjCCastStrict<SnippetSearchEngineItem>(
          [model itemAtIndexPath:indexPath]);
  newDefaultEngine.accessoryType = UITableViewCellAccessoryCheckmark;
  UITableViewCell* cell = [tableView cellForRowAtIndexPath:indexPath];
  SnippetSearchEngineCell* urlCell =
      base::apple::ObjCCastStrict<SnippetSearchEngineCell>(cell);

  cell.accessoryType = UITableViewCellAccessoryCheckmark;
  UIImageView* checkedCircleView = CreateCheckedCircle();
  [urlCell setAccessoryView:checkedCircleView];
  urlCell.snippetLabel.numberOfLines = 0;

  CHECK(self.delegate);
  [self.delegate selectSearchEngineAtRow:_selectedRow];
  [self.tableView beginUpdates];
  [self.tableView endUpdates];
}

#pragma mark - UIScrollViewDelegate

- (void)scrollViewDidScroll:(UIScrollView*)scrollView {
  [self updateDidReachBottomFlag];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  // If the view controller has already been dismissed, do not load anything.
  if (!_faviconLoader) {
    return cell;
  }

  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  SnippetSearchEngineItem* engineItem =
      base::apple::ObjCCastStrict<SnippetSearchEngineItem>(item);
  SnippetSearchEngineCell* urlCell =
      base::apple::ObjCCastStrict<SnippetSearchEngineCell>(cell);

  NSString* itemIdentifier = engineItem.uniqueIdentifier;
  _faviconLoader->FaviconForPageUrl(
      engineItem.URL, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/YES, ^(FaviconAttributes* attributes) {
        // Only set favicon if the cell hasn't been reused.
        if ([urlCell.cellUniqueIdentifier isEqualToString:itemIdentifier]) {
          [urlCell.faviconView configureWithAttributes:attributes];
        }
      });

  UIImageView* circleView;
  if (_selectedRow >= 0 && indexPath.row == _selectedRow) {
    circleView = CreateCheckedCircle();
  } else {
    circleView = CreateEmptyCircle();
  }
  [urlCell setAccessoryView:circleView];
  return cell;
}

#pragma mark - SearchEngineChoiceTableConsumer

- (void)reloadData {
  [self loadModel];
  [self.tableView reloadData];
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
