// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_recent_fills_view_controller.h"

#import "base/apple/foundation_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_search_item.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Size of the cell symbol.
constexpr CGFloat kSymbolSize = 24.0;

// Section identifiers for the recent fills table view.
enum SectionIdentifier {
  kRecentFillsSection = 0,
};

// Item types for the recent fills table view.
enum ItemType {
  kRecentFillsItemType = 0,
};

}  // namespace

@implementation AtMemoryRecentFillsViewController {
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

  RegisterTableViewHeaderFooter<TableViewTextHeaderFooterView>(self.tableView);

  [self loadModel];
}

- (void)setItems:(NSArray<AtMemorySearchItem*>*)items {
  _items = [items copy];
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
    @(kRecentFillsSection),
  ]];

  NSMutableArray<TableViewItem*>* items = [[NSMutableArray alloc] init];
  for (NSUInteger index = 0; index < self.items.count; index++) {
    AtMemorySearchItem* searchItem = self.items[index];
    TableViewInfoButtonItem* item =
        [self recentFillsItemFromSearchItem:searchItem index:index];
    [items addObject:item];
  }

  [snapshot appendItemsWithIdentifiers:items
             intoSectionWithIdentifier:@(kRecentFillsSection)];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  if (section == kRecentFillsSection) {
    TableViewTextHeaderFooterView* header =
        DequeueTableViewHeaderFooter<TableViewTextHeaderFooterView>(tableView);
    [header setTitle:l10n_util::GetNSString(
                         IDS_IOS_AUTOFILL_AI_PREVIOUSLY_FILLED_SECTION_TITLE)];
    return header;
  }
  return nil;
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  TableViewItem* item = [_dataSource itemIdentifierForIndexPath:indexPath];
  TableViewInfoButtonItem* infoItem =
      base::apple::ObjCCastStrict<TableViewInfoButtonItem>(item);
  if (infoItem) {
    [self.delegate recentFillsViewController:self
                            didSelectContent:infoItem.text];
  }
}

#pragma mark - Action

- (void)didTapInfoButton:(UIButton*)button {
  NSUInteger index = button.tag;
  if (index < self.items.count) {
    AtMemorySearchItem* searchItem = self.items[index];
    [self.delegate recentFillsViewController:self didTapInfoForItem:searchItem];
  }
}

#pragma mark - Private

// Creates a TableViewInfoButtonItem from `searchItem` at `index`.
// TODO(crbug.com/532090671): to be updated when search item is finalized.
- (TableViewInfoButtonItem*)recentFillsItemFromSearchItem:
                                (AtMemorySearchItem*)searchItem
                                                    index:(NSUInteger)index {
  TableViewInfoButtonItem* item =
      [[TableViewInfoButtonItem alloc] initWithType:kRecentFillsItemType];
  item.text = searchItem.text;
  item.detailText = searchItem.detailText;
  item.iconImage = CustomSymbolWithPointSize(kTextSparkSymbol, kSymbolSize);
  item.iconTintColor = [UIColor colorNamed:kTextPrimaryColor];
  item.target = self;
  item.selector = @selector(didTapInfoButton:);
  item.tag = index;
  return item;
}

@end
