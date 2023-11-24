// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/search_engine_choice/search_engine_choice_table/search_engine_choice_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/favicon/favicon_loader.h"
#import "ios/chrome/browser/shared/ui/list_model/list_model.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_url_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
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

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // With no header on first appearance, UITableView adds a 35 points space at
  // the beginning of the table view. This space remains after this table view
  // reloads with headers. Setting a small tableHeaderView avoids this.
  self.tableView.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];

  self.tableView.separatorInset =
      UIEdgeInsetsMake(0, kTableViewSeparatorLeadingInset, 0, 0);
  self.tableView.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  self.styler.cellBackgroundColor =
      [UIColor colorNamed:kTertiaryBackgroundColor];
  self.tableView.separatorColor = [UIColor colorNamed:kGrey300Color];

  [self loadModel];
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
      UIImageView* circleView = CreateEmptyCircle();
      [cell setAccessoryView:circleView];
    }
  }

  // Show the checkmark on the new default engine.
  SnippetSearchEngineItem* newDefaultEngine =
      base::apple::ObjCCastStrict<SnippetSearchEngineItem>(
          [model itemAtIndexPath:indexPath]);
  newDefaultEngine.accessoryType = UITableViewCellAccessoryCheckmark;
  UITableViewCell* cell = [tableView cellForRowAtIndexPath:indexPath];

  cell.accessoryType = UITableViewCellAccessoryCheckmark;
  UIImageView* checkedCircleView = CreateCheckedCircle();
  [cell setAccessoryView:checkedCircleView];

  [self.delegate selectSearchEngineAtRow:_selectedRow];
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
  TableViewURLCell* urlCell =
      base::apple::ObjCCastStrict<TableViewURLCell>(cell);

  NSString* itemIdentifier = engineItem.uniqueIdentifier;
  _faviconLoader->FaviconForPageUrl(
      engineItem.URL, kDesiredMediumFaviconSizePt, kMinFaviconSizePt,
      /*fallback_to_google_server=*/YES, ^(FaviconAttributes* attributes) {
        // Only set favicon if the cell hasn't been reused.
        if ([urlCell.cellUniqueIdentifier isEqualToString:itemIdentifier]) {
          [urlCell.faviconView configureWithAttributes:attributes];
        }
      });
  [urlCell
      setFaviconContainerBackgroundColor:[UIColor colorNamed:kBackgroundColor]];

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

@end
