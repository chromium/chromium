// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/downloads/downloads_settings_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/authentication/views/identity_button_control.h"
#import "ios/chrome/browser/ui/settings/downloads/downloads_settings_table_view_controller_action_delegate.h"
#import "ios/chrome/browser/ui/settings/downloads/downloads_settings_table_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/downloads/identity_button_cell.h"
#import "ios/chrome/browser/ui/settings/downloads/identity_button_item.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_account_selection_view_controller_presentation_delegate.h"
#import "ios/chrome/browser/ui/settings/downloads/save_to_photos/save_to_photos_settings_mutator.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSaveToPhotos = kSectionIdentifierEnumZero,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeHeader = kItemTypeEnumZero,
  ItemTypeDefaultIdentity,
  ItemTypeAskEveryTime,
};

}  // namespace

@interface DownloadsSettingsTableViewController ()

// Save to Photos items.
@property(nonatomic, strong)
    IdentityButtonItem* saveToPhotosDefaultIdentityItem;
@property(nonatomic, strong)
    TableViewSwitchItem* saveToPhotosAskEveryTimeSwitch;

@end

@implementation DownloadsSettingsTableViewController

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
  [self loadSaveToPhotosSection];
}

#pragma mark - SaveToPhotosSettingsAccountConfirmationConsumer

- (void)setIdentityButtonAvatar:(UIImage*)avatar
                           name:(NSString*)name
                          email:(NSString*)email
                         gaiaID:(NSString*)gaiaID
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
  NSIndexPath* indexPath = [self.tableViewModel indexPathForItem:switchItem];
  UITableViewCell* cell = [self.tableView cellForRowAtIndexPath:indexPath];
  TableViewSwitchCell* switchCell =
      base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
  if (switchCell.switchView.isOn != askEveryTimeSwitchOn) {
    // Systematically reconfiguring the switch would cancel its animation.
    // Instead, it is only reconfigured on "external" changes.
    [self reconfigureCellsForItems:@[ switchItem ]];
  }
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
    if (IsSaveToPhotosAccountPickerImprovementEnabled()) {
      _saveToPhotosAskEveryTimeSwitch.text = l10n_util::GetNSString(
          IDS_IOS_SAVE_TO_PHOTOS_ACCOUNT_PICKER_THIS_ACCOUNT_EVERY_TIME);
    } else {
      _saveToPhotosAskEveryTimeSwitch.text = l10n_util::GetNSString(
          IDS_IOS_SAVE_TO_PHOTOS_ACCOUNT_PICKER_ASK_EVERY_TIME);
    }
  }
  return _saveToPhotosAskEveryTimeSwitch;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  if ([cell isKindOfClass:[TableViewSwitchCell class]]) {
    TableViewSwitchCell* switchCell =
        base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
    [switchCell.switchView
               addTarget:self
                  action:@selector(saveToPhotosAskEveryTimeSwitchAction:)
        forControlEvents:UIControlEventValueChanged];
  } else if ([cell isKindOfClass:[IdentityButtonCell class]]) {
    IdentityButtonCell* identityButtonCell =
        base::apple::ObjCCastStrict<IdentityButtonCell>(cell);
    [identityButtonCell.identityButtonControl
               addTarget:self
                  action:@selector(saveToPhotosIdentityButtonAction:)
        forControlEvents:UIControlEventTouchUpInside];
  }
  return cell;
}

#pragma mark - Actions

- (void)saveToPhotosAskEveryTimeSwitchAction:(UISwitch*)sender {
  BOOL askWhichAccountToUseEveryTime = sender.isOn;
  if (IsSaveToPhotosAccountPickerImprovementEnabled()) {
    askWhichAccountToUseEveryTime = !sender.isOn;
  }
  [self.saveToPhotosSettingsMutator
      setAskWhichAccountToUseEveryTime:askWhichAccountToUseEveryTime];
  if (!askWhichAccountToUseEveryTime) {
    [self.saveToPhotosSettingsMutator
        setSelectedIdentityGaiaID:self.saveToPhotosDefaultIdentityItem
                                      .identityGaiaID];
  }
}

- (void)saveToPhotosIdentityButtonAction:(IdentityButtonControl*)sender {
  [self.actionDelegate
      downloadsSettingsTableViewControllerOpenSaveToPhotosAccountSelection:
          self];
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

@end
