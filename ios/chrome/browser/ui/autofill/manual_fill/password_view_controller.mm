// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/password_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "components/google/core/common/google_util.h"
#import "components/password_manager/core/browser/password_manager_constants.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_favicon_data_source.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_cell_utils.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_text_cell.h"
#import "ios/chrome/browser/ui/settings/password/branded_navigation_item_title_view.h"
#import "ios/chrome/browser/ui/settings/password/create_password_manager_title_view.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"
#import "ios/chrome/common/ui/favicon/favicon_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

typedef NS_ENUM(NSInteger, ManualFallbackItemType) {
  ManualFallbackItemTypeUnkown = kItemTypeEnumZero,
  ManualFallbackItemTypeHeader,
  ManualFallbackItemTypeCredential,
  ManualFallbackItemTypeEmptyCredential,
};

namespace manual_fill {

NSString* const kPasswordDoneButtonAccessibilityIdentifier =
    @"kManualFillPasswordDoneButtonAccessibilityIdentifier";
NSString* const kPasswordSearchBarAccessibilityIdentifier =
    @"kManualFillPasswordSearchBarAccessibilityIdentifier";
NSString* const kPasswordTableViewAccessibilityIdentifier =
    @"kManualFillPasswordTableViewAccessibilityIdentifier";

}  // namespace manual_fill

@interface PasswordViewController () <TableViewLinkHeaderFooterItemDelegate>

// Search controller if any.
@property(nonatomic, strong) UISearchController* searchController;

@end

@implementation PasswordViewController

- (instancetype)initWithSearchController:(UISearchController*)searchController {
  self = [super init];
  if (self) {
    _searchController = searchController;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.tableView.accessibilityIdentifier =
      manual_fill::kPasswordTableViewAccessibilityIdentifier;

  self.definesPresentationContext = YES;
  self.searchController.searchBar.backgroundColor = [UIColor clearColor];
  self.searchController.obscuresBackgroundDuringPresentation = NO;
  self.navigationItem.searchController = self.searchController;
  self.navigationItem.hidesSearchBarWhenScrolling = NO;
  self.searchController.searchBar.accessibilityIdentifier =
      manual_fill::kPasswordSearchBarAccessibilityIdentifier;
  self.title = l10n_util::GetNSString(IDS_IOS_PASSWORD_MANAGER);

  if (self.searchController) {
    [self setUpCustomTitleView];
    [self addHeaderItem];
  }

  UIBarButtonItem* doneButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(handleDoneButton)];
  doneButton.accessibilityIdentifier =
      manual_fill::kPasswordDoneButtonAccessibilityIdentifier;
  self.navigationItem.rightBarButtonItem = doneButton;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  DCHECK_EQ(tableView, self.tableView);
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  switch (itemType) {
    case ManualFallbackItemTypeCredential:
      // Retrieve favicons for credential cells.
      [self loadFaviconForCell:cell indexPath:indexPath];
      break;

    default:
      break;
  }
  return cell;
}

- (UIView*)tableView:(UITableView*)tableView
    viewForHeaderInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForHeaderInSection:section];

  if ([view isKindOfClass:[TableViewLinkHeaderFooterView class]]) {
    // Attach `self` as a delegate to ensure taps on the link are handled.
    TableViewLinkHeaderFooterView* linkHeader =
        base::apple::ObjCCastStrict<TableViewLinkHeaderFooterView>(view);
    linkHeader.delegate = self;

    // When the Keyboard Accessory Upgrade feature is disabled, indents are
    // needed for the header to be aligned with the other table view items.
    [linkHeader setForceIndents:!IsKeyboardAccessoryUpgradeEnabled()];
  }

  return view;
}

#pragma mark - ManualFillPasswordConsumer

- (void)presentCredentials:(NSArray<ManualFillCredentialItem*>*)credentials {
  if (self.searchController) {
    UMA_HISTOGRAM_COUNTS_1000("ManualFallback.PresentedOptions.AllPasswords",
                              credentials.count);
  } else {
    UMA_HISTOGRAM_COUNTS_100("ManualFallback.PresentedOptions.Passwords",
                             credentials.count);
  }

  for (ManualFillCredentialItem* credentialItem in credentials) {
    credentialItem.type = ManualFallbackItemTypeCredential;
  }

  // If no items were posted and there is no search bar, present the empty item
  // and return.
  if (!credentials.count && !self.searchController) {
    ManualFillTextItem* emptyCredentialItem = [[ManualFillTextItem alloc]
        initWithType:ManualFallbackItemTypeEmptyCredential];
    emptyCredentialItem.text =
        l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NO_PASSWORDS_FOR_SITE);
    emptyCredentialItem.textColor = [UIColor colorNamed:kDisabledTintColor];
    emptyCredentialItem.showSeparator = YES;
    [self presentDataItems:@[ emptyCredentialItem ]];
    return;
  }

  [self presentDataItems:credentials];
}

- (void)presentActions:(NSArray<ManualFillActionItem*>*)actions {
  [self presentActionItems:actions];
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  [self.delegate didTapLinkURL:URL];
}

#pragma mark - Private

// Retrieves favicon from FaviconLoader and sets image in `cell`.
- (void)loadFaviconForCell:(UITableViewCell*)cell
                 indexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  DCHECK(item);
  DCHECK(cell);

  ManualFillCredentialItem* passwordItem =
      base::apple::ObjCCastStrict<ManualFillCredentialItem>(item);
  if (passwordItem.isConnectedToPreviousItem) {
    return;
  }

  ManualFillPasswordCell* passwordCell =
      base::apple::ObjCCastStrict<ManualFillPasswordCell>(cell);

  NSString* itemIdentifier = passwordItem.uniqueIdentifier;
  CrURL* crurl = [[CrURL alloc] initWithGURL:passwordItem.faviconURL];
  [self.imageDataSource
      faviconForPageURL:crurl
             completion:^(FaviconAttributes* attributes) {
               // Only set favicon if the cell hasn't been reused.
               if ([passwordCell.uniqueIdentifier
                       isEqualToString:itemIdentifier]) {
                 DCHECK(attributes);
                 [passwordCell configureWithFaviconAttributes:attributes];
               }
             }];
}

- (void)handleDoneButton {
  [self.delegate passwordViewControllerDidTapDoneButton:self];
}

// Adds a custom title view branded with a Password Manager icon.
- (void)setUpCustomTitleView {
  self.navigationItem.largeTitleDisplayMode =
      UINavigationItemLargeTitleDisplayModeNever;
  self.navigationItem.titleView =
      password_manager::CreatePasswordManagerTitleView(/*title=*/self.title);
}

// Adds a header containing text and a link.
- (void)addHeaderItem {
  TableViewLinkHeaderFooterItem* headerItem =
      [[TableViewLinkHeaderFooterItem alloc]
          initWithType:ManualFallbackItemTypeHeader];

  headerItem.text =
      l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORDS_MANAGE_ACCOUNT_HEADER);
  headerItem.urls = @[ [[CrURL alloc]
      initWithGURL:google_util::AppendGoogleLocaleParam(
                       GURL(password_manager::kPasswordManagerHelpCenteriOSURL),
                       GetApplicationContext()->GetApplicationLocale())] ];

  [self presentHeaderItem:headerItem];
}

@end
