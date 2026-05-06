// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_profile_table_view_controller.h"

#import <algorithm>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/containers/to_vector.h"
#import "base/i18n/message_formatter.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/application_locale_storage/application_locale_storage.h"
#import "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#import "components/autofill/core/browser/data_manager/personal_data_manager.h"
#import "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#import "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#import "components/autofill/core/browser/data_quality/addresses/profile_requirement_utils.h"
#import "components/autofill/core/browser/integrators/autofill_ai/autofill_ai_labels.h"
#import "components/autofill/core/browser/integrators/autofill_ai/management_utils.h"
#import "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_metrics.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#import "components/autofill/ios/common/features.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/plus_addresses/core/browser/grit/plus_addresses_strings.h"
#import "components/plus_addresses/core/common/features.h"
#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_user_settings.h"
#import "ios/chrome/browser/autofill/autofill_ai/public/autofill_ai_ui_util.h"
#import "ios/chrome/browser/autofill/model/autofill_ai_util.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_factory.h"
#import "ios/chrome/browser/autofill/model/ios_autofill_entity_data_manager_observer_bridge.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/address_editor/autofill_edit_profile_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/settings_autofill_edit_profile_bottom_sheet_handler.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service.h"
#import "ios/chrome/browser/device_reauth/model/reauthentication_service_factory.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_coordinator.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/coordinator/autofill_ai_entity_edit_coordinator_delegate.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_add_entities_menu_builder.h"
#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/autofill_ai_entity_item.h"
#import "ios/chrome/browser/settings/autofill/utils/autofill_settings_ui_util.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_profile_edit_coordinator.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/autofill_settings_constants.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/cells/autofill_address_profile_record_type.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/cells/autofill_profile_item.h"
#import "ios/chrome/browser/settings/ui_bundled/autofill/enhanced_autofill_table_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/elements/enterprise_info_popover_view_controller.h"
#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller+toolbar_add.h"
#import "ios/chrome/browser/shared/coordinator/alert/action_sheet_coordinator.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_model.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/signin/model/authentication_service_factory.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_strings.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/strings/grit/ui_strings.h"

namespace {

// Entity types go into the "Identity docs" section of Settings.
// This set must be mutually exclusive with kTravel.
static constexpr autofill::DenseSet<autofill::EntityTypeName> kIdentityDocs = {
    autofill::EntityTypeName::kDriversLicense,
    autofill::EntityTypeName::kNationalIdCard,
    autofill::EntityTypeName::kPassport};

// Entity types go into the "Travel" section of Settings.
// This set must be mutually exclusive with kIdentityDocs.
static constexpr autofill::DenseSet<autofill::EntityTypeName> kTravel = {
    autofill::EntityTypeName::kFlightReservation,
    autofill::EntityTypeName::kKnownTravelerNumber,
    autofill::EntityTypeName::kRedressNumber,
    autofill::EntityTypeName::kVehicle};

// Plus Address Section header height.
const CGFloat kPlusAddressSectionHeaderHeight = 24;

// TODO(crbug.com/480934103): Update this URL.
constexpr std::string_view kWalletUrlString =
    "https://wallet.google.com/wallet/settings/managepassesdata";

// Point size for AI entity icons.
const CGFloat kEntityIconPointSize = 20;

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSwitches = kSectionIdentifierEnumZero,
  SectionIdentifierProfiles,
  SectionIdentifierPlusAddress,
  SectionIdentifierEnhancedAutofill,
  SectionIdentifierVerificationSwitch,
  SectionIdentifierWalletPromo,
  SectionIdentifierIdentityDocs,
  SectionIdentifierTravel,
  SectionIdentifierOther
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeAutofillAddressSwitch = kItemTypeEnumZero,
  ItemTypeAutofillAddressManaged,
  ItemTypeAddress,
  ItemTypeHeader,
  ItemTypeFooter,
  ItemTypePlusAddress,
  ItemTypePlusAddressFooter,
  ItemTypeEnhancedAutofill,
  ItemTypeEnhancedAutofillManaged,
  ItemTypeVerificationSwitch,
  ItemTypeVerificationFooter,
  ItemTypeWalletPromoInfo,
  ItemTypeWalletPromoButton,
  ItemTypeIdentityDoc,
  ItemTypeIdentityDocHeader,
  ItemTypeTravel,
  ItemTypeTravelHeader,
  ItemTypeOther,
  ItemTypeOtherHeader
};

// Returns the fallback detail text for a local profile when its detail text is
// empty.
NSString* GetFallbackDetailTextForLocalProfile(
    const autofill::AutofillProfile& autofill_profile,
    const std::string& locale) {
  const autofill::FieldType fallback_types[] = {
      autofill::ADDRESS_HOME_CITY, autofill::ADDRESS_HOME_STATE,
      autofill::ADDRESS_HOME_ZIP, autofill::ADDRESS_HOME_COUNTRY};

  NSString* detail_text;

  for (const autofill::FieldType& type : fallback_types) {
    detail_text =
        base::SysUTF16ToNSString(autofill_profile.GetInfo(type, locale));
    if ([detail_text length] != 0) {
      return detail_text;
    }
  }
  return @"";
}

// Returns true if the item type is not user deletable.
bool CanDeleteItemType(NSInteger itemType) {
  return itemType == ItemTypeAddress || itemType == ItemTypeIdentityDoc ||
         itemType == ItemTypeTravel;
}

ItemType ItemTypeForEntitySection(SectionIdentifier section_identifier) {
  switch (section_identifier) {
    case SectionIdentifierIdentityDocs:
      return ItemTypeIdentityDoc;
    case SectionIdentifierTravel:
      return ItemTypeTravel;
    case SectionIdentifierOther:
    default:
      return ItemTypeOther;
  }
}

NSString* HeaderTextForEntitySection(SectionIdentifier section_identifier) {
  switch (section_identifier) {
    case SectionIdentifierIdentityDocs:
      return l10n_util::GetNSString(IDS_AUTOFILL_IDENTITY_DOCS_TITLE);
    case SectionIdentifierTravel:
      return l10n_util::GetNSString(IDS_AUTOFILL_TRAVEL_TITLE);
    case SectionIdentifierOther:
    default:
      return l10n_util::GetNSString(IDS_IOS_AUTOFILL_AI_OTHER_TITLE);
  }
}

ItemType ItemTypeForEntitySectionHeader(SectionIdentifier section_identifier) {
  switch (section_identifier) {
    case SectionIdentifierIdentityDocs:
      return ItemTypeIdentityDocHeader;
    case SectionIdentifierTravel:
      return ItemTypeTravelHeader;
    case SectionIdentifierOther:
    default:
      return ItemTypeOtherHeader;
  }
}

}  // namespace

#pragma mark - AutofillProfileTableViewController

@interface AutofillProfileTableViewController () <
    AutofillAIAddEntitiesMenuDelegate,
    AutofillAIEntityEditCoordinatorDelegate,
    AutofillProfileEditCoordinatorDelegate,
    PersonalDataManagerObserver,
    PrefObserverDelegate,
    IOSAutofillEntityDataManagerObserver,
    PopoverLabelViewControllerDelegate> {
  raw_ptr<autofill::PersonalDataManager> _personalDataManager;
  raw_ptr<autofill::EntityDataManager> _entityDataManager;

  raw_ptr<Browser> _browser;
  std::unique_ptr<autofill::PersonalDataManagerObserverBridge> _observer;
  std::unique_ptr<autofill::IOSAutofillEntityDataManagerObserverBridge>
      _entityDataManagerObserver;

  // Deleting profiles updates PersonalDataManager resulting in an observer
  // callback, which handles general data updates with a reloadData.
  // It is better to handle user-initiated changes with more specific actions
  // such as inserting or removing items/sections. This boolean is used to
  // stop the observer callback from acting on user-initiated changes.
  BOOL _deletionInProgress;

  // Item for the Enhanced Autofill settings menu.
  TableViewDetailIconItem* _enhancedAutofillItem;

  // Whether Settings have been dismissed.
  BOOL _settingsAreDismissed;

  // The account email of the signed-in user, or nil if there is no
  // signed-in user.
  NSString* _userEmail;

  // Coordinator that manages a UIAlertController to delete addresses.
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
  AutofillEditProfileCoordinator* _autofillAddProfileCoordinator;

  // Pref observer to track changes to prefs.
  std::optional<PrefObserverBridge> _prefObserverBridge;
  // TODO(crbug.com/40492152): Refactor PrefObserverBridge so it owns the
  // PrefChangeRegistrar.
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;

  // A reference to the Wallet promo button item for quick access.
  TableViewTextItem* _walletPromoButtonItem;

  // Reauthentication module.
  ReauthenticationModule* _reauthenticationModule;

  // Coordinator to view/edit entity details.
  AutofillAIEntityEditCoordinator* _autofillAiEntityEditCoordinator;
}

@property(nonatomic, getter=isAutofillProfileEnabled)
    BOOL autofillProfileEnabled;

@end

@implementation AutofillProfileTableViewController

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);

  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    self.title =
        l10n_util::GetNSString(IsYourSavedInfoSettingsPageIosEnabled()
                                   ? IDS_AUTOFILL_CONTACT_INFO_TITLE
                                   : IDS_AUTOFILL_ADDRESSES_SETTINGS_TITLE);
    self.shouldDisableDoneButtonOnEdit = YES;
    _browser = browser;
    _personalDataManager = autofill::PersonalDataManagerFactory::GetForProfile(
        _browser->GetProfile());
    _observer = std::make_unique<autofill::PersonalDataManagerObserverBridge>(
        _personalDataManager, self);

    _entityDataManager = IOSAutofillEntityDataManagerFactory::GetForProfile(
        _browser->GetProfile());
    if (_entityDataManager) {
      _entityDataManagerObserver = std::make_unique<
          autofill::IOSAutofillEntityDataManagerObserverBridge>(
          _entityDataManager, self);
    }

    _reauthenticationModule =
        ReauthenticationServiceFactory::GetForProfile(_browser->GetProfile())
            ->GetReauthModule();

    _prefChangeRegistrar.Init(_browser->GetProfile()->GetPrefs());
    _prefObserverBridge.emplace(self);
    // Register to observe any changes on Perf backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(
        autofill::prefs::kAutofillAiOptInStatus, &_prefChangeRegistrar);
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

  bool isEnhancedAutofillEnabled = base::FeatureList::IsEnabled(
      autofill::features::kAutofillAiWithDataSchema);
  if (isEnhancedAutofillEnabled) {
    [model addSectionWithIdentifier:SectionIdentifierEnhancedAutofill];
    BOOL addressManagedAndDisabled = autofill::prefs::IsAutofillProfileManaged(
                                         _browser->GetProfile()->GetPrefs()) &&
                                     !autofill::prefs::IsAutofillProfileEnabled(
                                         _browser->GetProfile()->GetPrefs());
    if (addressManagedAndDisabled ||
        autofill::IsAutofillAiDisabledByEnterprisePolicy(
            _browser->GetProfile()->GetPrefs())) {
      [model addItem:[self managedEnhancedAutofillItem]
          toSectionWithIdentifier:SectionIdentifierEnhancedAutofill];
    } else {
      [model addItem:[self enhancedAutofillItem]
          toSectionWithIdentifier:SectionIdentifierEnhancedAutofill];
    }

    [self populateVerificationAndWalletSections];
  }

  [self populateProfileSection];

  // Add identity and travel docs sections after profile (addresses) section.
  if (isEnhancedAutofillEnabled) {
    [self populateEntitySections];
  }
}

- (void)populateEntitySections {
  if (!_entityDataManager) {
    return;
  }

  base::span<const autofill::EntityInstance> instances =
      _entityDataManager->GetEntityInstances();

  if (instances.empty()) {
    return;
  }

  std::vector<const autofill::EntityInstance*> identityDocs;
  std::vector<const autofill::EntityInstance*> travelDocs;
  std::vector<const autofill::EntityInstance*> other;

  for (const auto& instance : instances) {
    if (kIdentityDocs.contains(instance.type().name())) {
      identityDocs.push_back(&instance);
    } else if (kTravel.contains(instance.type().name())) {
      travelDocs.push_back(&instance);
    } else {
      other.push_back(&instance);
    }
  }

  [self addEntities:identityDocs toSection:SectionIdentifierIdentityDocs];
  [self addEntities:travelDocs toSection:SectionIdentifierTravel];
  [self addEntities:other toSection:SectionIdentifierOther];
}

- (TableViewItem*)itemForEntityInstance:
                      (const autofill::EntityInstance&)instance
                              withLabel:(const autofill::EntityLabel&)label
                                   type:(ItemType)type {
  AutofillAIEntityItem* item = [[AutofillAIEntityItem alloc] initWithType:type];
  item.name = base::SysUTF16ToNSString(
      base::JoinString(label, autofill::kLabelSeparator));
  item.typeDescription =
      base::SysUTF16ToNSString(instance.type().GetNameForI18n());
  item.guid = instance.guid();
  item.entityTypeName = instance.type().name();

  if (instance.IsServerInstance()) {
    item.isServerWalletItem = YES;
    item.trailingText = l10n_util::GetNSString(IDS_IOS_AUTOFILL_WALLET_TEXT);
  }

  item.icon = autofill::DefaultIconForAutofillAiEntityType(
      instance.type().name(), kEntityIconPointSize, /*tint_color=*/nil);
  return item;
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
  switchItem.target = self;
  switchItem.selector = @selector(autofillAddressSwitchChanged:);
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

- (TableViewItem*)enhancedAutofillItem {
  NSString* text = l10n_util::GetNSString(IDS_SETTINGS_AUTOFILL_AI_PAGE_TITLE);

  _enhancedAutofillItem =
      [[TableViewDetailIconItem alloc] initWithType:ItemTypeEnhancedAutofill];
  _enhancedAutofillItem.text = text;
  _enhancedAutofillItem.accessibilityTraits |= UIAccessibilityTraitButton;
  _enhancedAutofillItem.accessibilityIdentifier = kEnhancedAutofillTableViewId;

  [self configureEnhancedAutofillItem];

  return _enhancedAutofillItem;
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
  managedAddressItem.target = self;
  managedAddressItem.selector = @selector(didTapManagedUIInfoButton:);
  managedAddressItem.accessibilityIdentifier = kAutofillAddressManagedViewId;
  return managedAddressItem;
}

- (TableViewInfoButtonItem*)managedEnhancedAutofillItem {
  TableViewInfoButtonItem* managedEnhancedAutofillItem =
      [[TableViewInfoButtonItem alloc]
          initWithType:ItemTypeEnhancedAutofillManaged];
  managedEnhancedAutofillItem.text =
      l10n_util::GetNSString(IDS_SETTINGS_AUTOFILL_AI_PAGE_TITLE);
  // The status could only be off when the pref is managed.
  managedEnhancedAutofillItem.statusText =
      l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  managedEnhancedAutofillItem.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT);
  managedEnhancedAutofillItem.target = self;
  managedEnhancedAutofillItem.selector = @selector(didTapManagedUIInfoButton:);
  return managedEnhancedAutofillItem;
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
  AutofillProfileItem* item =
      [[AutofillProfileItem alloc] initWithType:ItemTypeAddress];
  const auto& locale =
      GetApplicationContext()->GetApplicationLocaleStorage()->Get();
  autofill::AutofillProfile::RecordType recordType =
      autofillProfile.record_type();

  item.title = base::SysUTF16ToNSString(
      autofillProfile.GetInfo(autofill::NAME_FULL, locale));
  item.detailText = base::SysUTF16ToNSString(
      autofillProfile.GetInfo(autofill::ADDRESS_HOME_LINE1, locale));
  item.GUID = autofillProfile.guid();
  item.accessibilityIdentifier = item.title;
  item.accessoryType = UITableViewCellAccessoryDisclosureIndicator;
  item.showMigrateToAccountButton = NO;
  item.localProfileIconShown = NO;

  switch (recordType) {
    case autofill::AutofillProfile::RecordType::kAccountHome:
      item.trailingDetailText =
          l10n_util::GetNSString(IDS_IOS_PROFILE_RECORD_TYPE_HOME);
      item.autofillProfileRecordType =
          AutofillAddressProfileRecordType::AutofillAccountHomeProfile;
      break;

    case autofill::AutofillProfile::RecordType::kAccountWork:
      item.trailingDetailText =
          l10n_util::GetNSString(IDS_IOS_PROFILE_RECORD_TYPE_WORK);
      item.autofillProfileRecordType =
          AutofillAddressProfileRecordType::AutofillAccountWorkProfile;
      break;

    case autofill::AutofillProfile::RecordType::kAccountNameEmail:
      item.autofillProfileRecordType =
          AutofillAddressProfileRecordType::AutofillAccountNameEmailProfile;
      item.detailText = base::SysUTF16ToNSString(
          autofillProfile.GetInfo(autofill::EMAIL_ADDRESS, locale));
      break;

    default:
      if (autofillProfile.IsAccountProfile()) {
        item.autofillProfileRecordType =
            AutofillAddressProfileRecordType::AutofillAccountProfile;
      } else {
        // This is a local profile.
        item.autofillProfileRecordType = AutofillLocalProfile;
        if ([item.detailText length] == 0) {
          item.detailText =
              GetFallbackDetailTextForLocalProfile(autofillProfile, locale);
        }
        if ([self shouldShowCloudOffIconForProfile:autofillProfile]) {
          item.showMigrateToAccountButton = YES;
          item.localProfileIconShown = YES;
          item.image = CustomSymbolTemplateWithPointSize(
              kCloudSlashSymbol, kCloudSlashSymbolPointSize);
        }
      }
      break;
  }

  return item;
}

- (BOOL)localProfilesExist {
  return !_settingsAreDismissed && !_personalDataManager->address_data_manager()
                                        .GetProfilesForSettings()
                                        .empty();
}

// Checks if there are any local entities.
- (BOOL)hasLocalEntities {
  if (_settingsAreDismissed || !_entityDataManager) {
    return NO;
  }
  return std::ranges::any_of(
      _entityDataManager->GetEntityInstances(), [](const auto& instance) {
        return instance.record_type() !=
               autofill::EntityInstance::RecordType::kServerWallet;
      });
}

#pragma mark - LoadModel Helpers for Enhanced Autofill

// Populates the Verification and Wallet related section.
- (void)populateVerificationAndWalletSections {
  TableViewModel* model = self.tableViewModel;

  if ([self shouldShowVerificationSwitch]) {
    [model addSectionWithIdentifier:SectionIdentifierVerificationSwitch];
    [model addItem:[self verificationSwitchItem]
        toSectionWithIdentifier:SectionIdentifierVerificationSwitch];
    [model setFooter:[self verificationFooter]
        forSectionWithIdentifier:SectionIdentifierVerificationSwitch];
  }

  if ([self shouldShowWalletPromo]) {
    [model addSectionWithIdentifier:SectionIdentifierWalletPromo];
    [model addItem:[self walletPromoInfoItem]
        toSectionWithIdentifier:SectionIdentifierWalletPromo];
    [model addItem:[self walletPromoButtonItem]
        toSectionWithIdentifier:SectionIdentifierWalletPromo];
  }
}

// Returns whether to show the Enhanced Autofill toggle to enable
// reauthentication before filling sensitive information.
- (BOOL)shouldShowVerificationSwitch {
  return base::FeatureList::IsEnabled(
      autofill::features::kAutofillAiReauthRequired);
}

// Returns YES if the Google Wallet promotion should be shown.
- (BOOL)shouldShowWalletPromo {
  return autofill::CanPerformAutofillAiAction(
      _browser->GetProfile(),
      autofill::AutofillAiAction::kWalletDataSharingPromotion);
}

// Returns YES if the user can modify the Enhanced Autofill setting.
- (BOOL)canModifyEnhancedAutofill {
  return autofill::CanPerformAutofillAiAction(
      _browser->GetProfile(), autofill::AutofillAiAction::kOptIn);
}

// Configures the enhancedAutofillItem based on capability to modify the
// setting.
- (void)configureEnhancedAutofillItem {
  bool canModify = [self canModifyEnhancedAutofill];

  _enhancedAutofillItem.detailText =
      canModify && autofill::IsEnhancedAutofillEnabled(_browser->GetProfile())
          ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
          : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);

  if (canModify) {
    _enhancedAutofillItem.accessoryType =
        UITableViewCellAccessoryDisclosureIndicator;
    _enhancedAutofillItem.accessibilityTraits &=
        ~UIAccessibilityTraitNotEnabled;
  } else {
    _enhancedAutofillItem.accessoryType = UITableViewCellAccessoryNone;
    _enhancedAutofillItem.selectionStyle = UITableViewCellSelectionStyleNone;
    _enhancedAutofillItem.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }
}

// Returns the verification (reauthentication) switch item.
- (TableViewItem*)verificationSwitchItem {
  TableViewSwitchItem* switchItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeVerificationSwitch];
  switchItem.text =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_VERIFICATION_INFO_LABEL);
  BOOL canAttemptReauth = [_reauthenticationModule canAttemptReauth];
  switchItem.on = canAttemptReauth &&
                  autofill::prefs::IsAutofillAiReauthBeforeFillingEnabled(
                      _browser->GetProfile()->GetPrefs());
  switchItem.enabled = canAttemptReauth;
  switchItem.target = self;
  switchItem.selector = @selector(verificationSwitchChanged:);
  switchItem.accessibilityIdentifier = kAutofillVerificationSwitchTableViewId;
  return switchItem;
}

// Returns the verification footer item.
- (TableViewHeaderFooterItem*)verificationFooter {
  TableViewLinkHeaderFooterItem* footer = [[TableViewLinkHeaderFooterItem alloc]
      initWithType:ItemTypeVerificationFooter];
  footer.text =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_VERIFICATION_INFO_FOOTER);
  return footer;
}

// Returns the Google Wallet promo info item.
- (TableViewItem*)walletPromoInfoItem {
  TableViewDetailTextItem* item =
      [[TableViewDetailTextItem alloc] initWithType:ItemTypeWalletPromoInfo];
  item.text = l10n_util::GetNSString(IDS_IOS_AUTOFILL_WALLET_PROMO_TITLE);
  item.detailText =
      l10n_util::GetNSString(IDS_IOS_AUTOFILL_WALLET_PROMO_DETAIL_TEXT);
  item.allowMultilineDetailText = YES;
  return item;
}

// Returns the Google Wallet promo button item.
- (TableViewItem*)walletPromoButtonItem {
  TableViewTextItem* item =
      [[TableViewTextItem alloc] initWithType:ItemTypeWalletPromoButton];
  _walletPromoButtonItem = item;
  item.text = l10n_util::GetNSString(IDS_IOS_AUTOFILL_WALLET_PROMO_LINK_TEXT);
  item.textColor = [UIColor colorNamed:kBlueColor];
  item.accessibilityTraits |= UIAccessibilityTraitButton;
  item.titleNumberOfLines = 0;
  return item;
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

  [self stopAutofillAddProfileCoordinator];

  [self stopAutofillProfileEditCoordinator];
  [self stopAutofillAIEntityEditCoordinator];
  [self dismissDeletionSheet];

  // Remove pref changes registrations.
  _prefChangeRegistrar.RemoveAll();

  // Remove observer bridges.
  _prefObserverBridge.reset();
  _observer.reset();
  _entityDataManagerObserver.reset();

  // Clear C++ ivars.
  _personalDataManager = nullptr;
  _entityDataManager = nullptr;
  _browser = nullptr;

  _settingsAreDismissed = YES;
}

#pragma mark - SettingsRootTableViewController

- (BOOL)editButtonEnabled {
  // Entities stored in Google Wallet are not editable by the app.
  // So, here only local entities are considered.
  return [self localProfilesExist] || [self hasLocalEntities];
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
  else if (self.navigationController.topViewController == self) {
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
  [self setWalletPromoButtonItemEnabled:!self.tableView.editing];
  [self updatedToolbarForEditState];
}

// Override.
- (void)deleteItems:(NSArray<NSIndexPath*>*)indexPaths {
  [self showDeletionConfirmationForIndexPaths:indexPaths];
}

- (UIBarButtonItem*)customLeftToolbarButton {
  // When in edit mode, a "Delete" button is shown as the left toolbar button.
  // This button shouldn't be overridden with a custom one.
  if (self.tableView.isEditing) {
    return nil;
  }

  return self.addButtonInToolbar;
}

#pragma mark - Helper methods

- (void)setWalletPromoButtonItemEnabled:(BOOL)enabled {
  if (!_walletPromoButtonItem) {
    return;
  }

  // Update the model.
  _walletPromoButtonItem.enabled = enabled;
  _walletPromoButtonItem.textColor =
      enabled ? [UIColor colorNamed:kBlueColor]
              : [UIColor colorNamed:kTextSecondaryColor];

  // Update the table view.
  [self reconfigureCellsForItems:@[ _walletPromoButtonItem ]];
}

#pragma mark - UITableViewDelegate

- (UITableViewCellEditingStyle)tableView:(UITableView*)tableView
           editingStyleForRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  if ([item isKindOfClass:[AutofillAIEntityItem class]]) {
    AutofillAIEntityItem* aiItem =
        base::apple::ObjCCastStrict<AutofillAIEntityItem>(item);
    return aiItem.isServerWalletItem ? UITableViewCellEditingStyleNone
                                     : UITableViewCellEditingStyleDelete;
  }
  return UITableViewCellEditingStyleDelete;
}

- (BOOL)tableView:(UITableView*)tableView
    shouldIndentWhileEditingRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  if ([item isKindOfClass:[AutofillAIEntityItem class]]) {
    AutofillAIEntityItem* aiItem =
        base::apple::ObjCCastStrict<AutofillAIEntityItem>(item);
    return !aiItem.isServerWalletItem;
  }
  return YES;
}

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

  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];

  switch (itemType) {
    case ItemTypePlusAddress: {
      base::RecordAction(
          base::UserMetricsAction("Settings.ManageOptionOnSettingsSelected"));
      OpenNewTabCommand* command = [OpenNewTabCommand
          commandWithURLFromChrome:
              GURL(plus_addresses::features::kPlusAddressManagementUrl.Get())];
      [self.sceneHandler closePresentedViewsAndOpenURL:command];
      return;
    }
    case ItemTypeEnhancedAutofill: {
      if (![self canModifyEnhancedAutofill]) {
        [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
        return;
      }
      CHECK(self.navigationController);
      base::RecordAction(base::UserMetricsAction("Settings.EnhancedAutofill"));
      EnhancedAutofillTableViewController* controller =
          [[EnhancedAutofillTableViewController alloc]
              initWithBrowser:_browser];
      [self configureHandlersForRootViewController:controller];
      [self.navigationController pushViewController:controller animated:YES];
      return;
    }
    case ItemTypeWalletPromoButton: {
      [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
      [self openGoogleWallet];
      return;
    }
    case ItemTypeIdentityDoc:
    case ItemTypeTravel:
    case ItemTypeOther: {
      AutofillAIEntityItem* item =
          base::apple::ObjCCastStrict<AutofillAIEntityItem>(
              [self.tableViewModel itemAtIndexPath:indexPath]);
      [self startAutofillAIEntityEditCoordinatorWithEntityID:item.guid];
      [self.tableView deselectRowAtIndexPath:indexPath animated:YES];
      return;
    }
    default:
      break;
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

// Opens a URL to Google Wallet for users to manage their passes data.
- (void)openGoogleWallet {
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:GURL(kWalletUrlString)];
  [self.sceneHandler closePresentedViewsAndOpenURL:command];
}

#pragma mark - UITableViewDataSource

- (BOOL)tableView:(UITableView*)tableView
    canEditRowAtIndexPath:(NSIndexPath*)indexPath {
  if (_settingsAreDismissed) {
    return NO;
  }

  TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
  if ([item isKindOfClass:[AutofillAIEntityItem class]]) {
    AutofillAIEntityItem* aiItem =
        base::apple::ObjCCastStrict<AutofillAIEntityItem>(item);
    return !aiItem.isServerWalletItem;
  }

  return [item isKindOfClass:[AutofillProfileItem class]];
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
      [UIColor colorNamed:kTertiaryBackgroundColor];
  cell.selectedBackgroundView = selectedBackgroundView;

  return cell;
}

- (NSString*)tableView:(UITableView*)tableView
    titleForDeleteConfirmationButtonForRowAtIndexPath:(NSIndexPath*)indexPath {
  if ([self isItemTypeForIndexPathAddress:indexPath]) {
    AutofillProfileItem* item =
        base::apple::ObjCCastStrict<AutofillProfileItem>(
            [self.tableViewModel itemAtIndexPath:indexPath]);
    if (item.autofillProfileRecordType == AutofillAccountHomeProfile ||
        item.autofillProfileRecordType == AutofillAccountWorkProfile ||
        item.autofillProfileRecordType == AutofillAccountNameEmailProfile) {
      return l10n_util::GetNSString(
          IDS_IOS_SETTINGS_AUTOFILL_REMOVE_ADDRESS_LABEL);
    }
  }

  return l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE);
}

#pragma mark - Switch Callbacks

- (void)autofillAddressSwitchChanged:(UISwitch*)switchView {
  BOOL switchOn = [switchView isOn];
  [self setSwitchItemOn:switchOn itemType:ItemTypeAutofillAddressSwitch];
  [self setAutofillProfileEnabled:switchOn];

  [self updateAddButtonInToolbar];
}

- (void)verificationSwitchChanged:(UISwitch*)switchView {
  if (![_reauthenticationModule canAttemptReauth]) {
    // This should normally not happen: the switch should not even be enabled.
    // Early return to fallback gracefully just in case.
    return;
  }

  NSString* reauthReason = l10n_util::GetNSString(
      IDS_IOS_SETTINGS_AUTOFILL_VERIFICATION_TOGGLE_REAUTH_REASON);

  __weak __typeof(self) weakSelf = self;

  // Just capture switchView directly. It will be strongly retained for the
  // duration of the block, ensuring it isn't deallocated before the callback
  // fires.
  auto completionHandler = ^(ReauthenticationResult result) {
    [weakSelf onReauthCompletedForVerificationSwitch:switchView result:result];
  };

  [_reauthenticationModule attemptReauthWithLocalizedReason:reauthReason
                                       canReusePreviousAuth:YES
                                                    handler:completionHandler];
}

// Called when the reauthentication process is completed for the Enhanced
// Autofill User Verification toggle.
- (void)onReauthCompletedForVerificationSwitch:(UISwitch*)switchView
                                        result:(ReauthenticationResult)result {
  BOOL switchOn = [switchView isOn];
  if (result == ReauthenticationResult::kFailure) {
    // Revert the switch if authentication wasn't successful.
    switchOn = !switchOn;
  }

  [switchView setOn:switchOn animated:YES];
  [self setSwitchItemOn:switchOn itemType:ItemTypeVerificationSwitch];
  autofill::prefs::SetAutofillAiReauthBeforeFillingEnabled(
      _browser->GetProfile()->GetPrefs(), switchOn);
}

#pragma mark - Switch Helpers

// Sets switchItem's state to `on`.
- (void)setSwitchItemOn:(BOOL)on itemType:(ItemType)switchItemType {
  TableViewModel* model = self.tableViewModel;
  NSIndexPath* switchPath = nil;

  if ([model hasItemForItemType:switchItemType
              sectionIdentifier:SectionIdentifierSwitches]) {
    switchPath = [model indexPathForItemType:switchItemType
                           sectionIdentifier:SectionIdentifierSwitches];
  } else if ([model hasItemForItemType:switchItemType
                     sectionIdentifier:SectionIdentifierVerificationSwitch]) {
    switchPath =
        [model indexPathForItemType:switchItemType
                  sectionIdentifier:SectionIdentifierVerificationSwitch];
  } else {
    return;
  }

  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [model itemAtIndexPath:switchPath]);
  switchItem.on = on;
  [self reconfigureCellsForItems:@[ switchItem ]];
}

// Sets switchItem's enabled status to `enabled` and reconfigures the
// corresponding cell.
- (void)setSwitchItemEnabled:(BOOL)enabled itemType:(ItemType)switchItemType {
  TableViewModel* model = self.tableViewModel;
  NSIndexPath* switchPath = nil;

  if ([model hasItemForItemType:switchItemType
              sectionIdentifier:SectionIdentifierSwitches]) {
    switchPath = [model indexPathForItemType:switchItemType
                           sectionIdentifier:SectionIdentifierSwitches];
  } else if ([model hasItemForItemType:switchItemType
                     sectionIdentifier:SectionIdentifierVerificationSwitch]) {
    switchPath =
        [model indexPathForItemType:switchItemType
                  sectionIdentifier:SectionIdentifierVerificationSwitch];
  } else {
    return;
  }

  TableViewSwitchItem* switchItem =
      base::apple::ObjCCastStrict<TableViewSwitchItem>(
          [model itemAtIndexPath:switchPath]);
  [switchItem setEnabled:enabled];
  [self reconfigureCellsForItems:@[ switchItem ]];
}

#pragma mark - IOSAutofillEntityDataManagerObserver

- (void)onEntityInstancesChanged {
  if (_deletionInProgress) {
    return;
  }

  [self reloadData];
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
  id<SystemIdentity> identity = authenticationService->GetPrimaryIdentity();
  if (identity) {
    _userEmail = identity.userEmail;
  }
}

- (UIBarButtonItem*)addButtonInToolbar {
  if (!_addButtonInToolbar) {
    _addButtonInToolbar =
        [self addButtonWithAction:@selector(handleAddAddress)];
    [self updateAddButtonInToolbar];
  }
  return _addButtonInToolbar;
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  // If the model hasn't been created yet, no need to update anything.
  if (!self.tableViewModel) {
    return;
  }

  if (preferenceName == autofill::prefs::kAutofillAiOptInStatus) {
    [self configureEnhancedAutofillItem];
    [self reconfigureCellsForItems:@[ _enhancedAutofillItem ]];

    [self updateAddButtonInToolbar];
  }
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

#pragma mark - AutofillAIEntityEditCoordinatorDelegate

- (void)autofillAIEntityEditCoordinatorDidFinish:
    (AutofillAIEntityEditCoordinator*)coordinator {
  [self stopAutofillAIEntityEditCoordinator];
}

#pragma mark - AutofillAIAddEntitiesMenuDelegate

- (void)didSelectAddAutofillProfile {
  [self handleAddAddress];
}

// Called when an entity type is selected to be added.
- (void)didSelectAddEntityWithType:(autofill::EntityType)type {
  [self startAutofillAIEntityEditCoordinatorWithEntityType:type];
}

#pragma mark - Private

- (void)dismissDeletionSheet {
  [_deletionSheetCoordinator stop];
  _deletionSheetCoordinator = nil;
}

- (void)startAutofillAIEntityEditCoordinatorWithEntityID:
    (autofill::EntityInstance::EntityId)entityID {
  [self stopAutofillAIEntityEditCoordinator];
  _autofillAiEntityEditCoordinator = [[AutofillAIEntityEditCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:_browser
                              entityID:entityID];
  _autofillAiEntityEditCoordinator.delegate = self;

  [_autofillAiEntityEditCoordinator start];
}

- (void)startAutofillAIEntityEditCoordinatorWithEntityType:
    (autofill::EntityType)entityType {
  [self stopAutofillAIEntityEditCoordinator];
  _autofillAiEntityEditCoordinator = [[AutofillAIEntityEditCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:_browser
                            entityType:entityType];
  _autofillAiEntityEditCoordinator.delegate = self;

  [_autofillAiEntityEditCoordinator start];
}

- (void)stopAutofillAIEntityEditCoordinator {
  [_autofillAiEntityEditCoordinator stop];
  _autofillAiEntityEditCoordinator.delegate = nil;
  _autofillAiEntityEditCoordinator = nil;
}

- (void)stopAutofillProfileEditCoordinator {
  _autofillProfileEditCoordinator.delegate = nil;
  [_autofillProfileEditCoordinator stop];
  _autofillProfileEditCoordinator = nil;
}

- (void)stopAutofillAddProfileCoordinator {
  [_autofillAddProfileCoordinator stop];
  _autofillAddProfileCoordinator = nil;
  _addProfileBottomSheetHandler = nil;
}

// Removes the item from the personal data manager model.
- (void)willDeleteItemsAtIndexPaths:(NSArray*)indexPaths {
  if (_settingsAreDismissed) {
    return;
  }

  _deletionInProgress = YES;
  for (NSIndexPath* indexPath in indexPaths) {
    TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
    if ([item isKindOfClass:[AutofillProfileItem class]]) {
      AutofillProfileItem* profileItem =
          base::apple::ObjCCastStrict<AutofillProfileItem>(item);
      _personalDataManager->address_data_manager().RemoveProfile(
          [profileItem GUID]);
    } else if ([item isKindOfClass:[AutofillAIEntityItem class]]) {
      AutofillAIEntityItem* aiItem =
          base::apple::ObjCCastStrict<AutofillAIEntityItem>(item);
      if (_entityDataManager) {
        autofill::EntityInstance::RecordType recordType =
            aiItem.isServerWalletItem
                ? autofill::EntityInstance::RecordType::kServerWallet
                : autofill::EntityInstance::RecordType::kLocal;
        autofill::LogEntityDeletedFromSettings(
            autofill::EntityType(aiItem.entityTypeName), recordType);
        _entityDataManager->RemoveEntityInstance(aiItem.guid);
      }
    }
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
  BOOL hasLocalProfile = NO;
  BOOL hasAccountProfile = NO;
  BOOL hasHomeProfile = NO;
  BOOL hasWorkProfile = NO;
  BOOL hasNameEmailProfile = NO;
  int profileCount = 0;
  int aiEntityCount = 0;

  for (NSIndexPath* indexPath in indexPaths) {
    NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
    if (!CanDeleteItemType(itemType)) {
      continue;
    }

    TableViewItem* item = [self.tableViewModel itemAtIndexPath:indexPath];
    if ([item isKindOfClass:[AutofillProfileItem class]]) {
      profileCount++;
      AutofillProfileItem* profileItem =
          base::apple::ObjCCastStrict<AutofillProfileItem>(item);

      switch (profileItem.autofillProfileRecordType) {
        case AutofillLocalProfile:
          hasLocalProfile = YES;
          break;
        case AutofillAccountProfile:
          hasAccountProfile = YES;
          break;
        case AutofillAccountHomeProfile:
          hasHomeProfile = YES;
          break;
        case AutofillAccountWorkProfile:
          hasWorkProfile = YES;
          break;
        case AutofillAccountNameEmailProfile:
          hasNameEmailProfile = YES;
          break;
      }
    } else if ([item isKindOfClass:[AutofillAIEntityItem class]]) {
      AutofillAIEntityItem* aiItem =
          base::apple::ObjCCastStrict<AutofillAIEntityItem>(item);
      // Only local entities can be deleted. Server wallet items should not be
      // selected for deletion.
      if (!aiItem.isServerWalletItem) {
        aiEntityCount++;
      }
    }
  }

  // Can happen if user presses delete in quick succession.
  if (profileCount == 0 && aiEntityCount == 0) {
    return;
  }

  BOOL hasHomeWorkNameEmailProfile =
      (hasHomeProfile || hasWorkProfile || hasNameEmailProfile);
  NSString* deletionConfirmationString = [self
      getDeletionConfirmationStringForProfileCount:profileCount
                                       hasEntities:aiEntityCount
                                   hasLocalProfile:hasLocalProfile
                                 hasAccountProfile:hasAccountProfile
                       hasHomeWorkNameEmailProfile:hasHomeWorkNameEmailProfile];

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
  NSString* confirmationButtonText =
      [self confirmationButtonText:profileCount hasEntities:aiEntityCount > 0];

  // This block shows "Edit in Google Account" button. When the selection is
  // mixed, meaning AI entities are selected, we are still going to show this
  // button, and guide the user to edit in Google Account.
  // In this case, entities selected are ignored.
  if (hasHomeWorkNameEmailProfile && !hasLocalProfile && !hasAccountProfile) {
    confirmationButtonText = l10n_util::GetNSString(
        IDS_IOS_SETTINGS_AUTOFILL_REMOVE_ADDRESS_CONFIRMATION_BUTTON);
    [_deletionSheetCoordinator
        addItemWithTitle:
            l10n_util::GetNSString(
                IDS_IOS_SETTINGS_AUTOFILL_EDIT_HOME_WORK_ADDRESS_CONFIRMATION_BUTTON)
                  action:^{
                    [weakSelf dismissDeletionSheet];
                    OpenNewTabCommand* command = [OpenNewTabCommand
                        commandWithURLFromChrome:
                            GURL(hasHomeProfile ? kGoogleMyAccountHomeAddressURL
                                 : hasWorkProfile
                                     ? kGoogleMyAccountWorkAddressURL
                                     : kGoogleAccountNameEmailAddressEditURL)];
                    [weakSelf.sceneHandler
                        closePresentedViewsAndOpenURL:command];
                  }
                   style:UIAlertActionStyleDefault];
  }
  [_deletionSheetCoordinator
      addItemWithTitle:confirmationButtonText
                action:^{
                  [weakSelf willDeleteItemsAtIndexPaths:indexPaths];
                  // TODO(crbug.com/41277594) Generalize removing empty sections
                  [weakSelf removeSectionIfEmptyForSectionWithIdentifier:
                                SectionIdentifierProfiles];
                  [weakSelf removeSectionIfEmptyForSectionWithIdentifier:
                                SectionIdentifierIdentityDocs];
                  [weakSelf removeSectionIfEmptyForSectionWithIdentifier:
                                SectionIdentifierTravel];
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

// Returns the confirmation button text.
- (NSString*)confirmationButtonText:(int)profileCount
                        hasEntities:(BOOL)hasEntities {
  // If there are AI entities selected, use the generic delete action title.
  // It is "Delete" instead of "Delete addresses" or "Delete address".
  if (hasEntities) {
    return l10n_util::GetNSString(IDS_IOS_DELETE_ACTION_TITLE);
  }

  return base::FeatureList::IsEnabled(
             autofill::features::kAutofillEnableSupportForHomeAndWork)
             ? l10n_util::GetNSString(
                   IDS_IOS_SETTINGS_AUTOFILL_DELETE_ADDRESSES_CONFIRMATION_BUTTON)
             : l10n_util::GetPluralNSStringF(
                   IDS_IOS_SETTINGS_AUTOFILL_DELETE_ADDRESS_CONFIRMATION_BUTTON,
                   profileCount);
}

// Returns the deletion confirmation message string based on
// `profileCount` and if it the source has any local, account or home/work
// profiles.
- (NSString*)getDeletionConfirmationStringForProfileCount:(int)profileCount
                                              hasEntities:(BOOL)hasEntities
                                          hasLocalProfile:(BOOL)hasLocalProfile
                                        hasAccountProfile:
                                            (BOOL)hasAccountProfile
                              hasHomeWorkNameEmailProfile:
                                  (BOOL)hasHomeWorkNameEmailProfile {
  std::u16string userEmail = base::SysNSStringToUTF16(_userEmail);
  if (hasEntities) {
    return GetDeletionConfirmationStringWithEntities(
        hasAccountProfile || hasHomeWorkNameEmailProfile, userEmail);
  }
  return GetDeletionConfirmationString(profileCount, hasLocalProfile,
                                       hasAccountProfile,
                                       hasHomeWorkNameEmailProfile, userEmail);
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

  _autofillAddProfileCoordinator = [[AutofillEditProfileCoordinator alloc]
      initWithBaseViewController:self
                         browser:_browser
                         handler:_addProfileBottomSheetHandler];
  [_autofillAddProfileCoordinator start];
}

// Returns whether to show the add menu with addresses and entities.
// Two notes:
// 1. "Save and fill addresses" being disabled disables Forms AI.
// 2. When the default availability flag is on, Autofill AI enterprise policy
// does not control adding/editing entities.
- (bool)shouldShowAddMenu {
  if (!_entityDataManager || ![self isAutofillProfileEnabled]) {
    return false;
  }

  return base::FeatureList::IsEnabled(
             autofill::features::kAutofillAiAvailableByDefault)
             ? autofill::CanPerformAutofillAiAction(
                   _browser->GetProfile(),
                   autofill::AutofillAiAction::kEnableOrDisable)
             : autofill::CanPerformAutofillAiAction(
                   _browser->GetProfile(), autofill::AutofillAiAction::kOptIn);
}

// Updates the add button in the toolbar based on whether the add menu should be
// shown and whether autofill profile is enabled.
- (void)updateAddButtonInToolbar {
  BOOL profileEnabled = [self isAutofillProfileEnabled];
  if ([self shouldShowAddMenu]) {
    _addButtonInToolbar.action = nil;
    _addButtonInToolbar.target = nil;
    _addButtonInToolbar.menu =
        [self buildAddEntitiesMenuWithProfileEnabled:profileEnabled];
    _addButtonInToolbar.enabled = YES;
  } else {
    _addButtonInToolbar.enabled = profileEnabled;
  }
}

// Returns whether it is allowed to add entities. When adding entities is not
// allowed, menu items on the add menu are disabled.
- (bool)canAddEntities {
  if (!_entityDataManager) {
    return false;
  }

  return base::FeatureList::IsEnabled(
             autofill::features::kAutofillAiAvailableByDefault)
             ? autofill::CanPerformAutofillAiAction(
                   _browser->GetProfile(),
                   autofill::AutofillAiAction::kEnableOrDisable)
             : autofill::CanPerformAutofillAiAction(
                   _browser->GetProfile(),
                   autofill::AutofillAiAction::kOptIn) &&
                   autofill::IsEnhancedAutofillEnabled(_browser->GetProfile());
}

- (UIMenu*)buildAddEntitiesMenuWithProfileEnabled:(BOOL)profileEnabled {
  std::vector<autofill::EntityType> writableTypes =
      base::ToVector(autofill::GetWritableEntityTypes(
          _entityDataManager->GetVariationCountryCode()));
  return [AutofillAIAddEntitiesMenuBuilder
      buildMenuWithTypes:std::move(writableTypes)
          profileEnabled:profileEnabled
         entitiesEnabled:[self canAddEntities]
                delegate:self];
}

// Adds the given `instances` to the table view model under the given
// `sectionIdentifier`.
- (void)addEntities:
            (const std::vector<const autofill::EntityInstance*>&)instances
          toSection:(SectionIdentifier)sectionIdentifier {
  if (instances.empty()) {
    return;
  }

  TableViewModel* model = self.tableViewModel;
  [model addSectionWithIdentifier:sectionIdentifier];

  TableViewTextHeaderFooterItem* header = [[TableViewTextHeaderFooterItem alloc]
      initWithType:ItemTypeForEntitySectionHeader(sectionIdentifier)];
  header.text = HeaderTextForEntitySection(sectionIdentifier);
  [model setHeader:header forSectionWithIdentifier:sectionIdentifier];

  const std::string& locale =
      GetApplicationContext()->GetApplicationLocaleStorage()->Get();

  std::vector<autofill::EntityLabel> labels = autofill::GetLabelsForEntities(
      instances, /*attribute_types_to_ignore=*/{},
      /*only_disambiguating_types=*/false, /*obfuscate_sensitive_types=*/true,
      locale);

  ItemType itemType = ItemTypeForEntitySection(sectionIdentifier);
  for (size_t i = 0; i < instances.size(); ++i) {
    [model addItem:[self itemForEntityInstance:*instances[i]
                                     withLabel:labels[i]
                                          type:itemType]
        toSectionWithIdentifier:sectionIdentifier];
  }
}

@end
