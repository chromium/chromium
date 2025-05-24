// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_profile_table_view_controller.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/i18n/message_formatter.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/core/browser/data_quality/addresses/profile_requirement_utils.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#import "components/autofill/ios/common/features.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/plus_addresses/features.h"
#import "components/plus_addresses/grit/plus_addresses_strings.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/autofill_edit_profile_bottom_sheet_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/settings_autofill_edit_profile_bottom_sheet_handler.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_profile_edit_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/cells/autofill_address_profile_record_type.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/cells/autofill_profile_item.h"
#import "ios/chrome/browser/settings/ui_bundled/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller+toolbar_add.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

namespace {

// Plus Address Section header height.
const CGFloat kPlusAddressSectionHeaderHeight = 24;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSwitches = kSectionIdentifierEnumZero,
  SectionIdentifierProfiles,
  SectionIdentifierPlusAddress
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeAutofillAddressSwitch = kItemTypeEnumZero,
  ItemTypeAutofillAddressManaged,
  ItemTypeAddress,
  ItemTypeHeader,
  ItemTypeFooter,
  ItemTypePlusAddress,
  ItemTypePlusAddressFooter
};

}  // namespace

#pragma mark - AutofillProfileTableViewController

@interface AutofillProfileTableViewController () <
    AutofillProfileEditCoordinatorDelegate,
    PersonalDataManagerObserver,
    PopoverLabelViewControllerDelegate> {
  raw_ptr<autofill::PersonalDataManager> _personalDataManager;

  raw_ptr<Browser> _browser;
  std::unique_ptr<autofill::PersonalDataManagerObserverBridge> _observer;

  // Deleting profiles updates PersonalDataManager resulting in an observer
  // callback, which handles general data updates with a reloadData.
  // It is better to handle user-initiated changes with more specific actions
  // such as inserting or removing items/sections. This boolean is used to
  // stop the observer callback from acting on user-initiated changes.
  BOOL _deletionInProgress;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;

  // The account email of the signed-in user, or nil if there is no
  // signed-in user.
  NSString* _userEmail;

  // Coordinator that managers a UIAlertController to delete addresses.
  ActionSheetCoordinator* _deletionSheetCoordinator;

  // Coordinator to view/edit profile details.
  AutofillProfileEditCoordinator* _autofillProfileEditCoordinator;

  // Add button for the toolbar, which allows the user to manually add a new
  // address.
  UIBarButtonItem* _addButtonInToolbar;

  // Handler used to manage the settings workflow for manually adding an
  // address.
  SettingsAutofillEditProfileBottomSheetHandler* _addProfileBottomSheetHandler;

  // Coordinator to present and manage the bottom sheet for manually adding an
  // address.
  AutofillEditProfileBottomSheetCoordinator*
      _autofillAddProfileBottomSheetCoordinator;
}

@property(nonatomic, getter=isAutofillProfileEnabled)
    BOOL autofillProfileEnabled;

@end

@implementation AutofillProfileTableViewController

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE);
    self.shouldDisableDoneButtonOnEdit = YES;
    _browser = browser;
    _personalDataManager = autofill::PersonalDataManagerFactory::GetForProfile(
        _browser->GetProfile());
    _observer.reset(new autofill::PersonalDataManagerObserverBridge(self));
    _personalDataManager->AddObserver(_observer.get());
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.allowsMultipleSelectionDuringEditing = YES;
  self.tableView.accessibilityIdentifier = kAutofillProfileTableViewID;
  [self determineUserEmail];
  [self updateUIForEditState];
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  if (_settingsAreDismissed) {
    return;
  }

  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSwitches];

  if (_browser->GetProfile()->GetPrefs()->IsManagedPreference(
          autofill::prefs::kAutofillProfileEnabled)) {
    [model addItem:[self managedAddressItem]
        toSectionWithIdentifier:SectionIdentifierSwitches];
  } else {
    [model addItem:[self addressSwitchItem]
        toSectionWithIdentifier:SectionIdentifierSwitches];
  }

  [model setFooter:[self addressSwitchFooter]
      forSectionWithIdentifier:SectionIdentifierSwitches];

  if (base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressesEnabled) &&
      _userEmail) {
    [model addSectionWithIdentifier:SectionIdentifierPlusAddress];
    [model addItem:[self plusAddressItem]
        toSectionWithIdentifier:SectionIdentifierPlusAddress];

    [model setFooter:[self plusAddressFooter]
        forSectionWithIdentifier:SectionIdentifierPlusAddress];
  }

  [self populateProfileSection];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  self.navigationController.toolbarHidden = NO;
}

#pragma mark - LoadModel Helpers

// Populates profile section using personalDataManager.
- (void)populateProfileSection {
  if (_settingsAreDismissed) {
    return;
  }

  TableViewModel* model = self.tableViewModel;
  const std::vector<const autofill::AutofillProfile*> autofillProfiles =
      _personalDataManager->address_data_manager().GetProfilesForSettings();
  if (!autofillProfiles.empty()) {
    [model addSectionWithIdentifier:SectionIdentifierProfiles];
    [model setHeader:[self profileSectionHeader]
        forSectionWithIdentifier:SectionIdentifierProfiles];
    for (const autofill::AutofillProfile* autofillProfile : autofillProfiles) {
      DCHECK(autofillProfile);
      [model addItem:[self itemForProfile:*autofillProfile]
          toSectionWithIdentifier:SectionIdentifierProfiles];
    }
  }
}

- (TableViewItem*)addressSwitchItem {
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeAutofillAddressSwitch];
  switchItem.text =
      l10n_util::GetNSString(IDS_AUTOFILL_ENABLE_PROFILES_TOGGLE_LABEL);
  switchItem.on = [self isAutofillProfileEnabled];
  switchItem.accessibilityIdentifier = kAutofillAddressSwitchViewId;
  return switchItem;
}

- (TableViewItem*)plusAddressItem {
  TableViewDetailTextItem* plusAddressItem =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypePlusAddress];

  plusAddressItem.text =
      l10n_util::GetNSString(IDS_PLUS_ADDRESS_SETTINGS_LABEL);
  plusAddressItem.accessorySymbol =
      TableViewDetailTextCellAccessorySymbolExternalLink;
  return plusAddressItem;
}

- (TableViewHeaderFooterItem*)plusAddressFooter {
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypePlusAddressFooter];
  footer.text = l10n_util::GetNSString(IDS_PLUS_ADDRESS_SETTINGS_SUBLABEL);
  return footer;
}

- (TableViewInfoButtonItem*)managedAddressItem {
  TableViewInfoButtonItem* managedAddressItem = [[TableViewInfoButtonItem alloc]
      initWithType:ItemTypeAutofillAddressManaged];
  managedAddressItem.text =
      l10n_util::GetNSString(IDS_AUTOFILL_ENABLE_PROFILES_TOGGLE_LABEL);
  // The status could only be off when the pref is managed.
  managedAddressItem.statusText = l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  managedAddressItem.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT);
  managedAddressItem.accessibilityIdentifier = kAutofillAddressManagedViewId;
  return managedAddressItem;
}

- (TableViewHeaderFooterItem*)addressSwitchFooter {
  TableViewLinkHeaderFooterItem* footer =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  footer.text =
      l10n_util::GetNSString(IDS_AUTOFILL_ENABLE_PROFILES_TOGGLE_SUBLABEL);
  return footer;
}

- (TableViewHeaderFooterItem*)profileSectionHeader {
  TableViewTextHeaderFooterItem* header =
      [[TableViewTextHeaderFooterItem alloc] initWithType:ItemTypeHeader];
  header.text = l10n_util::GetNSString(IDS_AUTOFILL_ADDRESSES);
  return header;
}

- (TableViewItem*)itemForProfile:
    (const autofill::AutofillProfile&)autofillProfile {
  std::string guid(autofillProfile.guid());
  NSString* title = base::SysUTF16ToNSString(
      autofillProfile.GetInfo(autofill::AutofillType(autofill::NAME_FULL),
                              GetApplicationContext()->GetApplicationLocale()));
  NSString* subTitle = base::SysUTF16ToNSString(autofillProfile.GetInfo(
      autofill::AutofillType(autofill::ADDRESS_HOME_LINE1),
      GetApplicationContext()->GetApplicationLocale()));

  AutofillProfileItem* item =
      [[AutofillProfileItem alloc] initWithType:ItemTypeAddress];
  item.title = title;
  item.detailText = subTitle;

  item.deletable = YES;
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableSupportForHomeAndWork)) {
    autofill::AutofillProfile::RecordType recordType =
        autofillProfile.record_type();
    if (recordType == autofill::AutofillProfile::RecordType::kAccountHome) {
      item.trailingDetailText =
          l10n_util::GetNSString(IDS_IOS_PROFILE_RECORD_TYPE_HOME);
      item.deletable = NO;
    } else if (recordType ==
               autofill::AutofillProfile::RecordType::kAccountWork) {
      item.trailingDetailText =
          l10n_util::GetNSString(IDS_IOS_PROFILE_RECORD_TYPE_WORK);
      item.deletable = NO;
    }
  }

  item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  item.accessibilityIdentifier = title;
  item.GUID = guid;
  item.showMigrateToAccountButton = NO;
  item.localProfileIconShown = NO;
  if (autofillProfile.IsAccountProfile()) {
    item.autofillProfileRecordType =
        AutofillAddressProfileRecordType::AutofillAccountProfile;
  } else {
    item.autofillProfileRecordType = AutofillLocalProfile;
    if ([self shouldShowCloudOffIconForProfile:autofillProfile]) {
      item.showMigrateToAccountButton = YES;
      item.image = CustomSymbolTemplateWithPointSize(
          kCloudSlashSymbol, kCloudSlashSymbolPointSize);
      item.localProfileIconShown = YES;
    }
  }
  return item;
}

- (BOOL)localProfilesExist {
  return !_settingsAreDismissed && !_personalDataManager->address_data_manager()
                                        .GetProfilesForSettings()
                                        .empty();
}

#pragma mark - SettingsControllerProtocol

- (void)reportDismissalUserAction {
  base::RecordAction(base::UserMetricsAction("MobileAddressesSettingsClose"));
}

- (void)reportBackUserAction {
  base::RecordAction(base::UserMetricsAction("MobileAddressesSettingsBack"));
}

- (void)settingsWillBeDismissed {
  DCHECK(!_settingsAreDismissed);

  [self stopAutofillEditProfileBottomSheetCoordinator];

  [self stopAutofillProfileEditCoordinator];
  _personalDataManager->RemoveObserver(_observer.get());
  [self dismissDeletionSheet];

  // Remove observer bridges.
  _observer.reset();

  // Clear C++ ivars.
  _personalDataManager = nullptr;
  _browser = nullptr;

  _settingsAreDismissed = YES;
}

#pragma mark - SettingsRootTableViewController

- (BOOL)editButtonEnabled {
  return [self localProfilesExist];
}

- (BOOL)shouldHideToolbar {
  // Hide the toolbar if the visible view controller is not the current view
  // controller or the `deletionSheetCoordinator` is shown.
  if (self.navigationController.visibleViewController == self) {
    return NO;
  } else if (_deletionSheetCoordinator != nil) {
    return ![_deletionSheetCoordinator isVisible];
  }
  // TODO(crbug.com/407298266): Temporarily keep the toolbar visible when this
  // view controller is at the top of the navigation stack.
  else if (IsAddAddressManuallyEnabled() &&
           self.navigationController.topViewController == self) {
    return NO;
  }
  return YES;
}

- (BOOL)shouldShowEditDoneButton {
  return NO;
}

- (void)updateUIForEditState {
  [super updateUIForEditState];
  [self setSwitchItemEnabled:!self.tableView.editing
                    itemType:ItemTypeAutofillAddressSwitch];
  [self updatedToolbarForEditState];
}

// Override.
- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths {
  [self showDeletionConfirmationForIndexPaths:indexPaths];
}

- (UIBarButtonItem*)customLeftToolbarButton {
  // When in edit mode, a "Delete" button is shown as the left toolbar button.
  // This button shouldn't be overridden with a custom one.
  if (self.tableView.isEditing || !IsAddAddressManuallyEnabled()) {
    return nil;
  }

  return self.addButtonInToolbar;
}

#pragma mark - UITableViewDelegate

- (CGFloat)tableView:(UITableView*)tableView
    heightForHeaderInSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];

  if (base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressesEnabled) &&
      sectionIdentifier == SectionIdentifierPlusAddress) {
    return kPlusAddressSectionHeaderHeight;
  }

  return [super tableView:tableView heightForHeaderInSection:section];
}

- (CGFloat)tableView:(UITableView*)tableView
    heightForFooterInSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.tableViewModel sectionIdentifierForSectionIndex:section];

  if (base::FeatureList::IsEnabled(
          plus_addresses::features::kPlusAddressesEnabled) &&
      sectionIdentifier == SectionIdentifierPlusAddress) {
    return kTableViewHeaderFooterViewHeight;
  }

  return [super tableView:tableView heightForFooterInSection:section];
}

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  [super setEditing:editing animated:animated];
  if (_settingsAreDismissed) {
    return;
  }

  [self updateUIForEditState];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  if (_settingsAreDismissed) {
    return;
  }

  // Edit mode is the state where the user can select and delete entries. In
  // edit mode, selection is handled by the superclass. When not in edit mode
  // selection presents the editing controller for the selected entry.
  if ([self.tableView isEditing]) {
    self.deleteButton.enabled = YES;
    return;
  }

  if ([self.tableViewModel itemTypeForIndexPath:indexPath] ==
      ItemTypePlusAddress) {
    base::RecordAction(
        base::UserMetricsAction("Settings.ManageOptionOnSettingsSelected"));
    OpenNewTabCommand* command = [OpenNewTabCommand
        commandWithURLFromChrome:
            GURL(plus_addresses::features::kPlusAddressManagementUrl.Get())];
    [self.applicationHandler closePresentedViewsAndOpenURL:command];
    return;
  }

  if (![self isItemTypeForIndexPathAddress:indexPath]) {
    return;
  }

  AutofillProfileItem* item = base::apple::ObjCCastStrict<AutofillProfileItem>(
      [self.tableViewModel itemAtIndexPath:indexPath]);
  [self
      showAddressProfileDetailsPageForProfile:_personalDataManager
                                                  ->address_data_manager()
                                                  .GetProfileByGUID(item.GUID)
                   withMigrateToAccountButton:item.showMigrateToAccountButton];
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didDeselectRowAtIndexPath:indexPath];
  if (_settingsAreDismissed || !self.tableView.editing) {
    return;
  }

  if (self.tableView.indexPathsForSelectedRows.count == 0) {
    self.deleteButton.enabled = NO;
  }
}

#pragma mark - Actions

// Called when the user clicks on the information button of the managed
// setting's UI. Shows a textual bubble with the information of the enterprise.
- (void)didTapManagedUIInfoButton:(UIButton*)buttonView {
  if (_settingsAreDismissed) {
    return;
  }

  EnterpriseInfoPopoverViewController* bubbleViewController =
      [[EnterpriseInfoPopoverViewController alloc] initWithEnterpriseName:nil];
  bubbleViewController.delegate = self;
  [self presentViewController:bubbleViewController animated:YES completion:nil];

  // Disable the button when showing the bubble.
  buttonView.enabled = NO;

  // Set the anchor and arrow direction of the bubble.
  bubbleViewController.popoverPresentationController.sourceView = buttonView;
  bubbleViewController.popoverPresentationController.sourceRect =
      buttonView.bounds;
  bubbleViewController.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;
}

#pragma mark - UITableViewDataSource

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  if (_settingsAreDismissed) {
    return NO;
  }

  if (![self isItemTypeForIndexPathAddress:indexPath]) {
    return NO;
  }

  AutofillProfileItem* item = base::apple::ObjCCastStrict<AutofillProfileItem>(
      [self.tableViewModel itemAtIndexPath:indexPath]);

  return [item isDeletable];
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  if (editingStyle != UITableViewCellEditingStyleDelete ||
      _settingsAreDismissed) {
    return;
  }
  [self deleteItems:@[ indexPath ]];
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  UIView* selectedBackgroundView = [[UIView alloc] init];
  selectedBackgroundView.backgroundColor =
      [UIColor colorNamed:kUpdatedTertiaryBackgroundColor];
  cell.selectedBackgroundView = selectedBackgroundView;
  if (_settingsAreDismissed) {
    return cell;
  }

  switch (static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath])) {
    case ItemTypeAddress:
    case ItemTypeHeader:
    case ItemTypeFooter:
    case ItemTypePlusAddress:
    case ItemTypePlusAddressFooter:
      break;
    case ItemTypeAutofillAddressSwitch: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(autofillAddressSwitchChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeAutofillAddressManaged: {
      TableViewInfoButtonCell* managedCell =
          base::apple::ObjCCastStrict<TableViewInfoButtonCell>(cell);
      [managedCell.trailingButton
                 addTarget:self
                    action:@selector(didTapManagedUIInfoButton:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
  }

  return cell;
}

#pragma mark - Switch Callbacks

- (void)autofillAddressSwitchChanged:(UISwitch*)switchView {
  BOOL switchOn = [switchView isOn];
  [self setSwitchItemOn:switchOn itemType:ItemTypeAutofillAddressSwitch];
  [self setAutofillProfileEnabled:switchOn];
  _addButtonInToolbar.enabled = switchOn;
}

#pragma mark - Switch Helpers

// Sets switchItem's state to `on`. It is important that there is only one item
// of `switchItemType` in SectionIdentifierSwitches.
- (void)setSwitchItemOn:(BOOL)on itemType:(ItemType)switchItemType {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:switchItemType
                              sectionIdentifier:SectionIdentifierSwitches];
  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [self.tableViewModel itemAtIndexPath:switchPath]);
  switchItem.on = on;
}

// Sets switchItem's enabled status to `enabled` and reconfigures the
// corresponding cell. It is important that there is no more than one item of
// `switchItemType` in SectionIdentifierSwitches.
- (void)setSwitchItemEnabled:(BOOL)enabled itemType:(ItemType)switchItemType {
  TableViewModel* model = self.tableViewModel;

  if (![model hasItemForItemType:switchItemType
               sectionIdentifier:SectionIdentifierSwitches]) {
    return;
  }
  NSIndexPath* switchPath =
      [model indexPathForItemType:switchItemType
                sectionIdentifier:SectionIdentifierSwitches];
  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [model itemAtIndexPath:switchPath]);
  [switchItem setEnabled:enabled];
  [self reconfigureCellsForItems:@[ switchItem ]];
}

#pragma mark - PersonalDataManagerObserver

- (void)onPersonalDataChanged {
  if (_deletionInProgress) {
    return;
  }

  if ([self.tableView isEditing]) {
    // Turn off edit mode.
    [self setEditing:NO animated:NO];
  }

  [self determineUserEmail];
  [self updateUIForEditState];
  [self reloadData];
}

#pragma mark - Getters and Setter

- (BOOL)isAutofillProfileEnabled {
  return autofill::prefs::IsAutofillProfileEnabled(
      _browser->GetProfile()->GetPrefs());
}

- (void)setAutofillProfileEnabled:(BOOL)isEnabled {
  return autofill::prefs::SetAutofillProfileEnabled(
      _browser->GetProfile()->GetPrefs(), isEnabled);
}

- (void)determineUserEmail {
  _userEmail = nil;
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForProfile(_browser->GetProfile());
  CHECK(authenticationService);
  id<SystemIdentity> identity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSignin);
  if (identity) {
    _userEmail = identity.userEmail;
  }
}

- (UIBarButtonItem*)addButtonInToolbar {
  if (!_addButtonInToolbar) {
    _addButtonInToolbar =
        [self addButtonWithAction:@selector(handleAddAddress)];
    _addButtonInToolbar.enabled = [self isAutofillProfileEnabled];
  }
  return _addButtonInToolbar;
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [self view:nil didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]];
}

#pragma mark - AutofillProfileEditCoordinatorDelegate

- (void)autofillProfileEditCoordinatorTableViewControllerDidFinish:
    (AutofillProfileEditCoordinator*)coordinator {
  DCHECK_EQ(_autofillProfileEditCoordinator, coordinator);
  [self stopAutofillProfileEditCoordinator];
}

#pragma mark - Private
- (void)dismissDeletionSheet {
  [_deletionSheetCoordinator stop];
  _deletionSheetCoordinator = nil;
}

- (void)stopAutofillProfileEditCoordinator {
  _autofillProfileEditCoordinator.delegate = nil;
  [_autofillProfileEditCoordinator stop];
  _autofillProfileEditCoordinator = nil;
}

- (void)stopAutofillEditProfileBottomSheetCoordinator {
  [_autofillAddProfileBottomSheetCoordinator stop];
  _autofillAddProfileBottomSheetCoordinator = nil;
  _addProfileBottomSheetHandler = nil;
}

// Removes the item from the personal data manager model.
- (void)willDeleteItemsAtIndexPaths:(NSArray*)indexPaths {
  if (_settingsAreDismissed) {
    return;
  }

  _deletionInProgress = YES;
  for (NSIndexPath* indexPath in indexPaths) {
    AutofillProfileItem* item =
        base::apple::ObjCCastStrict<AutofillProfileItem>(
            [self.tableViewModel itemAtIndexPath:indexPath]);
    _personalDataManager->address_data_manager().RemoveProfile([item GUID]);
  }

  [self.tableView
      performBatchUpdates:^{
        [self removeFromModelItemAtIndexPaths:indexPaths];
        [self.tableView
            deleteRowsAtIndexPaths:indexPaths
                  withRowAnimation:UITableViewRowAnimationAutomatic];
      }
               completion:nil];
}

// Remove the section from the model and collectionView if there are no more
// items in the section.
- (void)removeSectionIfEmptyForSectionWithIdentifier:
    (SectionIdentifier)sectionIdentifier {
  if (_settingsAreDismissed ||
      ![self.tableViewModel hasSectionForSectionIdentifier:sectionIdentifier]) {
    _deletionInProgress = NO;
    return;
  }
  NSInteger section =
      [self.tableViewModel sectionForSectionIdentifier:sectionIdentifier];
  if ([self.tableView numberOfRowsInSection:section] == 0) {
    // Avoid reference cycle in block.
    __weak AutofillProfileTableViewController* weakSelf = self;
    [self.tableView
        performBatchUpdates:^{
          // Obtain strong reference again.
          AutofillProfileTableViewController* strongSelf = weakSelf;
          if (!strongSelf) {
            return;
          }

          // Remove section from model and collectionView.
          [[strongSelf tableViewModel]
              removeSectionWithIdentifier:sectionIdentifier];
          [[strongSelf tableView]
                deleteSections:[NSIndexSet indexSetWithIndex:section]
              withRowAnimation:UITableViewRowAnimationAutomatic];
        }
        completion:^(BOOL finished) {
          // Obtain strong reference again.
          AutofillProfileTableViewController* strongSelf = weakSelf;
          if (!strongSelf) {
            return;
          }

          // Turn off edit mode if there is nothing to edit.
          if (![strongSelf localProfilesExist] &&
              [strongSelf.tableView isEditing]) {
            [strongSelf setEditing:NO animated:YES];
          }
          [strongSelf updateUIForEditState];
          strongSelf->_deletionInProgress = NO;
        }];
  } else {
    _deletionInProgress = NO;
  }
}

// Shows the action sheet asking for the confirmation on delete from the user.
- (void)showDeletionConfirmationForIndexPaths:
    (NSArray<NSIndexPath*>*)indexPaths {
  BOOL accountProfiles = NO;

  int profileCount = 0;

  for (NSIndexPath* indexPath in indexPaths) {
    if (![self isItemTypeForIndexPathAddress:indexPath]) {
      continue;
    }
    profileCount++;
    AutofillProfileItem* item =
        base::apple::ObjCCastStrict<AutofillProfileItem>(
            [self.tableViewModel itemAtIndexPath:indexPath]);
    switch (item.autofillProfileRecordType) {
      case AutofillAccountProfile:
        accountProfiles = YES;
        break;
      case AutofillLocalProfile:
        break;
    }
  }

  // Can happen if user presses delete in quick succesion.
  if (!profileCount) {
    return;
  }

  NSString* deletionConfirmationString =
      [self getDeletionConfirmationStringUsingProfileCount:profileCount
                                           accountProfiles:accountProfiles];
  _deletionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser
                           title:deletionConfirmationString
                         message:nil
                   barButtonItem:self.deleteButton];

  if (UIContentSizeCategoryIsAccessibilityCategory(
          UIApplication.sharedApplication.preferredContentSizeCategory)) {
    _deletionSheetCoordinator.alertStyle = UIAlertControllerStyleAlert;
  }

  _deletionSheetCoordinator.popoverArrowDirection = UIPopoverArrowDirectionAny;
  __weak AutofillProfileTableViewController* weakSelf = self;
  [_deletionSheetCoordinator
      addItemWithTitle:
          l10n_util::GetPluralNSStringF(
              IDS_IOS_SETTINGS_AUTOFILL_DELETE_ADDRESS_CONFIRMATION_BUTTON,
              profileCount)
                action:^{
                  [weakSelf willDeleteItemsAtIndexPaths:indexPaths];
                  // TODO(crbug.com/41277594) Generalize removing empty sections
                  [weakSelf removeSectionIfEmptyForSectionWithIdentifier:
                                SectionIdentifierProfiles];
                  [weakSelf dismissDeletionSheet];
                }
                 style:UIAlertActionStyleDestructive];
  [_deletionSheetCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_APP_CANCEL)
                action:^{
                  [weakSelf dismissDeletionSheet];
                }
                 style:UIAlertActionStyleCancel];
  [_deletionSheetCoordinator start];
}

// Returns the deletion confirmation message string based on
// `profileCount` and if it the source has any `accountProfiles`.
- (NSString*)getDeletionConfirmationStringUsingProfileCount:(int)profileCount
                                            accountProfiles:
                                                (BOOL)accountProfiles {
  if (accountProfiles) {
    std::u16string pattern = l10n_util::GetStringUTF16(
        IDS_IOS_SETTINGS_AUTOFILL_DELETE_ACCOUNT_ADDRESS_CONFIRMATION_TITLE);
    std::u16string confirmationString =
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            pattern, "email", base::SysNSStringToUTF16(_userEmail), "count",
            profileCount);
    return base::SysUTF16ToNSString(confirmationString);
  }
  return l10n_util::GetPluralNSStringF(
      IDS_IOS_SETTINGS_AUTOFILL_DELETE_LOCAL_ADDRESS_CONFIRMATION_TITLE,
      profileCount);
}

// Returns true when the item type for `indexPath` is Address.
- (BOOL)isItemTypeForIndexPathAddress:(NSIndexPath*)indexPath {
  return
      [self.tableViewModel itemTypeForIndexPath:indexPath] == ItemTypeAddress;
}

- (void)showAddressProfileDetailsPageForProfile:
            (const autofill::AutofillProfile*)profile
                     withMigrateToAccountButton:(BOOL)migrateToAccountButton {
  _autofillProfileEditCoordinator = [[AutofillProfileEditCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:_browser
                               profile:*profile
                migrateToAccountButton:migrateToAccountButton];
  _autofillProfileEditCoordinator.delegate = self;
  [_autofillProfileEditCoordinator start];
}

// Returns YES if the cloud off icon should be shown next to the profile. Only
// those profiles, that are eligible for the migration to Account show cloud off
// icon.
- (BOOL)shouldShowCloudOffIconForProfile:
    (const autofill::AutofillProfile&)profile {
  return IsEligibleForMigrationToAccount(
             _personalDataManager->address_data_manager(), profile) &&
         _userEmail != nil;
}

// Opens a new view controller `AutofillAddAddressViewController` for filling
// and saving an address.
- (void)handleAddAddress {
  if (_settingsAreDismissed) {
    return;
  }

  base::RecordAction(base::UserMetricsAction("AddAddressManually_Opened"));

  autofill::AddressDataManager& addressDataManager =
      _personalDataManager->address_data_manager();

  _addProfileBottomSheetHandler =
      [[SettingsAutofillEditProfileBottomSheetHandler alloc]
          initWithAddressDataManager:&addressDataManager
                           userEmail:_userEmail];

  _autofillAddProfileBottomSheetCoordinator =
      [[AutofillEditProfileBottomSheetCoordinator alloc]
          initWithBaseViewController:self
                             browser:_browser
                             handler:_addProfileBottomSheetHandler];
  [_autofillAddProfileBottomSheetCoordinator start];
}

@end
