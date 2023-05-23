// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_table_view_controller.h"

#import "base/check.h"
#import "base/i18n/message_formatter.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
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
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_constants.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_coordinator.h"
#import "ios/chrome/browser/ui/settings/autofill/cells/autofill_address_profile_source.h"
#import "ios/chrome/browser/ui/settings/autofill/cells/autofill_profile_item.h"
#import "ios/chrome/browser/ui/settings/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/mac/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSwitches = kSectionIdentifierEnumZero,
  SectionIdentifierProfiles,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeAutofillAddressSwitch = kItemTypeEnumZero,
  ItemTypeAutofillAddressManaged,
  ItemTypeAddress,
  ItemTypeHeader,
  ItemTypeFooter,
};

}  // namespace

#pragma mark - AutofillProfileTableViewController

@interface AutofillProfileTableViewController () <
    AutofillProfileEditCoordinatorDelegate,
    PersonalDataManagerObserver,
    PopoverLabelViewControllerDelegate> {
  autofill::PersonalDataManager* _personalDataManager;

  Browser* _browser;
  std::unique_ptr<autofill::PersonalDataManagerObserverBridge> _observer;

  // Deleting profiles updates PersonalDataManager resulting in an observer
  // callback, which handles general data updates with a reloadData.
  // It is better to handle user-initiated changes with more specific actions
  // such as inserting or removing items/sections. This boolean is used to
  // stop the observer callback from acting on user-initiated changes.
  BOOL _deletionInProgress;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;
}

@property(nonatomic, getter=isAutofillProfileEnabled)
    BOOL autofillProfileEnabled;

// If the syncing is enabled, stores the signed in user's email.
@property(nonatomic, strong) NSString* syncingUserEmail;

// Default NO. YES, when the autofill syncing is enabled.
@property(nonatomic, assign, getter=isSyncEnabled) BOOL syncEnabled;

// Default NO. YES, if the user is signed in.
@property(nonatomic, assign) BOOL signedIn;

// Coordinator that managers a UIAlertController to delete addresses.
@property(nonatomic, strong) ActionSheetCoordinator* deletionSheetCoordinator;

// Coordinator to view/edit profile details.
@property(nonatomic, strong)
    AutofillProfileEditCoordinator* autofillProfileEditCoordinator;

@end

@implementation AutofillProfileTableViewController

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title = l10n_util::GetNSString(IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE);
    self.shouldDisableDoneButtonOnEdit = YES;
    _browser = browser;
    _personalDataManager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(
            _browser->GetBrowserState());
    _observer.reset(new autofill::PersonalDataManagerObserverBridge(self));
    _personalDataManager->AddObserver(_observer.get());
  }
  return self;
}

- (void)dealloc {
  if (!_settingsAreDismissed)
    _personalDataManager->RemoveObserver(_observer.get());
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.allowsMultipleSelectionDuringEditing = YES;
  self.tableView.accessibilityIdentifier = kAutofillProfileTableViewID;
  self.tableView.estimatedSectionFooterHeight =
      kTableViewHeaderFooterViewHeight;
  [self setSyncingUserEmail];
  [self updateUIForEditState];
  [self loadModel];
}

- (void)loadModel {
  [super loadModel];
  if (_settingsAreDismissed)
    return;

  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSwitches];

  if (_browser->GetBrowserState()->GetPrefs()->IsManagedPreference(
          autofill::prefs::kAutofillProfileEnabled)) {
    [model addItem:[self managedAddressItem]
        toSectionWithIdentifier:SectionIdentifierSwitches];
  } else {
    [model addItem:[self addressSwitchItem]
        toSectionWithIdentifier:SectionIdentifierSwitches];
  }

  [model setFooter:[self addressSwitchFooter]
      forSectionWithIdentifier:SectionIdentifierSwitches];

  [self populateProfileSection];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  self.navigationController.toolbarHidden = NO;
}

#pragma mark - LoadModel Helpers

// Populates profile section using personalDataManager.
- (void)populateProfileSection {
  if (_settingsAreDismissed)
    return;

  TableViewModel* model = self.tableViewModel;
  const std::vector<autofill::AutofillProfile*> autofillProfiles =
      _personalDataManager->GetProfilesForSettings();
  if (!autofillProfiles.empty()) {
    [model addSectionWithIdentifier:SectionIdentifierProfiles];
    [model setHeader:[self profileSectionHeader]
        forSectionWithIdentifier:SectionIdentifierProfiles];
    for (autofill::AutofillProfile* autofillProfile : autofillProfiles) {
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
  assert(autofillProfile.record_type() ==
         autofill::AutofillProfile::LOCAL_PROFILE);
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
  item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  item.accessibilityIdentifier = title;
  item.GUID = guid;
  if (autofillProfile.source() == autofill::AutofillProfile::Source::kAccount) {
    item.autofillProfileSource =
        AutofillAddressProfileSource::AutofillAccountProfile;
  } else if (self.syncEnabled) {
    item.autofillProfileSource =
        AutofillAddressProfileSource::AutofillSyncableProfile;
  } else {
    item.autofillProfileSource = AutofillLocalProfile;
    if (base::FeatureList::IsEnabled(
            autofill::features::kAutofillAccountProfileStorage) &&
        self.signedIn) {
      item.image = CustomSymbolTemplateWithPointSize(
          kCloudSlashSymbol, kCloudSlashSymbolPointSize);
    }
  }
  return item;
}

- (BOOL)localProfilesExist {
  return !_settingsAreDismissed &&
         !_personalDataManager->GetProfilesForSettings().empty();
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

  _personalDataManager->RemoveObserver(_observer.get());

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
  } else if (self.deletionSheetCoordinator != nil) {
    return ![self.deletionSheetCoordinator isVisible];
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
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillAccountProfilesUnionView)) {
    [self showDeletionConfirmationForIndexPaths:indexPaths];
  } else {
    // If there are no index paths, return early. This can happen if the user
    // presses the Delete button twice in quick succession.
    if (![indexPaths count]) {
      return;
    }
    [self willDeleteItemsAtIndexPaths:indexPaths];
    // TODO(crbug.com/650390) Generalize removing empty sections
    [self
        removeSectionIfEmptyForSectionWithIdentifier:SectionIdentifierProfiles];
  }
}

#pragma mark - UITableViewDelegate

- (void)setEditing:(BOOL)editing animated:(BOOL)animated {
  [super setEditing:editing animated:animated];
  if (_settingsAreDismissed)
    return;

  [self updateUIForEditState];
}

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didSelectRowAtIndexPath:indexPath];
  if (_settingsAreDismissed)
    return;

  // Edit mode is the state where the user can select and delete entries. In
  // edit mode, selection is handled by the superclass. When not in edit mode
  // selection presents the editing controller for the selected entry.
  if ([self.tableView isEditing]) {
    self.deleteButton.enabled = YES;
    return;
  }

  if (![self isItemTypeForIndexPathAddress:indexPath]) {
    return;
  }

  const std::vector<autofill::AutofillProfile*> autofillProfiles =
      _personalDataManager->GetProfilesForSettings();
  [self showAddressProfileDetailsPageForProfile:*autofillProfiles[indexPath
                                                                      .item]];
  [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (void)tableView:(UITableView*)tableView
    didDeselectRowAtIndexPath:(NSIndexPath*)indexPath {
  [super tableView:tableView didDeselectRowAtIndexPath:indexPath];
  if (_settingsAreDismissed || !self.tableView.editing)
    return;

  if (self.tableView.indexPathsForSelectedRows.count == 0)
    self.deleteButton.enabled = NO;
}

#pragma mark - Actions

// Called when the user clicks on the information button of the managed
// setting's UI. Shows a textual bubble with the information of the enterprise.
- (void)didTapManagedUIInfoButton:(UIButton*)buttonView {
  if (_settingsAreDismissed)
    return;

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
  if (_settingsAreDismissed)
    return NO;

  // Only profile data cells are editable.
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  return [item isKindOfClass:[AutofillProfileItem class]];
}

- (void)tableView:(UITableView*)tableView
    commitEditingStyle:(UITableViewCellEditingStyle)editingStyle
     forRowAtIndexPath:(NSIndexPath*)indexPath {
  if (editingStyle != UITableViewCellEditingStyleDelete ||
      _settingsAreDismissed)
    return;
  [self deleteItems:@[ indexPath ]];
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];
  if (_settingsAreDismissed)
    return cell;

  switch (static_cast<ItemType>(
      [self.tableViewModel itemTypeForIndexPath:indexPath])) {
    case ItemTypeAddress:
    case ItemTypeHeader:
    case ItemTypeFooter:
      break;
    case ItemTypeAutofillAddressSwitch: {
      TableViewSwitchCell* switchCell =
          base::mac::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(autofillAddressSwitchChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeAutofillAddressManaged: {
      TableViewInfoButtonCell* managedCell =
          base::mac::ObjCCastStrict<TableViewInfoButtonCell>(cell);
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
  [self setSwitchItemOn:[switchView isOn]
               itemType:ItemTypeAutofillAddressSwitch];
  [self setAutofillProfileEnabled:[switchView isOn]];
}

#pragma mark - Switch Helpers

// Sets switchItem's state to `on`. It is important that there is only one item
// of `switchItemType` in SectionIdentifierSwitches.
- (void)setSwitchItemOn:(BOOL)on itemType:(ItemType)switchItemType {
  NSIndexPath* switchPath =
      [self.tableViewModel indexPathForItemType:switchItemType
                              sectionIdentifier:SectionIdentifierSwitches];
  TableViewSwitchItem* switchItem =
      base::mac::ObjCCastStrict<TableViewSwitchItem>(
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
      base::mac::ObjCCastStrict<TableViewSwitchItem>(
          [model itemAtIndexPath:switchPath]);
  [switchItem setEnabled:enabled];
  [self reconfigureCellsForItems:@[ switchItem ]];
}

#pragma mark - PersonalDataManagerObserver

- (void)onPersonalDataChanged {
  if (_deletionInProgress)
    return;

  if ([self.tableView isEditing]) {
    // Turn off edit mode.
    [self setEditing:NO animated:NO];
  }

  [self setSyncingUserEmail];
  [self updateUIForEditState];
  [self reloadData];
}

#pragma mark - Getters and Setter

- (BOOL)isAutofillProfileEnabled {
  return autofill::prefs::IsAutofillProfileEnabled(
      _browser->GetBrowserState()->GetPrefs());
}

- (void)setAutofillProfileEnabled:(BOOL)isEnabled {
  return autofill::prefs::SetAutofillProfileEnabled(
      _browser->GetBrowserState()->GetPrefs(), isEnabled);
}

- (void)setSyncingUserEmail {
  self.syncEnabled = NO;
  self.signedIn = NO;
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(
          _browser->GetBrowserState());
  DCHECK(authenticationService);
  id<SystemIdentity> identity =
      authenticationService->GetPrimaryIdentity(signin::ConsentLevel::kSync);
  if (identity) {
    SyncSetupService* syncSetupService =
        SyncSetupServiceFactory::GetForBrowserState(
            _browser->GetBrowserState());
    self.signedIn = YES;
    if (syncSetupService->IsDataTypeActive(syncer::AUTOFILL)) {
      self.syncingUserEmail = identity.userEmail;
      self.syncEnabled = YES;
    }
  }
}

#pragma mark - PopoverLabelViewControllerDelegate

- (void)didTapLinkURL:(NSURL*)URL {
  [self view:nil didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]];
}

#pragma mark - AutofillProfileEditCoordinatorDelegate

- (void)autofillProfileEditCoordinatorTableViewControllerDidFinish:
    (AutofillProfileEditCoordinator*)coordinator {
  DCHECK_EQ(self.autofillProfileEditCoordinator, coordinator);
  self.autofillProfileEditCoordinator.delegate = nil;
  self.autofillProfileEditCoordinator = nil;
}

#pragma mark - Private

// Removes the item from the personal data manager model.
- (void)willDeleteItemsAtIndexPaths:(NSArray*)indexPaths {
  if (_settingsAreDismissed)
    return;

  _deletionInProgress = YES;
  for (NSIndexPath* indexPath in indexPaths) {
    AutofillProfileItem* item = base::mac::ObjCCastStrict<AutofillProfileItem>(
        [self.tableViewModel itemAtIndexPath:indexPath]);
    _personalDataManager->RemoveByGUID([item GUID]);
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
  BOOL syncProfiles = NO;

  int profileCount = 0;

  for (NSIndexPath* indexPath in indexPaths) {
    if (![self isItemTypeForIndexPathAddress:indexPath]) {
      continue;
    }
    profileCount++;
    AutofillProfileItem* item = base::mac::ObjCCastStrict<AutofillProfileItem>(
        [self.tableViewModel itemAtIndexPath:indexPath]);
    switch (item.autofillProfileSource) {
      case AutofillAccountProfile:
        accountProfiles = YES;
        break;
      case AutofillSyncableProfile:
        syncProfiles = YES;
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
                                           accountProfiles:accountProfiles
                                              syncProfiles:syncProfiles];
  self.deletionSheetCoordinator = [[ActionSheetCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser
                           title:deletionConfirmationString
                         message:nil
                   barButtonItem:self.deleteButton];

  if (UIContentSizeCategoryIsAccessibilityCategory(
          UIApplication.sharedApplication.preferredContentSizeCategory)) {
    self.deletionSheetCoordinator.alertStyle = UIAlertControllerStyleAlert;
  }

  self.deletionSheetCoordinator.popoverArrowDirection =
      UIPopoverArrowDirectionAny;
  __weak AutofillProfileTableViewController* weakSelf = self;
  [self.deletionSheetCoordinator
      addItemWithTitle:
          l10n_util::GetPluralNSStringF(
              IDS_IOS_SETTINGS_AUTOFILL_DELETE_ADDRESS_CONFIRMATION_BUTTON,
              profileCount)
                action:^{
                  [weakSelf willDeleteItemsAtIndexPaths:indexPaths];
                  // TODO(crbug.com/650390) Generalize removing empty sections
                  [weakSelf removeSectionIfEmptyForSectionWithIdentifier:
                                SectionIdentifierProfiles];
                }
                 style:UIAlertActionStyleDestructive];
  [self.deletionSheetCoordinator start];
}

// Returns the deletion confirmation message string based on
// `profileCount` and if it the source has any `accountProfiles` or
// `syncProfiles`.
- (NSString*)getDeletionConfirmationStringUsingProfileCount:(int)profileCount
                                            accountProfiles:
                                                (BOOL)accountProfiles
                                               syncProfiles:(BOOL)syncProfiles {
  if (accountProfiles) {
    std::u16string pattern = l10n_util::GetStringUTF16(
        IDS_IOS_SETTINGS_AUTOFILL_DELETE_ACCOUNT_ADDRESS_CONFIRMATION_TITLE);
    std::u16string confirmationString =
        base::i18n::MessageFormatter::FormatWithNamedArgs(
            pattern, "email", base::SysNSStringToUTF16(self.syncingUserEmail),
            "count", profileCount);
    return base::SysUTF16ToNSString(confirmationString);
  }
  if (syncProfiles) {
    return l10n_util::GetPluralNSStringF(
        IDS_IOS_SETTINGS_AUTOFILL_DELETE_SYNC_ADDRESS_CONFIRMATION_TITLE,
        profileCount);
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
    (const autofill::AutofillProfile&)profile {
  self.autofillProfileEditCoordinator = [[AutofillProfileEditCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:_browser
                               profile:profile];
  self.autofillProfileEditCoordinator.delegate = self;
  [self.autofillProfileEditCoordinator start];
}

@end
