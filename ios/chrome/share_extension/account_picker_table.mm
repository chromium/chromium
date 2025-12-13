// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/share_extension/account_picker_table.h"

#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/share_extension/account_picker_delegate.h"

namespace {

NSString* const kMainSectionIdentifier = @"MainAccountsSection";
CGFloat const kAvatarImageDimension = 30.0;

}  // namespace

@interface AccountPickerTable () <UITableViewDelegate>
@property(nonatomic, strong) AccountInfo* selectedAccount;
@end

@implementation AccountPickerTable {
  NSArray<AccountInfo*>* _accounts;
  UITableView* _accountsTable;
  UITableViewDiffableDataSource<NSString*, AccountInfo*>* _diffableDataSource;
}

- (instancetype)initWithAccounts:(NSArray<AccountInfo*>*)accounts
                 selectedAccount:(AccountInfo*)selectedAccount {
  self = [super initWithNibName:nil bundle:nil];

  if (self) {
    _accounts = [accounts copy];
    _selectedAccount = selectedAccount;
  }

  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  _accountsTable =
      [[UITableView alloc] initWithFrame:self.view.bounds
                                   style:UITableViewStyleInsetGrouped];
  _accountsTable.autoresizingMask =
      UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  _accountsTable.delegate = self;

  [_accountsTable registerClass:[UITableViewCell class]
         forCellReuseIdentifier:NSStringFromClass([UITableViewCell class])];

  self.title = NSLocalizedString(
      @"IDS_IOS_ACCOUNTS_TITLE_SHARE_EXTENSION",
      @"The title of the item representing a signed out user.");
  ;
  self.navigationItem.leftBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(doneButtonTapped)];
  self.navigationItem.rightBarButtonItem = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(cancelButtonTapped)];
  _accountsTable.tableHeaderView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];
  _accountsTable.tableFooterView =
      [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, CGFLOAT_MIN)];

  [self.view addSubview:_accountsTable];
  [self configureDataSource];
  [self applySnapshot];
  [self setUpBottomSheetPresentationController];
  [self setUpBottomSheetDetents];
}

#pragma mark - UITableViewDelegate
- (void)setSelectedAccount:(AccountInfo*)selectedAccount {
  if ([selectedAccount.gaiaIDString isEqual:_selectedAccount.gaiaIDString]) {
    return;
  }
  _selectedAccount = selectedAccount;
  [_accountsTable reloadData];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  self.selectedAccount = _accounts[indexPath.row];
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return [_accounts count];
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return 1;
}

#pragma mark - Private

- (UITableViewCell*)configureAccountCell:(UITableViewCell*)cell
                             accountInfo:(AccountInfo*)accountInfo {
  UIListContentConfiguration* content = cell.defaultContentConfiguration;
  if ([accountInfo.gaiaIDString isEqual:app_group::kNoAccount]) {
    content.text = NSLocalizedString(
        @"IDS_IOS_SIGNED_OUT_USER_TITLE_SHARE_EXTENSION",
        @"The title of the item representing a signed out user.");
    ;
    content.image = [[UIImage systemImageNamed:@"person.crop.circle"]
        imageWithTintColor:[UIColor colorNamed:kGrey400Color]
             renderingMode:UIImageRenderingModeAlwaysOriginal];

  } else {
    content.text = ([accountInfo.fullName length] > 0) ? accountInfo.fullName
                                                       : accountInfo.email;
    content.secondaryText = accountInfo.email;
    content.image = accountInfo.avatar;
    UIListContentImageProperties* imageProperties = content.imageProperties;
    imageProperties.cornerRadius = kAvatarImageDimension / 2.0;
    imageProperties.maximumSize =
        CGSize(kAvatarImageDimension, kAvatarImageDimension);
  }
  cell.contentConfiguration = content;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.accessoryType =
      (_selectedAccount.gaiaIDString == accountInfo.gaiaIDString)
          ? UITableViewCellAccessoryCheckmark
          : UITableViewCellAccessoryNone;
  return cell;
}

- (void)doneButtonTapped {
  [self.delegate didSelectAccountInTable:self selectedAccount:_selectedAccount];
  [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)cancelButtonTapped {
  [self dismissViewControllerAnimated:YES completion:nil];
}

- (UITableViewCell*)cellForIndexPath:(NSIndexPath*)indexPath
                           tableView:(UITableView*)tableView
                         accountInfo:(AccountInfo*)accountInfo {
  NSString* identifier = NSStringFromClass([UITableViewCell class]);
  UITableViewCell* cell =
      [tableView dequeueReusableCellWithIdentifier:identifier
                                      forIndexPath:indexPath];
  [self configureAccountCell:cell accountInfo:accountInfo];

  return cell;
}

- (void)configureDataSource {
  __weak __typeof(self) weakSelf = self;

  auto cellProvider =
      ^UITableViewCell*(UITableView* tableView, NSIndexPath* indexPath,
                        AccountInfo* accountInfo) {
        return [weakSelf cellForIndexPath:indexPath
                                tableView:tableView
                              accountInfo:accountInfo];
      };

  _diffableDataSource =
      [[UITableViewDiffableDataSource alloc] initWithTableView:_accountsTable
                                                  cellProvider:cellProvider];
  _accountsTable.dataSource = _diffableDataSource;
}

- (void)applySnapshot {
  NSDiffableDataSourceSnapshot<NSString*, AccountInfo*>* snapshot =
      [[NSDiffableDataSourceSnapshot alloc] init];

  [snapshot appendSectionsWithIdentifiers:@[ kMainSectionIdentifier ]];
  [snapshot appendItemsWithIdentifiers:_accounts
             intoSectionWithIdentifier:kMainSectionIdentifier];
  [_diffableDataSource applySnapshot:snapshot animatingDifferences:YES];
}

- (void)setUpBottomSheetPresentationController {
  self.modalPresentationStyle = UIModalPresentationFormSheet;
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.prefersEdgeAttachedInCompactHeight = YES;
  presentationController.widthFollowsPreferredContentSizeWhenEdgeAttached = YES;
  presentationController.preferredCornerRadius = 20;
}

// Configures the bottom sheet's detents.
- (void)setUpBottomSheetDetents {
  UISheetPresentationController* presentationController =
      self.sheetPresentationController;
  presentationController.detents = @[ self.customDetent ];
  presentationController.selectedDetentIdentifier =
      self.customDetent.identifier;
}

@end
