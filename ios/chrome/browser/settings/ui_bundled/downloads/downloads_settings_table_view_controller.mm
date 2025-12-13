// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/downloads/downloads_settings_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/notreached.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/authentication/ui_bundled/views/identity_button_control.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/downloads_settings_table_view_controller_action_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/downloads_settings_table_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/identity_button_cell.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/identity_button_item.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/save_to_photos/save_to_photos_settings_account_selection_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/settings/ui_bundled/downloads/save_to_photos/save_to_photos_settings_mutator.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSaveToPhotos = kSectionIdentifierEnumZero,
  SectionIdentifierAutoDeletion
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeDefaultIdentity,
  ItemTypeAskEveryTime,
  ItemTypeAutoDeletion
};

}  // namespace

@interface DownloadsSettingsTableViewController ()

// Save to Photos items.
@property(nonatomic, strong)
    IdentityButtonItem* saveToPhotosDefaultIdentityItem;
@property(nonatomic, strong)
    TableViewSwitchItem* saveToPhotosAskEveryTimeSwitch;

// Downloads Auto-deletion items.
@property(nonatomic, strong) TableViewSwitchItem* autoDeletionSwitch;

@end

@implementation DownloadsSettingsTableViewController {
  // YES if the current profile supports Save To Photos.
  BOOL _showSaveToPhotosSettings;

  // YES if Download Auto-deletion is enabled.
  BOOL _isAutoDeletionEnabled;
}

#pragma mark - Initialization

- (instancetype)init {
  return [super initWithStyle:ChromeTableViewStyle()];
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = l10n_util::GetNSString(IDS_IOS_SETTINGS_DOWNLOADS_TITLE);
  [self loadModel];
}

- (void)didMoveToParentViewController:(UIViewController*)parent {
  if (!parent) {
    [self.presentationDelegate
        downloadsSettingsTableViewControllerWasRemoved:self];
  }
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  if (_showSaveToPhotosSettings) {
    [self loadSaveToPhotosSection];
  }

  if (IsDownloadAutoDeletionFeatureEnabled()) {
    [self loadAutoDeletionSection];
  }
}

#pragma mark - SaveToPhotosSettingsAccountConfirmationConsumer

- (void)setIdentityButtonAvatar:(UIImage*)avatar
                           name:(NSString*)name
                          email:(NSString*)email
                         gaiaID:(const GaiaId&)gaiaID
           askEveryTimeSwitchOn:(BOOL)askEveryTimeSwitchOn {
  // Update the identity button item.
  IdentityButtonItem* identityButtonItem = self.saveToPhotosDefaultIdentityItem;
  identityButtonItem.identityAvatar = avatar;
  identityButtonItem.identityName = name;
  identityButtonItem.identityEmail = email;
  identityButtonItem.identityGaiaID = gaiaID;
  [self reconfigureCellsForItems:@[ identityButtonItem ]];

  // Update the "Ask which account to use every time" switch.
  TableViewSwitchItem* switchItem = self.saveToPhotosAskEveryTimeSwitch;
  switchItem.on = askEveryTimeSwitchOn;

  [self reconfigureCellsForItems:@[ switchItem ]];
}

- (void)displaySaveToPhotosSettingsUI {
  _showSaveToPhotosSettings = YES;

  if (!self.viewIfLoaded) {
    return;
  }

  [self loadModel];
  [self reloadData];
}

- (void)hideSaveToPhotosSettingsUI {
  _showSaveToPhotosSettings = NO;

  if (!self.viewIfLoaded) {
    return;
  }

  [self loadModel];
  [self reloadData];
}

#pragma mark - AutoDeletionSettingsConsumer

- (void)setAutoDeletionEnabled:(BOOL)status {
  _isAutoDeletionEnabled = status;
  TableViewSwitchItem* switchItem = self.autoDeletionSwitch;
  switchItem.on = status;
  [self reconfigureCellsForItems:@[ switchItem ]];
}

#pragma mark - Items

- (IdentityButtonItem*)saveToPhotosDefaultIdentityItem {
  if (!_saveToPhotosDefaultIdentityItem) {
    _saveToPhotosDefaultIdentityItem =
        [[IdentityButtonItem alloc] initWithType:ItemTypeDefaultIdentity];
    _saveToPhotosDefaultIdentityItem.arrowDirection =
        IdentityButtonControlArrowRight;
    _saveToPhotosDefaultIdentityItem.identityViewStyle =
        IdentityViewStyleConsistency;
  }
  return _saveToPhotosDefaultIdentityItem;
}

- (TableViewSwitchItem*)saveToPhotosAskEveryTimeSwitch {
  if (!_saveToPhotosAskEveryTimeSwitch) {
    _saveToPhotosAskEveryTimeSwitch =
        [[TableViewSwitchItem alloc] initWithType:ItemTypeAskEveryTime];
    _saveToPhotosAskEveryTimeSwitch.target = self;
    _saveToPhotosAskEveryTimeSwitch.selector =
        @selector(saveToPhotosAskEveryTimeSwitchAction:);
    _saveToPhotosAskEveryTimeSwitch.text = l10n_util::GetNSString(
        IDS_IOS_SAVE_TO_PHOTOS_ACCOUNT_PICKER_THIS_ACCOUNT_EVERY_TIME);
  }
  return _saveToPhotosAskEveryTimeSwitch;
}

- (TableViewSwitchItem*)autoDeletionSwitch {
  if (!_autoDeletionSwitch) {
    _autoDeletionSwitch =
        [[TableViewSwitchItem alloc] initWithType:ItemTypeAutoDeletion];
    _autoDeletionSwitch.text =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_DOWNLOADS_SWITCH_ITEM_HEADER);
    _autoDeletionSwitch.detailText = l10n_util::GetNSString(
        IDS_IOS_SETTINGS_DOWNLOADS_SWITCH_ITEM_DETAIL_TEXT);
    _autoDeletionSwitch.target = self;
    _autoDeletionSwitch.selector = @selector(autoDeletionSwitchAction:);
    _autoDeletionSwitch.on = _isAutoDeletionEnabled;
  }

  return _autoDeletionSwitch;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  if ([cell isKindOfClass:[IdentityButtonCell class]]) {
    SEL action = [self actionForItemAtIndexPath:indexPath];
    IdentityButtonCell* identityButtonCell =
        base::apple::ObjCCastStrict<IdentityButtonCell>(cell);
    [identityButtonCell.identityButtonControl
               addTarget:self
                  action:action
        forControlEvents:UIControlEventTouchUpInside];
  }
  return cell;
}

#pragma mark - Actions

- (void)saveToPhotosAskEveryTimeSwitchAction:(UISwitch*)sender {
  BOOL askWhichAccountToUseEveryTime = !sender.isOn;
  [self.saveToPhotosSettingsMutator
      setAskWhichAccountToUseEveryTime:askWhichAccountToUseEveryTime];
  if (!askWhichAccountToUseEveryTime) {
    GaiaId gaiaID = self.saveToPhotosDefaultIdentityItem.identityGaiaID;
    [self.saveToPhotosSettingsMutator setSelectedIdentityGaiaID:&gaiaID];
  }
}

- (void)saveToPhotosIdentityButtonAction:(IdentityButtonControl*)sender {
  [self.actionDelegate
      downloadsSettingsTableViewControllerOpenSaveToPhotosAccountSelection:
          self];
}

- (void)autoDeletionSwitchAction:(UISwitch*)sender {
  [self.autoDeletionSettingsMutator
      setDownloadAutoDeletionPermissionStatus:sender.isOn];
}

#pragma mark - Private

// Load Save to Photos section into model.
- (void)loadSaveToPhotosSection {
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSaveToPhotos];
  TableViewTextHeaderFooterItem* headerItem =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  headerItem.text =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_DOWNLOADS_SAVE_TO_PHOTOS_HEADER);
  [model setHeader:headerItem
      forSectionWithIdentifier:SectionIdentifierSaveToPhotos];

  [model addItem:self.saveToPhotosDefaultIdentityItem
      toSectionWithIdentifier:SectionIdentifierSaveToPhotos];

  [model addItem:self.saveToPhotosAskEveryTimeSwitch
      toSectionWithIdentifier:SectionIdentifierSaveToPhotos];
}

// Loads Auto-deletion section into model.
- (void)loadAutoDeletionSection {
  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierAutoDeletion];
  TableViewTextHeaderFooterItem* headerItem =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];

  headerItem.text =
      l10n_util::GetNSString(IDS_IOS_SETTINGS_DOWNLOADS_SECTION_HEADER);
  [model setHeader:headerItem
      forSectionWithIdentifier:SectionIdentifierAutoDeletion];

  [model addItem:self.autoDeletionSwitch
      toSectionWithIdentifier:SectionIdentifierAutoDeletion];
}

// Returns the action selector associated with an ItemType located at
// `indexPath`.
- (SEL)actionForItemAtIndexPath:(NSIndexPath*)indexPath {
  switch ([self.tableViewModel itemTypeForIndexPath:indexPath]) {
    case ItemTypeDefaultIdentity:
      return @selector(saveToPhotosIdentityButtonAction:);
    case ItemTypeAskEveryTime:
    case ItemTypeAutoDeletion:
      NOTREACHED();
  }

  NOTREACHED();
}

@end
