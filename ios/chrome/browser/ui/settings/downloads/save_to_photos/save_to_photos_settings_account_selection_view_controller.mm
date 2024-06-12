// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_selection_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/account_picker/ui_bundled/account_picker_selection/account_picker_selection_screen_identity_item_configurator.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_identity_item.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_selection_view_controller_action_delegate.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_selection_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_mutator.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierAccountSelection = kSectionIdentifierEnumZero,
  SectionIdentifierAddAccount,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeIdentity,
  ItemTypeAddAccount,
};

}  // namespace

@interface SaveToPhotosSettingsAccountSelectionViewController ()

// Items for accounts on the device.
@property(nonatomic, strong) NSArray<TableViewIdentityItem*>* identityItems;

// Item for the "Add account" button.
@property(nonatomic, strong) TableViewImageItem* addAccountItem;

@end

@implementation SaveToPhotosSettingsAccountSelectionViewController

#pragma mark - Initialization

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(
        IDS_IOS_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_HEADER);
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  if (!parent) {
    [self.presentationDelegate
            saveToPhotosSettingsAccountSelectionViewControllerWasRemoved];
  }
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];
  [self loadAccountSelectionSection];
  [self loadAddAccountSection];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
  ListItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  switch ((ItemType)item.type) {
    case ItemTypeHeader: {
      break;
    }
    case ItemTypeIdentity: {
      TableViewIdentityItem* identityItem =
          base::apple::ObjCCastStrict<TableViewIdentityItem>(item);
      [self.mutator setSelectedIdentityGaiaID:identityItem.gaiaID];
      break;
    }
    case ItemTypeAddAccount: {
      [self.actionDelegate
              saveToPhotosSettingsAccountSelectionViewControllerAddAccount];
      break;
    }
  }
}

#pragma mark - SaveToPhotosSettingsAccountSelectionConsumer

- (void)populateAccountsOnDevice:
    (NSArray<AccountPickerSelectionScreenIdentityItemConfigurator*>*)
        configurators {
  NSMutableArray<TableViewIdentityItem*>* identityItems =
      [[NSMutableArray alloc] init];
  for (AccountPickerSelectionScreenIdentityItemConfigurator* configurator in
           configurators) {
    TableViewIdentityItem* identityItem =
        [[TableViewIdentityItem alloc] initWithType:ItemTypeIdentity];
    identityItem.identityViewStyle = IdentityViewStyleConsistency;
    [configurator configureIdentityChooser:identityItem];
    [identityItems addObject:identityItem];
  }
  self.identityItems = identityItems;

  [self reloadData];
}

#pragma mark - Items

- (NSArray<TableViewIdentityItem*>*)identityItems {
  if (!_identityItems) {
    _identityItems = [[NSMutableArray alloc] init];
  }
  return _identityItems;
}

- (TableViewImageItem*)addAccountItem {
  if (!_addAccountItem) {
    _addAccountItem =
        [[TableViewImageItem alloc] initWithType:ItemTypeAddAccount];
    _addAccountItem.title =
        l10n_util::GetNSString(IDS_IOS_CONSISTENCY_PROMO_ADD_ACCOUNT);
    _addAccountItem.textColor = [UIColor colorNamed:kBlueColor];
  }
  return _addAccountItem;
}

#pragma mark - Private

// Loads the account selection section.
- (void)loadAccountSelectionSection {
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierAccountSelection];
  TableViewTextHeaderFooterItem* headerItem =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  headerItem.text = l10n_util::GetNSString(
      IDS_IOS_SETTINGS_DOWNLOADS_ACCOUNT_SELECTION_HEADER);
  [model setHeader:headerItem
      forSectionWithIdentifier:SectionIdentifierAccountSelection];

  for (TableViewIdentityItem* identityItem in self.identityItems) {
    [model addItem:identityItem
        toSectionWithIdentifier:SectionIdentifierAccountSelection];
  }
}

// Loads the "Add account" section.
- (void)loadAddAccountSection {
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierAddAccount];
  [model addItem:self.addAccountItem
      toSectionWithIdentifier:SectionIdentifierAddAccount];
}

@end
