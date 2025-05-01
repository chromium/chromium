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
#import "base/metrics/user_metrics.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_manager_metrics_util.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/credential_provider/model/features.h"
#import "ios/chrome/browser/passwords/model/features.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_manager_ui_features.h"
#import "ios/chrome/browser/settings/ui_bundled/password/password_settings/password_settings_constants.h"
#import "ios/chrome/browser/shared/coordinator/utils/credential_provider_settings_utils.h"
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
  SectionIdentifierImportPasswordsButton,
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
  ItemTypeImportPasswordsButton,
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

@implementation PasswordSettingsViewController {
  // Tracks whether or not the model has loaded.
  ModelLoadStatus _modelLoadStatus;

  // Whether the bulk move passwords to account section should be visible.
  BOOL _canBulkMoveLocalPasswordsToAccount;

  // Whether the change GPM Pin section should be visible.
  BOOL _canChangeGPMPin;

  // Whether the credential delete button should be enabled.
  BOOL _canDeleteAllCredentials;

  // Whether the exporter should be enabled.
  BOOL _canExportPasswords;

  // Whether automatic passkey upgrades is enabled.
  BOOL _automaticPasskeyUpgradesEnabled;

  // Whether saving passwords is enabled.
  BOOL _savingPasswordsEnabled;

  // Whether saving passwords is managed by the enterprise policy.
  BOOL _savingPasswordsManagedByPolicy;

  // Whether saving passkeys is enabled.
  BOOL _savingPasskeysEnabled;

  // The amount of local passwords present on device.
  int _localPasswordsCount;

  // Email of the signed in user account.
  NSString* _userEmail;

  // On-device encryption state according to the sync service.
  PasswordSettingsOnDeviceEncryptionState _onDeviceEncryptionState;

  // Whether Chromium has been enabled as a credential provider at the iOS
  // level. This may not be known at load time; the detail text showing on or
  // off status will be omitted until this is populated.
  // TODO(crbug.com/396694707): Should become a plain bool once the Passkeys M2
  // feature is launched.
  std::optional<bool> _passwordsInOtherAppsEnabled;

  // Whether the `turnOnPasswordsInOtherAppsItem` should be visible.
  BOOL _shouldShowTurnOnPasswordsInOtherAppsItem;

  // UI elements

  // The item related to the switch for the password manager setting.
  TableViewSwitchItem* _savePasswordsItem;

  // The item related to the enterprise managed save password setting.
  TableViewInfoButtonItem* _managedSavePasswordsItem;

  // The item related to the button allowing users to bulk move passwords to
  // their account.
  TableViewTextItem* _bulkMovePasswordsToAccountButtonItem;

  // The item showing the current status of Passwords in Other Apps (i.e.,
  // credential provider).
  TableViewMultiDetailTextItem* _passwordsInOtherAppsItem;

  // A button which triggers a prompt to allow the user to set the app as a
  // credential provider.
  TableViewTextItem* _turnOnPasswordsInOtherAppsItem;

  // The item related to the switch for the automatic passkey upgrades setting.
  TableViewSwitchItem* _automaticPasskeyUpgradesSwitchItem;

  // The item related to the button for deleting credentials.
  TableViewTextItem* _deleteCredentialsItem;

  // The footer item related to the button for deleting credentials.
  TableViewLinkHeaderFooterItem* _deleteCredentialsFooterItem;

  // The item related to the button for exporting passwords.
  TableViewTextItem* _exportPasswordsItem;

  // The item related to the button for importing passwords.
  TableViewTextItem* _importPasswordsItem;
}

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

  _modelLoadStatus = ModelIsLoading;

  TableViewModel* model = self.tableViewModel;

  [model addSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
  [self addSavePasswordsSwitchOrManagedInfo];

  [model addSectionWithIdentifier:SectionIdentifierPasswordsInOtherApps];
  _passwordsInOtherAppsItem = [self createPasswordsInOtherAppsItem];
  [self updatePasswordsInOtherAppsItem];
  [model addItem:_passwordsInOtherAppsItem
      toSectionWithIdentifier:SectionIdentifierPasswordsInOtherApps];
  [self updateTurnOnPasswordsInOtherAppsItemVisibility];

  if ([self shouldDisplayPasskeyUpgradesSwitch]) {
    [model addSectionWithIdentifier:
               SectionIdentifierAutomaticPasskeyUpgradesSwitch];
    [model addItem:[self createAutomaticPasskeyUpgradesSwitchItem]
        toSectionWithIdentifier:
            SectionIdentifierAutomaticPasskeyUpgradesSwitch];
  }

  if (_canChangeGPMPin) {
    [self updateChangeGPMPinButton];
  }

  if (_onDeviceEncryptionState !=
      PasswordSettingsOnDeviceEncryptionStateNotShown) {
    [self updateOnDeviceEncryptionSectionWithOldState:
              PasswordSettingsOnDeviceEncryptionStateNotShown];
  }

  // Export passwords button.
  [model addSectionWithIdentifier:SectionIdentifierExportPasswordsButton];
  _exportPasswordsItem = [self createExportPasswordsItem];
  [self updateExportPasswordsButton];
  [model addItem:_exportPasswordsItem
      toSectionWithIdentifier:SectionIdentifierExportPasswordsButton];

  // Import passwords button.
  if (base::FeatureList::IsEnabled(kImportPasswordsFromSafari)) {
    [model addSectionWithIdentifier:SectionIdentifierImportPasswordsButton];
    _importPasswordsItem = [self createImportPasswordsItem];
    [model addItem:_importPasswordsItem
        toSectionWithIdentifier:SectionIdentifierImportPasswordsButton];
  }

  if (base::FeatureList::IsEnabled(
          password_manager::features::kIOSEnableDeleteAllSavedCredentials)) {
    // Delete credentials button.
    [model addSectionWithIdentifier:SectionIdentifierDeleteCredentialsButton];
    _deleteCredentialsItem = [self createDeleteCredentialsItem];
    _deleteCredentialsFooterItem = [self createCredentialDeletionFooterItem];
    [self updateDeleteAllCredentialsSection];
    [model addItem:_deleteCredentialsItem
        toSectionWithIdentifier:SectionIdentifierDeleteCredentialsButton];

    // Add footer for the delete credential section.
    [model setFooter:_deleteCredentialsFooterItem
        forSectionWithIdentifier:SectionIdentifierDeleteCredentialsButton];
  }

  if (_canBulkMoveLocalPasswordsToAccount) {
    [self updateBulkMovePasswordsToAccountSection];
  }

  _modelLoadStatus = ModelLoadComplete;
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
        base::RecordAction(
            base::UserMetricsAction("MobilePasswordSettingsTurnOnAutoFill"));

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
      if (_canBulkMoveLocalPasswordsToAccount) {
        [self.delegate bulkMovePasswordsToAccountButtonClicked];
      }
      break;
    }
    case ItemTypeDeleteCredentialsButton: {
      if (_canDeleteAllCredentials) {
        [self.presentationDelegate startDeletionFlow];
      }
      break;
    }
    case ItemTypeExportPasswordsButton: {
      if (_canExportPasswords) {
        [self.presentationDelegate startExportFlow];
      }
      break;
    }
    case ItemTypeImportPasswordsButton: {
      // TODO(crbug.com/407587751): Start import flow.
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
      return _canDeleteAllCredentials;
    case ItemTypeExportPasswordsButton:
      return _canExportPasswords;
    case ItemTypeSavePasswordsSwitch:
      return NO;
    case ItemTypePasswordsInOtherApps:
      return [self shouldPasswordsInOtherAppsBeTappable];
  }
  return YES;
}

#pragma mark - UI item factories

// Creates the switch allowing users to enable/disable the saving of passwords.
- (TableViewSwitchItem*)createSavePasswordsItem {
  TableViewSwitchItem* savePasswordsItem =
      [[TableViewSwitchItem alloc] initWithType:ItemTypeSavePasswordsSwitch];
  savePasswordsItem.text = GetSavePasswordsItemTitle();
  savePasswordsItem.accessibilityIdentifier =
      kPasswordSettingsSavePasswordSwitchTableViewId;
  savePasswordsItem.on = _savingPasswordsEnabled;
  return savePasswordsItem;
}

// Creates the row which replaces `savePasswordsItem` when this preference is
// being managed by enterprise policy.
- (TableViewInfoButtonItem*)createManagedSavePasswordsItem {
  TableViewInfoButtonItem* managedSavePasswordsItem =
      [[TableViewInfoButtonItem alloc]
          initWithType:ItemTypeManagedSavePasswords];
  managedSavePasswordsItem.text = GetSavePasswordsItemTitle();
  managedSavePasswordsItem.accessibilityHint =
      l10n_util::GetNSString(IDS_IOS_TOGGLE_SETTING_MANAGED_ACCESSIBILITY_HINT);
  managedSavePasswordsItem.accessibilityIdentifier =
      kPasswordSettingsManagedSavePasswordSwitchTableViewId;
  managedSavePasswordsItem.statusText = l10n_util::GetNSString(
      _savingPasswordsEnabled ? IDS_IOS_SETTING_ON : IDS_IOS_SETTING_OFF);
  return managedSavePasswordsItem;
}

// Creates and returns the move passwords to account description item.
- (TableViewDetailTextItem*)createBulkMovePasswordsToAccountDescriptionItem {
  TableViewDetailTextItem* bulkMovePasswordsToAccountDescriptionItem =
      [[TableViewDetailTextItem alloc]
          initWithType:ItemTypeBulkMovePasswordsToAccountDescription];
  bulkMovePasswordsToAccountDescriptionItem.text = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_SECTION_TITLE);
  bulkMovePasswordsToAccountDescriptionItem.accessibilityIdentifier =
      kPasswordSettingsBulkMovePasswordsToAccountDescriptionTableViewId;
  bulkMovePasswordsToAccountDescriptionItem.allowMultilineDetailText = YES;

  std::u16string pattern = l10n_util::GetStringUTF16(
      IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_SECTION_DESCRIPTION);
  std::u16string result = base::i18n::MessageFormatter::FormatWithNamedArgs(
      pattern, "COUNT", _localPasswordsCount, "EMAIL",
      base::SysNSStringToUTF16(_userEmail));

  bulkMovePasswordsToAccountDescriptionItem.detailText =
      base::SysUTF16ToNSString(result);

  return bulkMovePasswordsToAccountDescriptionItem;
}

// Creates and returns the move passwords to account button.
- (TableViewTextItem*)createBulkMovePasswordsToAccountButtonItem {
  TableViewTextItem* bulkMovePasswordsToAccountButtonItem =
      [[TableViewTextItem alloc]
          initWithType:ItemTypeBulkMovePasswordsToAccountButton];
  bulkMovePasswordsToAccountButtonItem.text = l10n_util::GetPluralNSStringF(
      IDS_IOS_PASSWORD_SETTINGS_BULK_UPLOAD_PASSWORDS_SECTION_BUTTON,
      _localPasswordsCount);
  bulkMovePasswordsToAccountButtonItem.textColor =
      [UIColor colorNamed:kBlueColor];
  bulkMovePasswordsToAccountButtonItem.accessibilityTraits =
      UIAccessibilityTraitButton;
  bulkMovePasswordsToAccountButtonItem.accessibilityIdentifier =
      kPasswordSettingsBulkMovePasswordsToAccountButtonTableViewId;
  return bulkMovePasswordsToAccountButtonItem;
}

- (TableViewMultiDetailTextItem*)createPasswordsInOtherAppsItem {
  TableViewMultiDetailTextItem* passwordsInOtherAppsItem =
      [[TableViewMultiDetailTextItem alloc]
          initWithType:ItemTypePasswordsInOtherApps];
  passwordsInOtherAppsItem.text = GetPasswordsInOtherAppsItemTitle();
  if (IOSPasskeysM2Enabled()) {
    if (@available(iOS 18.0, *)) {
      passwordsInOtherAppsItem.leadingDetailText = l10n_util::GetNSString(
          IDS_IOS_PASSWORD_SETTINGS_PASSWORDS_IN_OTHER_APPS_DESCRIPTION);
    }
  } else {
    passwordsInOtherAppsItem.accessoryType =
        UITableViewCellAccessoryDisclosureIndicator;
    passwordsInOtherAppsItem.accessibilityTraits |= UIAccessibilityTraitButton;
  }
  passwordsInOtherAppsItem.accessibilityIdentifier =
      kPasswordSettingsPasswordsInOtherAppsRowId;
  return passwordsInOtherAppsItem;
}

- (TableViewTextItem*)createTurnOnPasswordsInOtherAppsItem {
  TableViewTextItem* turnOnPasswordsInOtherAppsItem = [[TableViewTextItem alloc]
      initWithType:ItemTypeTurnOnPasswordsInOtherAppsButton];
  turnOnPasswordsInOtherAppsItem.text = l10n_util::GetNSString(
      IDS_IOS_CREDENTIAL_PROVIDER_SETTINGS_TURN_ON_AUTOFILL);
  turnOnPasswordsInOtherAppsItem.textColor = [UIColor colorNamed:kBlueColor];
  turnOnPasswordsInOtherAppsItem.accessibilityTraits =
      UIAccessibilityTraitButton;
  return turnOnPasswordsInOtherAppsItem;
}

- (TableViewSwitchItem*)createAutomaticPasskeyUpgradesSwitchItem {
  TableViewSwitchItem* automaticPasskeyUpgradesSwitchItem =
      [[TableViewSwitchItem alloc]
          initWithType:ItemTypeAutomaticPasskeyUpgradesSwitch];
  automaticPasskeyUpgradesSwitchItem.text =
      l10n_util::GetNSString(IDS_IOS_ALLOW_AUTOMATIC_PASSKEY_UPGRADES);
  automaticPasskeyUpgradesSwitchItem.detailText =
      l10n_util::GetNSString(IDS_IOS_ALLOW_AUTOMATIC_PASSKEY_UPGRADES_SUBTITLE);
  automaticPasskeyUpgradesSwitchItem.on = _automaticPasskeyUpgradesEnabled;
  return automaticPasskeyUpgradesSwitchItem;
}

- (TableViewImageItem*)createChangeGooglePasswordManagerPinDescriptionItem {
  TableViewImageItem* changeGooglePasswordManagerPinDescriptionItem =
      [[TableViewImageItem alloc]
          initWithType:ItemTypeChangeGooglePasswordManagerPinDescription];
  changeGooglePasswordManagerPinDescriptionItem.title = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_GOOGLE_PASSWORD_MANAGER_PIN_TITLE);
  changeGooglePasswordManagerPinDescriptionItem.detailText =
      l10n_util::GetNSString(
          IDS_IOS_PASSWORD_SETTINGS_GOOGLE_PASSWORD_MANAGER_PIN_DESCRIPTION);
  changeGooglePasswordManagerPinDescriptionItem.accessibilityIdentifier =
      kPasswordSettingsChangePinDescriptionId;
  return changeGooglePasswordManagerPinDescriptionItem;
}

- (TableViewTextItem*)createChangeGooglePasswordManagerPinItem {
  TableViewTextItem* changeGooglePasswordManagerPinItem =
      [[TableViewTextItem alloc]
          initWithType:ItemTypeChangeGooglePasswordManagerPinButton];
  changeGooglePasswordManagerPinItem.text =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS_CHANGE_PIN);
  changeGooglePasswordManagerPinItem.textColor =
      [UIColor colorNamed:kBlueColor];
  changeGooglePasswordManagerPinItem.accessibilityTraits =
      UIAccessibilityTraitButton;
  changeGooglePasswordManagerPinItem.accessibilityIdentifier =
      kPasswordSettingsChangePinButtonId;
  return changeGooglePasswordManagerPinItem;
}

- (TableViewImageItem*)createOnDeviceEncryptionOptInDescriptionItem {
  TableViewImageItem* onDeviceEncryptionOptInDescriptionItem =
      [[TableViewImageItem alloc]
          initWithType:ItemTypeOnDeviceEncryptionOptInDescription];
  onDeviceEncryptionOptInDescriptionItem.title =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION);
  onDeviceEncryptionOptInDescriptionItem.detailText = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_OPT_IN);
  onDeviceEncryptionOptInDescriptionItem.enabled = NO;
  onDeviceEncryptionOptInDescriptionItem.accessibilityIdentifier =
      kPasswordSettingsOnDeviceEncryptionOptInId;
  return onDeviceEncryptionOptInDescriptionItem;
}

- (TableViewImageItem*)createOnDeviceEncryptionOptedInDescription {
  TableViewImageItem* onDeviceEncryptionOptedInDescription =
      [[TableViewImageItem alloc]
          initWithType:ItemTypeOnDeviceEncryptionOptedInDescription];
  onDeviceEncryptionOptedInDescription.title =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION);
  onDeviceEncryptionOptedInDescription.detailText = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_LEARN_MORE);
  onDeviceEncryptionOptedInDescription.enabled = NO;
  onDeviceEncryptionOptedInDescription.accessibilityIdentifier =
      kPasswordSettingsOnDeviceEncryptionOptedInTextId;
  return onDeviceEncryptionOptedInDescription;
}

- (TableViewTextItem*)createOnDeviceEncryptionOptedInLearnMore {
  TableViewTextItem* onDeviceEncryptionOptedInLearnMore =
      [[TableViewTextItem alloc]
          initWithType:ItemTypeOnDeviceEncryptionOptedInLearnMore];
  onDeviceEncryptionOptedInLearnMore.text = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_OPTED_IN_LEARN_MORE);
  onDeviceEncryptionOptedInLearnMore.textColor =
      [UIColor colorNamed:kBlueColor];
  onDeviceEncryptionOptedInLearnMore.accessibilityTraits =
      UIAccessibilityTraitButton;
  onDeviceEncryptionOptedInLearnMore.accessibilityIdentifier =
      kPasswordSettingsOnDeviceEncryptionLearnMoreId;
  return onDeviceEncryptionOptedInLearnMore;
}

- (TableViewTextItem*)createSetUpOnDeviceEncryptionItem {
  TableViewTextItem* setUpOnDeviceEncryptionItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeOnDeviceEncryptionSetUp];
  setUpOnDeviceEncryptionItem.text = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_ON_DEVICE_ENCRYPTION_SET_UP);
  setUpOnDeviceEncryptionItem.textColor = [UIColor colorNamed:kBlueColor];
  setUpOnDeviceEncryptionItem.accessibilityTraits = UIAccessibilityTraitButton;
  setUpOnDeviceEncryptionItem.accessibilityIdentifier =
      kPasswordSettingsOnDeviceEncryptionSetUpId;
  return setUpOnDeviceEncryptionItem;
}

// Creates the "Export Passwords..." button. Coloring and enabled/disabled state
// are handled by `updateExportPasswordsButton`, which should be called as soon
// as the mediator has provided the necessary state.
- (TableViewTextItem*)createExportPasswordsItem {
  TableViewTextItem* exportPasswordsItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeExportPasswordsButton];
  exportPasswordsItem.text = l10n_util::GetNSString(IDS_IOS_EXPORT_PASSWORDS);
  exportPasswordsItem.accessibilityTraits = UIAccessibilityTraitButton;
  return exportPasswordsItem;
}

// Creates the "Import Passwords..." button.
- (TableViewTextItem*)createImportPasswordsItem {
  TableViewTextItem* importPasswordsItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeImportPasswordsButton];
  importPasswordsItem.text = l10n_util::GetNSString(IDS_IOS_IMPORT_PASSWORDS);
  importPasswordsItem.accessibilityTraits = UIAccessibilityTraitButton;
  importPasswordsItem.textColor = [UIColor colorNamed:kBlueColor];
  return importPasswordsItem;
}

// Creates the "Delete all data" button.
- (TableViewTextItem*)createDeleteCredentialsItem {
  TableViewTextItem* deleteCredentialsItem =
      [[TableViewTextItem alloc] initWithType:ItemTypeDeleteCredentialsButton];
  deleteCredentialsItem.text = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SETTINGS_DELETE_ALL_SAVED_CREDENTIALS);
  deleteCredentialsItem.accessibilityTraits = UIAccessibilityTraitButton;
  return deleteCredentialsItem;
}

// Creates the footer item for "Delete all data" button.
- (TableViewLinkHeaderFooterItem*)createCredentialDeletionFooterItem {
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
  BOOL managedChanged = _savingPasswordsManagedByPolicy != managedByPolicy;
  if (!enabledChanged && !managedChanged) {
    return;
  }

  _savingPasswordsEnabled = enabled;
  _savingPasswordsManagedByPolicy = managedByPolicy;

  if (_modelLoadStatus == ModelNotLoaded) {
    return;
  }

  // If `_savingPasswordsManagedByPolicy` changed, the section needs to be
  // redrawn. Otherwise, the existing item needs to be updated.
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
  } else if (_savingPasswordsManagedByPolicy) {
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

- (void)setUserEmail:(NSString*)userEmail {
  _userEmail = userEmail;
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
      _canBulkMoveLocalPasswordsToAccount == showSection) {
    return;
  }

  _localPasswordsCount = count;
  _canBulkMoveLocalPasswordsToAccount = showSection;
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

  if (_modelLoadStatus == ModelNotLoaded) {
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

  if (_modelLoadStatus == ModelNotLoaded) {
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
  if (_savingPasswordsManagedByPolicy) {
    _managedSavePasswordsItem = [self createManagedSavePasswordsItem];
  } else {
    _savePasswordsItem = [self createSavePasswordsItem];
  }
  [self.tableViewModel addItem:_savingPasswordsManagedByPolicy
                                   ? _managedSavePasswordsItem
                                   : _savePasswordsItem
       toSectionWithIdentifier:SectionIdentifierSavePasswordsSwitch];
}

// Updates the appearance of the Managed Save Passwords item to reflect the
// current state of `isSavePasswordEnabled`.
- (void)updateManagedSavePasswordsItem {
  _managedSavePasswordsItem.statusText =
      _savingPasswordsEnabled ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
                              : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  [self reconfigureCellsForItems:@[ _managedSavePasswordsItem ]];
}

// Updates the appearance of the Save Passwords switch to reflect the current
// state of `isSavePasswordEnabled`.
- (void)updateSavePasswordsSwitch {
  _savePasswordsItem.on = _savingPasswordsEnabled;

  if (_modelLoadStatus != ModelLoadComplete) {
    return;
  }
  [self reconfigureCellsForItems:@[ _savePasswordsItem ]];
}

- (void)updateBulkMovePasswordsToAccountSection {
  UITableView* tableView = self.tableView;
  TableViewModel* tableViewModel = self.tableViewModel;
  BOOL sectionExists =
      [tableViewModel hasSectionForSectionIdentifier:
                          SectionIdentifierBulkMovePasswordsToAccount];

  // Remove the section if it exists and we shouldn't show it.
  if (!_canBulkMoveLocalPasswordsToAccount && sectionExists) {
    NSInteger section =
        [tableViewModel sectionForSectionIdentifier:
                            SectionIdentifierBulkMovePasswordsToAccount];
    [tableViewModel removeSectionWithIdentifier:
                        SectionIdentifierBulkMovePasswordsToAccount];
    if (_modelLoadStatus == ModelLoadComplete) {
      [tableView deleteSections:[NSIndexSet indexSetWithIndex:section]
               withRowAnimation:UITableViewRowAnimationAutomatic];
    }
    return;
  }

  if (!_canBulkMoveLocalPasswordsToAccount) {
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
  [tableViewModel addItem:[self createBulkMovePasswordsToAccountDescriptionItem]
      toSectionWithIdentifier:SectionIdentifierBulkMovePasswordsToAccount];
  _bulkMovePasswordsToAccountButtonItem =
      [self createBulkMovePasswordsToAccountButtonItem];
  [tableViewModel addItem:_bulkMovePasswordsToAccountButtonItem
      toSectionWithIdentifier:SectionIdentifierBulkMovePasswordsToAccount];

  NSIndexSet* indexSet = [NSIndexSet
      indexSetWithIndex:[tableViewModel
                            sectionForSectionIdentifier:
                                SectionIdentifierBulkMovePasswordsToAccount]];

  if (_modelLoadStatus != ModelLoadComplete) {
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
    _passwordsInOtherAppsItem.trailingDetailText =
        _passwordsInOtherAppsEnabled.value()
            ? l10n_util::GetNSString(IDS_IOS_SETTING_ON)
            : l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
    _passwordsInOtherAppsItem.accessoryType =
        UITableViewCellAccessoryDisclosureIndicator;
    _passwordsInOtherAppsItem.accessibilityTraits |= UIAccessibilityTraitButton;
  } else {
    _passwordsInOtherAppsItem.trailingDetailText = nil;
    _passwordsInOtherAppsItem.accessoryType = UITableViewCellAccessoryNone;
    _passwordsInOtherAppsItem.accessibilityTraits &=
        ~UIAccessibilityTraitButton;
  }

  if (_modelLoadStatus != ModelLoadComplete) {
    return;
  }
  [self reconfigureCellsForItems:@[ _passwordsInOtherAppsItem ]];

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
  if (_modelLoadStatus == ModelNotLoaded) {
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
    _turnOnPasswordsInOtherAppsItem =
        [self createTurnOnPasswordsInOtherAppsItem];
    [self setTurnOnPasswordsInOtherAppsItemEnabled:YES];
    [model addItem:_turnOnPasswordsInOtherAppsItem
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
  TableViewTextItem* item = _turnOnPasswordsInOtherAppsItem;
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
  // Record the user's decision.
  RecordTurnOnCredentialProviderExtensionPromptOutcome(
      TurnOnCredentialProviderExtensionPromptSource::kPasswordSettings,
      appWasEnabledForAutoFill);

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
  if (_onDeviceEncryptionState ==
          PasswordSettingsOnDeviceEncryptionStateNotShown &&
      [tableViewModel
          hasSectionForSectionIdentifier:SectionIdentifierOnDeviceEncryption]) {
    NSInteger section = [self.tableViewModel
        sectionForSectionIdentifier:SectionIdentifierOnDeviceEncryption];
    [tableViewModel
        removeSectionWithIdentifier:SectionIdentifierOnDeviceEncryption];
    if (_modelLoadStatus == ModelLoadComplete) {
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
  switch (_onDeviceEncryptionState) {
    case PasswordSettingsOnDeviceEncryptionStateOptedIn: {
      [tableViewModel addItem:[self createOnDeviceEncryptionOptedInDescription]
          toSectionWithIdentifier:SectionIdentifierOnDeviceEncryption];
      [tableViewModel addItem:[self createOnDeviceEncryptionOptedInLearnMore]
          toSectionWithIdentifier:SectionIdentifierOnDeviceEncryption];
      break;
    }
    case PasswordSettingsOnDeviceEncryptionStateOfferOptIn: {
      [tableViewModel addItem:[self
                                  createOnDeviceEncryptionOptInDescriptionItem]
          toSectionWithIdentifier:SectionIdentifierOnDeviceEncryption];
      [tableViewModel addItem:[self createSetUpOnDeviceEncryptionItem]
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
  if (_modelLoadStatus != ModelLoadComplete) {
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
  if (_modelLoadStatus != ModelLoadComplete) {
    return;
  }
  _automaticPasskeyUpgradesSwitchItem.on = _automaticPasskeyUpgradesEnabled;
  [self reconfigureCellsForItems:@[ _automaticPasskeyUpgradesSwitchItem ]];
}

// Updates the view to by either adding or removing the automatic passkey
// upgrades toggle section. The toggle should be visible if saving passkeys and
// passwords is enabled.
- (void)updateAutomaticPasskeyUpgradesSwitch {
  if (_modelLoadStatus != ModelLoadComplete) {
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
    _automaticPasskeyUpgradesSwitchItem =
        [self createAutomaticPasskeyUpgradesSwitchItem];
    [model addItem:_automaticPasskeyUpgradesSwitchItem
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
  if (_modelLoadStatus == ModelNotLoaded ||
      !base::FeatureList::IsEnabled(
          password_manager::features::kIOSEnableDeleteAllSavedCredentials)) {
    return;
  }

  if (_canDeleteAllCredentials) {
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
  if (_modelLoadStatus == ModelNotLoaded) {
    return;
  }

  if (_canExportPasswords) {
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
  if (_modelLoadStatus == ModelNotLoaded) {
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
    [model addItem:[self createChangeGooglePasswordManagerPinDescriptionItem]
        toSectionWithIdentifier:SectionIdentifierGooglePasswordManagerPin];
    [model addItem:[self createChangeGooglePasswordManagerPinItem]
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
