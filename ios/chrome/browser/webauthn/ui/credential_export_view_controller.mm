// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/webauthn/ui/credential_export_view_controller.h"

#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_view_controller_items.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/webauthn/ui/credential_export_view_controller_presentation_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// Identifier for the table view section that lists the credentials.
NSString* const kCredentialSectionIdentifier = @"CredentialSection";
// Required item type for the TableViewItem initializer.
const NSInteger kCredentialExportItemType = 0;

}  // namespace

@implementation CredentialExportViewController {
  // The data source that will manage the table view.
  UITableViewDiffableDataSource<NSString*, AffiliatedGroupTableViewItem*>*
      _dataSource;

  // The complete list of credential items to be displayed.
  NSArray<AffiliatedGroupTableViewItem*>* _items;
}

- (instancetype)init {
  return [super initWithStyle:ChromeTableViewStyle()];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS_AND_PASSKEYS);
  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(didTapDone)];

  self.tableView.allowsMultipleSelectionDuringEditing = YES;
  self.tableView.editing = YES;
  [self.tableView registerClass:[PasswordFormContentCell class]
         forCellReuseIdentifier:NSStringFromClass(
                                    [PasswordFormContentCell class])];

  [self configureDataSource];
}

#pragma mark - Actions

- (void)didTapDone {
  // TODO(crbug.com/449701042): Handle credentials selection
  [self.delegate userDidStartExport];
}

#pragma mark - CredentialExportConsumer

- (void)setAffiliatedGroups:
    (std::vector<password_manager::AffiliatedGroup>)affiliatedGroups {
  NSMutableArray<AffiliatedGroupTableViewItem*>* items =
      [[NSMutableArray alloc] initWithCapacity:affiliatedGroups.size()];
  for (password_manager::AffiliatedGroup& group : affiliatedGroups) {
    AffiliatedGroupTableViewItem* item = [[AffiliatedGroupTableViewItem alloc]
        initWithType:kCredentialExportItemType];
    item.affiliatedGroup = std::move(group);
    [items addObject:item];
  }
  _items = items;

  [self applySnapshotWithItems:_items animated:YES];
}

#pragma mark - UITableViewDelegate

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  TableViewTextHeaderFooterView* header =
      DequeueTableViewHeaderFooter<TableViewTextHeaderFooterView>(
          self.tableView);

  [header setSubtitle:l10n_util::GetNSString(
                          IDS_IOS_EXPORT_PASSWORDS_AND_PASSKEYS_SUBTITLE)];

  return header;
}

#pragma mark - Private

// Sets up the diffable data source and applies the initial snapshot.
- (void)configureDataSource {
  RegisterTableViewHeaderFooter<TableViewTextHeaderFooterView>(self.tableView);

  __weak __typeof(self) weakSelf = self;
  UITableViewDiffableDataSourceCellProvider cellProvider = ^UITableViewCell*(
      UITableView* tableView, NSIndexPath* indexPath,
      AffiliatedGroupTableViewItem* item) {
    __strong __typeof(self) strongSelf = weakSelf;
    if (!strongSelf) {
      return nil;
    }

    PasswordFormContentCell* cell = [tableView
        dequeueReusableCellWithIdentifier:NSStringFromClass(
                                              [PasswordFormContentCell class])
                             forIndexPath:indexPath];
    // TODO(crbug.com/453605350): Add account info as subtitle.
    cell.textLabel.text = item.title;

    return cell;
  };

  _dataSource =
      [[UITableViewDiffableDataSource alloc] initWithTableView:self.tableView
                                                  cellProvider:cellProvider];

  [self applySnapshotWithItems:_items animated:NO];
}

// Builds and applies a new snapshot to the data source.
- (void)applySnapshotWithItems:(NSArray<AffiliatedGroupTableViewItem*>*)items
                      animated:(BOOL)animated {
  if (!_dataSource) {
    return;
  }

  NSDiffableDataSourceSnapshot<NSString*, AffiliatedGroupTableViewItem*>*
      snapshot = [[NSDiffableDataSourceSnapshot alloc] init];
  [snapshot appendSectionsWithIdentifiers:@[ kCredentialSectionIdentifier ]];
  [snapshot appendItemsWithIdentifiers:items
             intoSectionWithIdentifier:kCredentialSectionIdentifier];

  [_dataSource applySnapshot:snapshot animatingDifferences:animated];
}

@end
