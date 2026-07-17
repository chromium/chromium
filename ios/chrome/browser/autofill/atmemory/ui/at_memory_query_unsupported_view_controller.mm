// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_query_unsupported_view_controller.h"

#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/atmemory/utils/atmemory_ui_util.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Section identifiers for the query unsupported table view.
enum SectionIdentifier {
  kMainSection = 0,
};

// Item identifiers for the query unsupported table view.
enum ItemIdentifier {
  kQueryUnsupportedItem = 0,
};

}  // namespace

@implementation AtMemoryQueryUnsupportedViewController {
  // The data source for the table view.
  UITableViewDiffableDataSource<NSNumber*, NSNumber*>* _dataSource;
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

- (void)loadModel {
  if (!_dataSource) {
    _dataSource = [[UITableViewDiffableDataSource alloc]
        initWithTableView:self.tableView
             cellProvider:^UITableViewCell*(UITableView* tableView,
                                            NSIndexPath* indexPath,
                                            NSNumber* itemIdentifier) {
               [TableViewCellContentConfiguration
                   registerCellForTableView:tableView];
               UITableViewCell* cell = [TableViewCellContentConfiguration
                   dequeueTableViewCell:tableView
                           forIndexPath:indexPath];

               TableViewCellContentConfiguration* contentConfiguration =
                   [[TableViewCellContentConfiguration alloc] init];
               contentConfiguration.title = l10n_util::GetNSString(
                   IDS_AUTOFILL_AT_MEMORY_UNSUPPORTED_QUERY_TITLE);
               contentConfiguration.subtitle = l10n_util::GetNSString(
                   IDS_AUTOFILL_AT_MEMORY_UNSUPPORTED_QUERY_DESCRIPTION);

               contentConfiguration.leadingConfiguration =
                   autofill::AtMemoryCellIconConfiguration(kSparklesSymbol);

               cell.contentConfiguration = contentConfiguration;
               cell.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
               return cell;
             }];
  }

  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ @(kMainSection) ]];
  [snapshot appendItemsWithIdentifiers:@[ @(kQueryUnsupportedItem) ]
             intoSectionWithIdentifier:@(kMainSection)];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
  [self.delegate queryUnsupportedViewControllerDidTapCell:self];
}

@end
