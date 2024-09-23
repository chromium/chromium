// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/safety_check/safety_check_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/numerics/safe_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "base/version.h"
#import "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#import "components/password_manager/core/browser/password_sync_util.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/safety_check/safety_check.h"
#import "components/sync/service/sync_service.h"
#import "components/sync/service/sync_user_settings.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/omaha/model/omaha_service.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/model/password_check_observer_bridge.h"
#import "ios/chrome/browser/passwords/model/password_checkup_utils.h"
#import "ios/chrome/browser/passwords/model/password_store_observer_bridge.h"
#import "ios/chrome/browser/push_notification/model/push_notification_client_id.h"
#import "ios/chrome/browser/push_notification/model/push_notification_settings_util.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/prefs/pref_backed_boolean.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/utils/observable_boolean.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/signin/model/authentication_service.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_constants.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_consumer.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_mediator+Testing.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_utils.h"
#import "ios/chrome/browser/upgrade/model/upgrade_constants.h"
#import "ios/chrome/browser/upgrade/model/upgrade_recommended_details.h"
#import "ios/chrome/browser/upgrade/model/upgrade_utils.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/common/url_scheme_util.h"
#import "net/base/apple/url_conversions.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/time_format.h"
#import "url/gurl.h"

using l10n_util::GetNSString;
using password_manager::WarningType;

namespace {

// The size of leading symbol icons.
constexpr NSInteger kLeadingSymbolImagePointSize = 22;

typedef NSArray<TableViewItem*>* ItemArray;

typedef NS_ENUM(NSInteger, SafteyCheckItemType) {
  // CheckTypes section.
  UpdateItemType = kItemTypeEnumZero,
  PasswordItemType,
  SafeBrowsingItemType,
  HeaderItem,
  // CheckStart section.
  CheckStartItemType,
  TimestampFooterItem,
  // Notifications opt-in section.
  NotificationsOptInItemType,
};

// The minimum time each of the three checks should show a running state. This
// is to prevent any check that finshes quicky from causing the UI to appear
// jittery. The numbers are all different so that no 2 tests finish at the same
// time if they all end up using their min delays.
constexpr double kUpdateRowMinDelay = 2.0;
constexpr double kPasswordRowMinDelay = 1.5;
constexpr double kSafeBrowsingRowMinDelay = 3.0;

// Returns true if any of the save passwords are insecure.
bool FoundInsecurePasswords(PasswordCheckRowStates password_check_row_state) {
  switch (password_check_row_state) {
    case PasswordCheckRowStateSafe:
    case PasswordCheckRowStateDefault:
    case PasswordCheckRowStateRunning:
    case PasswordCheckRowStateDisabled:
    case PasswordCheckRowStateError:
      return false;
    case PasswordCheckRowStateUnmutedCompromisedPasswords:
    case PasswordCheckRowStateReusedPasswords:
    case PasswordCheckRowStateWeakPasswords:
    case PasswordCheckRowStateDismissedWarnings:
      return true;
  }
}

// Helper method to determine whether the password check item is tappable or
// not.
bool IsPasswordCheckItemTappable(
    PasswordCheckRowStates password_check_row_state) {
  switch (password_check_row_state) {
    case PasswordCheckRowStateUnmutedCompromisedPasswords:
    case PasswordCheckRowStateReusedPasswords:
    case PasswordCheckRowStateWeakPasswords:
    case PasswordCheckRowStateDismissedWarnings:
    case PasswordCheckRowStateSafe:
      return true;
    case PasswordCheckRowStateDefault:
    case PasswordCheckRowStateRunning:
    case PasswordCheckRowStateDisabled:
    case PasswordCheckRowStateError:
      return false;
  }
}

// Resets the state of the given SettingsCheckItem.
void ResetSettingsCheckItem(SettingsCheckItem* item) {
  item.enabled = YES;
  item.indicatorHidden = YES;
  item.infoButtonHidden = YES;
  item.trailingImage = nil;
  item.trailingImageTintColor = nil;
  item.accessoryType = UITableViewCellAccessoryNone;
}

}  // namespace

@interface SafetyCheckMediator () <BooleanObserver, PasswordCheckObserver> {
  scoped_refptr<IOSChromePasswordCheckManager> _passwordCheckManager;

  // A helper object for observing changes in the password check status
  // and changes to the compromised credentials list. It needs to be destroyed
  // before `_passwordCheckManager`, so it needs to be declared afterwards.
  std::unique_ptr<PasswordCheckObserverBridge> _passwordCheckObserver;
}

// Header for the Safety Check page.
@property(nonatomic, strong) TableViewLinkHeaderFooterItem* headerItem;

// If the Safe Browsing preference is managed.
@property(nonatomic, assign) BOOL safeBrowsingPreferenceManaged;

// The service responsible for password check feature.
@property(nonatomic, assign) scoped_refptr<IOSChromePasswordCheckManager>
    passwordCheckManager;

// If any checks in safety check are still running.
@property(nonatomic, assign, readonly) BOOL checksRemaining;

// Service used to check if user is signed in.
@property(nonatomic, assign) AuthenticationService* authService;

// Service to check if passwords are synced.
@property(nonatomic, assign) syncer::SyncService* syncService;

// Service used to check user preference values.
@property(nonatomic, assign, readonly) PrefService* userPrefService;

// Service used to check local preference values.
@property(nonatomic, assign, readonly) PrefService* localPrefService;

// When the check was started.
@property(nonatomic, assign) base::Time checkStartTime;

// SettingsCheckItem used to display the state of the update check.
@property(nonatomic, strong) SettingsCheckItem* updateCheckItem;

// Current state of the update check.
@property(nonatomic, assign) UpdateCheckRowStates updateCheckRowState;

// Previous on load or finished check state of the update check.
@property(nonatomic, assign) UpdateCheckRowStates previousUpdateCheckRowState;

// SettingsCheckItem used to display the state of the password check.
@property(nonatomic, strong) SettingsCheckItem* passwordCheckItem;

// Current state of the password check.
@property(nonatomic, assign) PasswordCheckRowStates passwordCheckRowState;

// Previous on load or finished check state of the password check.
@property(nonatomic, assign)
    PasswordCheckRowStates previousPasswordCheckRowState;

// SettingsCheckItem used to display the state of the Safe Browsing check.
@property(nonatomic, strong) SettingsCheckItem* safeBrowsingCheckItem;

// Current state of the Safe Browsing check.
@property(nonatomic, assign)
    SafeBrowsingCheckRowStates safeBrowsingCheckRowState;

// Previous on load or finished check state of the Safe Browsing check.
@property(nonatomic, assign)
    SafeBrowsingCheckRowStates previousSafeBrowsingCheckRowState;

// Row button to start the safety check.
@property(nonatomic, strong) TableViewTextItem* checkStartItem;

// Current state of the start safety check row button.
@property(nonatomic, assign) CheckStartStates checkStartState;

// Row button to opt-in to Safety Check notifications.
@property(nonatomic, strong) TableViewTextItem* notificationsOptInItem;

// Whether or not a safety check just ran.
@property(nonatomic, assign) BOOL checkDidRun;

// Current state of password check.
@property(nonatomic, assign) PasswordCheckState currentPasswordCheckState;

// Preference value for Safe Browsing.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* safeBrowsingPreference;

// Preference value for Enhanced Safe Browsing.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* enhancedSafeBrowsingPreference;

@end

@implementation SafetyCheckMediator

@synthesize passwordCheckManager = _passwordCheckManager;

- (instancetype)
    initWithUserPrefService:(PrefService*)userPrefService
           localPrefService:(PrefService*)localPrefService
       passwordCheckManager:
           (scoped_refptr<IOSChromePasswordCheckManager>)passwordCheckManager
                authService:(AuthenticationService*)authService
                syncService:(syncer::SyncService*)syncService
                   referrer:(password_manager::PasswordCheckReferrer)referrer {
  self = [super init];

  if (self) {
    DCHECK(userPrefService);
    DCHECK(localPrefService);
    DCHECK(passwordCheckManager);
    DCHECK(authService);
    DCHECK(syncService);

    _userPrefService = userPrefService;
    _localPrefService = localPrefService;
    _authService = authService;
    _syncService = syncService;

    _passwordCheckManager = passwordCheckManager;
    _currentPasswordCheckState = _passwordCheckManager->GetPasswordCheckState();

    _passwordCheckObserver = std::make_unique<PasswordCheckObserverBridge>(
        self, _passwordCheckManager.get());

    _safeBrowsingPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kSafeBrowsingEnabled];
    _safeBrowsingPreference.observer = self;
    _safeBrowsingPreferenceManaged =
        userPrefService->IsManagedPreference(prefs::kSafeBrowsingEnabled);
    _enhancedSafeBrowsingPreference = [[PrefBackedBoolean alloc]
        initWithPrefService:userPrefService
                   prefName:prefs::kSafeBrowsingEnhanced];
    _enhancedSafeBrowsingPreference.observer = self;

    _headerItem =
        [[TableViewLinkHeaderFooterItem alloc] initWithType:HeaderItem];

    _headerItem.text =
        referrer ==
                password_manager::PasswordCheckReferrer::kSafetyCheckMagicStack
            ? l10n_util::GetNSString(
                  IDS_IOS_SETTINGS_SAFETY_CHECK_MAGIC_STACK_PAGE_HEADER)
            : l10n_util::GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_PAGE_HEADER);

    _updateCheckRowState = UpdateCheckRowStateDefault;
    _updateCheckItem = [[SettingsCheckItem alloc] initWithType:UpdateItemType];
    _updateCheckItem.text =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_TITLE);
    UIImage* updateCheckIcon = DefaultSymbolTemplateWithPointSize(
        kInfoCircleSymbol, kLeadingSymbolImagePointSize);
    _updateCheckItem.leadingIcon = updateCheckIcon;
    _updateCheckItem.leadingIconTintColor = [UIColor colorNamed:kGrey400Color];
    ResetSettingsCheckItem(_updateCheckItem);

    // Show unsafe state if the app is out of date and safety check already
    // found an issue.
    if (!IsAppUpToDate() && PreviousSafetyCheckIssueFound()) {
      _updateCheckRowState = UpdateCheckRowStateOutOfDate;
    }

    _previousUpdateCheckRowState = _updateCheckRowState;

    _passwordCheckRowState = PasswordCheckRowStateDefault;
    _passwordCheckItem =
        [[SettingsCheckItem alloc] initWithType:PasswordItemType];
    _passwordCheckItem.text =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_PASSWORDS_TITLE);

    UIImage* passwordCheckIcon = CustomSymbolTemplateWithPointSize(
        kPasswordSymbol, kLeadingSymbolImagePointSize);

    _passwordCheckItem.leadingIcon = passwordCheckIcon;
    _passwordCheckItem.leadingIconTintColor =
        [UIColor colorNamed:kGrey400Color];
    ResetSettingsCheckItem(_passwordCheckItem);

    // Show unsafe state if user already ran safety check and there are insecure
    // credentials.
    std::vector<password_manager::CredentialUIEntry> insecureCredentials =
        _passwordCheckManager->GetInsecureCredentials();
    if (!insecureCredentials.empty() && PreviousSafetyCheckIssueFound()) {
      _passwordCheckRowState =
          [self passwordCheckRowStateFromHighestPriorityWarningType:
                    insecureCredentials];
    }

    _previousPasswordCheckRowState = _passwordCheckRowState;

    _safeBrowsingCheckRowState = SafeBrowsingCheckRowStateDefault;
    _previousSafeBrowsingCheckRowState = _safeBrowsingCheckRowState;
    _safeBrowsingCheckItem =
        [[SettingsCheckItem alloc] initWithType:SafeBrowsingItemType];
    _safeBrowsingCheckItem.text = l10n_util::GetNSString(
        IDS_IOS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_TITLE);
    UIImage* safeBrowsingCheckIcon =
        CustomSymbolWithPointSize(kPrivacySymbol, kLeadingSymbolImagePointSize);
    _safeBrowsingCheckItem.leadingIcon = safeBrowsingCheckIcon;
    _safeBrowsingCheckItem.leadingIconTintColor =
        [UIColor colorNamed:kGrey400Color];
    ResetSettingsCheckItem(_safeBrowsingCheckItem);

    _checkStartState = CheckStartStateDefault;
    _checkStartItem =
        [[TableViewTextItem alloc] initWithType:CheckStartItemType];
    _checkStartItem.accessibilityIdentifier =
        kSafetyCheckCheckNowButtonAccessibilityID;
    _checkStartItem.text = GetNSString(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON);
    _checkStartItem.textColor = [UIColor colorNamed:kBlueColor];
    _checkStartItem.accessibilityTraits |= UIAccessibilityTraitButton;

    if (IsSafetyCheckNotificationsEnabled()) {
      TableViewTextItem* notificationsOptInItem =
          [[TableViewTextItem alloc] initWithType:NotificationsOptInItemType];

      notificationsOptInItem.accessibilityIdentifier =
          kSafetyCheckNotificationsOptInButtonAccessibilityID;
      notificationsOptInItem.text =
          push_notification_settings::
                  GetMobileNotificationPermissionStatusForClient(
                      PushNotificationClientId::kSafetyCheck, "")
              ? GetNSString(
                    IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_TURN_OFF_NOTIFICATIONS_ELLIPSIS)
              : GetNSString(
                    IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_TURN_ON_NOTIFICATIONS_ELLIPSIS);
      notificationsOptInItem.textColor = [UIColor colorNamed:kBlueColor];
      notificationsOptInItem.accessibilityTraits |= UIAccessibilityTraitButton;

      self.notificationsOptInItem = notificationsOptInItem;
    }
  }

  return self;
}

- (void)setConsumer:(id<SafetyCheckConsumer>)consumer {
  if (_consumer == consumer)
    return;
  _consumer = consumer;
  NSArray* checkItems = @[
    self.updateCheckItem, self.passwordCheckItem, self.safeBrowsingCheckItem
  ];
  [_consumer setCheckItems:checkItems];
  [_consumer setSafetyCheckHeaderItem:self.headerItem];
  [_consumer setCheckStartItem:self.checkStartItem];
  [_consumer setNotificationsOptInItem:self.notificationsOptInItem];

  // Need to reconfigure the safety check items if there are remaining issues
  // from the last check ran.
  [self reconfigurePasswordCheckItem];
  [self reconfigureUpdateCheckItem];
  [self reconfigureSafeBrowsingCheckItem];
}

#pragma mark - Public Methods

- (void)startCheckIfNotRunning {
  if (self.checksRemaining) {
    return;
  }
  [self startCheck];
}

- (void)reconfigureNotificationsSection:(BOOL)enabled {
  CHECK(IsSafetyCheckNotificationsEnabled());

  // If notifications are `enabled`, the button should prompt users to disable
  // them.
  self.notificationsOptInItem.text =
      enabled
          ? GetNSString(
                IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_TURN_OFF_NOTIFICATIONS_ELLIPSIS)
          : GetNSString(
                IDS_IOS_SAFETY_CHECK_NOTIFICATIONS_TURN_ON_NOTIFICATIONS_ELLIPSIS);

  [self reconfigureCellForItem:self.notificationsOptInItem];
}

#pragma mark - PasswordCheckObserver

- (void)passwordCheckStateDidChange:(PasswordCheckState)state {
  if (state == self.currentPasswordCheckState) {
    return;
  }

  // If password check reports the device is offline, propogate this information
  // to the update check.
  if (state == PasswordCheckState::kOffline) {
    [self handleUpdateCheckOffline];
  }
  self.passwordCheckRowState = [self computePasswordCheckRowState:state];
  // Push update to the display.
  [self reconfigurePasswordCheckItem];
}

- (void)insecureCredentialsDidChange {
  self.passwordCheckRowState =
      [self computePasswordCheckRowState:self.currentPasswordCheckState];
  // Push update to the display.
  [self reconfigurePasswordCheckItem];
}

- (void)passwordCheckManagerWillShutdown {
  _passwordCheckObserver.reset();
}

#pragma mark - SafetyCheckServiceDelegate

- (void)didSelectItem:(TableViewItem*)item {
  SafteyCheckItemType type = static_cast<SafteyCheckItemType>(item.type);
  switch (type) {
    // Few selections are handled here explicitly, but all states are laid out
    // to have one location that shows all actions that are taken from the
    // safety check page.
    // i tap: tap on the info i handled by infoButtonWasTapped.
    case UpdateItemType: {
      switch (self.updateCheckRowState) {
        case UpdateCheckRowStateDefault:       // No tap action.
        case UpdateCheckRowStateRunning:       // No tap action.
        case UpdateCheckRowStateUpToDate:      // No tap action.
        case UpdateCheckRowStateChannel:       // No tap action.
        case UpdateCheckRowStateManaged:       // i tap: Managed state popover.
        case UpdateCheckRowStateOmahaError:    // i tap: Show error popover.
        case UpdateCheckRowStateNetError:      // i tap: Show error popover.
          break;
        case UpdateCheckRowStateOutOfDate: {  // i tap: Go to app store.
          PrefService* prefService = GetApplicationContext()->GetLocalState();
          std::string updateLocation =
              prefService->GetString(kIOSChromeUpgradeURLKey);
          base::RecordAction(base::UserMetricsAction(
              "Settings.SafetyCheck.RelaunchAfterUpdates"));
          base::UmaHistogramEnumeration(
              kSafetyCheckInteractions,
              SafetyCheckInteractions::kUpdatesRelaunch);
          [self.handler
              showUpdateAtLocation:base::SysUTF8ToNSString(updateLocation)];
          break;
        }
      }
      break;
    }
    case PasswordItemType: {
      switch (self.passwordCheckRowState) {
        case PasswordCheckRowStateDefault:   // No tap action.
        case PasswordCheckRowStateRunning:   // No tap action.
        case PasswordCheckRowStateDisabled:  // i tap: Show error popover.
        case PasswordCheckRowStateError:     // i tap: Show error popover.
          break;
        case PasswordCheckRowStateSafe:
        case PasswordCheckRowStateReusedPasswords:
        case PasswordCheckRowStateWeakPasswords:
        case PasswordCheckRowStateDismissedWarnings:
        case PasswordCheckRowStateUnmutedCompromisedPasswords:  // Go to
                                                                // password
                                                                // issues or
                                                                // password
                                                                // checkup page.
          base::RecordAction(
              base::UserMetricsAction("Settings.SafetyCheck.ManagePasswords"));
          base::UmaHistogramEnumeration(
              kSafetyCheckInteractions,
              SafetyCheckInteractions::kPasswordsManage);

          [self.handler showPasswordCheckupPage];
          break;
      }
      break;
    }
    case SafeBrowsingItemType: {
      switch (self.safeBrowsingCheckRowState) {
        case SafeBrowsingCheckRowStateDefault:  // No tap action.
        case SafeBrowsingCheckRowStateRunning:  // No tap action.
        case SafeBrowsingCheckRowStateManaged:  // i tap: Managed state popover.
          break;
        case SafeBrowsingCheckRowStateSafe:
        case SafeBrowsingCheckRowStateUnsafe:  // Show Safe Browsing settings.
          [self.handler showSafeBrowsingPreferencePage];
          break;
      }
      break;
    }
    case CheckStartItemType: {  // Start or stop a safety check.
      [self checkStartOrCancel];
      break;
    }
    case NotificationsOptInItemType: {
      [self.delegate toggleSafetyCheckNotifications];
      break;
    }
    case HeaderItem:
    case TimestampFooterItem:
      break;
  }
}

- (BOOL)isItemClickable:(TableViewItem*)item {
  SafteyCheckItemType type = static_cast<SafteyCheckItemType>(item.type);
  switch (type) {
    case UpdateItemType:
      return self.updateCheckRowState == UpdateCheckRowStateOutOfDate;
    case PasswordItemType:
      return IsPasswordCheckItemTappable(self.passwordCheckRowState);
    case CheckStartItemType:
      return YES;
    case SafeBrowsingItemType:
      return safe_browsing::GetSafeBrowsingState(*self.userPrefService) ==
                 safe_browsing::SafeBrowsingState::STANDARD_PROTECTION ||
             self.safeBrowsingCheckRowState == SafeBrowsingCheckRowStateUnsafe;
    case HeaderItem:
    case TimestampFooterItem:
      return NO;
    case NotificationsOptInItemType:
      return YES;
  }
}

- (BOOL)isItemWithErrorInfo:(TableViewItem*)item {
  SafteyCheckItemType type = static_cast<SafteyCheckItemType>(item.type);
  return (type != CheckStartItemType && type != NotificationsOptInItemType);
}

- (void)infoButtonWasTapped:(UIButton*)buttonView
              usingItemType:(NSInteger)itemType {
  // Show the managed popover if needed.
  if (itemType == SafeBrowsingItemType &&
      self.safeBrowsingCheckRowState == SafeBrowsingCheckRowStateManaged) {
    [self.handler showManagedInfoFrom:buttonView];
    return;
  }

    if (itemType == SafeBrowsingItemType) {
      // Directly open Safe Browsing settings instead of showing a popover.
      [self.handler showSafeBrowsingPreferencePage];
      return;
    }

  if (itemType == UpdateItemType &&
      self.updateCheckRowState == UpdateCheckRowStateManaged) {
    [self.handler showManagedInfoFrom:buttonView];
    return;
  }

  // If not managed compute error info to show in popover, if available.
  NSAttributedString* info = [self popoverInfoForType:itemType];

  // If `info` is empty there is no popover to display.
  if (!info)
    return;

  // Push popover to coordinator.
  [self.handler showErrorInfoFrom:buttonView withText:info];
}

#pragma mark - BooleanObserver

- (void)booleanDidChange:(id<ObservableBoolean>)observableBoolean {
  [self checkAndReconfigureSafeBrowsingState];
}

#pragma mark - Private methods

// Computes the text needed for a popover on `itemType` if available.
- (NSAttributedString*)popoverInfoForType:(NSInteger)itemType {
  SafteyCheckItemType type = static_cast<SafteyCheckItemType>(itemType);
  switch (type) {
    case PasswordItemType:
      return [self passwordCheckErrorInfo];
    case UpdateItemType:
      return [self updateCheckErrorInfoString];
    case CheckStartItemType:
    case HeaderItem:
    case SafeBrowsingItemType:
    case TimestampFooterItem:
    case NotificationsOptInItemType:
      return nil;
  }
}

// Computes the appropriate display state of the password check row based on
// `currentPasswordCheckState`.
- (PasswordCheckRowStates)computePasswordCheckRowState:
    (PasswordCheckState)newState {
  BOOL wasRunning =
      self.currentPasswordCheckState == PasswordCheckState::kRunning;
  self.currentPasswordCheckState = newState;

  std::vector<password_manager::CredentialUIEntry> insecureCredentials =
      _passwordCheckManager->GetInsecureCredentials();
  BOOL noInsecurePasswords = insecureCredentials.empty();

  switch (self.currentPasswordCheckState) {
    case PasswordCheckState::kRunning:
      return PasswordCheckRowStateRunning;
    case PasswordCheckState::kNoPasswords:
      return PasswordCheckRowStateDisabled;
    case PasswordCheckState::kSignedOut:
      base::UmaHistogramEnumeration(kSafetyCheckMetricsPasswords,
                                    safety_check::PasswordsStatus::kSignedOut);
      return PasswordCheckRowStateError;
    case PasswordCheckState::kOffline:
      base::UmaHistogramEnumeration(kSafetyCheckMetricsPasswords,
                                    safety_check::PasswordsStatus::kOffline);
      return PasswordCheckRowStateError;
    case PasswordCheckState::kQuotaLimit:
      base::UmaHistogramEnumeration(kSafetyCheckMetricsPasswords,
                                    safety_check::PasswordsStatus::kQuotaLimit);
      return PasswordCheckRowStateError;
    case PasswordCheckState::kOther:
      base::UmaHistogramEnumeration(kSafetyCheckMetricsPasswords,
                                    safety_check::PasswordsStatus::kError);
      return PasswordCheckRowStateError;
    case PasswordCheckState::kCanceled:
    case PasswordCheckState::kIdle: {
      if (self.currentPasswordCheckState == PasswordCheckState::kIdle) {
        // Safe state is only possible after the state transitioned from
        // kRunning to kIdle.
        if (wasRunning) {
          if (noInsecurePasswords) {
            base::UmaHistogramEnumeration(kSafetyCheckMetricsPasswords,
                                          safety_check::PasswordsStatus::kSafe);
            return PasswordCheckRowStateSafe;
          }
          // Reaching this point means that there are insecure passwords.
          return [self passwordCheckRowStateFromHighestPriorityWarningType:
                           insecureCredentials];
        }
      }
      return PasswordCheckRowStateDefault;
    }
  }
}

// Returns the right PasswordCheckRowState depending on the highest priority
// warning type.
- (PasswordCheckRowStates)passwordCheckRowStateFromHighestPriorityWarningType:
    (const std::vector<password_manager::CredentialUIEntry>&)
        insecureCredentials {
  switch (GetWarningOfHighestPriority(insecureCredentials)) {
    case WarningType::kCompromisedPasswordsWarning:
      base::UmaHistogramEnumeration(
          kSafetyCheckMetricsPasswords,
          safety_check::PasswordsStatus::kCompromisedExist);
      return PasswordCheckRowStateUnmutedCompromisedPasswords;
    case WarningType::kReusedPasswordsWarning:
      base::UmaHistogramEnumeration(
          kSafetyCheckMetricsPasswords,
          safety_check::PasswordsStatus::kReusedPasswordsExist);
      return PasswordCheckRowStateReusedPasswords;
    case WarningType::kWeakPasswordsWarning:
      base::UmaHistogramEnumeration(
          kSafetyCheckMetricsPasswords,
          safety_check::PasswordsStatus::kWeakPasswordsExist);
      return PasswordCheckRowStateWeakPasswords;
    case WarningType::kDismissedWarningsWarning:
      base::UmaHistogramEnumeration(
          kSafetyCheckMetricsPasswords,
          safety_check::PasswordsStatus::kMutedCompromisedExist);
      return PasswordCheckRowStateDismissedWarnings;
    case WarningType::kNoInsecurePasswordsWarning:
      base::UmaHistogramEnumeration(kSafetyCheckMetricsPasswords,
                                    safety_check::PasswordsStatus::kSafe);
      return PasswordCheckRowStateSafe;
  }
}

// Computes the appropriate error info to be displayed in the updates popover.
- (NSAttributedString*)updateCheckErrorInfoString {
  NSString* message;

  switch (self.updateCheckRowState) {
    case UpdateCheckRowStateDefault:
    case UpdateCheckRowStateRunning:
    case UpdateCheckRowStateUpToDate:
    case UpdateCheckRowStateOutOfDate:
    case UpdateCheckRowStateManaged:
      return nil;
    case UpdateCheckRowStateOmahaError:
      message = l10n_util::GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_ERROR_INFO);
      break;
    case UpdateCheckRowStateNetError:
      message = l10n_util::GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_OFFLINE_INFO);
      break;
    case UpdateCheckRowStateChannel:
      break;
  }
  return [self attributedStringWithText:message link:GURL()];
}

// Computes the appropriate error info to be displayed in the passwords popover.
- (NSAttributedString*)passwordCheckErrorInfo {
  if (!self.passwordCheckManager->GetInsecureCredentials().empty()) {
    return nil;
  }

  NSString* message;
  GURL linkURL;

  switch (self.currentPasswordCheckState) {
    case PasswordCheckState::kRunning:
    case PasswordCheckState::kNoPasswords:
      message =
          l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ERROR_NO_PASSWORDS);
      break;
    case PasswordCheckState::kCanceled:
    case PasswordCheckState::kIdle:
      return nil;
    case PasswordCheckState::kSignedOut:
      message =
          l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ERROR_SIGNED_OUT);
      break;
    case PasswordCheckState::kOffline:
      message = l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ERROR_OFFLINE);
      break;
    case PasswordCheckState::kQuotaLimit:
      if ([self canUseAccountPasswordCheckup]) {
        message = l10n_util::GetNSString(
            IDS_IOS_PASSWORD_CHECKUP_ERROR_QUOTA_LIMIT_VISIT_GOOGLE);
        linkURL = password_manager::GetPasswordCheckupURL(
            password_manager::PasswordCheckupReferrer::kPasswordCheck);
      } else {
        message =
            l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ERROR_QUOTA_LIMIT);
      }
      break;
    case PasswordCheckState::kOther:
      message = l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECKUP_ERROR_OTHER);
      break;
  }
  return [self attributedStringWithText:message link:linkURL];
}

// Computes whether user is capable to run password check in Google Account.
- (BOOL)canUseAccountPasswordCheckup {
  return password_manager::sync_util::GetAccountForSaving(self.userPrefService,
                                                          self.syncService) &&
         !self.syncService->GetUserSettings()->IsEncryptEverythingEnabled();
}

// Configures check error info with a link for popovers.
- (NSAttributedString*)attributedStringWithText:(NSString*)text
                                           link:(GURL)link {
  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleSubheadline],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor]
  };

  if (link.is_empty()) {
    return [[NSMutableAttributedString alloc] initWithString:text
                                                  attributes:textAttributes];
  }
  NSDictionary* linkAttributes =
      @{NSLinkAttributeName : net::NSURLWithGURL(link)};

  return AttributedStringFromStringWithLink(text, textAttributes,
                                            linkAttributes);
}

// Upon a tap of checkStartItem either starts or cancels a safety check.
- (void)checkStartOrCancel {
  // If a check is already running cancel it.
  if (self.checksRemaining) {
    self.checkDidRun = NO;
    // Revert checks that are still running to their previous state.
    if (self.updateCheckRowState == UpdateCheckRowStateRunning) {
      self.updateCheckRowState = self.previousUpdateCheckRowState;
      [self reconfigureUpdateCheckItem];
    }
    if (self.passwordCheckRowState == PasswordCheckRowStateRunning) {
      self.passwordCheckManager->StopPasswordCheck();
      self.passwordCheckRowState = self.previousPasswordCheckRowState;
      [self reconfigurePasswordCheckItem];
    }
    if (self.safeBrowsingCheckRowState == SafeBrowsingCheckRowStateRunning) {
      self.safeBrowsingCheckRowState = self.previousSafeBrowsingCheckRowState;
      [self reconfigureSafeBrowsingCheckItem];
    }

    // Change checkStartItem to default state.
    self.checkStartState = CheckStartStateDefault;
    [self reconfigureCheckStartSection];

    return;
  }
  [self startCheck];
}

// Starts a safety check
- (void)startCheck {
  // Otherwise start a check.
  self.checkStartTime = base::Time::Now();

  if (IsSafetyCheckMagicStackEnabled()) {
    [self updateTimestampOfLastRun];
  }

  // Record the current state of the checks.
  self.previousUpdateCheckRowState = self.updateCheckRowState;
  self.previousPasswordCheckRowState = self.passwordCheckRowState;
  self.previousSafeBrowsingCheckRowState = self.safeBrowsingCheckRowState;

  // Set check items to spinning wheel.
  self.updateCheckRowState = UpdateCheckRowStateRunning;
  self.passwordCheckRowState = PasswordCheckRowStateRunning;
  self.safeBrowsingCheckRowState = SafeBrowsingCheckRowStateRunning;

  // Record all running.
  base::RecordAction(base::UserMetricsAction("Settings.SafetyCheck.Start"));
  base::UmaHistogramEnumeration(kSafetyCheckInteractions,
                                SafetyCheckInteractions::kStarted);
  base::UmaHistogramEnumeration(kSafetyCheckMetricsUpdates,
                                safety_check::UpdateStatus::kChecking);
  base::UmaHistogramEnumeration(kSafetyCheckMetricsPasswords,
                                safety_check::PasswordsStatus::kChecking);
  base::UmaHistogramEnumeration(kSafetyCheckMetricsSafeBrowsing,
                                safety_check::SafeBrowsingStatus::kChecking);

  // Change checkStartItem to cancel state.
  self.checkStartState = CheckStartStateCancel;

  // Hide the timestamp while running.
  [self.consumer setTimestampFooterItem:nil];

  self.checkDidRun = YES;

  // Update the display.
  [self reconfigureUpdateCheckItem];
  [self reconfigurePasswordCheckItem];
  [self reconfigureSafeBrowsingCheckItem];
  [self reconfigureCheckStartSection];

  // The display should be changed to loading icons before any checks are
  // started.
  if (self.checksRemaining) {
    // Only perfom update check on supported channels.
    switch (::GetChannel()) {
      case version_info::Channel::STABLE:
      case version_info::Channel::BETA:
      case version_info::Channel::DEV: {
        [self performUpdateCheck];
        break;
      }
      case version_info::Channel::CANARY:
      case version_info::Channel::UNKNOWN: {
        [self possiblyDelayReconfigureUpdateCheckItemWithState:
                  UpdateCheckRowStateChannel];
        break;
      }
    }
    __weak __typeof__(self) weakSelf = self;
    // This handles a discrepancy between password check and safety check.  In
    // password check a user cannot start a check if they have no passwords, but
    // in safety check they can, but the `passwordCheckManager` won't even start
    // a check. This if block below allows safety check to push the disabled
    // state after check now is pressed.
    if (self.currentPasswordCheckState == PasswordCheckState::kNoPasswords) {
      // Want to show the loading wheel momentarily.
      dispatch_after(
          dispatch_time(DISPATCH_TIME_NOW,
                        (int64_t)(kPasswordRowMinDelay * NSEC_PER_SEC)),
          dispatch_get_main_queue(), ^{
            // Check if the check was cancelled while waiting, we do not want to
            // push a completed state to the UI if the check was cancelled.
            if (weakSelf.checksRemaining) {
              weakSelf.passwordCheckRowState = PasswordCheckRowStateDisabled;
              [weakSelf reconfigurePasswordCheckItem];

              base::UmaHistogramEnumeration(
                  kSafetyCheckMetricsPasswords,
                  safety_check::PasswordsStatus::kNoPasswords);
            }
          });
    } else {
      self.passwordCheckManager->StartPasswordCheck(
          password_manager::LeakDetectionInitiator::kBulkSyncedPasswordsCheck);
    }
    // Want to show the loading wheel momentarily.
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW,
                      (int64_t)(kSafeBrowsingRowMinDelay * NSEC_PER_SEC)),
        dispatch_get_main_queue(), ^{
          // Check if the check was cancelled while waiting, we do not want to
          // push a completed state to the UI if the check was cancelled.
          if (weakSelf.checksRemaining)
            [weakSelf checkAndReconfigureSafeBrowsingState];

          NSString* announcement = weakSelf.updateCheckItem.detailText;
          announcement = [announcement
              stringByAppendingString:weakSelf.passwordCheckItem.detailText];
          announcement = [announcement
              stringByAppendingString:weakSelf.safeBrowsingCheckItem
                                          .detailText];
          UIAccessibilityPostNotification(
              UIAccessibilityScreenChangedNotification, announcement);
        });
  }
}

// Checks if any of the safety checks are still running, resets `checkStartItem`
// if all checks have finished.
- (void)resetsCheckStartItemIfNeeded {
  if (self.checksRemaining) {
    return;
  }

  // If a check has finished and issues were found, update the timestamp.
  BOOL issuesFound =
      (self.updateCheckRowState == UpdateCheckRowStateOutOfDate) ||
      (FoundInsecurePasswords(self.passwordCheckRowState));
  if (self.checkDidRun && issuesFound) {
    [self updateTimestampOfLastCheck];

    if (IsSafetyCheckMagicStackEnabled()) {
      [self updateTimestampOfLastRun];
    }

    self.checkDidRun = NO;
  } else if (self.checkDidRun && !issuesFound) {
    // Clear the timestamp if the last check found no issues.
    [[NSUserDefaults standardUserDefaults]
        setDouble:base::Time().InSecondsFSinceUnixEpoch()
           forKey:kTimestampOfLastIssueFoundKey];
    self.checkDidRun = NO;

    if (IsSafetyCheckMagicStackEnabled()) {
      [self updateTimestampOfLastRun];
    }
  }
  // If no checks are still running, reset `checkStartItem`.
  self.checkStartState = CheckStartStateDefault;
  [self reconfigureCheckStartSection];

  // Since no checks are running, attempt to show the timestamp.
  [self showTimestampIfNeeded];
}

// Computes if any of the safety checks are still running.
- (BOOL)checksRemaining {
  BOOL passwordCheckRunning =
      self.passwordCheckRowState == PasswordCheckRowStateRunning;
  BOOL safeBrowsingCheckRunning =
      self.safeBrowsingCheckRowState == SafeBrowsingCheckRowStateRunning;
  BOOL updateCheckRunning =
      self.updateCheckRowState == UpdateCheckRowStateRunning;
  return updateCheckRunning || passwordCheckRunning || safeBrowsingCheckRunning;
}

// Updates `updateCheckItem` to reflect the device being offline if the check
// was running.
- (void)handleUpdateCheckOffline {
  if (self.updateCheckRowState == UpdateCheckRowStateRunning) {
    self.updateCheckRowState = UpdateCheckRowStateNetError;
    [self reconfigureUpdateCheckItem];

    base::UmaHistogramEnumeration(kSafetyCheckMetricsUpdates,
                                  safety_check::UpdateStatus::kFailedOffline);
  }
}

// Verifies if the Omaha service returned an answer, if not sets
// `updateCheckItem` to an Omaha error state.
- (void)verifyUpdateCheckComplete {
  // If still in running state assume Omaha error.
  if (self.updateCheckRowState == UpdateCheckRowStateRunning) {
    self.updateCheckRowState = UpdateCheckRowStateOmahaError;
    [self reconfigureUpdateCheckItem];

    base::UmaHistogramEnumeration(kSafetyCheckMetricsUpdates,
                                  safety_check::UpdateStatus::kFailed);
  }
}

// If the update check would have completed too quickly, making the UI appear
// jittery, delay the reconfigure call, using `newRowState`.
- (void)possiblyDelayReconfigureUpdateCheckItemWithState:
    (UpdateCheckRowStates)newRowState {
  double secondsSinceStart = base::Time::Now().InSecondsFSinceUnixEpoch() -
                             self.checkStartTime.InSecondsFSinceUnixEpoch();
  double minDelay = kUpdateRowMinDelay;
  if (secondsSinceStart < minDelay) {
    // Want to show the loading wheel for minimum time.
    __weak __typeof__(self) weakSelf = self;
    dispatch_after(
        dispatch_time(DISPATCH_TIME_NOW,
                      (int64_t)((minDelay - secondsSinceStart) * NSEC_PER_SEC)),
        dispatch_get_main_queue(), ^{
          // Check if the check was cancelled while waiting, we do not want to
          // push a completed state to the UI if the check was cancelled.
          if (weakSelf.checksRemaining) {
            weakSelf.updateCheckRowState = newRowState;
            [weakSelf reconfigureUpdateCheckItem];
          }
        });
  } else {
    self.updateCheckRowState = newRowState;
    [self reconfigureUpdateCheckItem];
  }
}

// Processes the response from the Omaha service.
- (void)handleOmahaResponse:(const UpgradeRecommendedDetails&)details {
  // If before the response the check was canceled, or Omaha assumed faulty,
  // do nothing.
  if (self.updateCheckRowState != UpdateCheckRowStateRunning) {
    return;
  }

  if (details.is_up_to_date) {
    [self possiblyDelayReconfigureUpdateCheckItemWithState:
              UpdateCheckRowStateUpToDate];
    base::UmaHistogramEnumeration(kSafetyCheckMetricsUpdates,
                                  safety_check::UpdateStatus::kUpdated);
  } else {
    // upgradeURL and next_version are only set if not up to date.
    const GURL& upgradeUrl = details.upgrade_url;

    if (!upgradeUrl.is_valid()) {
      [self possiblyDelayReconfigureUpdateCheckItemWithState:
                UpdateCheckRowStateOmahaError];

      base::UmaHistogramEnumeration(kSafetyCheckMetricsUpdates,
                                    safety_check::UpdateStatus::kFailed);
      return;
    }

    if (!details.next_version.size() ||
        !base::Version(details.next_version).IsValid()) {
      [self possiblyDelayReconfigureUpdateCheckItemWithState:
                UpdateCheckRowStateOmahaError];

      base::UmaHistogramEnumeration(kSafetyCheckMetricsUpdates,
                                    safety_check::UpdateStatus::kFailed);
      return;
    }
    [self possiblyDelayReconfigureUpdateCheckItemWithState:
              UpdateCheckRowStateOutOfDate];

    base::UmaHistogramEnumeration(kSafetyCheckMetricsUpdates,
                                  safety_check::UpdateStatus::kOutdated);

    // Valid results, update all prefs.
    PrefService* prefService = GetApplicationContext()->GetLocalState();
    prefService->SetString(kIOSChromeNextVersionKey, details.next_version);
    prefService->SetString(kIOSChromeUpgradeURLKey, upgradeUrl.spec());

    // Treat the safety check finding the device out of date as if the update
    // infobar was just shown to not overshow the infobar to the user.
    prefService->SetTime(kLastInfobarDisplayTimeKey, base::Time::Now());
  }
}

// Performs the update check and triggers the display update to
// `updateCheckItem`.
- (void)performUpdateCheck {
  __weak __typeof__(self) weakSelf = self;

  OmahaService::CheckNow(base::BindOnce(^(UpgradeRecommendedDetails details) {
    [weakSelf handleOmahaResponse:details];
  }));

  // If after 30 seconds the Omaha server has not responded, assume Omaha error.
  dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(30 * NSEC_PER_SEC)),
                 dispatch_get_main_queue(), ^{
                   [weakSelf verifyUpdateCheckComplete];
                 });
}

// Performs the Safe Browsing check and triggers the display update/
- (void)checkAndReconfigureSafeBrowsingState {
  if (!self.safeBrowsingPreferenceManaged) {
    if (self.safeBrowsingPreference.value) {
      self.safeBrowsingCheckRowState = SafeBrowsingCheckRowStateSafe;
      base::UmaHistogramEnumeration(kSafetyCheckMetricsSafeBrowsing,
                                    safety_check::SafeBrowsingStatus::kEnabled);
    } else {
      self.safeBrowsingCheckRowState = SafeBrowsingCheckRowStateUnsafe;
      base::UmaHistogramEnumeration(
          kSafetyCheckMetricsSafeBrowsing,
          safety_check::SafeBrowsingStatus::kDisabled);
    }
  }
  if (self.safeBrowsingCheckRowState == SafeBrowsingCheckRowStateUnsafe &&
      self.safeBrowsingPreferenceManaged) {
    self.safeBrowsingCheckRowState = SafeBrowsingCheckRowStateManaged;
    base::UmaHistogramEnumeration(
        kSafetyCheckMetricsSafeBrowsing,
        safety_check::SafeBrowsingStatus::kDisabledByAdmin);
  }

  [self reconfigureSafeBrowsingCheckItem];
}

// Reconfigures the display of the `updateCheckItem` based on current state of
// `updateCheckRowState`.
- (void)reconfigureUpdateCheckItem {
  // Reset state to prevent conflicts.
  ResetSettingsCheckItem(self.updateCheckItem);

  // On any item update, see if `checkStartItem` should be updated.
  [self resetsCheckStartItemIfNeeded];

  switch (self.updateCheckRowState) {
    case UpdateCheckRowStateDefault: {
      self.updateCheckItem.detailText =
          GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_DESCRIPTION);
      break;
    }
    case UpdateCheckRowStateRunning: {
      self.updateCheckItem.indicatorHidden = NO;
      break;
    }
    case UpdateCheckRowStateUpToDate: {
      self.updateCheckItem.detailText =
          GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_UP_TO_DATE_DESC);
      self.updateCheckItem.warningState = WarningState::kSafe;
      break;
    }
    case UpdateCheckRowStateOutOfDate: {
      self.updateCheckItem.detailText =
          GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_OUT_OF_DATE_DESC);
      self.updateCheckItem.warningState = WarningState::kSevereWarning;
      self.updateCheckItem.accessoryType =
          UITableViewCellAccessoryDisclosureIndicator;
      break;
    }
    case UpdateCheckRowStateManaged: {
      self.updateCheckItem.infoButtonHidden = NO;
      self.updateCheckItem.detailText =
          GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_MANAGED_DESC);
      break;
    }
    case UpdateCheckRowStateOmahaError: {
      self.updateCheckItem.infoButtonHidden = NO;
      self.updateCheckItem.detailText =
          GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_ERROR_DESC);
      break;
    }
    case UpdateCheckRowStateNetError: {
      self.updateCheckItem.infoButtonHidden = NO;
      self.updateCheckItem.detailText =
          GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_OFFLINE_DESC);
      break;
    }
    case UpdateCheckRowStateChannel: {
      switch (::GetChannel()) {
        case version_info::Channel::STABLE:
        case version_info::Channel::DEV:
          break;
        case version_info::Channel::BETA: {
          self.updateCheckItem.detailText = GetNSString(
              IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_CHANNEL_BETA_DESC);
          break;
        }
        case version_info::Channel::CANARY:
        case version_info::Channel::UNKNOWN: {
          self.updateCheckItem.detailText = GetNSString(
              IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_CHANNEL_CANARY_DESC);
          break;
        }
      }
    }
  }
  [self reconfigureCellForItem:_updateCheckItem];
}

// Reconfigures the display of the `passwordCheckItem` based on current state of
// `passwordCheckRowState`.
- (void)reconfigurePasswordCheckItem {
  // Reset state to prevent conflicts.
  ResetSettingsCheckItem(self.passwordCheckItem);

  // Set the accessory type.
  if (IsPasswordCheckItemTappable(self.passwordCheckRowState)) {
    self.passwordCheckItem.accessoryType =
        UITableViewCellAccessoryDisclosureIndicator;
  }

  // On any item update, see if `checkStartItem` should be updated.
  [self resetsCheckStartItemIfNeeded];

  switch (self.passwordCheckRowState) {
    case PasswordCheckRowStateDefault: {
      self.passwordCheckItem.detailText =
          GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_PASSWORDS_DESCRIPTION);
      break;
    }
    case PasswordCheckRowStateRunning: {
      self.passwordCheckItem.detailText =
          GetNSString(IDS_IOS_SAFETY_CHECK_PASSWORD_CHECKUP_ONGOING);
      self.passwordCheckItem.indicatorHidden = NO;
      break;
    }
    case PasswordCheckRowStateSafe: {
      DCHECK(self.passwordCheckManager->GetInsecureCredentials().empty());
      self.passwordCheckItem.detailText =
          base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
              IDS_IOS_PASSWORD_CHECKUP_COMPROMISED_COUNT, 0));
      self.passwordCheckItem.warningState = WarningState::kSafe;
      break;
    }
    case PasswordCheckRowStateUnmutedCompromisedPasswords: {
      NSInteger compromisedPasswordCount = GetPasswordCountForWarningType(
          WarningType::kCompromisedPasswordsWarning,
          self.passwordCheckManager->GetInsecureCredentials());
      self.passwordCheckItem.detailText =
          base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
              IDS_IOS_PASSWORD_CHECKUP_COMPROMISED_COUNT,
              compromisedPasswordCount));
      self.passwordCheckItem.warningState = WarningState::kSevereWarning;
      break;
    }
    case PasswordCheckRowStateReusedPasswords: {
      NSInteger reusedPasswordCount = GetPasswordCountForWarningType(
          WarningType::kReusedPasswordsWarning,
          self.passwordCheckManager->GetInsecureCredentials());
      self.passwordCheckItem.detailText =
          l10n_util::GetNSStringF(IDS_IOS_PASSWORD_CHECKUP_REUSED_COUNT,
                                  base::NumberToString16(reusedPasswordCount));
      self.passwordCheckItem.trailingImage = nil;
      break;
    }
    case PasswordCheckRowStateWeakPasswords: {
      NSInteger weakPasswordCount = GetPasswordCountForWarningType(
          WarningType::kWeakPasswordsWarning,
          self.passwordCheckManager->GetInsecureCredentials());
      self.passwordCheckItem.detailText =
          base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
              IDS_IOS_PASSWORD_CHECKUP_WEAK_COUNT, weakPasswordCount));
      self.passwordCheckItem.trailingImage = nil;
      break;
    }
    case PasswordCheckRowStateDismissedWarnings: {
      NSInteger dismissedWarningCount = GetPasswordCountForWarningType(
          WarningType::kDismissedWarningsWarning,
          self.passwordCheckManager->GetInsecureCredentials());
      self.passwordCheckItem.detailText =
          base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
              IDS_IOS_PASSWORD_CHECKUP_DISMISSED_COUNT, dismissedWarningCount));
      self.passwordCheckItem.trailingImage = nil;
      break;
    }
    case PasswordCheckRowStateDisabled:
    case PasswordCheckRowStateError: {
      self.passwordCheckItem.detailText =
          l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECK_ERROR);
      self.passwordCheckItem.infoButtonHidden = NO;
      break;
    }
  }

  [self reconfigureCellForItem:_passwordCheckItem];
}

// Reconfigures the display of the `safeBrowsingCheckItem` based on current
// state of `safeBrowsingCheckRowState`.
- (void)reconfigureSafeBrowsingCheckItem {
  // Reset state to prevent conflicts.
  ResetSettingsCheckItem(self.safeBrowsingCheckItem);

  // On any item update, see if `checkStartItem` should be updated.
  [self resetsCheckStartItemIfNeeded];

  switch (self.safeBrowsingCheckRowState) {
    case SafeBrowsingCheckRowStateDefault: {
      self.safeBrowsingCheckItem.detailText =
          GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_DESCRIPTION);
      break;
    }
    case SafeBrowsingCheckRowStateRunning: {
      self.safeBrowsingCheckItem.indicatorHidden = NO;
      break;
    }
    case SafeBrowsingCheckRowStateManaged: {
      self.safeBrowsingCheckItem.infoButtonHidden = NO;
      self.safeBrowsingCheckItem.detailText =
          GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_MANAGED_DESC);
      break;
    }
    case SafeBrowsingCheckRowStateSafe: {
      self.safeBrowsingCheckItem.detailText =
          [self safeBrowsingCheckItemDetailText];
      self.safeBrowsingCheckItem.warningState = WarningState::kSafe;
      if (safe_browsing::GetSafeBrowsingState(*self.userPrefService) ==
          safe_browsing::SafeBrowsingState::STANDARD_PROTECTION) {
        self.safeBrowsingCheckItem.accessoryType =
            UITableViewCellAccessoryDisclosureIndicator;
      }
      break;
    }
    case SafeBrowsingCheckRowStateUnsafe: {
      self.safeBrowsingCheckItem.detailText = GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_DISABLED_DESC);
      self.safeBrowsingCheckItem.warningState = WarningState::kSevereWarning;
      self.safeBrowsingCheckItem.accessoryType =
          UITableViewCellAccessoryDisclosureIndicator;
      break;
    }
  }

  [self reconfigureCellForItem:_safeBrowsingCheckItem];
}

// Chooses the Safe Browsing detail text string that should be used based on the
// Safe Browsing preference chosen.
- (NSString*)safeBrowsingCheckItemDetailText {
  safe_browsing::SafeBrowsingState safeBrowsingState =
      safe_browsing::GetSafeBrowsingState(*self.userPrefService);
  switch (safeBrowsingState) {
    case safe_browsing::SafeBrowsingState::STANDARD_PROTECTION:
      return GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_STANDARD_PROTECTION_ENABLED_DESC_WITH_ENHANCED_PROTECTION);
    case safe_browsing::SafeBrowsingState::ENHANCED_PROTECTION:
      return GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_ENHANCED_PROTECTION_ENABLED_DESC);
    default:
      NOTREACHED_IN_MIGRATION();
      return nil;
  }
}

// Updates the display of checkStartItem based on its current state.
- (void)reconfigureCheckStartSection {
  if (self.checkStartState == CheckStartStateDefault) {
    self.checkStartItem.text = GetNSString(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON);
  } else {
    self.checkStartItem.text =
        GetNSString(IDS_IOS_CANCEL_PASSWORD_CHECK_BUTTON);
  }
  [self reconfigureCellForItem:_checkStartItem];
}

// Updates the timestamp of when safety check last found an issue.
- (void)updateTimestampOfLastCheck {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setDouble:base::Time::Now().InSecondsFSinceUnixEpoch()
               forKey:kTimestampOfLastIssueFoundKey];
}

// Updates the timestamp of when the safety check was most recently run.
//
// TODO(crbug.com/40930653): Remove this method once Settings Safety Check is
// refactored to use the new Safety Check Manager.
- (void)updateTimestampOfLastRun {
  _localPrefService->SetTime(prefs::kIosSettingsSafetyCheckLastRunTime,
                             base::Time::Now());
}

// Shows the timestamp if the last safety check found issues.
- (void)showTimestampIfNeeded {
  if (PreviousSafetyCheckIssueFound()) {
    TableViewLinkHeaderFooterItem* footerItem =
        [[TableViewLinkHeaderFooterItem alloc]
            initWithType:TimestampFooterItem];
    footerItem.text = [self formatElapsedTimeSinceLastCheck];
    [self.consumer setTimestampFooterItem:footerItem];
  } else {
    // Hide the timestamp if the last safety check didn't find issues.
    [self.consumer setTimestampFooterItem:nil];
  }
}

// Formats the last safety check issues found timestamp for display.
- (NSString*)formatElapsedTimeSinceLastCheck {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  base::Time lastCompletedCheck = base::Time::FromSecondsSinceUnixEpoch(
      [defaults doubleForKey:kTimestampOfLastIssueFoundKey]);

  base::TimeDelta elapsedTime = base::Time::Now() - lastCompletedCheck;

  // If check found issues less than 1 minuete ago.
  if (elapsedTime < base::Minutes(1)) {
    return l10n_util::GetNSString(IDS_IOS_CHECK_FINISHED_JUST_NOW);
  }

  std::u16string timestamp = ui::TimeFormat::SimpleWithMonthAndYear(
      ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_LONG, elapsedTime,
      true);

  return l10n_util::GetNSStringF(
      IDS_IOS_SETTINGS_SAFETY_CHECK_ISSUES_FOUND_TIME, timestamp);
}

// Updates the consumer's cell corresponding to `item`.
- (void)reconfigureCellForItem:(TableViewItem*)item {
  CHECK(item);
  // Reconfiguration can change the height the cell needs for displaying its
  // content. Wrapping it around `performBatchTableViewUpdates` so the cell is
  // properly resized.
  __weak __typeof(self) weakSelf = self;
  [self.consumer
      performBatchTableViewUpdates:^{
        [weakSelf.consumer reconfigureCellsForItems:@[ item ]];
      }
                        completion:nil];
}

@end
