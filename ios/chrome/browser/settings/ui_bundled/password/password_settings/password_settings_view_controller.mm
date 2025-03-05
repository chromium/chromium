// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/ui_bundled/password/password_settings/password_settings_view_controller.h"

#import <AuthenticationServices/AuthenticationServices.h>

#import <optional>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/feature_list.h"
#import "base/i18n/message_formatter.h"
#import "base/metrics/histogram_functions.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/credential_provider/model/features.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_ui_features.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings/password_settings_constants.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_image_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_info_button_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_multi_detail_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_switch_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Delay before which the "Turn on AutoFill" button associated with the
// Passwords in Other Apps cell can be re-enabled.
constexpr base::TimeDelta kReEnableTurnOnPasswordsInOtherAppsButtonDelay =
    base::Seconds(10);

// Sections of the password settings UI.
typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierSavePasswordsSwitch = kSectionIdentifierEnumZero,
  SectionIdentifierBulkMovePasswordsToAccount,
  SectionIdentifierPasswordsInOtherApps,
  SectionIdentifierAutomaticPasskeyUpgradesSwitch,
  SectionIdentifierGooglePasswordManagerPin,
  SectionIdentifierOnDeviceEncryption,
  SectionIdentifierExportPasswordsButton,
  SectionIdentifierDeleteCredentialsButton,
};

// Items within the password settings UI.
typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeSavePasswordsSwitch = kItemTypeEnumZero,
  ItemTypeManagedSavePasswords,
  ItemTypeBulkMovePasswordsToAccountDescription,
  ItemTypeBulkMovePasswordsToAccountButton,
  ItemTypePasswordsInOtherApps,
  ItemTypeTurnOnPasswordsInOtherAppsButton,
  ItemTypeAutomaticPasskeyUpgradesSwitch,
  ItemTypeChangeGooglePasswordManagerPinDescription,
  ItemTypeChangeGooglePasswordManagerPinButton,
  ItemTypeOnDeviceEncryptionOptInDescription,
  ItemTypeOnDeviceEncryptionOptedInDescription,
  ItemTypeOnDeviceEncryptionOptedInLearnMore,
  ItemTypeOnDeviceEncryptionSetUp,
  ItemTypeExportPasswordsButton,
  ItemTypeDeleteCredentialsButton,
  ItemTypeFooter,
};

// Indicates whether the model has not started loading, is in the process of
// loading, or has completed loading.
typedef NS_ENUM(NSInteger, ModelLoadStatus) {
  ModelNotLoaded = 0,
  ModelIsLoading,
  ModelLoadComplete,
};

// Helper method that returns the delay before which the "Turn on AutoFill"
// button can be re-enabled.
base::TimeDelta GetDelayForReEnablingTurnOnPasswordsInOtherAppsButton() {
  // Check if the delay has been overridden by a test hook.
  const base::TimeDelta overridden_delay = tests_hook::
      GetOverriddenDelayForRequestingTurningOnCredentialProviderExtension();
  if (overridden_delay != base::Seconds(0)) {
    return overridden_delay;
  }

  return kReEnableTurnOnPasswordsInOtherAppsButtonDelay;
}

// Helper method that returns the string to use as title for `savePasswordsItem`
// and `managedSavePasswordsItem`.
NSString* GetSavePasswordsItemTitle() {
  return l10n_util::GetNSString(IOSPasskeysM2Enabled()
                                    ? IDS_IOS_OFFER_TO_SAVE_PASSWORDS_PASSKEYS
                                    : IDS_IOS_OFFER_TO_SAVE_PASSWORDS);
}

// Helper method that returns the string to use as title for the
// `passwordsInOtherAppsItem`.
NSString* GetPasswordsInOtherAppsItemTitle() {
  if (!IOSPasskeysM2Enabled()) {
    return l10n_util::GetNSString(IDS_IOS_SETTINGS_PASSWORDS_IN_OTHER_APPS);
  }

  if (@available(iOS 18.0, *)) {
    return l10n_util::GetNSString(
        IDS_IOS_SETTINGS_PASSWORDS_PASSKEYS_IN_OTHER_APPS_IOS18);
  } else {
    return l10n_util::GetNSString(
        IDS_IOS_SETTINGS_PASSWORDS_PASSKEYS_IN_OTHER_APPS);
  }
}

// Helper method that returns whether the `turnOnPasswordsInOtherAppsItem`
// should be visible depending on the given `passwords_in_other_apps_enabled`
// status.
BOOL ShouldShowTurnOnPasswordsInOtherAppsItem(
    BOOL passwords_in_other_apps_enabled) {
  BOOL should_show_item = NO;
  if (@available(iOS 18, *)) {
    should_show_item =
        IOSPasskeysM2Enabled() && !passwords_in_other_apps_enabled;
  }
  return should_show_item;
}

// Whether automatic passkey upgrades feature is enabled.
BOOL AutomaticPasskeyUpgradeFeatureEnabled() {
  return base::FeatureList::IsEnabled(
      kCredentialProviderAutomaticPasskeyUpgrade);
}

}  // namespace

@interface PasswordSettingsViewController () {
  // The item related to the button for deleting credentials.
  TableViewTextItem* _deleteCredentialsItem;

  // The item related to the button for exporting passwords.
  TableViewTextItem* _exportPasswordsItem;

  // The footer item related to the button for deleting credentials.
  TableViewLinkHeaderFooterItem* _deleteCredentialsFooterItem;

  // Whether or not Chromium has been enabled as a credential provider at the
  // iOS level. This may not be known at load time; the detail text showing on
  // or off status will be omitted until this is populated.
  // TODO(crbug.com/396694707): Should become a plain bool once the Passkeys M2
  // feature is launched.
  std::optional<bool> _passwordsInOtherAppsEnabled;

  // Whether the `turnOnPasswordsInOtherAppsItem` should be visible.
  BOOL _shouldShowTurnOnPasswordsInOtherAppsItem;

  // Whether the `changeGooglePasswordManagerPinItem` should be visible.
  BOOL _canChangeGPMPin;
}

// State

// Tracks whether or not the model has loaded.
@property(nonatomic, assign) ModelLoadStatus modelLoadStatus;

// Whether or not the credential delete button should be enabled.
@property(nonatomic, assign) BOOL canDeleteAllCredentials;

// Whether or not the exporter should be enabled.
@property(nonatomic, assign) BOOL canExportPasswords;

// Whether or not the Password Manager is managed by enterprise policy.
@property(nonatomic, assign, getter=isManagedByPolicy) BOOL managedByPolicy;

// Whether automatic passkey upgrades are enabled.
@property(nonatomic, assign) BOOL automaticPasskeyUpgradesEnabled;

// Indicates whether or not "Offer to Save Passwords" is set to enabled.
@property(nonatomic, assign, getter=isSavingPasswordsEnabled)
    BOOL savingPasswordsEnabled;

// Whether saving passkeys is enabled.
@property(nonatomic, assign) BOOL savingPasskeysEnabled;

// The amount of local passwords present on device.
@property(nonatomic, assign) int localPasswordsCount;

// Inidicates whether or not the bulk move passwords to account section should
// be shown.
@property(nonatomic, assign) BOOL showBulkMovePasswordsToAccount;

// Indicates the email of the signed in account.
@property(nonatomic, copy) NSString* userEmail;

// On-device encryption state according to the sync service.
@property(nonatomic, assign)
    PasswordSettingsOnDeviceEncryptionState onDeviceEncryptionState;

// UI elements

// The item related to the switch for the password manager setting.
@property(nonatomic, readonly) TableViewSwitchItem* savePasswordsItem;

// The item related to the enterprise managed save password setting.
@property(nonatomic, readonly)
    TableViewInfoButtonItem* managedSavePasswordsItem;

// The item related to the description of bulk moving passwords to the user's
// account.
@property(nonatomic, readonly)
    TableViewDetailTextItem* bulkMovePasswordsToAccountDescriptionItem;

// The item related to the button allowing users to bulk move passwords to their
// account.
@property(nonatomic, readonly)
    TableViewTextItem* bulkMovePasswordsToAccountButtonItem;

// The item showing the current status of Passwords in Other Apps (i.e.,
// credential provider).
@property(nonatomic, readonly)
    TableViewMultiDetailTextItem* passwordsInOtherAppsItem;

// A button which triggers a prompt to allow the user to set the app as a
// credential provider.
@property(nonatomic, readonly)
    TableViewTextItem* turnOnPasswordsInOtherAppsItem;

// The item related to the switch for the automatic passkey upgrades setting.
@property(nonatomic, readonly)
    TableViewSwitchItem* automaticPasskeyUpgradesSwitchItem;

// Descriptive text shown when the user has an option of changing their Google
// Password Manager PIN.
@property(nonatomic, readonly)
    TableViewImageItem* changeGooglePasswordManagerPinDescriptionItem;

// A button which triggers the change Google Password Manager PIN flow.
@property(nonatomic, readonly)
    TableViewTextItem* changeGooglePasswordManagerPinItem;

// Descriptive text shown when the user has the option of enabling on-device
// encryption.
@property(nonatomic, readonly)
    TableViewImageItem* onDeviceEncryptionOptInDescriptionItem;

// Descriptive text shown when the user has already enabled on-device
// encryption.
@property(nonatomic, readonly)
    TableViewImageItem* onDeviceEncryptionOptedInDescription;

// A button giving the user more information about on-device encrpytion, shown
// when they have already enabled it.
@property(nonatomic, readonly)
    TableViewTextItem* onDeviceEncryptionOptedInLearnMore;

// A button which triggers the setup of on-device encryption.
@property(nonatomic, readonly) TableViewTextItem* setUpOnDeviceEncryptionItem;

@end

@implementation PasswordSettingsViewController

@synthesize savePasswordsItem = _savePasswordsItem;
@synthesize managedSavePasswordsItem = _managedSavePasswordsItem;
@synthesize bulkMovePasswordsToAccountDescriptionItem =
    _bulkMovePasswordsToAccountDescriptionItem;
@synthesize bulkMovePasswordsToAccountButtonItem =
    _bulkMovePasswordsToAccountButtonItem;
@synthesize passwordsInOtherAppsItem = _passwordsInOtherAppsItem;
@synthesize turnOnPasswordsInOtherAppsItem = _turnOnPasswordsInOtherAppsItem;
@synthesize automaticPasskeyUpgradesSwitchItem =
    _automaticPasskeyUpgradesSwitchItem;
@synthesize changeGooglePasswordManagerPinDescriptionItem =
    _changeGooglePasswordManagerPinDescriptionItem;
@synthesize changeGooglePasswordManagerPinItem =
    _changeGooglePasswordManagerPinItem;
@synthesize onDeviceEncryptionOptInDescriptionItem =
    _onDeviceEncryptionOptInDescriptionItem;
@synthesize onDeviceEncryptionOptedInDescription =
    _onDeviceEncryptionOptedInDescription;
@synthesize onDeviceEncryptionOptedInLearnMore =
    _onDeviceEncryptionOptedInLearnMore;
@synthesize setUpOnDeviceEncryptionItem = _setUpOnDeviceEncryptionItem;

- (instancetype)init {
  self = [super initWithStyle:ChromeTableViewStyle()];
  if (self) {
    if (IOSPasskeysM2Enabled()) {
      // An "undefined" `passwordsInOtherAppsEnabled` value isn't supported when
      // the Passkeys M2 feature is enabled.
      _passwordsInOtherAppsEnabled = NO;
      _shouldShowTurnOnPasswordsInOtherAppsItem =
          ShouldShowTurnOnPasswordsInOtherAppsItem(
              _passwordsInOtherAppsEnabled.value());
    } else {
      _shouldShowTurnOnPasswordsInOtherAppsItem = NO;
    }
  }
  return self;
}

- (CGRect)sourceRectForBulkMovePasswordsToAccount {
  return [self.tableView
             cellForRowAtIndexPath:
                 [self.tableViewModel
                     indexPathForItem:_bulkMovePasswordsToAccountButtonItem]]
      .frame;
}

- (CGRect)sourceRectForCredentialDeletionAlerts {
  return [self.tableView
             cellForRowAtIndexPath:[self.tableViewModel
                                       indexPathForItem:_deleteCredentialsItem]]
      .frame;
}

- (CGRect)sourceRectForPasswordExportAlerts {
  return [self.tableView
             cellForRowAtIndexPath:[self.tableViewModel
                                       indexPathForItem:_exportPasswordsItem]]
      .frame;
}

- (UIView*)sourceViewForAlerts {
  return self.tableView;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  self.title = l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS);
  self.tableView.accessibilityIdentifier = kPasswordsSettingsTableViewId;

  [self loadModel];
}

#pragma mark - LegacyChromeTableViewController

- (void)loadModel {
  [super loadModel];

  self.modelLoadStatus = ModelIsLoading;

  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
  [self addSavePasswordsSwitchOrManagedInfo];

  [model addSectionWithIdentifier:SectionIdentifierPasswordsInOtherApps];
  [model addItem:[self passwordsInOtherAppsItem]
      toSectionWithIdentifier:SectionIdentifierPasswordsInOtherApps];
  [self updateTurnOnPasswordsInOtherAppsItemVisibility];

  if ([self shouldDisplayPasskeyUpgradesSwitch]) {
    [model addSectionWithIdentifier:
               SectionIdentifierAutomaticPasskeyUpgradesSwitch];
    [model addItem:[self automaticPasskeyUpgradesSwitchItem]
        toSectionWithIdentifier:
            SectionIdentifierAutomaticPasskeyUpgradesSwitch];
  }

  if (_canChangeGPMPin) {
    [self updateChangeGPMPinButton];
  }

  if (self.onDeviceEncryptionState !=
      PasswordSettingsOnDeviceEncryptionStateNotShown) {
    [self updateOnDeviceEncryptionSectionWithOldState:
              PasswordSettingsOnDeviceEncryptionStateNotShown];
  }

  // Export passwords button.
  [model addSectionWithIdentifier:SectionIdentifierExportPasswordsButton];
  _exportPasswordsItem = [self makeExportPasswordsItem];
  [self updateExportPasswordsButton];
  [model addItem:_exportPasswordsItem
      toSectionWithIdentifier:SectionIdentifierExportPasswordsButton];

  if (base::FeatureList::IsEnabled(
          password_manager::features::kIOSEnableDeleteAllSavedCredentials)) {
    // Delete credentials button.
    [model addSectionWithIdentifier:SectionIdentifierDeleteCredentialsButton];
    _deleteCredentialsItem = [self makeDeleteCredentialsItem];
    _deleteCredentialsFooterItem = [self makeCredentialDeletionFooterItem];
    [self updateDeleteAllCredentialsSection];
    [model addItem:_deleteCredentialsItem
        toSectionWithIdentifier:SectionIdentifierDeleteCredentialsButton];

    // Add footer for the delete credential section.
    [model setFooter:_deleteCredentialsFooterItem
        forSectionWithIdentifier:SectionIdentifierDeleteCredentialsButton];
  }

  if (self.showBulkMovePasswordsToAccount) {
    [self updateBulkMovePasswordsToAccountSection];
  }

  self.modelLoadStatus = ModelLoadComplete;
}

#pragma mark - UITableViewDataSource

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  UITableViewCell* cell = [super tableView:tableView
                     cellForRowAtIndexPath:indexPath];

  switch ([self.tableViewModel itemTypeForIndexPath:indexPath]) {
    case ItemTypeSavePasswordsSwitch: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView addTarget:self
                                action:@selector(savePasswordsSwitchChanged:)
                      forControlEvents:UIControlEventValueChanged];
      break;
    }
    case ItemTypeManagedSavePasswords: {
      TableViewInfoButtonCell* managedCell =
          base::apple::ObjCCastStrict<TableViewInfoButtonCell>(cell);
      [managedCell.trailingButton
                 addTarget:self
                    action:@selector(didTapManagedUIInfoButton:)
          forControlEvents:UIControlEventTouchUpInside];
      break;
    }
    case ItemTypeAutomaticPasskeyUpgradesSwitch: {
      TableViewSwitchCell* switchCell =
          base::apple::ObjCCastStrict<TableViewSwitchCell>(cell);
      [switchCell.switchView
                 addTarget:self
                    action:@selector(automaticPasskeyUpgradesSwitchChanged:)
          forControlEvents:(UIControlEvents)UIControlEventValueChanged];
    }
  }
  return cell;
}

#pragma mark - UITableViewDelegate

- (void)tableView:(UITableView*)tableView
    didSelectRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypePasswordsInOtherApps: {
      if ([self shouldPasswordsInOtherAppsBeTappable]) {
        [self.presentationDelegate showPasswordsInOtherAppsScreen];
      }
      break;
    }
    case ItemTypeTurnOnPasswordsInOtherAppsButton: {
      if (@available(iOS 18.0, *)) {
        // Disable the button as the API that's about to be called
        // (`-requestToTurnOnCredentialProviderExtensionWithCompletionHandler`)
        // won't accept other requests for the following 10 seconds.
        [self setTurnOnPasswordsInOtherAppsItemEnabled:NO];

        // Show the prompt that allows setting the app as a credential provider.
        scoped_refptr<base::SequencedTaskRunner> currentTaskRunner =
            base::SequencedTaskRunner::GetCurrentDefault();
        __weak __typeof(self) weakSelf = self;
        [ASSettingsHelper
            requestToTurnOnCredentialProviderExtensionWithCompletionHandler:^(
                BOOL appWasEnabledForAutoFill) {
              [weakSelf
                  handleTurnOnAutofillPromptOutcome:appWasEnabledForAutoFill
                                  currentTaskRunner:currentTaskRunner];
            }];
      } else {
        // This item shouldn't be shown on iOS versions prior to 18.
        NOTREACHED();
      }
      break;
    }
    case ItemTypeBulkMovePasswordsToAccountButton: {
      if (self.showBulkMovePasswordsToAccount) {
        [self.delegate bulkMovePasswordsToAccountButtonClicked];
      }
      break;
    }
    case ItemTypeDeleteCredentialsButton: {
      if (self.canDeleteAllCredentials) {
        [self.presentationDelegate startDeletionFlow];
      }
      break;
    }
    case ItemTypeExportPasswordsButton: {
      if (self.canExportPasswords) {
        [self.presentationDelegate startExportFlow];
      }
      break;
    }
    case ItemTypeOnDeviceEncryptionSetUp: {
      [self.presentationDelegate showOnDeviceEncryptionSetUp];
      break;
    }
    case ItemTypeOnDeviceEncryptionOptedInLearnMore: {
      [self.presentationDelegate showOnDeviceEncryptionHelp];
      break;
    }
    case ItemTypeChangeGooglePasswordManagerPinButton: {
      [self.presentationDelegate showChangeGPMPinDialog];
      break;
    }
    case ItemTypeOnDeviceEncryptionOptedInDescription:
    case ItemTypeOnDeviceEncryptionOptInDescription:
    case ItemTypeSavePasswordsSwitch:
    case ItemTypeManagedSavePasswords: {
      DUMP_WILL_BE_NOTREACHED();
    }
  }
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

- (BOOL)tableView:(UITableView*)tableView
    shouldHighlightRowAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger itemType = [self.tableViewModel itemTypeForIndexPath:indexPath];
  switch (itemType) {
    case ItemTypeDeleteCredentialsButton:
      return self.canDeleteAllCredentials;
    case ItemTypeExportPasswordsButton:
      return self.canExportPasswords;
    case ItemTypeSavePasswordsSwitch:
      return NO;
    case ItemTypePasswordsInOtherApps:
      return [self shouldPasswordsInOtherAppsBeTappable];
  }
  return YES;
}

#pragma mark - UI item factories

// Creates the switch allowing users to enable/disable the saving of passwords.
- (TableViewSwitchItem*)savePasswordsItem {
  if (_savePasswordsItem) {
    return _savePasswordsItem;
  }

  _savePasswordsItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeSavePasswordsSwitch];
  _savePasswordsItem.text = GetSavePasswordsItemTitle();
  _savePasswordsItem.accessibilityIdentifier =
      kPasswordSettingsSavePasswordSwitchTableViewId;
  [self updateSavePasswordsSwitch];
  return _savePasswordsItem;
}

// Creates the row which replaces `savePasswordsItem` when this preference is
// being managed by enterprise policy.
- (TableViewInfoButtonItem*)managedSavePasswordsItem {
  if (_managedSavePasswordsItem) {
    return _managedSavePasswordsItem;
  }

  _managedSavePasswordsItem = [[TableViewInfoButtonItem alloc]
      initWithType:ItemTypeManagedSavePasswords];
  _managedSavePasswordsItem.text = GetSavePasswordsItemTitle();
  _managedSavePasswordsItem.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT);
  _managedSavePasswordsItem.accessibilityIdentifier =
      kPasswordSettingsManagedSavePasswordSwitchTableViewId;
  [self updateManagedSavePasswordsItem];
  return _managedSavePasswordsItem;
}

// Creates and returns the move passwords to account description item.
- (TableViewDetailTextItem*)bulkMovePasswordsToAccountDescriptionItem {
  if (_bulkMovePasswordsToAccountDescriptionItem) {
    return _bulkMovePasswordsToAccountDescriptionItem;
  }

  _bulkMovePasswordsToAccountDescriptionItem = [[TableViewDetailTextItem alloc]
      initWithType:ItemTypeBulkMovePasswordsToAccountDescription];
  _bulkMovePasswordsToAccountDescriptionItem.text = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_SECTION_TITLE);
  _bulkMovePasswordsToAccountDescriptionItem.accessibilityIdentifier =
      kPasswordSettingsBulkMovePasswordsToAccountDescriptionTableViewId;
  _bulkMovePasswordsToAccountDescriptionItem.allowMultilineDetailText = YES;

  std::u16string pattern = l10n_util::GetStringUTF16(
      IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_SECTION_DESCRIPTION);
  std::u16string result = base::i18n::MessageFormatter::FormatWithNamedArgs(
      pattern, "COUNT", self.localPasswordsCount, "EMAIL",
      base::SysNSStringToUTF16(self.userEmail));

  _bulkMovePasswordsToAccountDescriptionItem.detailText =
      base::SysUTF16ToNSString(result);

  return _bulkMovePasswordsToAccountDescriptionItem;
}

// Creates and returns the move passwords to account button.
- (TableViewTextItem*)bulkMovePasswordsToAccountButtonItem {
  if (_bulkMovePasswordsToAccountButtonItem) {
    return _bulkMovePasswordsToAccountButtonItem;
  }

  _bulkMovePasswordsToAccountButtonItem = [[TableViewTextItem alloc]
      initWithType:ItemTypeBulkMovePasswordsToAccountButton];
  _bulkMovePasswordsToAccountButtonItem.text = l10n_util::GetPluralNSStringF(
      IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_SECTION_BUTTON,
      self.localPasswordsCount);
  _bulkMovePasswordsToAccountButtonItem.textColor =
      [UIColor colorNamed:kBlueColor];
  _bulkMovePasswordsToAccountButtonItem.accessibilityTraits =
      UIAccessibilityTraitButton;
  _bulkMovePasswordsToAccountButtonItem.accessibilityIdentifier =
      kPasswordSettingsBulkMovePasswordsToAccountButtonTableViewId;
  return _bulkMovePasswordsToAccountButtonItem;
}

- (TableViewMultiDetailTextItem*)passwordsInOtherAppsItem {
  if (_passwordsInOtherAppsItem) {
    return _passwordsInOtherAppsItem;
  }

  _passwordsInOtherAppsItem = [[TableViewMultiDetailTextItem alloc]
      initWithType:ItemTypePasswordsInOtherApps];
  _passwordsInOtherAppsItem.text = GetPasswordsInOtherAppsItemTitle();
  if (IOSPasskeysM2Enabled()) {
    if (@available(iOS 18.0, *)) {
      _passwordsInOtherAppsItem.leadingDetailText = l10n_util::GetNSString(
          IDS_IOS_PASSWORD_SETTINGS_PASSWORDS_IN_OTHER_APPS_DESCRIPTION);
    }
  } else {
    _passwordsInOtherAppsItem.accessoryType =
        UITableViewCellAccessoryDisclosureIndicator;
    _passwordsInOtherAppsItem.accessibilityTraits |= UIAccessibilityTraitButton;
  }
  _passwordsInOtherAppsItem.accessibilityIdentifier =
      kPasswordSettingsPasswordsInOtherAppsRowId;
  [self updatePasswordsInOtherAppsItem];
  return _passwordsInOtherAppsItem;
}

- (TableViewTextItem*)turnOnPasswordsInOtherAppsItem {
  if (_turnOnPasswordsInOtherAppsItem) {
    return _turnOnPasswordsInOtherAppsItem;
  }

  _turnOnPasswordsInOtherAppsItem = [[TableViewTextItem alloc]
      initWithType:ItemTypeTurnOnPasswordsInOtherAppsButton];
  _turnOnPasswordsInOtherAppsItem.text = l10n_util::GetNSString(
      IDS_IOS_CREDENTIAL_PROVIDER_SETTINGS_TURN_ON_AUTOFILL);
  _turnOnPasswordsInOtherAppsItem.textColor = [UIColor colorNamed:kBlueColor];
  _turnOnPasswordsInOtherAppsItem.accessibilityTraits =
      UIAccessibilityTraitButton;
  return _turnOnPasswordsInOtherAppsItem;
}

- (TableViewSwitchItem*)automaticPasskeyUpgradesSwitchItem {
  _automaticPasskeyUpgradesSwitchItem = [[TableViewSwitchItem alloc]
      initWithType:ItemTypeAutomaticPasskeyUpgradesSwitch];
  _automaticPasskeyUpgradesSwitchItem.text =
      l10n_util::GetNSString(IDS_IOS_ALLOW_AUTOMATIC_PASSKEY_UPGRADES);
  _automaticPasskeyUpgradesSwitchItem.detailText =
      l10n_util::GetNSString(IDS_IOS_ALLOW_AUTOMATIC_PASSKEY_UPGRADES_SUBTITLE);
  _automaticPasskeyUpgradesSwitchItem.on = self.automaticPasskeyUpgradesEnabled;
  return _automaticPasskeyUpgradesSwitchItem;
}

- (TableViewImageItem*)changeGooglePasswordManagerPinDescriptionItem {
  _changeGooglePasswordManagerPinDescriptionItem = [[TableViewImageItem alloc]
      initWithType:ItemTypeChangeGooglePasswordManagerPinDescription];
  _changeGooglePasswordManagerPinDescriptionItem.title = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_GOOGLE_PASSWORD_MANAGER_PIN_TITLE);
  _changeGooglePasswordManagerPinDescriptionItem.detailText =
      l10n_util::GetNSString(
          IDS_IOS_PASSWORD_SETTINGS_GOOGLE_PASSWORD_MANAGER_PIN_DESCRIPTION);
  _changeGooglePasswordManagerPinDescriptionItem.accessibilityIdentifier =
      kPasswordSettingsChangePinDescriptionId;
  return _changeGooglePasswordManagerPinDescriptionItem;
}

- (TableViewTextItem*)changeGooglePasswordManagerPinItem {
  _changeGooglePasswordManagerPinItem = [[TableViewTextItem alloc]
      initWithType:ItemTypeChangeGooglePasswordManagerPinButton];
  _changeGooglePasswordManagerPinItem.text =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS_CHANGE_PIN);
  _changeGooglePasswordManagerPinItem.textColor =
      [UIColor colorNamed:kBlueColor];
  _changeGooglePasswordManagerPinItem.accessibilityTraits =
      UIAccessibilityTraitButton;
  _changeGooglePasswordManagerPinItem.accessibilityIdentifier =
      kPasswordSettingsChangePinButtonId;
  return _changeGooglePasswordManagerPinItem;
}

- (TableViewImageItem*)onDeviceEncryptionOptInDescriptionItem {
  if (_onDeviceEncryptionOptInDescriptionItem) {
    return _onDeviceEncryptionOptInDescriptionItem;
  }

  _onDeviceEncryptionOptInDescriptionItem = [[TableViewImageItem alloc]
      initWithType:ItemTypeOnDeviceEncryptionOptInDescription];
  _onDeviceEncryptionOptInDescriptionItem.title =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION);
  _onDeviceEncryptionOptInDescriptionItem.detailText = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_OPT_IN);
  _onDeviceEncryptionOptInDescriptionItem.enabled = NO;
  _onDeviceEncryptionOptInDescriptionItem.accessibilityIdentifier =
      kPasswordSettingsOnDeviceEncryptionOptInId;
  return _onDeviceEncryptionOptInDescriptionItem;
}

- (TableViewImageItem*)onDeviceEncryptionOptedInDescription {
  if (_onDeviceEncryptionOptedInDescription) {
    return _onDeviceEncryptionOptedInDescription;
  }

  _onDeviceEncryptionOptedInDescription = [[TableViewImageItem alloc]
      initWithType:ItemTypeOnDeviceEncryptionOptedInDescription];
  _onDeviceEncryptionOptedInDescription.title =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION);
  _onDeviceEncryptionOptedInDescription.detailText = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_LEARN_MORE);
  _onDeviceEncryptionOptedInDescription.enabled = NO;
  _onDeviceEncryptionOptedInDescription.accessibilityIdentifier =
      kPasswordSettingsOnDeviceEncryptionOptedInTextId;
  return _onDeviceEncryptionOptedInDescription;
}

- (TableViewTextItem*)onDeviceEncryptionOptedInLearnMore {
  if (_onDeviceEncryptionOptedInLearnMore) {
    return _onDeviceEncryptionOptedInLearnMore;
  }

  _onDeviceEncryptionOptedInLearnMore = [[TableViewTextItem alloc]
      initWithType:ItemTypeOnDeviceEncryptionOptedInLearnMore];
  _onDeviceEncryptionOptedInLearnMore.text = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_OPTED_IN_LEARN_MORE);
  _onDeviceEncryptionOptedInLearnMore.textColor =
      [UIColor colorNamed:kBlueColor];
  _onDeviceEncryptionOptedInLearnMore.accessibilityTraits =
      UIAccessibilityTraitButton;
  _onDeviceEncryptionOptedInLearnMore.accessibilityIdentifier =
      kPasswordSettingsOnDeviceEncryptionLearnMoreId;
  return _onDeviceEncryptionOptedInLearnMore;
}

- (TableViewTextItem*)setUpOnDeviceEncryptionItem {
  if (_setUpOnDeviceEncryptionItem) {
    return _setUpOnDeviceEncryptionItem;
  }

  _setUpOnDeviceEncryptionItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeOnDeviceEncryptionSetUp];
  _setUpOnDeviceEncryptionItem.text = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_SET_UP);
  _setUpOnDeviceEncryptionItem.textColor = [UIColor colorNamed:kBlueColor];
  _setUpOnDeviceEncryptionItem.accessibilityTraits = UIAccessibilityTraitButton;
  _setUpOnDeviceEncryptionItem.accessibilityIdentifier =
      kPasswordSettingsOnDeviceEncryptionSetUpId;
  return _setUpOnDeviceEncryptionItem;
}

// Creates the "Export Passwords..." button. Coloring and enabled/disabled state
// are handled by `updateExportPasswordsButton`, which should be called as soon
// as the mediator has provided the necessary state.
- (TableViewTextItem*)makeExportPasswordsItem {
  TableViewTextItem* exportPasswordsItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeExportPasswordsButton];
  exportPasswordsItem.text = l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS);
  exportPasswordsItem.accessibilityTraits = UIAccessibilityTraitButton;
  return exportPasswordsItem;
}

// Creates the "Delete all data" button.
- (TableViewTextItem*)makeDeleteCredentialsItem {
  TableViewTextItem* deleteCredentialsItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeDeleteCredentialsButton];
  deleteCredentialsItem.text = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_DELETE_ALL_SAVED_CREDENTIALS);
  deleteCredentialsItem.accessibilityTraits = UIAccessibilityTraitButton;
  return deleteCredentialsItem;
}

// Creates the footer item for "Delete all data" button.
- (TableViewLinkHeaderFooterItem*)makeCredentialDeletionFooterItem {
  TableViewLinkHeaderFooterItem* item =
      [[TableViewLinkHeaderFooterItem alloc] initWithType:ItemTypeFooter];
  item.text = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_CREDENTIAL_DELETION_TEXT);
  return item;
}

#pragma mark - PasswordSettingsConsumer

- (void)setSavingPasswordsEnabled:(BOOL)enabled
                  managedByPolicy:(BOOL)managedByPolicy {
  BOOL enabledChanged = _savingPasswordsEnabled != enabled;
  BOOL managedChanged = _managedByPolicy != managedByPolicy;
  if (!enabledChanged && !managedChanged) {
    return;
  }

  _savingPasswordsEnabled = enabled;
  _managedByPolicy = managedByPolicy;

  if (self.modelLoadStatus == ModelNotLoaded) {
    return;
  }

  // If `_managedByPolicy` changed, the section needs to be redrawn.
  if (managedChanged) {
    TableViewModel* model = self.tableViewModel;
    [model deleteAllItemsFromSectionWithIdentifier:
               SectionIdentifierSavePasswordsSwitch];
    [self addSavePasswordsSwitchOrManagedInfo];
    NSIndexSet* indexSet = [[NSIndexSet alloc]
        initWithIndex:[model sectionForSectionIdentifier:
                                 SectionIdentifierSavePasswordsSwitch]];
    [self.tableView reloadSections:indexSet
                  withRowAnimation:UITableViewRowAnimationAutomatic];
  }

  if (_managedByPolicy) {
    [self updateManagedSavePasswordsItem];
  } else {
    [self updateSavePasswordsSwitch];
  }

  [self updateAutomaticPasskeyUpgradesSwitch];
}

- (void)setAutomaticPasskeyUpgradesEnabled:(BOOL)enabled {
  if (_automaticPasskeyUpgradesEnabled == enabled) {
    return;
  }

  _automaticPasskeyUpgradesEnabled = enabled;
  [self updateAutomaticPasskeyUpgradesSwitch];
}

- (void)setSavingPasskeysEnabled:(BOOL)enabled {
  if (_savingPasskeysEnabled == enabled) {
    return;
  }

  _savingPasskeysEnabled = enabled;
  [self updateAutomaticPasskeyUpgradesSwitch];
}

- (void)setCanChangeGPMPin:(BOOL)canChangeGPMPin {
  if (_canChangeGPMPin == canChangeGPMPin) {
    return;
  }

  _canChangeGPMPin = canChangeGPMPin;
  [self updateChangeGPMPinButton];
}

- (void)setCanDeleteAllCredentials:(BOOL)canDeleteAllCredentials {
  if (_canDeleteAllCredentials == canDeleteAllCredentials) {
    return;
  }

  _canDeleteAllCredentials = canDeleteAllCredentials;
  [self updateDeleteAllCredentialsSection];
}

- (void)setCanExportPasswords:(BOOL)canExportPasswords {
  if (_canExportPasswords == canExportPasswords) {
    return;
  }

  _canExportPasswords = canExportPasswords;
  [self updateExportPasswordsButton];
}

- (void)setCanBulkMove:(BOOL)canBulkMove localPasswordsCount:(int)count {
  BOOL showSection = count > 0 && canBulkMove;

  if (_localPasswordsCount == count &&
      _showBulkMovePasswordsToAccount == showSection) {
    return;
  }

  _localPasswordsCount = count;
  _showBulkMovePasswordsToAccount = showSection;
  [self updateBulkMovePasswordsToAccountSection];
}

- (void)setPasswordsInOtherAppsEnabled:(BOOL)enabled {
  if (_passwordsInOtherAppsEnabled.has_value() &&
      _passwordsInOtherAppsEnabled.value() == enabled) {
    return;
  }

  _passwordsInOtherAppsEnabled = enabled;
  _shouldShowTurnOnPasswordsInOtherAppsItem =
      ShouldShowTurnOnPasswordsInOtherAppsItem(
          _passwordsInOtherAppsEnabled.value());

  if (self.modelLoadStatus == ModelNotLoaded) {
    return;
  }

  [self updatePasswordsInOtherAppsItem];
  [self updateTurnOnPasswordsInOtherAppsItemVisibility];
}

- (void)setOnDeviceEncryptionState:
    (PasswordSettingsOnDeviceEncryptionState)onDeviceEncryptionState {
  PasswordSettingsOnDeviceEncryptionState oldState = _onDeviceEncryptionState;
  if (oldState == onDeviceEncryptionState) {
    return;
  }
  _onDeviceEncryptionState = onDeviceEncryptionState;

  if (self.modelLoadStatus == ModelNotLoaded) {
    return;
  }

  [self updateOnDeviceEncryptionSectionWithOldState:oldState];
}

#pragma mark - Actions

- (void)savePasswordsSwitchChanged:(UISwitch*)switchView {
  base::UmaHistogramBoolean(
      "PasswordManager.Settings.ToggleOfferToSavePasswords", switchView.on);
  [self.delegate savedPasswordSwitchDidChange:switchView.on];
}

// Called when the user clicks on the information button of the managed
// setting's UI. Shows a textual bubble with the information of the enterprise.
- (void)didTapManagedUIInfoButton:(UIButton*)buttonView {
  [self.presentationDelegate showManagedPrefInfoForSourceView:buttonView];
  // Disable the button when showing the bubble.
  buttonView.enabled = NO;
}

// Called when the user changes the state of the automatic passkey upgrades
// switch.
- (void)automaticPasskeyUpgradesSwitchChanged:(UISwitch*)switchView {
  [self.delegate automaticPasskeyUpgradesSwitchDidChange:switchView.on];
}

#pragma mark - Private

// Adds the appropriate content to the Save Passwords Switch section depending
// on whether or not the pref is managed.
- (void)addSavePasswordsSwitchOrManagedInfo {
  TableViewItem* item = self.isManagedByPolicy ? self.managedSavePasswordsItem
                                               : self.savePasswordsItem;
  [self.tableViewModel addItem:item
       toSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
}

// Updates the appearance of the Managed Save Passwords item to reflect the
// current state of `isSavePasswordEnabled`.
- (void)updateManagedSavePasswordsItem {
  self.managedSavePasswordsItem.statusText =
      self.isSavingPasswordsEnabled
          ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
          : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  [self reconfigureCellsForItems:@[ self.managedSavePasswordsItem ]];
}

// Updates the appearance of the Save Passwords switch to reflect the current
// state of `isSavePasswordEnabled`.
- (void)updateSavePasswordsSwitch {
  self.savePasswordsItem.on = self.isSavingPasswordsEnabled;

  if (self.modelLoadStatus != ModelLoadComplete) {
    return;
  }
  [self reconfigureCellsForItems:@[ self.savePasswordsItem ]];
}

- (void)updateBulkMovePasswordsToAccountSection {
  UITableView* tableView = self.tableView;
  TableViewModel* tableViewModel = self.tableViewModel;
  BOOL sectionExists =
      [tableViewModel hasSectionForSectionIdentifier:
                          SectionIdentifierBulkMovePasswordsToAccount];

  // Remove the section if it exists and we shouldn't show it.
  if (!_showBulkMovePasswordsToAccount && sectionExists) {
    NSInteger section =
        [tableViewModel sectionForSectionIdentifier:
                            SectionIdentifierBulkMovePasswordsToAccount];
    [tableViewModel removeSectionWithIdentifier:
                        SectionIdentifierBulkMovePasswordsToAccount];
    if (self.modelLoadStatus == ModelLoadComplete) {
      [tableView deleteSections:[NSIndexSet indexSetWithIndex:section]
               withRowAnimation:UITableViewRowAnimationAutomatic];
    }
    return;
  }

  if (!_showBulkMovePasswordsToAccount) {
    return;
  }

  // Prepare the section in the model, either by clearing or adding it.
  if (sectionExists) {
    [tableViewModel deleteAllItemsFromSectionWithIdentifier:
                        SectionIdentifierBulkMovePasswordsToAccount];
  } else {
    // Find the section that's supposed to be before Bulk Move Passwords to
    // Account, and insert after that.
    NSInteger bulkMovePasswordsToAccountSectionIndex =
        [tableViewModel
            sectionForSectionIdentifier:SectionIdentifierSavePasswordsSwitch] +
        1;
    [tableViewModel
        insertSectionWithIdentifier:SectionIdentifierBulkMovePasswordsToAccount
                            atIndex:bulkMovePasswordsToAccountSectionIndex];

    // Record histogram only if the section doesn't already exist but is about
    // to be shown.
    base::UmaHistogramEnumeration(
        "PasswordManager.AccountStorage.MoveToAccountStoreFlowOffered",
        password_manager::metrics_util::MoveToAccountStoreTrigger::
            kExplicitlyTriggeredForMultiplePasswordsInSettings);
  }

  // Add the description and button items to the bulk move passwords to account
  // section.
  [tableViewModel addItem:self.bulkMovePasswordsToAccountDescriptionItem
      toSectionWithIdentifier:SectionIdentifierBulkMovePasswordsToAccount];
  [tableViewModel addItem:self.bulkMovePasswordsToAccountButtonItem
      toSectionWithIdentifier:SectionIdentifierBulkMovePasswordsToAccount];

  NSIndexSet* indexSet = [NSIndexSet
      indexSetWithIndex:[tableViewModel
                            sectionForSectionIdentifier:
                                SectionIdentifierBulkMovePasswordsToAccount]];

  if (self.modelLoadStatus != ModelLoadComplete) {
    return;
  }

  // Reload the section if it exists, otherwise insert it if it does not.
  if (sectionExists) {
    [tableView reloadSections:indexSet
             withRowAnimation:UITableViewRowAnimationAutomatic];
  } else {
    [tableView insertSections:indexSet
             withRowAnimation:UITableViewRowAnimationAutomatic];
  }
}

// Updates the appearance of the Passwords In Other Apps item to reflect the
// current state of `_passwordsInOtherAppsEnabled`.
- (void)updatePasswordsInOtherAppsItem {
  if (!_passwordsInOtherAppsEnabled.has_value()) {
    // A value should have been set upon initialization of this class when the
    // Passkeys M2 feature is on.
    CHECK(!IOSPasskeysM2Enabled());
    return;
  }

  // Whether the `passwordsInOtherAppsItem` should be tappable and allow the
  // user to access the Passwords in Other Apps view. The UI of the cell varies
  // depending on whether or not it is tappable.
  BOOL shouldPasswordsInOtherAppsItemBeTappable =
      [self shouldPasswordsInOtherAppsBeTappable];

  if (shouldPasswordsInOtherAppsItemBeTappable) {
    self.passwordsInOtherAppsItem.trailingDetailText =
        _passwordsInOtherAppsEnabled.value()
            ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
            : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    self.passwordsInOtherAppsItem.accessoryType =
        UITableViewCellAccessoryDisclosureIndicator;
    self.passwordsInOtherAppsItem.accessibilityTraits |=
        UIAccessibilityTraitButton;
  } else {
    self.passwordsInOtherAppsItem.trailingDetailText = nil;
    self.passwordsInOtherAppsItem.accessoryType = UITableViewCellAccessoryNone;
    self.passwordsInOtherAppsItem.accessibilityTraits &=
        ~UIAccessibilityTraitButton;
  }

  if (self.modelLoadStatus != ModelLoadComplete) {
    return;
  }
  [self reconfigureCellsForItems:@[ self.passwordsInOtherAppsItem ]];

  // Refresh the cells' height.
  [self.tableView beginUpdates];
  [self.tableView endUpdates];
}

// Whether the `passwordsInOtherAppsItem` should be tappable and lead to the
// Passwords in Other Apps screen.
- (BOOL)shouldPasswordsInOtherAppsBeTappable {
  return !_shouldShowTurnOnPasswordsInOtherAppsItem;
}

// Adds or removes the `turnOnPasswordsInOtherAppsItem` from the table view if
// needed.
- (void)updateTurnOnPasswordsInOtherAppsItemVisibility {
  if (self.modelLoadStatus == ModelNotLoaded) {
    return;
  }

  TableViewModel* model = self.tableViewModel;
  CHECK([model
      hasSectionForSectionIdentifier:SectionIdentifierPasswordsInOtherApps]);

  BOOL itemAlreadyExists =
      [model hasItemForItemType:ItemTypeTurnOnPasswordsInOtherAppsButton
              sectionIdentifier:SectionIdentifierPasswordsInOtherApps];

  // First check if an update is required or if the item's visibility is already
  // as needed.
  if (_shouldShowTurnOnPasswordsInOtherAppsItem == itemAlreadyExists) {
    return;
  }

  if (_shouldShowTurnOnPasswordsInOtherAppsItem) {
    [self setTurnOnPasswordsInOtherAppsItemEnabled:YES];
    [model addItem:self.turnOnPasswordsInOtherAppsItem
        toSectionWithIdentifier:SectionIdentifierPasswordsInOtherApps];
    [self.tableView insertRowsAtIndexPaths:@[
      [self turnOnPasswordsInOtherAppsItemIndexPath]
    ]
                          withRowAnimation:UITableViewRowAnimationAutomatic];
  } else {
    NSIndexPath* turnOnPasswordsInOtherAppsItemIndexPath =
        [self turnOnPasswordsInOtherAppsItemIndexPath];
    [self removeFromModelItemAtIndexPaths:@[
      turnOnPasswordsInOtherAppsItemIndexPath
    ]];
    [self.tableView
        deleteRowsAtIndexPaths:@[ turnOnPasswordsInOtherAppsItemIndexPath ]
              withRowAnimation:UITableViewRowAnimationAutomatic];
  }
}

// Returns the index path of the `turnOnPasswordsInOtherAppsItem`
- (NSIndexPath*)turnOnPasswordsInOtherAppsItemIndexPath {
  return [self.tableViewModel
      indexPathForItemType:ItemTypeTurnOnPasswordsInOtherAppsButton
         sectionIdentifier:SectionIdentifierPasswordsInOtherApps];
}

// Configures the `turnOnPasswordsInOtherAppsItem` to reflect the provided
// `enabled` state.
- (void)setTurnOnPasswordsInOtherAppsItemEnabled:(BOOL)enabled {
  TableViewTextItem* item = self.turnOnPasswordsInOtherAppsItem;
  if (enabled) {
    item.enabled = YES;
    item.textColor = [UIColor colorNamed:kBlueColor];
    item.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
  } else {
    item.enabled = NO;
    item.textColor = [UIColor colorNamed:kTextSecondaryColor];
    item.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }
  [self reconfigureCellsForItems:@[ item ]];
}

// Handles whether the user accepted the prompt to set the app as a credential
// provider.
- (void)handleTurnOnAutofillPromptOutcome:(BOOL)appWasEnabledForAutoFill
                        currentTaskRunner:
                            (scoped_refptr<base::SequencedTaskRunner>)
                                currentTaskRunner {
  if (appWasEnabledForAutoFill) {
    // Inform the delegate of the status change. This will have the effect of
    // removing the `turnOnPasswordsInOtherAppsItem` from the view.
    [self.delegate passwordAutoFillWasTurnedOn];
    return;
  }

  // Delay re-enabling the `turnOnPasswordsInOtherAppsItem` as it will only be
  // possible to re-trigger the prompt after a 10 seconds delay.
  __weak __typeof(self) weakSelf = self;
  currentTaskRunner->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf setTurnOnPasswordsInOtherAppsItemEnabled:YES];
      }),
      GetDelayForReEnablingTurnOnPasswordsInOtherAppsButton());
}

// Updates the UI to present the correct elements for the user's current
// on-device encryption state. `oldState` indicates the currently-displayed UI
// at the time of invocation and is used to determine if we need to add a new
// section or clear (and possibly reload) an existing one.
- (void)updateOnDeviceEncryptionSectionWithOldState:
    (PasswordSettingsOnDeviceEncryptionState)oldState {
  UITableView* tableView = self.tableView;
  TableViewModel* tableViewModel = self.tableViewModel;

  // Easy case: the section just needs to be removed.
  if (self.onDeviceEncryptionState ==
          PasswordSettingsOnDeviceEncryptionStateNotShown &&
      [tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierOnDeviceEncryption]) {
    NSInteger section = [self.tableViewModel
        sectionForSectionIdentifier:SectionIdentifierOnDeviceEncryption];
    [tableViewModel
        removeSectionWithIdentifier:SectionIdentifierOnDeviceEncryption];
    if (self.modelLoadStatus == ModelLoadComplete) {
      [tableView deleteSections:[NSIndexSet indexSetWithIndex:section]
               withRowAnimation:UITableViewRowAnimationAutomatic];
    }
    return;
  }

  // Prepare the section in the model, either by clearing or adding it.
  if ([tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierOnDeviceEncryption]) {
    [tableViewModel deleteAllItemsFromSectionWithIdentifier:
                        SectionIdentifierOnDeviceEncryption];
  } else {
    // Find the section that's supposed to be before On-Device Encryption, and
    // insert after that.
    [tableViewModel
        insertSectionWithIdentifier:SectionIdentifierOnDeviceEncryption
                            atIndex:[self
                                        computeOnDeviceEncryptionSectionIndex]];
  }

  // Actually populate the section.
  switch (self.onDeviceEncryptionState) {
    case PasswordSettingsOnDeviceEncryptionStateOptedIn: {
      [tableViewModel addItem:self.onDeviceEncryptionOptedInDescription
          toSectionWithIdentifier:SectionIdentifierOnDeviceEncryption];
      [tableViewModel addItem:self.onDeviceEncryptionOptedInLearnMore
          toSectionWithIdentifier:SectionIdentifierOnDeviceEncryption];
      break;
    }
    case PasswordSettingsOnDeviceEncryptionStateOfferOptIn: {
      [tableViewModel addItem:self.onDeviceEncryptionOptInDescriptionItem
          toSectionWithIdentifier:SectionIdentifierOnDeviceEncryption];
      [tableViewModel addItem:self.setUpOnDeviceEncryptionItem
          toSectionWithIdentifier:SectionIdentifierOnDeviceEncryption];
      break;
    }
    default: {
      // If the state is PasswordSettingsOnDeviceEncryptionStateNotShown, then
      // we shouldn't be trying to populate this section. If it's some other
      // value, then this switch needs to be updated.
      NOTREACHED();
    }
  }

  // If the model hasn't finished loading, there's no need to update the table
  // view.
  if (self.modelLoadStatus != ModelLoadComplete) {
    return;
  }

  NSIndexSet* indexSet = [NSIndexSet
      indexSetWithIndex:
          [tableViewModel
              sectionForSectionIdentifier:SectionIdentifierOnDeviceEncryption]];

  if (oldState == PasswordSettingsOnDeviceEncryptionStateNotShown) {
    [tableView insertSections:indexSet
             withRowAnimation:UITableViewRowAnimationAutomatic];
  } else {
    [tableView reloadSections:indexSet
             withRowAnimation:UITableViewRowAnimationAutomatic];
  }
}

// Returns section index for the change GPM Pin button.
- (NSInteger)computeGPMPinSectionIndex {
  NSInteger previousSection =
      [self shouldDisplayPasskeyUpgradesSwitch]
          ? SectionIdentifierAutomaticPasskeyUpgradesSwitch
          : SectionIdentifierPasswordsInOtherApps;
  return [self.tableViewModel sectionForSectionIdentifier:previousSection] + 1;
}

// Returns section index for the on device encryption. With syncing webauthn
// credentials enabled, it should be after GPM Pin section if it exists
// (otherwise after automatic passkey upgrades switch). Without the feature
// enabled, it should be after passwords in other apps section.
- (NSInteger)computeOnDeviceEncryptionSectionIndex {
  TableViewModel* tableViewModel = self.tableViewModel;
  NSInteger previousSection = SectionIdentifierPasswordsInOtherApps;

  if ([tableViewModel hasSectionForSectionIdentifier:
                          SectionIdentifierGooglePasswordManagerPin]) {
    previousSection = SectionIdentifierGooglePasswordManagerPin;
  } else if ([self shouldDisplayPasskeyUpgradesSwitch]) {
    previousSection = SectionIdentifierAutomaticPasskeyUpgradesSwitch;
  }

  return [tableViewModel sectionForSectionIdentifier:previousSection] + 1;
}

- (void)updateAutomaticPasskeyUpgradesSwitchState {
  if (self.modelLoadStatus != ModelLoadComplete) {
    return;
  }
  self.automaticPasskeyUpgradesSwitchItem.on =
      self.automaticPasskeyUpgradesEnabled;
  [self reconfigureCellsForItems:@[ self.automaticPasskeyUpgradesSwitchItem ]];
}

// Updates the view to by either adding or removing the automatic passkey
// upgrades toggle section. The toggle should be visible if saving passkeys and
// passwords is enabled.
- (void)updateAutomaticPasskeyUpgradesSwitch {
  if (self.modelLoadStatus != ModelLoadComplete) {
    return;
  }

  TableViewModel* model = self.tableViewModel;
  BOOL shouldDisplaySwitch = [self shouldDisplayPasskeyUpgradesSwitch];
  // In this case the whole section doesn't need to be added / removed, just
  // update the switch state if it should be displayed.
  if ([model hasSectionForSectionIdentifier:
                 SectionIdentifierAutomaticPasskeyUpgradesSwitch] ==
      shouldDisplaySwitch) {
    if (shouldDisplaySwitch) {
      [self updateAutomaticPasskeyUpgradesSwitchState];
    }
    return;
  }

  UITableView* tableView = self.tableView;
  if (shouldDisplaySwitch) {
    NSInteger previousSectionIndex = [model
        sectionForSectionIdentifier:SectionIdentifierPasswordsInOtherApps];
    [model insertSectionWithIdentifier:
               SectionIdentifierAutomaticPasskeyUpgradesSwitch
                               atIndex:previousSectionIndex + 1];
    [model addItem:[self automaticPasskeyUpgradesSwitchItem]
        toSectionWithIdentifier:
            SectionIdentifierAutomaticPasskeyUpgradesSwitch];
    NSIndexSet* indexSet = [NSIndexSet
        indexSetWithIndex:
            [model sectionForSectionIdentifier:
                       SectionIdentifierAutomaticPasskeyUpgradesSwitch]];
    [tableView insertSections:indexSet
             withRowAnimation:UITableViewRowAnimationAutomatic];
  } else {
    NSInteger section =
        [model sectionForSectionIdentifier:
                   SectionIdentifierAutomaticPasskeyUpgradesSwitch];
    [model removeSectionWithIdentifier:
               SectionIdentifierAutomaticPasskeyUpgradesSwitch];
    [tableView deleteSections:[NSIndexSet indexSetWithIndex:section]
             withRowAnimation:UITableViewRowAnimationAutomatic];
  }
}

// Automatic passkey upgrades switch should be displayed if the feature is
// enabled and both saving passkeys and password setting is enabled.
- (BOOL)shouldDisplayPasskeyUpgradesSwitch {
  return AutomaticPasskeyUpgradeFeatureEnabled() && _savingPasswordsEnabled &&
         _savingPasskeysEnabled;
}

- (void)updateDeleteAllCredentialsSection {
  if (self.modelLoadStatus == ModelNotLoaded ||
      !base::FeatureList::IsEnabled(
          password_manager::features::kIOSEnableDeleteAllSavedCredentials)) {
    return;
  }

  if (self.canDeleteAllCredentials) {
    _deleteCredentialsItem.textColor = [UIColor colorNamed:kRedColor];
    _deleteCredentialsItem.accessibilityTraits &=
        ~UIAccessibilityTraitNotEnabled;

    _deleteCredentialsFooterItem.text = l10n_util::GetNSString(
        IDS_IOS_PASSWORD_SETTINGS_CREDENTIAL_DELETION_TEXT);
  } else {
    // Disable, rather than remove, because the button will go back and forth
    // between enabled/disabled status as the flow progresses.
    _deleteCredentialsItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _deleteCredentialsItem.accessibilityTraits |=
        UIAccessibilityTraitNotEnabled;

    _deleteCredentialsFooterItem.text = l10n_util::GetNSString(
        IDS_IOS_PASSWORD_SETTINGS_NO_CREDENTIAL_DELETION_TEXT);
  }

  NSIndexSet* section = [NSIndexSet
      indexSetWithIndex:[self.tableViewModel
                            sectionForSectionIdentifier:
                                SectionIdentifierDeleteCredentialsButton]];
  [self.tableView reloadSections:section
                withRowAnimation:UITableViewRowAnimationAutomatic];
}

- (void)updateExportPasswordsButton {
  // This can be invoked before the item is ready when passwords are received
  // too early.
  if (self.modelLoadStatus == ModelNotLoaded) {
    return;
  }

  if (self.canExportPasswords) {
    _exportPasswordsItem.textColor = [UIColor colorNamed:kBlueColor];
    _exportPasswordsItem.accessibilityTraits &= ~UIAccessibilityTraitNotEnabled;
  } else {
    // Disable, rather than remove, because the button will go back and forth
    // between enabled/disabled status as the flow progresses.
    _exportPasswordsItem.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _exportPasswordsItem.accessibilityTraits |= UIAccessibilityTraitNotEnabled;
  }
  [self reconfigureCellsForItems:@[ _exportPasswordsItem ]];
}

- (void)updateChangeGPMPinButton {
  if (self.modelLoadStatus == ModelNotLoaded) {
    return;
  }

  TableViewModel* model = self.tableViewModel;
  if ([model hasSectionForSectionIdentifier:
                 SectionIdentifierGooglePasswordManagerPin] ==
      _canChangeGPMPin) {
    return;
  }

  UITableView* tableView = self.tableView;
  if (_canChangeGPMPin) {
    [model insertSectionWithIdentifier:SectionIdentifierGooglePasswordManagerPin
                               atIndex:[self computeGPMPinSectionIndex]];
    [model addItem:[self changeGooglePasswordManagerPinDescriptionItem]
        toSectionWithIdentifier:SectionIdentifierGooglePasswordManagerPin];
    [model addItem:[self changeGooglePasswordManagerPinItem]
        toSectionWithIdentifier:SectionIdentifierGooglePasswordManagerPin];
    NSIndexSet* indexSet = [NSIndexSet
        indexSetWithIndex:[model
                              sectionForSectionIdentifier:
                                  SectionIdentifierGooglePasswordManagerPin]];
    [tableView insertSections:indexSet
             withRowAnimation:UITableViewRowAnimationAutomatic];
  } else {
    NSInteger section = [model
        sectionForSectionIdentifier:SectionIdentifierGooglePasswordManagerPin];
    [model
        removeSectionWithIdentifier:SectionIdentifierGooglePasswordManagerPin];
    [tableView deleteSections:[NSIndexSet indexSetWithIndex:section]
             withRowAnimation:UITableViewRowAnimationAutomatic];
  }
}

@end
