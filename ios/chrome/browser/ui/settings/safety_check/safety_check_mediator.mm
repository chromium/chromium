// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/safety_check/safety_check_mediator.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/numerics/safe_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "base/version.h"
#import "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/prefs/pref_service.h"
#import "components/safe_browsing/core/common/features.h"
#import "components/safe_browsing/core/common/safe_browsing_prefs.h"
#import "components/safety_check/safety_check.h"
#import "components/version_info/version_info.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/omaha/omaha_service.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#import "ios/chrome/browser/passwords/password_check_observer_bridge.h"
#import "ios/chrome/browser/passwords/password_store_observer_bridge.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/ui/icons/symbols.h"
#import "ios/chrome/browser/ui/settings/cells/settings_check_item.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_constants.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_consumer.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_utils.h"
#import "ios/chrome/browser/ui/settings/utils/observable_boolean.h"
#import "ios/chrome/browser/ui/settings/utils/pref_backed_boolean.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_text_item.h"
#import "ios/chrome/browser/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/upgrade/upgrade_constants.h"
#import "ios/chrome/browser/upgrade/upgrade_recommended_details.h"
#import "ios/chrome/browser/upgrade/upgrade_utils.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/common/url_scheme_util.h"
#import "net/base/mac/url_conversions.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/l10n/time_format.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

namespace {

// The size of  leading symbol icons.
NSInteger kLeadingSymbolImagePointSize = 22;

// The size of trailing symbol icons.
NSInteger kTrailingSymbolImagePointSize = 18;

constexpr char kSafetyCheckMetricsUpdates[] =
    "Settings.SafetyCheck.UpdatesResult";
constexpr char kSafetyCheckMetricsPasswords[] =
    "Settings.SafetyCheck.PasswordsResult";
constexpr char kSafetyCheckMetricsSafeBrowsing[] =
    "Settings.SafetyCheck.SafeBrowsingResult";
constexpr char kSafetyCheckInteractions[] = "Settings.SafetyCheck.Interactions";

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
};

// The minimum time each of the three checks should show a running state. This
// is to prevent any check that finshes quicky from causing the UI to appear
// jittery. The numbers are all different so that no 2 tests finish at the same
// time if they all end up using their min delays.
constexpr double kUpdateRowMinDelay = 2.0;
constexpr double kPasswordRowMinDelay = 1.5;
constexpr double kSafeBrowsingRowMinDelay = 3.0;

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

// SettingsCheckItem used to display the state of the Safe Browsing check.
@property(nonatomic, strong) SettingsCheckItem* safeBrowsingCheckItem;

// Current state of the Safe Browsing check.
@property(nonatomic, assign)
    SafeBrowsingCheckRowStates safeBrowsingCheckRowState;

// Previous on load or finished check state of the Safe Browsing check.
@property(nonatomic, assign)
    SafeBrowsingCheckRowStates previousSafeBrowsingCheckRowState;

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

// Row button to start the safety check.
@property(nonatomic, strong) TableViewTextItem* checkStartItem;

// Current state of the start safety check row button.
@property(nonatomic, assign) CheckStartStates checkStartState;

// Preference value for Safe Browsing.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* safeBrowsingPreference;

// Preference value for Enhanced Safe Browsing.
@property(nonatomic, strong, readonly)
    PrefBackedBoolean* enhancedSafeBrowsingPreference;

// If the Safe Browsing preference is managed.
@property(nonatomic, assign) BOOL safeBrowsingPreferenceManaged;

// The service responsible for password check feature.
@property(nonatomic, assign) scoped_refptr<IOSChromePasswordCheckManager>
    passwordCheckManager;

// Current state of password check.
@property(nonatomic, assign) PasswordCheckState currentPasswordCheckState;

// If any checks in safety check are still running.
@property(nonatomic, assign, readonly) BOOL checksRemaining;

// Service used to check if user is signed in.
@property(nonatomic, assign) AuthenticationService* authService;

// Service to check if passwords are synced.
@property(nonatomic, assign) SyncSetupService* syncService;

// Service used to check user preference values.
@property(nonatomic, assign, readonly) PrefService* userPrefService;

// Whether or not a safety check just ran.
@property(nonatomic, assign) BOOL checkDidRun;

// When the check was started.
@property(nonatomic, assign) base::Time checkStartTime;

@end

@implementation SafetyCheckMediator

@synthesize passwordCheckManager = _passwordCheckManager;

- (instancetype)initWithUserPrefService:(PrefService*)userPrefService
                   passwordCheckManager:
                       (scoped_refptr<IOSChromePasswordCheckManager>)
                           passwordCheckManager
                            authService:(AuthenticationService*)authService
                            syncService:(SyncSetupService*)syncService {
  self = [super init];
  if (self) {
    DCHECK(userPrefService);
    DCHECK(passwordCheckManager);
    DCHECK(authService);
    DCHECK(syncService);

    _userPrefService = userPrefService;
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
        l10n_util::GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_PAGE_HEADER);

    _updateCheckRowState = UpdateCheckRowStateDefault;
    _updateCheckItem = [[SettingsCheckItem alloc] initWithType:UpdateItemType];
    _updateCheckItem.text =
        l10n_util::GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_TITLE);
    UIImage* updateCheckIcon =
        UseSymbols()
            ? DefaultSymbolTemplateWithPointSize(kInfoCircleSymbol,
                                                 kLeadingSymbolImagePointSize)
            : [[UIImage imageNamed:@"settings_info"]
                  imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _updateCheckItem.leadingIcon = updateCheckIcon;
    _updateCheckItem.leadingIconTintColor = [UIColor colorNamed:kGrey400Color];
    _updateCheckItem.enabled = YES;
    _updateCheckItem.indicatorHidden = YES;
    _updateCheckItem.infoButtonHidden = YES;
    _updateCheckItem.trailingImage = nil;

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

    UIImage* passwordCheckIcon = nil;
    if (UseSymbols()) {
      passwordCheckIcon = CustomSymbolTemplateWithPointSize(
          kPasswordSymbol, kLeadingSymbolImagePointSize);
    } else {
      NSString* imageName = base::FeatureList::IsEnabled(
                                password_manager::features::
                                    kIOSEnablePasswordManagerBrandingUpdate)
                                ? @"password_key"
                                : @"legacy_password_key";
      passwordCheckIcon = [[UIImage imageNamed:imageName]
          imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    }

    _passwordCheckItem.leadingIcon = passwordCheckIcon;
    _passwordCheckItem.leadingIconTintColor =
        [UIColor colorNamed:kGrey400Color];
    _passwordCheckItem.enabled = YES;
    _passwordCheckItem.indicatorHidden = YES;
    _passwordCheckItem.infoButtonHidden = YES;
    _passwordCheckItem.trailingImage = nil;

    // Show unsafe state if user already ran safety check and there are
    // compromised credentials.
    if (!_passwordCheckManager->GetUnmutedCompromisedCredentials().empty() &&
        PreviousSafetyCheckIssueFound()) {
      _passwordCheckRowState = PasswordCheckRowStateUnSafe;
    }

    _previousPasswordCheckRowState = _passwordCheckRowState;

    _safeBrowsingCheckRowState = SafeBrowsingCheckRowStateDefault;
    _previousSafeBrowsingCheckRowState = _safeBrowsingCheckRowState;
    _safeBrowsingCheckItem =
        [[SettingsCheckItem alloc] initWithType:SafeBrowsingItemType];
    _safeBrowsingCheckItem.text = l10n_util::GetNSString(
        IDS_IOS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_TITLE);
    UIImage* safeBrowsingCheckIcon =
        UseSymbols()
            ? CustomSymbolWithPointSize(kPrivacySymbol,
                                        kLeadingSymbolImagePointSize)
            : [[UIImage imageNamed:@"settings_safe_browsing"]
                  imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    _safeBrowsingCheckItem.leadingIcon = safeBrowsingCheckIcon;
    _safeBrowsingCheckItem.leadingIconTintColor =
        [UIColor colorNamed:kGrey400Color];
    _safeBrowsingCheckItem.enabled = YES;
    _safeBrowsingCheckItem.indicatorHidden = YES;
    _safeBrowsingCheckItem.infoButtonHidden = YES;
    _safeBrowsingCheckItem.trailingImage = nil;

    _checkStartState = CheckStartStateDefault;
    _checkStartItem =
        [[TableViewTextItem alloc] initWithType:CheckStartItemType];
    _checkStartItem.text = GetNSString(IDS_IOS_CHECK_PASSWORDS_NOW_BUTTON);
    _checkStartItem.textColor = [UIColor colorNamed:kBlueColor];
    _checkStartItem.accessibilityTraits |= UIAccessibilityTraitButton;
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

- (void)compromisedCredentialsDidChange {
  self.passwordCheckRowState =
      [self computePasswordCheckRowState:self.currentPasswordCheckState];
  // Push update to the display.
  [self reconfigurePasswordCheckItem];
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
          NSString* updateLocation = [[NSUserDefaults standardUserDefaults]
              stringForKey:kIOSChromeUpgradeURLKey];
          base::RecordAction(base::UserMetricsAction(
              "Settings.SafetyCheck.RelaunchAfterUpdates"));
          base::UmaHistogramEnumeration(
              kSafetyCheckInteractions,
              SafetyCheckInteractions::kUpdatesRelaunch);
          [self.handler showUpdateAtLocation:updateLocation];
          break;
        }
      }
      break;
    }
    case PasswordItemType: {
      switch (self.passwordCheckRowState) {
        case PasswordCheckRowStateDefault:   // No tap action.
        case PasswordCheckRowStateRunning:   // No tap action.
        case PasswordCheckRowStateSafe:      // No tap action.
        case PasswordCheckRowStateDisabled:  // i tap: Show error popover.
        case PasswordCheckRowStateError:     // i tap: Show error popover.
          break;
        case PasswordCheckRowStateUnSafe:  // Go to password issues page.
          base::RecordAction(
              base::UserMetricsAction("Settings.SafetyCheck.ManagePasswords"));
          base::UmaHistogramEnumeration(
              kSafetyCheckInteractions,
              SafetyCheckInteractions::kPasswordsManage);
          password_manager::LogPasswordCheckReferrer(
              password_manager::PasswordCheckReferrer::kSafetyCheck);
          [self.handler showPasswordIssuesPage];
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
      return self.passwordCheckRowState == PasswordCheckRowStateUnSafe;
    case CheckStartItemType:
      return YES;
    case SafeBrowsingItemType:
      return safe_browsing::GetSafeBrowsingState(*self.userPrefService) ==
                 safe_browsing::SafeBrowsingState::STANDARD_PROTECTION ||
             self.safeBrowsingCheckRowState == SafeBrowsingCheckRowStateUnsafe;
    case HeaderItem:
    case TimestampFooterItem:
      return NO;
  }
}

- (BOOL)isItemWithErrorInfo:(TableViewItem*)item {
  SafteyCheckItemType type = static_cast<SafteyCheckItemType>(item.type);
  return (type != CheckStartItemType);
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

  BOOL noCompromisedPasswords =
      self.passwordCheckManager->GetUnmutedCompromisedCredentials().empty();

  switch (self.currentPasswordCheckState) {
    case PasswordCheckState::kRunning:
      return PasswordCheckRowStateRunning;
    case PasswordCheckState::kNoPasswords:
      return PasswordCheckRowStateDefault;
    case PasswordCheckState::kSignedOut:
      base::UmaHistogramEnumeration(kSafetyCheckMetricsPasswords,
                                    safety_check::PasswordsStatus::kSignedOut);
      return noCompromisedPasswords ? PasswordCheckRowStateError
                                    : PasswordCheckRowStateUnSafe;
    case PasswordCheckState::kOffline:
      base::UmaHistogramEnumeration(kSafetyCheckMetricsPasswords,
                                    safety_check::PasswordsStatus::kOffline);
      return noCompromisedPasswords ? PasswordCheckRowStateError
                                    : PasswordCheckRowStateUnSafe;
    case PasswordCheckState::kQuotaLimit:
      base::UmaHistogramEnumeration(kSafetyCheckMetricsPasswords,
                                    safety_check::PasswordsStatus::kQuotaLimit);
      return noCompromisedPasswords ? PasswordCheckRowStateError
                                    : PasswordCheckRowStateUnSafe;
    case PasswordCheckState::kOther:
      base::UmaHistogramEnumeration(kSafetyCheckMetricsPasswords,
                                    safety_check::PasswordsStatus::kError);
      return noCompromisedPasswords ? PasswordCheckRowStateError
                                    : PasswordCheckRowStateUnSafe;
    case PasswordCheckState::kCanceled:
    case PasswordCheckState::kIdle: {
      if (!noCompromisedPasswords) {
        base::UmaHistogramEnumeration(
            kSafetyCheckMetricsPasswords,
            safety_check::PasswordsStatus::kCompromisedExist);
        return PasswordCheckRowStateUnSafe;
      } else if (self.currentPasswordCheckState == PasswordCheckState::kIdle) {
        // Safe state is only possible after the state transitioned from
        // kRunning to kIdle.
        if (wasRunning) {
          base::UmaHistogramEnumeration(kSafetyCheckMetricsPasswords,
                                        safety_check::PasswordsStatus::kSafe);
          return PasswordCheckRowStateSafe;
        } else {
          return PasswordCheckRowStateDefault;
        }
      }
      return PasswordCheckRowStateDefault;
    }
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
  if (!self.passwordCheckManager->GetUnmutedCompromisedCredentials().empty())
    return nil;

  NSString* message;
  GURL linkURL;

  switch (self.currentPasswordCheckState) {
    case PasswordCheckState::kRunning:
    case PasswordCheckState::kNoPasswords:
      message =
          l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECK_ERROR_NO_PASSWORDS);
      break;
    case PasswordCheckState::kCanceled:
    case PasswordCheckState::kIdle:
      return nil;
    case PasswordCheckState::kSignedOut:
      message = l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECK_ERROR_SIGNED_OUT);
      break;
    case PasswordCheckState::kOffline:
      message = l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECK_ERROR_OFFLINE);
      break;
    case PasswordCheckState::kQuotaLimit:
      if ([self canUseAccountPasswordCheckup]) {
        message = l10n_util::GetNSString(
            IDS_IOS_PASSWORD_CHECK_ERROR_QUOTA_LIMIT_VISIT_GOOGLE);
        linkURL = password_manager::GetPasswordCheckupURL(
            password_manager::PasswordCheckupReferrer::kPasswordCheck);
      } else {
        message =
            l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECK_ERROR_QUOTA_LIMIT);
      }
      break;
    case PasswordCheckState::kOther:
      message = l10n_util::GetNSString(IDS_IOS_PASSWORD_CHECK_ERROR_OTHER);
      break;
  }
  return [self attributedStringWithText:message link:linkURL];
}

// Computes whether user is capable to run password check in Google Account.
- (BOOL)canUseAccountPasswordCheckup {
  return self.syncService->CanSyncFeatureStart() &&
         !self.syncService->IsEncryptEverythingEnabled();
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
      self.passwordCheckManager->StartPasswordCheck();
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
      (self.passwordCheckRowState == PasswordCheckRowStateUnSafe);
  if (self.checkDidRun && issuesFound) {
    [self updateTimestampOfLastCheck];
    self.checkDidRun = NO;
  } else if (self.checkDidRun && !issuesFound) {
    // Clear the timestamp if the last check found no issues.
    [[NSUserDefaults standardUserDefaults]
        setDouble:base::Time().ToDoubleT()
           forKey:kTimestampOfLastIssueFoundKey];
    self.checkDidRun = NO;
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
  double secondsSinceStart =
      base::Time::Now().ToDoubleT() - self.checkStartTime.ToDoubleT();
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

  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

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

    // Valid results, update all NSUserDefaults.
    [defaults setValue:base::SysUTF8ToNSString(upgradeUrl.spec())
                forKey:kIOSChromeUpgradeURLKey];
    [defaults setValue:base::SysUTF8ToNSString(details.next_version)
                forKey:kIOSChromeNextVersionKey];

    // Treat the safety check finding the device out of date as if the update
    // infobar was just shown to not overshow the infobar to the user.
    [defaults setObject:[NSDate date] forKey:kLastInfobarDisplayTimeKey];
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
  self.updateCheckItem.enabled = YES;
  self.updateCheckItem.indicatorHidden = YES;
  self.updateCheckItem.infoButtonHidden = YES;
  self.updateCheckItem.trailingImage = nil;
  self.updateCheckItem.trailingImageTintColor = nil;
  self.updateCheckItem.accessoryType = UITableViewCellAccessoryNone;

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
      UIImage* safeIconImage =
          UseSymbols()
              ? DefaultSymbolTemplateWithPointSize(
                    kCheckmarkCircleFillSymbol, kTrailingSymbolImagePointSize)
              : [[UIImage imageNamed:@"settings_safe_state"]
                    imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      self.updateCheckItem.trailingImage = safeIconImage;
      self.updateCheckItem.trailingImageTintColor =
          [UIColor colorNamed:kGreenColor];
      self.updateCheckItem.detailText =
          GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_UP_TO_DATE_DESC);
      break;
    }
    case UpdateCheckRowStateOutOfDate: {
      UIImage* unSafeIconImage =
          UseSymbols()
              ? DefaultSymbolTemplateWithPointSize(
                    kWarningFillSymbol, kTrailingSymbolImagePointSize)
              : [[UIImage imageNamed:@"settings_unsafe_state"]
                    imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      self.updateCheckItem.trailingImage = unSafeIconImage;
      self.updateCheckItem.trailingImageTintColor =
          [UIColor colorNamed:kRedColor];
      self.updateCheckItem.detailText =
          GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_UPDATES_OUT_OF_DATE_DESC);
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

  [self.consumer reconfigureCellsForItems:@[ self.updateCheckItem ]];
}

// Reconfigures the display of the `passwordCheckItem` based on current state of
// `passwordCheckRowState`.
- (void)reconfigurePasswordCheckItem {
  // Reset state to prevent conflicts.
  self.passwordCheckItem.enabled = YES;
  self.passwordCheckItem.indicatorHidden = YES;
  self.passwordCheckItem.infoButtonHidden = YES;
  self.passwordCheckItem.trailingImage = nil;
  self.passwordCheckItem.trailingImageTintColor = nil;
  self.passwordCheckItem.accessoryType = UITableViewCellAccessoryNone;

  // On any item update, see if `checkStartItem` should be updated.
  [self resetsCheckStartItemIfNeeded];

  switch (self.passwordCheckRowState) {
    case PasswordCheckRowStateDefault: {
      self.passwordCheckItem.detailText =
          GetNSString(IDS_IOS_SETTINGS_SAFETY_CHECK_PASSWORDS_DESCRIPTION);
      break;
    }
    case PasswordCheckRowStateRunning: {
      self.passwordCheckItem.indicatorHidden = NO;
      break;
    }
    case PasswordCheckRowStateSafe: {
      DCHECK(self.passwordCheckManager->GetUnmutedCompromisedCredentials()
                 .empty());
      UIImage* safeIconImage =
          UseSymbols()
              ? DefaultSymbolTemplateWithPointSize(
                    kCheckmarkCircleFillSymbol, kTrailingSymbolImagePointSize)
              : [[UIImage imageNamed:@"settings_safe_state"]
                    imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      self.passwordCheckItem.detailText =
          base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
              IDS_IOS_CHECK_PASSWORDS_COMPROMISED_COUNT, 0));
      self.passwordCheckItem.trailingImage = safeIconImage;
      self.passwordCheckItem.trailingImageTintColor =
          [UIColor colorNamed:kGreenColor];
      break;
    }
    case PasswordCheckRowStateUnSafe: {
      self.passwordCheckItem.detailText =
          base::SysUTF16ToNSString(l10n_util::GetPluralStringFUTF16(
              IDS_IOS_CHECK_PASSWORDS_COMPROMISED_COUNT,
              self.passwordCheckManager->GetUnmutedCompromisedCredentials()
                  .size()));
      UIImage* unSafeIconImage =
          UseSymbols()
              ? DefaultSymbolTemplateWithPointSize(
                    kWarningFillSymbol, kTrailingSymbolImagePointSize)
              : [[UIImage imageNamed:@"settings_unsafe_state"]
                    imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      self.passwordCheckItem.trailingImage = unSafeIconImage;
      self.passwordCheckItem.trailingImageTintColor =
          [UIColor colorNamed:kRedColor];
      self.passwordCheckItem.accessoryType =
          UITableViewCellAccessoryDisclosureIndicator;
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

  [self.consumer reconfigureCellsForItems:@[ self.passwordCheckItem ]];
}

// Reconfigures the display of the `safeBrowsingCheckItem` based on current
// state of `safeBrowsingCheckRowState`.
- (void)reconfigureSafeBrowsingCheckItem {
  // Reset state to prevent conflicts.
  self.safeBrowsingCheckItem.enabled = YES;
  self.safeBrowsingCheckItem.indicatorHidden = YES;
  self.safeBrowsingCheckItem.infoButtonHidden = YES;
  self.safeBrowsingCheckItem.trailingImage = nil;
  self.safeBrowsingCheckItem.trailingImageTintColor = nil;
  self.safeBrowsingCheckItem.accessoryType = UITableViewCellAccessoryNone;

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
      UIImage* safeIconImage =
          UseSymbols()
              ? DefaultSymbolTemplateWithPointSize(
                    kCheckmarkCircleFillSymbol, kTrailingSymbolImagePointSize)
              : [[UIImage imageNamed:@"settings_safe_state"]
                    imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      self.safeBrowsingCheckItem.trailingImage = safeIconImage;
      self.safeBrowsingCheckItem.trailingImageTintColor =
          [UIColor colorNamed:kGreenColor];
      self.safeBrowsingCheckItem.detailText =
          [self safeBrowsingCheckItemDetailText];
      if (safe_browsing::GetSafeBrowsingState(*self.userPrefService) ==
          safe_browsing::SafeBrowsingState::STANDARD_PROTECTION) {
        self.safeBrowsingCheckItem.accessoryType =
            UITableViewCellAccessoryDisclosureIndicator;
      }
      break;
    }
    case SafeBrowsingCheckRowStateUnsafe: {
      UIImage* unSafeIconImage =
          UseSymbols()
              ? DefaultSymbolTemplateWithPointSize(
                    kWarningFillSymbol, kTrailingSymbolImagePointSize)
              : [[UIImage imageNamed:@"settings_unsafe_state"]
                    imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
      self.safeBrowsingCheckItem.trailingImage = unSafeIconImage;
      self.safeBrowsingCheckItem.trailingImageTintColor =
          [UIColor colorNamed:kRedColor];
      self.safeBrowsingCheckItem.accessoryType =
          UITableViewCellAccessoryDisclosureIndicator;
      self.safeBrowsingCheckItem.detailText = GetNSString(
          IDS_IOS_SETTINGS_SAFETY_CHECK_SAFE_BROWSING_DISABLED_DESC);
      break;
    }
  }

  [self.consumer reconfigureCellsForItems:@[ self.safeBrowsingCheckItem ]];
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
      NOTREACHED();
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
  [self.consumer reconfigureCellsForItems:@[ self.checkStartItem ]];
}

// Updates the timestamp of when safety check last found an issue.
- (void)updateTimestampOfLastCheck {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  [defaults setDouble:base::Time::Now().ToDoubleT()
               forKey:kTimestampOfLastIssueFoundKey];
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
  base::Time lastCompletedCheck = base::Time::FromDoubleT(
      [defaults doubleForKey:kTimestampOfLastIssueFoundKey]);

  base::TimeDelta elapsedTime = base::Time::Now() - lastCompletedCheck;

  std::u16string timestamp;
  // If check found issues less than 1 minuete ago.
  if (elapsedTime < base::Minutes(1)) {
    timestamp = l10n_util::GetStringUTF16(IDS_IOS_CHECK_FINISHED_JUST_NOW);
  } else {
    timestamp = ui::TimeFormat::SimpleWithMonthAndYear(
        ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_LONG,
        elapsedTime, true);
  }

  return l10n_util::GetNSStringF(
      IDS_IOS_SETTINGS_SAFETY_CHECK_ISSUES_FOUND_TIME, timestamp);
}

@end
