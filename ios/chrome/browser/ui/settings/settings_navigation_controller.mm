// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/ios/ios_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/password_check_referrer.h"
#import "components/strings/grit/components_strings.h"
#import "components/sync/service/sync_service.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_util.h"
#import "ios/chrome/browser/keyboard/ui_bundled/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/quick_delete_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/sync/model/enterprise_utils.h"
#import "ios/chrome/browser/sync/model/sync_service_factory.h"
#import "ios/chrome/browser/tabs/model/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_edit_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_edit_coordinator.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_coordinator.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/features.h"
#import "ios/chrome/browser/ui/settings/content_settings/content_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/default_browser/default_browser_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_accounts/accounts_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_constants.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/notifications/notifications_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/passwords_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_coordinator.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_coordinator.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller_constants.h"
#import "ios/chrome/browser/ui/settings/settings_root_view_controlling.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/tabs/inactive_tabs/inactive_tabs_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/utils/password_utils.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {

// Helper function to configure handlers for child view controllers.

void ConfigureHandlers(id<SettingsRootViewControlling> controller,
                       CommandDispatcher* dispatcher) {
  controller.applicationHandler =
      HandlerForProtocol(dispatcher, ApplicationCommands);
  controller.browserHandler = HandlerForProtocol(dispatcher, BrowserCommands);
  controller.settingsHandler = HandlerForProtocol(dispatcher, SettingsCommands);
  controller.snackbarHandler = HandlerForProtocol(dispatcher, SnackbarCommands);
}

}  // namespace

NSString* const kSettingsDoneButtonId = @"kSettingsDoneButtonId";

@interface SettingsNavigationController () <
    AutofillProfileEditCoordinatorDelegate,
    ClearBrowsingDataCoordinatorDelegate,
    GoogleServicesSettingsCoordinatorDelegate,
    ManageSyncSettingsCoordinatorDelegate,
    NotificationsCoordinatorDelegate,
    PasswordDetailsCoordinatorDelegate,
    PasswordsCoordinatorDelegate,
    PrivacyCoordinatorDelegate,
    PrivacySafeBrowsingCoordinatorDelegate,
    SafetyCheckCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate,
    UINavigationControllerDelegate>

// Google services settings coordinator.
@property(nonatomic, strong)
    GoogleServicesSettingsCoordinator* googleServicesSettingsCoordinator;

// Privacy settings coordinator.
@property(nonatomic, strong) PrivacyCoordinator* privacySettingsCoordinator;

// Sync settings coordinator.
@property(nonatomic, strong)
    ManageSyncSettingsCoordinator* manageSyncSettingsCoordinator;

// Saved passwords settings coordinator.
@property(nonatomic, strong) PasswordsCoordinator* savedPasswordsCoordinator;

// Password details coordinator.
@property(nonatomic, strong)
    PasswordDetailsCoordinator* passwordDetailsCoordinator;

// Autofill profile edit coordinator.
@property(nonatomic, strong)
    AutofillProfileEditCoordinator* autofillProfileEditCoordinator;

// TODO(crbug.com/335387869): Delete this coordinator when Quick Delete is fully
// launched. The coordinator for the clear browsing data screen.
@property(nonatomic, strong)
    ClearBrowsingDataCoordinator* clearBrowsingDataCoordinator;

// Safety Check coordinator.
@property(nonatomic, strong) SafetyCheckCoordinator* safetyCheckCoordinator;

// Privacy Safe Browsing coordinator.
@property(nonatomic, strong)
    PrivacySafeBrowsingCoordinator* privacySafeBrowsingCoordinator;

// Coordinator for the inactive tabs settings.
@property(nonatomic, strong)
    InactiveTabsSettingsCoordinator* inactiveTabsSettingsCoordinator;

// Coordinator for the Notifications settings.
@property(nonatomic, strong) NotificationsCoordinator* notificationsCoordinator;

// Accounts coordinator.
@property(nonatomic, strong) AccountsCoordinator* accountsCoordinator;

// Handler for Snackbar Commands.
@property(nonatomic, weak) id<SnackbarCommands> snackbarCommandsHandler;

// Current UIViewController being presented by this Navigation Controller.
// If nil it means the Navigation Controller is not presenting anything, or the
// VC being presented doesn't conform to
// UIAdaptivePresentationControllerDelegate.
@property(nonatomic, weak)
    UIViewController<UIAdaptivePresentationControllerDelegate>*
        currentPresentedViewController;

// The SettingsNavigationControllerDelegate for this NavigationController.
@property(nonatomic, weak) id<SettingsNavigationControllerDelegate>
    settingsNavigationDelegate;

// The Browser instance this controller is configured with.
@property(nonatomic, assign) Browser* browser;

@end

@implementation SettingsNavigationController

#pragma mark - SettingsNavigationController methods.

+ (instancetype)
    mainSettingsControllerForBrowser:(Browser*)browser
                            delegate:(id<SettingsNavigationControllerDelegate>)
                                         delegate
            hasDefaultBrowserBlueDot:(BOOL)hasDefaultBrowserBlueDot {
  SettingsTableViewController* controller = [[SettingsTableViewController alloc]
               initWithBrowser:browser
      hasDefaultBrowserBlueDot:hasDefaultBrowserBlueDot];
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:controller
                             browser:browser
                            delegate:delegate];
  [controller navigationItem].rightBarButtonItem =
      [navigationController doneButton];
  return navigationController;
}

+ (instancetype)
    accountsControllerForBrowser:(Browser*)browser
                        delegate:
                            (id<SettingsNavigationControllerDelegate>)delegate {
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:nil
                             browser:browser
                            delegate:delegate];
  navigationController.accountsCoordinator = [[AccountsCoordinator alloc]
      initWithBaseNavigationController:navigationController
                               browser:browser
             closeSettingsOnAddAccount:YES];
  navigationController.accountsCoordinator.showSignoutButton = YES;
  [navigationController.accountsCoordinator start];
  return navigationController;
}

+ (instancetype)
    googleServicesControllerForBrowser:(Browser*)browser
                              delegate:
                                  (id<SettingsNavigationControllerDelegate>)
                                      delegate {
  // GoogleServicesSettings uses a coordinator to be presented, therefore the
  // view controller is not accessible. Prefer creating a
  // `SettingsNavigationController` with a nil root view controller and then
  // use the coordinator to push the GoogleServicesSettings as the first
  // root view controller.
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:nil
                             browser:browser
                            delegate:delegate];
  [navigationController showGoogleServices];
  return navigationController;
}

+ (instancetype)
    syncSettingsControllerForBrowser:(Browser*)browser
                            delegate:(id<SettingsNavigationControllerDelegate>)
                                         delegate {
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:nil
                             browser:browser
                            delegate:delegate];
  [navigationController showSyncServices];
  return navigationController;
}

+ (instancetype)
    priceNotificationsControllerForBrowser:(Browser*)browser
                                  delegate:
                                      (id<SettingsNavigationControllerDelegate>)
                                          delegate {
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:nil
                             browser:browser
                            delegate:delegate];
  [navigationController showPriceNotificationsSettings];
  return navigationController;
}

// Creates a new SafetyCheckTableViewController and the chrome
// around it.
+ (instancetype)
    safetyCheckControllerForBrowser:(Browser*)browser
                           delegate:(id<SettingsNavigationControllerDelegate>)
                                        delegate
                           referrer:(password_manager::PasswordCheckReferrer)
                                        referrer {
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:nil
                             browser:browser
                            delegate:delegate];

  [navigationController showSafetyCheckAndStartSafetyCheck:referrer];

  return navigationController;
}

+ (instancetype)
    safeBrowsingControllerForBrowser:(Browser*)browser
                            delegate:(id<SettingsNavigationControllerDelegate>)
                                         delegate {
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:nil
                             browser:browser
                            delegate:delegate];

  [navigationController showSafeBrowsing];

  return navigationController;
}

+ (instancetype)
    syncPassphraseControllerForBrowser:(Browser*)browser
                              delegate:
                                  (id<SettingsNavigationControllerDelegate>)
                                      delegate {
  SyncEncryptionPassphraseTableViewController* controller =
      [[SyncEncryptionPassphraseTableViewController alloc]
          initWithBrowser:browser];
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:controller
                             browser:browser
                            delegate:delegate];
  [controller navigationItem].leftBarButtonItem =
      [navigationController cancelButton];
  return navigationController;
}

+ (instancetype)
    savePasswordsControllerForBrowser:(Browser*)browser
                             delegate:(id<SettingsNavigationControllerDelegate>)
                                          delegate
                     showCancelButton:(BOOL)showCancelButton {
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:nil
                             browser:browser
                            delegate:delegate];
  [navigationController showSavedPasswordsAndShowCancelButton:showCancelButton];

  return navigationController;
}

+ (instancetype)
    passwordManagerSearchControllerForBrowser:(Browser*)browser
                                     delegate:
                                         (id<SettingsNavigationControllerDelegate>)
                                             delegate {
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:nil
                             browser:browser
                            delegate:delegate];

  [navigationController showPasswordManagerSearchPage];

  return navigationController;
}

+ (instancetype)
    passwordDetailsControllerForBrowser:(Browser*)browser
                               delegate:
                                   (id<SettingsNavigationControllerDelegate>)
                                       delegate
                             credential:
                                 (password_manager::CredentialUIEntry)credential
                             inEditMode:(BOOL)editMode {
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:nil
                             browser:browser
                            delegate:delegate];
  [navigationController showPasswordDetailsForCredential:credential
                                              inEditMode:editMode];

  return navigationController;
}

+ (instancetype)
    userFeedbackControllerForBrowser:(Browser*)browser
                            delegate:(id<SettingsNavigationControllerDelegate>)
                                         delegate
                    userFeedbackData:(UserFeedbackData*)userFeedbackData {
  DCHECK(ios::provider::IsUserFeedbackSupported());
  id<ApplicationCommands> applicationHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), ApplicationCommands);
  UserFeedbackConfiguration* configuration =
      [[UserFeedbackConfiguration alloc] init];
  configuration.data = userFeedbackData;
  configuration.handler = applicationHandler;
  configuration.singleSignOnService =
      GetApplicationContext()->GetSingleSignOnService();

  UIViewController* controller =
      ios::provider::CreateUserFeedbackViewController(configuration);

  DCHECK(controller);
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:controller
                             browser:browser
                            delegate:delegate];

  // Fix for https://crbug.com/1042741 (hide the double header display).
  navigationController.navigationBarHidden = YES;

  // If the controller overrides overrideUserInterfaceStyle, respect that in the
  // SettingsNavigationController.
  navigationController.overrideUserInterfaceStyle =
      controller.overrideUserInterfaceStyle;
  return navigationController;
}

+ (instancetype)
    privacyControllerForBrowser:(Browser*)browser
                       delegate:
                           (id<SettingsNavigationControllerDelegate>)delegate {
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:nil
                             browser:browser
                            delegate:delegate];
  [navigationController showPrivacy];
  return navigationController;
}

+ (instancetype)
    addressDetailsControllerForBrowser:(Browser*)browser
                              delegate:
                                  (id<SettingsNavigationControllerDelegate>)
                                      delegate
                               address:(autofill::AutofillProfile)address
                            inEditMode:(BOOL)editMode
                 offerMigrateToAccount:(BOOL)offerMigrateToAccount {
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:nil
                             browser:browser
                            delegate:delegate];
  [navigationController showAddressDetails:std::move(address)
                                inEditMode:editMode
                     offerMigrateToAccount:offerMigrateToAccount];
  return navigationController;
}

+ (instancetype)
    autofillProfileControllerForBrowser:(Browser*)browser
                               delegate:
                                   (id<SettingsNavigationControllerDelegate>)
                                       delegate {
  AutofillProfileTableViewController* controller =
      [[AutofillProfileTableViewController alloc] initWithBrowser:browser];

  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:controller
                             browser:browser
                            delegate:delegate];

  // Make sure the cancel button is always present, as the Autofill screen
  // isn't just shown from Settings.
  [controller navigationItem].leftBarButtonItem =
      [navigationController cancelButton];
  return navigationController;
}

+ (instancetype)
    autofillCreditCardControllerForBrowser:(Browser*)browser
                                  delegate:
                                      (id<SettingsNavigationControllerDelegate>)
                                          delegate {
  AutofillCreditCardTableViewController* controller =
      [[AutofillCreditCardTableViewController alloc] initWithBrowser:browser];

  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:controller
                             browser:browser
                            delegate:delegate];

  // Make sure the cancel button is always present, as the Autofill screen
  // isn't just shown from Settings.
  [controller navigationItem].leftBarButtonItem =
      [navigationController cancelButton];
  return navigationController;
}

+ (instancetype)
    autofillCreditCardEditControllerForBrowser:(Browser*)browser
                                      delegate:
                                          (id<SettingsNavigationControllerDelegate>)
                                              delegate
                                    creditCard:(autofill::CreditCard)creditCard
                                    inEditMode:(BOOL)editMode {
  ProfileIOS* profile = browser->GetProfile()->GetOriginalProfile();
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForProfile(
          profile->GetOriginalProfile());

  AutofillCreditCardEditTableViewController* controller =
      [[AutofillCreditCardEditTableViewController alloc]
           initWithCreditCard:creditCard
          personalDataManager:personalDataManager];
  if (editMode) {
    // If `creditCard` needs to be edited from the Payments web page, then a
    // command to open the Payments URL in a new tab should be dispatched. A
    // SettingsNavigationController shouldn't be created in this case.
    CHECK(
        ![AutofillCreditCardUtil shouldEditCardFromPaymentsWebPage:creditCard]);
    [controller editButtonPressed];
  }

  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:controller
                             browser:browser
                            delegate:delegate];

  // Make sure the cancel button is always present, as the Autofill screen isn't
  // just shown from Settings.
  [controller navigationItem].leftBarButtonItem =
      [navigationController cancelButton];
  return navigationController;
}

+ (instancetype)
    defaultBrowserControllerForBrowser:(Browser*)browser
                              delegate:
                                  (id<SettingsNavigationControllerDelegate>)
                                      delegate
                          sourceForUMA:
                              (DefaultBrowserSettingsPageSource)source {
  DefaultBrowserSettingsTableViewController* controller =
      [[DefaultBrowserSettingsTableViewController alloc] init];
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:controller
                             browser:browser
                            delegate:delegate];
  [controller navigationItem].leftBarButtonItem =
      [navigationController cancelButton];
  controller.source = source;
  return navigationController;
}

+ (instancetype)
    clearBrowsingDataControllerForBrowser:(Browser*)browser
                                 delegate:
                                     (id<SettingsNavigationControllerDelegate>)
                                         delegate {
  CHECK(!IsIosQuickDeleteEnabled());
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:nil
                             browser:browser
                            delegate:delegate];
  navigationController.clearBrowsingDataCoordinator =
      [[ClearBrowsingDataCoordinator alloc]
          initWithBaseNavigationController:navigationController
                                   browser:browser];
  navigationController.clearBrowsingDataCoordinator.delegate =
      navigationController;
  [navigationController.clearBrowsingDataCoordinator start];
  return navigationController;
}

+ (instancetype)
    inactiveTabsControllerForBrowser:(Browser*)browser
                            delegate:(id<SettingsNavigationControllerDelegate>)
                                         delegate {
  CHECK(IsInactiveTabsAvailable());
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:nil
                             browser:browser
                            delegate:delegate];
  navigationController.inactiveTabsSettingsCoordinator =
      [[InactiveTabsSettingsCoordinator alloc]
          initWithBaseNavigationController:navigationController
                                   browser:browser];
  [navigationController.inactiveTabsSettingsCoordinator start];
  return navigationController;
}

+ (instancetype)
    contentSettingsControllerForBrowser:(Browser*)browser
                               delegate:
                                   (id<SettingsNavigationControllerDelegate>)
                                       delegate {
  ContentSettingsTableViewController* controller =
      [[ContentSettingsTableViewController alloc] initWithBrowser:browser];

  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:controller
                             browser:browser
                            delegate:delegate];

  // Make sure the cancel button is always present, as the Contents Settings
  // screen isn't just shown from Settings.
  [controller navigationItem].leftBarButtonItem =
      [navigationController cancelButton];
  return navigationController;
}

+ (instancetype)
    notificationsSettingsControllerForBrowser:(Browser*)browser
                                     delegate:
                                         (id<SettingsNavigationControllerDelegate>)
                                             delegate {
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:nil
                             browser:browser
                            delegate:delegate];
  [navigationController showNotificationsSettings];
  return navigationController;
}

#pragma mark - Lifecycle

- (instancetype)initWithRootViewController:(UIViewController*)rootViewController
                                   browser:(Browser*)browser
                                  delegate:
                                      (id<SettingsNavigationControllerDelegate>)
                                          delegate {
  CHECK(browser);
  CHECK(!browser->GetProfile()->IsOffTheRecord());
  self = [super initWithRootViewController:rootViewController];
  if (self) {
    _browser = browser;
    _settingsNavigationDelegate = delegate;

    // FIXME -- RTTI is bad.
    if ([rootViewController
            isKindOfClass:[SettingsRootTableViewController class]]) {
      SettingsRootTableViewController* settingsRootViewController =
          base::apple::ObjCCast<SettingsRootTableViewController>(
              rootViewController);
      // Set up handlers.
      ConfigureHandlers(settingsRootViewController,
                        _browser->GetCommandDispatcher());
    }

    self.modalPresentationStyle = UIModalPresentationFormSheet;
    // Set the presentationController delegate. This is used for swipe down to
    // dismiss. This needs to be set after the modalPresentationStyle.
    self.presentationController.delegate = self;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  self.view.backgroundColor = [UIColor colorNamed:kPrimaryBackgroundColor];
  self.toolbar.translucent = NO;
  self.navigationBar.barTintColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  self.toolbar.barTintColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

  self.navigationBar.prefersLargeTitles = YES;
  self.navigationBar.accessibilityIdentifier =
      password_manager::kSettingsNavigationBarAccessibilityID;

  // Set the NavigationController delegate.
  self.delegate = self;
}

#pragma mark - Public

- (UIBarButtonItem*)cancelButton {
  UIBarButtonItem* item = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(closeSettings)];
  return item;
}

- (UIBarButtonItem*)doneButton {
  if (self.presentingViewController == nil) {
    // This can be called while being dismissed. In that case, return nil. See
    // crbug.com/1346604.
    return nil;
  }

  UIBarButtonItem* item = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                           target:self
                           action:@selector(closeSettings)];
  item.accessibilityIdentifier = kSettingsDoneButtonId;
  return item;
}

- (void)cleanUpSettings {
  // Notify all controllers of a Settings dismissal.
  for (UIViewController* controller in [self viewControllers]) {
    if ([controller respondsToSelector:@selector(settingsWillBeDismissed)]) {
      [controller performSelector:@selector(settingsWillBeDismissed)];
    }
  }

  // GoogleServicesSettingsCoordinator and PasswordsCoordinator must be stopped
  // before dismissing the sync settings view.
  [self.accountsCoordinator stop];
  self.accountsCoordinator = nil;
  [self stopSyncSettingsCoordinator];
  [self stopGoogleServicesSettingsCoordinator];
  [self stopPasswordsCoordinator];
  [self stopSafetyCheckCoordinator];
  [self stopClearBrowsingDataCoordinator];
  [self stopPrivacySafeBrowsingCoordinator];
  [self stopPrivacySettingsCoordinator];
  [self stopInactiveTabSettingsCoordinator];
  [self stopPasswordDetailsCoordinator];
  [self stopAutofillProfileEditCoordinator];
  [self stopNotificationsCoordinator];

  // Reset the delegate to prevent any queued transitions from attempting to
  // close the settings.
  self.settingsNavigationDelegate = nil;
  self.snackbarCommandsHandler = nil;
  self.currentPresentedViewController = nil;
  self.presentationController.delegate = nil;
  _browser = nil;
}

- (void)closeSettings {
  for (UIViewController* controller in [self viewControllers]) {
    if ([controller conformsToProtocol:@protocol(SettingsControllerProtocol)]) {
      [controller performSelector:@selector(reportDismissalUserAction)];
    }
  }

  [self.settingsNavigationDelegate closeSettings];
}

- (void)popViewControllerOrCloseSettingsAnimated:(BOOL)animated {
  if (self.viewControllers.count > 1) {
    // Pop the top view controller to reveal the view controller underneath.
    [self popViewControllerAnimated:animated];
  } else {
    // If there is only one view controller in the navigation stack,
    // simply close settings.
    [self closeSettings];
  }
}

#pragma mark - Private

// Pushes a GoogleServicesSettingsViewController on this settings navigation
// controller. Does nothing id the top view controller is already of type
// `GoogleServicesSettingsViewController`.
- (void)showGoogleServices {
  if ([self.topViewController
          isKindOfClass:[GoogleServicesSettingsViewController class]]) {
    // The top view controller is already the Google services settings panel.
    // No need to open it.
    return;
  }
  self.googleServicesSettingsCoordinator =
      [[GoogleServicesSettingsCoordinator alloc]
          initWithBaseNavigationController:self
                                   browser:self.browser];
  self.googleServicesSettingsCoordinator.delegate = self;
  [self.googleServicesSettingsCoordinator start];
}

- (void)showPrivacy {
  self.privacySettingsCoordinator = [[PrivacyCoordinator alloc]
      initWithBaseNavigationController:self
                               browser:self.browser];
  self.privacySettingsCoordinator.delegate = self;
  [self.privacySettingsCoordinator start];
}

- (void)showSyncServices {
  if ([self.topViewController
          isKindOfClass:[ManageSyncSettingsCoordinator class]]) {
    // The top view controller is already the Sync settings panel.
    // No need to open it.
    return;
  }
  DCHECK(!self.manageSyncSettingsCoordinator);
  // TODO(crbug.com/40066949): Remove usage of HasSyncConsent() after kSync
  // users migrated to kSignin in phase 3. See ConsentLevel::kSync
  // documentation for details.
  SyncSettingsAccountState accountState =
      SyncServiceFactory::GetForProfile(self.browser->GetProfile())
              ->HasSyncConsent()
          ? SyncSettingsAccountState::kSyncing
          : SyncSettingsAccountState::kSignedIn;
  self.manageSyncSettingsCoordinator = [[ManageSyncSettingsCoordinator alloc]
      initWithBaseNavigationController:self
                               browser:self.browser
                          accountState:accountState];
  self.manageSyncSettingsCoordinator.delegate = self;
  [self.manageSyncSettingsCoordinator start];
}

// Shows the Safety Check page and starts the Safety Check for `referrer`.
- (void)showSafetyCheckAndStartSafetyCheck:
    (password_manager::PasswordCheckReferrer)referrer {
  if ([self.topViewController isKindOfClass:[SafetyCheckCoordinator class]] ||
      [self.safetyCheckCoordinator.baseViewController isBeingDismissed]) {
    // Do not open the Safety Check panel if:
    // [1] The top view controller is already the Safety Check panel, or
    // [2] The Safety Check view controller is currently being dismissed.
    return;
  }
  DCHECK(!self.safetyCheckCoordinator);
  self.safetyCheckCoordinator = [[SafetyCheckCoordinator alloc]
      initWithBaseNavigationController:self
                               browser:self.browser
                              referrer:referrer];
  self.safetyCheckCoordinator.delegate = self;
  [self.safetyCheckCoordinator start];
  [self.safetyCheckCoordinator startCheckIfNotRunning];
}

- (void)showSafeBrowsing {
  if ([self.topViewController
          isKindOfClass:[PrivacySafeBrowsingCoordinator class]]) {
    // The top view controller is already the Safe Browsing panel.
    // No need to open it.
    return;
  }
  DCHECK(!self.privacySafeBrowsingCoordinator);
  self.privacySafeBrowsingCoordinator = [[PrivacySafeBrowsingCoordinator alloc]
      initWithBaseNavigationController:self
                               browser:self.browser];
  self.privacySafeBrowsingCoordinator.delegate = self;
  [self.privacySafeBrowsingCoordinator start];
}

// Stops the underlying Google services settings coordinator if it exists.
- (void)stopGoogleServicesSettingsCoordinator {
  [self.googleServicesSettingsCoordinator stop];
  self.googleServicesSettingsCoordinator = nil;
}

// Stops the privacy settings coordinator if it exsists.
- (void)stopPrivacySettingsCoordinator {
  [self.privacySettingsCoordinator stop];
  self.privacySettingsCoordinator = nil;
}

// Stops the underlying Sync settings coordinator if it exists.
- (void)stopSyncSettingsCoordinator {
  [self.manageSyncSettingsCoordinator stop];
  self.manageSyncSettingsCoordinator = nil;
}

// Shows the saved passwords. If `showCancelButton` is true, adds a cancel
// button as the left navigation item.
- (void)showSavedPasswordsAndShowCancelButton:(BOOL)showCancelButton {
  self.savedPasswordsCoordinator = [[PasswordsCoordinator alloc]
      initWithBaseNavigationController:self
                               browser:self.browser];
  self.savedPasswordsCoordinator.delegate = self;
  [self.savedPasswordsCoordinator start];
  if (showCancelButton) {
    [self.savedPasswordsCoordinator.viewController navigationItem]
        .leftBarButtonItem = [self cancelButton];
  }
}

- (void)showPasswordManagerSearchPage {
  self.savedPasswordsCoordinator = [[PasswordsCoordinator alloc]
      initWithBaseNavigationController:self
                               browser:self.browser];
  self.savedPasswordsCoordinator.delegate = self;
  self.savedPasswordsCoordinator.openViewControllerForPasswordSearch = true;
  [self.savedPasswordsCoordinator start];
}

- (void)showPasswordDetailsForCredential:
            (password_manager::CredentialUIEntry)credential
                              inEditMode:(BOOL)editMode {
  // TODO(crbug.com/40067451): Switch back to DCHECK if the number of reports is
  // low.
  DUMP_WILL_BE_CHECK(!self.passwordDetailsCoordinator);
  self.passwordDetailsCoordinator = [[PasswordDetailsCoordinator alloc]
      initWithBaseNavigationController:self
                               browser:self.browser
                            credential:credential
                          reauthModule:password_manager::
                                           BuildReauthenticationModule()
                               context:DetailsContext::kOutsideSettings];
  self.passwordDetailsCoordinator.delegate = self;
  self.passwordDetailsCoordinator.openInEditMode = editMode;
  [self.passwordDetailsCoordinator start];
}

// Stops the underlying passwords coordinator if it exists.
- (void)stopPasswordsCoordinator {
  [self.savedPasswordsCoordinator stop];
  self.savedPasswordsCoordinator.delegate = nil;
  self.savedPasswordsCoordinator = nil;
}

// Stops the underlying clear browsing data coordinator if it exists.
- (void)stopClearBrowsingDataCoordinator {
  [self.clearBrowsingDataCoordinator stop];
  self.clearBrowsingDataCoordinator.delegate = nil;
  self.clearBrowsingDataCoordinator = nil;
}

// Stops the underlying inactive tabs settings coordinator if it exists.
- (void)stopInactiveTabSettingsCoordinator {
  [self.inactiveTabsSettingsCoordinator stop];
  self.inactiveTabsSettingsCoordinator = nil;
}

// Stops the underlying SafetyCheck coordinator if it exists.
- (void)stopSafetyCheckCoordinator {
  [self.safetyCheckCoordinator stop];
  self.safetyCheckCoordinator.delegate = nil;
  self.safetyCheckCoordinator = nil;
}

// Stops the underlying PrivacySafeBrowsing coordinator if it exists.
- (void)stopPrivacySafeBrowsingCoordinator {
  [self.privacySafeBrowsingCoordinator stop];
  self.privacySafeBrowsingCoordinator.delegate = nil;
  self.privacySafeBrowsingCoordinator = nil;
}

// Stops the underlying PasswordDetailsCoordinator.
- (void)stopPasswordDetailsCoordinator {
  [self.passwordDetailsCoordinator stop];
  self.passwordDetailsCoordinator.delegate = nil;
  self.passwordDetailsCoordinator = nil;
}

// Stops the underlying AutofillProfileEditCoordinator.
- (void)stopAutofillProfileEditCoordinator {
  [self.autofillProfileEditCoordinator stop];
  self.autofillProfileEditCoordinator.delegate = nil;
  self.autofillProfileEditCoordinator = nil;
}

// Stops the underlying NotificationsCoordinator.
- (void)stopNotificationsCoordinator {
  [self.notificationsCoordinator stop];
  self.notificationsCoordinator.delegate = nil;
  self.notificationsCoordinator = nil;
}

#pragma mark - GoogleServicesSettingsCoordinatorDelegate

- (void)googleServicesSettingsCoordinatorDidRemove:
    (GoogleServicesSettingsCoordinator*)coordinator {
  DCHECK_EQ(self.googleServicesSettingsCoordinator, coordinator);
  [self stopGoogleServicesSettingsCoordinator];
}

#pragma mark - PrivacyCoordinatorDelegate

- (void)privacyCoordinatorViewControllerWasRemoved:
    (PrivacyCoordinator*)coordinator {
  DCHECK_EQ(self.privacySettingsCoordinator, coordinator);
  [self stopPrivacySettingsCoordinator];
}

#pragma mark - ManageSyncSettingsCoordinatorDelegate

- (void)manageSyncSettingsCoordinatorWasRemoved:
    (ManageSyncSettingsCoordinator*)coordinator {
  DCHECK_EQ(self.manageSyncSettingsCoordinator, coordinator);
  [self stopSyncSettingsCoordinator];
}

- (NSString*)manageSyncSettingsCoordinatorTitle {
  return l10n_util::GetNSString(IDS_IOS_MANAGE_SYNC_SETTINGS_TITLE);
}

#pragma mark - PasswordsCoordinatorDelegate

- (void)passwordsCoordinatorDidRemove:(PasswordsCoordinator*)coordinator {
  DCHECK_EQ(self.savedPasswordsCoordinator, coordinator);
  [self stopPasswordsCoordinator];
}

#pragma mark - PasswordManagerReauthenticationDelegate

- (void)dismissPasswordManagerAfterFailedReauthentication {
  id<ApplicationCommands> applicationHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);

  [applicationHandler closeSettingsUI];
}

#pragma mark PasswordDetailsCoordinatorDelegate

- (void)passwordDetailsCoordinatorDidRemove:
    (PasswordDetailsCoordinator*)coordinator {
  DCHECK_EQ(self.passwordDetailsCoordinator, coordinator);
  [self stopPasswordDetailsCoordinator];
}

- (void)passwordDetailsCancelButtonWasTapped {
  [self closeSettings];
}

#pragma mark - AutofillProfileEditCoordinatorDelegate

- (void)autofillProfileEditCoordinatorTableViewControllerDidFinish:
    (AutofillProfileEditCoordinator*)coordinator {
  DCHECK_EQ(self.autofillProfileEditCoordinator, coordinator);
  [self stopAutofillProfileEditCoordinator];
}

#pragma mark - ClearBrowsingDataCoordinatorDelegate

- (void)clearBrowsingDataCoordinatorViewControllerWasRemoved:
    (ClearBrowsingDataCoordinator*)coordinator {
  DCHECK_EQ(self.clearBrowsingDataCoordinator, coordinator);
  [self stopClearBrowsingDataCoordinator];
}

#pragma mark - SafetyCheckCoordinatorDelegate

- (void)safetyCheckCoordinatorDidRemove:(SafetyCheckCoordinator*)coordinator {
  DCHECK_EQ(self.safetyCheckCoordinator, coordinator);
  [self stopSafetyCheckCoordinator];
}

#pragma mark - PrivacySafeBrowsingCoordinatorDelegate

- (void)privacySafeBrowsingCoordinatorDidRemove:
    (PrivacySafeBrowsingCoordinator*)coordinator {
  DCHECK_EQ(self.privacySafeBrowsingCoordinator, coordinator);
  [self stopPrivacySafeBrowsingCoordinator];
}

#pragma mark - NotificationsCoordinatorDelegate

- (void)notificationsCoordinatorDidRemove:
    (NotificationsCoordinator*)coordinator {
  DCHECK_EQ(self.notificationsCoordinator, coordinator);
  [self stopNotificationsCoordinator];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  if ([self.currentPresentedViewController
          respondsToSelector:@selector(presentationControllerShouldDismiss:)]) {
    return [self.currentPresentedViewController
        presentationControllerShouldDismiss:presentationController];
  }
  return NO;
}

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  if ([self.currentPresentedViewController
          respondsToSelector:@selector(presentationControllerDidDismiss:)]) {
    [self.currentPresentedViewController
        presentationControllerDidDismiss:presentationController];
  }
  // Call settingsWasDismissed to make sure any necessary cleanup is performed.
  [self.settingsNavigationDelegate settingsWasDismissed];
}

- (void)presentationControllerWillDismiss:
    (UIPresentationController*)presentationController {
  if ([self.currentPresentedViewController
          respondsToSelector:@selector(presentationControllerWillDismiss:)]) {
    [self.currentPresentedViewController
        presentationControllerWillDismiss:presentationController];
  }
}

#pragma mark - Accessibility

- (BOOL)accessibilityPerformEscape {
  UIViewController* poppedController = [self popViewControllerAnimated:YES];
  if (!poppedController)
    [self closeSettings];
  return YES;
}

#pragma mark - UINavigationController

// Ensures that the keyboard is always dismissed during a navigation transition.
- (BOOL)disablesAutomaticKeyboardDismissal {
  return NO;
}

#pragma mark - UINavigationControllerDelegate

- (void)navigationController:(UINavigationController*)navigationController
      willShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  if ([viewController isMemberOfClass:[SettingsTableViewController class]] &&
      ![self.currentPresentedViewController
          isMemberOfClass:[SettingsTableViewController class]] &&
      [self.currentPresentedViewController
          conformsToProtocol:@protocol(SettingsControllerProtocol)]) {
    // Navigated back to root SettingsController from leaf SettingsController.
    [self.currentPresentedViewController
        performSelector:@selector(reportBackUserAction)];
  }
  self.currentPresentedViewController = base::apple::ObjCCast<
      UIViewController<UIAdaptivePresentationControllerDelegate>>(
      viewController);
}

#pragma mark - UIResponder

// To always be able to register key commands via -keyCommands, the VC must be
// able to become first responder.
- (BOOL)canBecomeFirstResponder {
  return YES;
}

- (NSArray<UIKeyCommand*>*)keyCommands {
  if ([self presentedViewController]) {
    return nil;
  }

  return @[ UIKeyCommand.cr_close ];
}

- (void)keyCommand_close {
  base::RecordAction(base::UserMetricsAction("MobileKeyCommandClose"));
  [self closeSettings];
}

#pragma mark - SettingsCommands

// TODO(crbug.com/41352590) : Do not pass `baseViewController` through
// dispatcher.
- (void)showAccountsSettingsFromViewController:
            (UIViewController*)baseViewController
                          skipIfUINotAvailable:(BOOL)skipIfUINotAvailable {
  // This command should only be triggered by the settinsg, therefore there is
  // no issue for the UI to be available or not.
  CHECK(!skipIfUINotAvailable);
  AccountsCoordinator* accountsCoordinator =
      [[AccountsCoordinator alloc] initWithBaseNavigationController:self
                                                            browser:self.browser
                                          closeSettingsOnAddAccount:NO];
  [accountsCoordinator start];
  self.accountsCoordinator = accountsCoordinator;
}

// TODO(crbug.com/41352590) : Do not pass `baseViewController` through
// dispatcher.
- (void)showGoogleServicesSettingsFromViewController:
    (UIViewController*)baseViewController {
  [self showGoogleServices];
}

// TODO(crbug.com/41352590) : Do not pass `baseViewController` through
// dispatcher.
- (void)showSyncSettingsFromViewController:
    (UIViewController*)baseViewController {
  [self showSyncServices];
}

// TODO(crbug.com/41352590) : Do not pass `baseViewController` through
// dispatcher.
- (void)showSyncPassphraseSettingsFromViewController:
    (UIViewController*)baseViewController {
  SyncEncryptionPassphraseTableViewController* controller =
      [[SyncEncryptionPassphraseTableViewController alloc]
          initWithBrowser:self.browser];
  ConfigureHandlers(controller, _browser->GetCommandDispatcher());
  [self pushViewController:controller animated:YES];
}

// TODO(crbug.com/41352590) : Do not pass `baseViewController` through
// dispatcher.
- (void)showSavedPasswordsSettingsFromViewController:
            (UIViewController*)baseViewController
                                    showCancelButton:(BOOL)showCancelButton {
  [self showSavedPasswordsAndShowCancelButton:showCancelButton];
}

- (void)showAddressDetails:(const autofill::AutofillProfile)address
                inEditMode:(BOOL)editMode
     offerMigrateToAccount:(BOOL)offerMigrateToAccount {
  self.autofillProfileEditCoordinator = [[AutofillProfileEditCoordinator alloc]
      initWithBaseNavigationController:self
                               browser:self.browser
                               profile:std::move(address)
                migrateToAccountButton:offerMigrateToAccount];
  self.autofillProfileEditCoordinator.delegate = self;
  self.autofillProfileEditCoordinator.openInEditMode = editMode;
  [self.autofillProfileEditCoordinator start];
}

// TODO(crbug.com/41352590) : Do not pass `baseViewController` through
// dispatcher.
- (void)showProfileSettingsFromViewController:
    (UIViewController*)baseViewController {
  AutofillProfileTableViewController* controller =
      [[AutofillProfileTableViewController alloc] initWithBrowser:self.browser];
  ConfigureHandlers(controller, _browser->GetCommandDispatcher());
  [self pushViewController:controller animated:YES];
}

- (void)showCreditCardSettings {
  AutofillCreditCardTableViewController* controller =
      [[AutofillCreditCardTableViewController alloc]
          initWithBrowser:self.browser];
  ConfigureHandlers(controller, _browser->GetCommandDispatcher());
  [self pushViewController:controller animated:YES];
}

- (void)showCreditCardDetails:(autofill::CreditCard)creditCard
                   inEditMode:(BOOL)editMode {
  ProfileIOS* profile = self.browser->GetProfile()->GetOriginalProfile();
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForProfile(
          profile->GetOriginalProfile());
  AutofillCreditCardEditTableViewController* controller =
      [[AutofillCreditCardEditTableViewController alloc]
           initWithCreditCard:creditCard
          personalDataManager:personalDataManager];
  if (editMode) {
    // If `creditCard` needs to be edited from the Payments web page, then a
    // command to open the Payments URL in a new tab should be dispatched.
    CHECK(
        ![AutofillCreditCardUtil shouldEditCardFromPaymentsWebPage:creditCard]);
    [controller editButtonPressed];
  }
  ConfigureHandlers(controller, _browser->GetCommandDispatcher());
  [self pushViewController:controller animated:YES];
}

- (void)showDefaultBrowserSettingsFromViewController:
            (UIViewController*)baseViewController
                                        sourceForUMA:
                                            (DefaultBrowserSettingsPageSource)
                                                source {
  DefaultBrowserSettingsTableViewController* controller =
      [[DefaultBrowserSettingsTableViewController alloc] init];
  ConfigureHandlers(controller, _browser->GetCommandDispatcher());
  controller.source = source;
  [self pushViewController:controller animated:YES];
}

- (void)showClearBrowsingDataSettings {
  CHECK(!IsIosQuickDeleteEnabled());
  [self stopClearBrowsingDataCoordinator];

  self.clearBrowsingDataCoordinator = [[ClearBrowsingDataCoordinator alloc]
      initWithBaseNavigationController:self
                               browser:self.browser];
  self.clearBrowsingDataCoordinator.delegate = self;
  [self.clearBrowsingDataCoordinator start];
}

// Shows the Safety Check page and starts the Safety Check for `referrer`.
- (void)showAndStartSafetyCheckForReferrer:
    (password_manager::PasswordCheckReferrer)referrer {
  [self showSafetyCheckAndStartSafetyCheck:referrer];
}

- (void)showSafeBrowsingSettings {
  [self showSafeBrowsing];
}

- (void)showSafeBrowsingSettingsFromPromoInteraction {
  [self showSafeBrowsing];
  self.privacySafeBrowsingCoordinator.openedFromPromoInteraction = YES;
}

- (void)showPasswordSearchPage {
  [self showPasswordManagerSearchPage];
}

- (void)showContentsSettingsFromViewController:
    (UIViewController*)baseViewController {
  if ([self.topViewController
          isKindOfClass:[ContentSettingsTableViewController class]]) {
    // The top view controller is already the Contents Settings panel.
    // No need to open it.
    return;
  }
  ContentSettingsTableViewController* controller =
      [[ContentSettingsTableViewController alloc] initWithBrowser:self.browser];
  [self pushViewController:controller animated:YES];
}

- (void)showNotificationsSettings {
  [self stopNotificationsCoordinator];
  self.notificationsCoordinator = [[NotificationsCoordinator alloc]
      initWithBaseNavigationController:self
                               browser:_browser];
  self.notificationsCoordinator.delegate = self;
  [self.notificationsCoordinator start];
}

- (void)showPriceNotificationsSettings {
  [self stopNotificationsCoordinator];
  self.notificationsCoordinator = [[NotificationsCoordinator alloc]
      initWithBaseNavigationController:self
                               browser:_browser];
  self.notificationsCoordinator.delegate = self;
  [self.notificationsCoordinator start];
  [self.notificationsCoordinator showTrackingPrice];
}

@end
