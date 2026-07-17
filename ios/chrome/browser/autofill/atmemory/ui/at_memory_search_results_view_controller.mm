// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_results_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_result_item.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Size of the cell symbol.
constexpr CGFloat kSymbolSize = 24.0;

// Section identifiers for the search results table view.
enum SectionIdentifier {
  kResultsSection = 0,
};

// Item types for the search results table view.
enum ItemType {
  kResultsItemType = 0,
};

}  // namespace

@implementation AtMemorySearchResultsViewController {
  // The data source for the table view.
  UITableViewDiffableDataSource<NSNumber*, TableViewItem*>* _dataSource;
}

- (instancetype)initWithStyle:(UITableViewStyle)style {
  self = [super initWithStyle:style];
  if (self) {
    self.title =
        l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_FIND_AND_FILL_TITLE);
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.allowsSelection = YES;
  [self loadModel];
}

- (void)setResults:(NSArray<AtMemorySearchResultItem*>*)results {
  _results = [results copy];
  if (self.isViewLoaded) {
    [self loadModel];
  }
}

- (void)loadModel {
  if (!_dataSource) {
    _dataSource = [[UITableViewDiffableDataSource alloc]
        initWithTableView:self.tableView
             cellProvider:^UITableViewCell*(UITableView* tableView,
                                            NSIndexPath* indexPath,
                                            TableViewItem* item) {
               LegacyTableViewCell* cell = [item cellForTableView:tableView];
               [item configureCell:cell];
               return cell;
             }];
  }

  NSDiffableDataSourceSnapshot<NSNumber*, TableViewItem*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[
    @(kResultsSection),
  ]];

  NSMutableArray<TableViewItem*>* items = [[NSMutableArray alloc] init];
  for (AtMemorySearchResultItem* resultItem in self.results) {
    [items addObject:[self tableViewItemFromResultItem:resultItem]];
  }

  [snapshot appendItemsWithIdentifiers:items
             intoSectionWithIdentifier:@(kResultsSection)];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  TableViewItem* item = [_dataSource itemIdentifierForIndexPath:indexPath];
  TableViewInfoButtonItem* infoItem =
      base::apple::ObjCCastStrict<TableViewInfoButtonItem>(item);
  if (infoItem) {
    [self.delegate searchResultsViewController:self
                              didSelectContent:infoItem.text];
  }
}

#pragma mark - Action

- (void)didTapInfoButton:(UIButton*)button {
  [self.delegate searchResultsViewControllerDidTapInfo:self];
}

#pragma mark - Private

// Creates and returns a table view item configured from a search result item.
- (TableViewItem*)tableViewItemFromResultItem:
    (AtMemorySearchResultItem*)resultItem {
  TableViewInfoButtonItem* item =
      [[TableViewInfoButtonItem alloc] initWithType:kResultsItemType];
  item.text = resultItem.fillingText;
  item.detailText = resultItem.subtitle;
  NSString* iconSymbolName = resultItem.iconSymbolName ?: kTextSparkSymbol;
  item.iconImage = CustomSymbolWithPointSize(iconSymbolName, kSymbolSize);
  item.iconTintColor = [UIColor colorNamed:kTextPrimaryColor];
  item.target = self;
  item.selector = @selector(didTapInfoButton:);
  return item;
}

@end
