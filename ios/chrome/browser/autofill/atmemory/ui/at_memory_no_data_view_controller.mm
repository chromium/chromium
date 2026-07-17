// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_no_data_view_controller.h"

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

// Section identifiers for the no data table view.
enum SectionIdentifier {
  kMainSection = 0,
};

// Item identifiers for the no data table view.
enum ItemIdentifier {
  kNoDataItem = 0,
};

}  // namespace

@implementation AtMemoryNoDataViewController {
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

  self.tableView.allowsSelection = NO;

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

               contentConfiguration.leadingConfiguration =
                   autofill::AtMemoryCellIconConfiguration(kErrorCircleSymbol);

               cell.contentConfiguration = contentConfiguration;
               return cell;
             }];
  }

  NSDiffableDataSourceSnapshot<NSNumber*, NSNumber*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ @(kMainSection) ]];
  [snapshot appendItemsWithIdentifiers:@[ @(kNoDataItem) ]
             intoSectionWithIdentifier:@(kMainSection)];

  [_dataSource applySnapshot:snapshot animatingDifferences:NO];
}

@end
