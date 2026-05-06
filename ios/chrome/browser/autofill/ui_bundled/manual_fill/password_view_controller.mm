// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/password_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/sys_string_conversions.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/google/core/common/google_util.h"
#import "components/password_manager/core/browser/password_manager_constants.h"
#import "components/webauthn/ios/features.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_action_cell.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_cell_utils.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_password_cell.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/passwords/password_suggestion/ui/password_suggestion_utils.h"
#import "ios/chrome/browser/settings/ui_bundled/password/create_password_manager_title_view.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/ui/list_model/list_item+Controller.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/elements/branded_navigation_item_title_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

namespace manual_fill {

enum ManualFallbackItemType : NSInteger {
  kHeader = kItemTypeEnumZero,
  kCredential,
  kNoCredentialsMessage,
};

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
    case manual_fill::ManualFallbackItemType::kCredential:
      // Set the icon of credential cells.
      [self setIconForCredentialCell:cell indexPath:indexPath];
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

  self.noRegularDataItemsToShowHeaderItem = nil;

  for (ManualFillCredentialItem* credentialItem in credentials) {
    credentialItem.type = manual_fill::ManualFallbackItemType::kCredential;
  }

  // If no items were posted and there is no search bar, present the empty item
  // and return.
  if (!credentials.count && !self.searchController) {
    TableViewTextHeaderFooterItem* textHeaderFooterItem =
        [[TableViewTextHeaderFooterItem alloc]
            initWithType:manual_fill::ManualFallbackItemType::
                             kNoCredentialsMessage];

    textHeaderFooterItem.text = l10n_util::GetNSString(
        IsConditionalPasskeyLoginEnabled()
            ? IDS_IOS_MANUAL_FALLBACK_NO_PASSWORDS_OR_PASSKEYS_FOR_SITE
            : IDS_IOS_MANUAL_FALLBACK_NO_PASSWORDS_FOR_SITE);
    self.noRegularDataItemsToShowHeaderItem = textHeaderFooterItem;
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

// Sets the icon for the given credential `cell` at `indexPath`.
- (void)setIconForCredentialCell:(UITableViewCell*)cell
                       indexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  DCHECK(item);
  DCHECK(cell);

  ManualFillCredentialItem* passwordItem =
      base::apple::ObjCCastStrict<ManualFillCredentialItem>(item);

  ManualFillPasswordCell* passwordCell =
      base::apple::ObjCCastStrict<ManualFillPasswordCell>(cell);

  if ([passwordCell isBackupCredential]) {
    [passwordCell configureWithSymbol:GetBackupPasswordSuggestionIcon()];
  } else {
    [self loadFaviconForCellIdentifier:passwordCell.uniqueIdentifier
                        itemIdentifier:passwordItem.uniqueIdentifier
                            faviconURL:passwordItem.faviconURL
                            completion:^(FaviconAttributes* faviconAttributes) {
                              [passwordCell configureWithFaviconAttributes:
                                                faviconAttributes];
                            }];
  }
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
          initWithType:manual_fill::ManualFallbackItemType::kHeader];

  headerItem.text =
      l10n_util::GetNSString(IDS_IOS_SAVE_PASSWORDS_MANAGE_ACCOUNT_HEADER);
  headerItem.urls = @[ [[CrURL alloc]
      initWithGURL:google_util::AppendGoogleLocaleParam(
                       GURL(password_manager::kPasswordManagerHelpCenteriOSURL),
                       GetApplicationContext()
                           ->GetApplicationLocaleStorage()
                           ->Get())] ];

  [self presentHeaderItem:headerItem];
}

@end
