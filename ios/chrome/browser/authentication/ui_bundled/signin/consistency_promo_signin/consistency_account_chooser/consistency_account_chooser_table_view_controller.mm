// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/ios/block_types.h"
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_identity_cell.h"
#import "ios/chrome/browser/authentication/ui_bundled/cells/table_view_identity_item.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/enterprise_utils.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_table_view_controller_action_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_account_chooser/consistency_account_chooser_table_view_controller_model_delegate.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/consistency_promo_signin/consistency_account_chooser/identity_item_configurator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"

namespace {

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  IdentitySectionIdentifier = kSectionIdentifierEnumZero,
  AddAccountSectionIdentifier,
};

typedef NS_ENUM(NSInteger, ItemType) {
  // IdentitySectionIdentifier section.
  IdentityItemType = kItemTypeEnumZero,
  // Indication that some restricted accounts were removed from the list.
  ItemTypeRestrictedAccountsFooter,
  // AddAccountSectionIdentifier section.
  AddAccountItemType,
};

// Table view header/footer height.
constexpr CGFloat kSectionHeaderHeight = 8.;
constexpr CGFloat kSectionFooterHeight = 8.;

}  // namespace

@interface ConsistencyAccountChooserTableViewController () <
    TableViewLinkHeaderFooterItemDelegate>

@end

@implementation ConsistencyAccountChooserTableViewController

#pragma mark - UIView

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
  [self.tableView reloadData];
  self.view.backgroundColor = UIColor.clearColor;
}

#pragma mark - UITableViewController

- (void)loadModel {
  [super loadModel];
  [self loadIdentitySection];
  [self loadAddAccountSection];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
  ListItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  switch ((ItemType)item.type) {
    case IdentityItemType: {
      TableViewIdentityItem* identityItem =
          base::apple::ObjCCastStrict<TableViewIdentityItem>(item);
      DCHECK(identityItem);
      base::RecordAction(base::UserMetricsAction(
          "Signin_BottomSheet_IdentityChooser_Selected"));

      // Find index of the previously selected identity.
      NSIndexPath* previousIndexPath = nil;
      NSInteger section = [self.tableViewModel
          sectionForSectionIdentifier:IdentitySectionIdentifier];
      for (int i = 0; i < [self.tableViewModel numberOfItemsInSection:section];
           i++) {
        NSIndexPath* path = [NSIndexPath indexPathForRow:i inSection:section];
        TableViewIdentityItem* currentItem =
            base::apple::ObjCCastStrict<TableViewIdentityItem>(
                [self.tableViewModel itemAtIndexPath:path]);
        if (currentItem.selected) {
          previousIndexPath = path;
          break;
        }
      }

      // Deselect the previous identity and update the cell.
      if (previousIndexPath) {
        TableViewIdentityItem* previousIdentityItem =
            base::apple::ObjCCastStrict<TableViewIdentityItem>(
                [self.tableViewModel itemAtIndexPath:previousIndexPath]);
        previousIdentityItem.selected = NO;
        LegacyTableViewCell* oldCell =
            [self.tableView cellForRowAtIndexPath:previousIndexPath];
        if (oldCell) {
          [previousIdentityItem configureCell:oldCell withStyler:self.styler];
        }
      }

      // Select the new identity and update the cell.
      identityItem.selected = YES;
      LegacyTableViewCell* newCell =
          [self.tableView cellForRowAtIndexPath:indexPath];
      __weak ConsistencyAccountChooserTableViewController* weakSelf = self;
      ProceduralBlock completionBlock = ^{
        [weakSelf.actionDelegate
            consistencyAccountChooserTableViewController:weakSelf
                             didSelectIdentityWithGaiaID:GaiaId(identityItem
                                                                    .gaiaID)];
      };
      if (newCell) {
        [identityItem configureCell:newCell
                         withStyler:self.styler
                         completion:completionBlock];
      } else {
        // The cell is not visible, so the animation will not be seen.
        // The completion can be called immediately.
        completionBlock();
      }
      break;
    }
    case AddAccountItemType:
      base::RecordAction(base::UserMetricsAction(
          "Signin_BottomSheet_IdentityChooser_AddAccount"));
      [self.actionDelegate
          consistencyAccountChooserTableViewControllerDidTapOnAddAccount:self];
      break;
    case ItemTypeRestrictedAccountsFooter:
      NOTREACHED();
  }
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  return kSectionHeaderHeight;
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  if ([self.tableViewModel footerForSectionIndex:section]) {
    return UITableViewAutomaticDimension;
  }
  return kSectionFooterHeight;
}

#pragma mark - Model Items

- (TableViewLinkHeaderFooterItem*)restrictedIdentitiesFooterItem {
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypeRestrictedAccountsFooter];
  footer.text =
      l10n_util::GetNSString(IDS_IOS_OPTIONS_ACCOUNTS_RESTRICTED_IDENTITIES);
  footer.urls = @[ [[CrURL alloc] initWithGURL:GURL(kChromeUIManagementURL)] ];
  return footer;
}

#pragma mark - Private

// Creates the identity section in the table view model.
- (void)loadIdentitySection {
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:IdentitySectionIdentifier];
  [self loadIdentityItems];

  if (IsRestrictAccountsToPatternsEnabled()) {
    [model setFooter:[self restrictedIdentitiesFooterItem]
        forSectionWithIdentifier:IdentitySectionIdentifier];
  }
}

// Creates all the identity items in the table view model.
- (void)loadIdentityItems {
  TableViewModel* model = self.tableViewModel;
  for (IdentityItemConfigurator* configurator in self.modelDelegate
           .sortedIdentityItemConfigurators) {
    TableViewIdentityItem* item =
        [[TableViewIdentityItem alloc] initWithType:IdentityItemType];
    item.identityViewStyle = IdentityViewStyleConsistency;
    [configurator configureIdentityChooser:item];
    [model addItem:item toSectionWithIdentifier:IdentitySectionIdentifier];
  }
}

// Creates the add account section in the table view model.
- (void)loadAddAccountSection {
  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:AddAccountSectionIdentifier];
  TableViewImageItem* item =
      [[TableViewImageItem alloc] initWithType:AddAccountItemType];
  item.title = l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_ADD_ACCOUNT);
  item.accessibilityIdentifier = kConsistencyAccountChooserAddAccountIdentifier;
  item.textColor = [UIColor colorNamed:kBlueColor];
  item.isAccessibilityElement = YES;
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  [model addItem:item toSectionWithIdentifier:AddAccountSectionIdentifier];
}

#pragma mark - UITableViewDataSource

- (UIView*)tableView:(UITableView*)tableView
    viewForFooterInSection:(NSInteger)section {
  UIView* view = [super tableView:tableView viewForFooterInSection:section];
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];
  switch (sectionIdentifier) {
    case IdentitySectionIdentifier: {
      TableViewLinkHeaderFooterView* linkView =
          base::apple::ObjCCast<TableViewLinkHeaderFooterView>(view);
      linkView.delegate = self;
    } break;
    case AddAccountSectionIdentifier:
      break;
  }
  return view;
}

#pragma mark - TableViewLinkHeaderFooterItemDelegate

- (void)view:(TableViewLinkHeaderFooterView*)view didTapLinkURL:(CrURL*)URL {
  DCHECK(URL.gurl == GURL(kChromeUIManagementURL));
  DCHECK(self.actionDelegate);
  base::RecordAction(base::UserMetricsAction(
      "Signin_BottomSheet_IdentityChooser_ShowManagementHelpPage"));
  [self.actionDelegate showManagementHelpPage];
}

#pragma mark - ConsistencyAccountChooserConsumer

- (void)reloadAllIdentities {
  TableViewModel* model = self.tableViewModel;
  [model deleteAllItemsFromSectionWithIdentifier:IdentitySectionIdentifier];
  [self loadIdentityItems];

  NSUInteger sectionIndex =
      [model sectionForSectionIdentifier:IdentitySectionIdentifier];
  NSIndexSet* indexSet = [NSIndexSet indexSetWithIndex:sectionIndex];
  [self.tableView reloadSections:indexSet
                withRowAnimation:UITableViewRowAnimationFade];
}

- (void)reloadIdentityForIdentityItemConfigurator:
    (IdentityItemConfigurator*)configurator {
  TableViewModel* model = self.tableViewModel;
  NSInteger section =
      [model sectionForSectionIdentifier:IdentitySectionIdentifier];
  NSInteger itemCount = [model numberOfItemsInSection:section];
  for (NSInteger itemIndex = 0; itemIndex < itemCount; ++itemIndex) {
    NSIndexPath* path = [NSIndexPath indexPathForItem:itemIndex
                                            inSection:section];
    TableViewIdentityItem* item =
        base::apple::ObjCCastStrict<TableViewIdentityItem>(
            [model itemAtIndexPath:path]);
    if (item.gaiaID == configurator.gaiaID) {
      [configurator configureIdentityChooser:item];
      [self reconfigureCellsForItems:@[ item ]];
      [self.tableView reloadRowsAtIndexPaths:@[ path ]
                            withRowAnimation:UITableViewRowAnimationNone];
      break;
    }
  }
}

@end
