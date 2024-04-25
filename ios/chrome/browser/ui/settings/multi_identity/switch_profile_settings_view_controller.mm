// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/multi_identity/switch_profile_settings_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/authentication/cells/table_view_account_item.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// List of sections.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  CurrentProfilesIdentifier = kSectionIdentifierEnumZero,
  LoadedProfilesIdentifier,
};

// List of items.
typedef NS_ENUM(NSInteger, ItemType) {
  SwitchProfileButton = kItemTypeEnumZero,
  CurrentAccount,
  ItemTypeAccount,
};

}  // namespace

@implementation SwitchProfileSettingsTableViewController {
}

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title =
        l10n_util::GetNSString(IDS_IOS_SWITCH_PROFILE_MANAGEMENT_SETTINGS);
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.estimatedRowHeight = 70;
  self.tableView.rowHeight = UITableViewAutomaticDimension;

  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:CurrentProfilesIdentifier];
  [model addSectionWithIdentifier:LoadedProfilesIdentifier];

  // CurrentProfilesIdentifier.
  TableViewTextHeaderFooterItem* currentProfileTitle =
      [[TableViewTextHeaderFooterItem alloc] initWithType:kItemTypeEnumZero];
  currentProfileTitle.text =
      l10n_util::GetNSString(IDS_IOS_SWITCH_PROFILE_CURRENT_PROFILE_TITLE);
  [model setHeader:currentProfileTitle
      forSectionWithIdentifier:CurrentProfilesIdentifier];

  TableViewAccountItem* currentProfileDetail =
      [[TableViewAccountItem alloc] initWithType:CurrentAccount];
  currentProfileDetail.image = ios::provider::GetSigninDefaultAvatar();
  currentProfileDetail.text = self.activeBrowserStateName;
  currentProfileDetail.mode = TableViewAccountModeNonTappable;
  [model addItem:currentProfileDetail
      toSectionWithIdentifier:CurrentProfilesIdentifier];

  // LoadedProfilesIdentifier.
  TableViewTextHeaderFooterItem* switchToProfileTitle =
      [[TableViewTextHeaderFooterItem alloc] initWithType:kItemTypeEnumZero];
  switchToProfileTitle.text =
      l10n_util::GetNSString(IDS_IOS_SWITCH_PROFILE_SWITCH_TITLE);
  switchToProfileTitle.subtitle =
      l10n_util::GetNSString(IDS_IOS_SWITCH_PROFILE_SWITCH_SUBTITLE);
  [model setHeader:switchToProfileTitle
      forSectionWithIdentifier:LoadedProfilesIdentifier];

  PrefService* localState = GetApplicationContext()->GetLocalState();
  // TODO(crbug.com/336767700): kBrowserStatesLastActive should not be used
  // here. Use a new prefService key (containing also info of not loaded
  // browserStates) once available.
  const base::Value::List& lastActiveBrowserStates =
      localState->GetList(prefs::kBrowserStatesLastActive);
  for (const auto& browserStateName : lastActiveBrowserStates) {
    TableViewAccountItem* accountItemDetail =
        [[TableViewAccountItem alloc] initWithType:ItemTypeAccount];
    accountItemDetail.image = ios::provider::GetSigninDefaultAvatar();
    accountItemDetail.text =
        base::SysUTF8ToNSString(browserStateName.GetString());
    if ([accountItemDetail.text isEqualToString:self.activeBrowserStateName]) {
      accountItemDetail.mode = TableViewAccountModeDisabled;
    }
    [model addItem:accountItemDetail
        toSectionWithIdentifier:LoadedProfilesIdentifier];
  }

  TableViewTextButtonItem* switchProfileButtonItem =
      [[TableViewTextButtonItem alloc] initWithType:SwitchProfileButton];
  switchProfileButtonItem.buttonText =
      l10n_util::GetNSString(IDS_IOS_SWITCH_PROFILE_MANAGEMENT_SETTINGS);
  [model addItem:switchProfileButtonItem
      toSectionWithIdentifier:LoadedProfilesIdentifier];
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  ItemType itemType = static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath]);
  TableViewTextButtonCell* tableViewTextButtonCell =
      base::apple::ObjCCast<TableViewTextButtonCell>(cell);
  if (itemType == SwitchProfileButton) {
    [tableViewTextButtonCell.button
               addTarget:self
                  action:@selector(switchProfileButtonWasTapped)
        forControlEvents:UIControlEventTouchUpInside];
  }
  return cell;
}

#pragma mark - Private

- (void)switchProfileButtonWasTapped {
  // TODO(crbug.com/333520714): Add logic once the API is available.
}

@end
