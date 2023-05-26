// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/symbols/chrome_icon.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/ui/keyboard/UIKeyCommand+Chrome.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_credit_card_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_profile_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_coordinator.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/default_browser/default_browser_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/accounts_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_view_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/import_data_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/passwords_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_safe_browsing_coordinator.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_coordinator.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/tabs/inactive_tabs/inactive_tabs_settings_coordinator.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_api.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using password_manager::features::IsPasswordCheckupEnabled;

NSString* const kSettingsDoneButtonId = @"kSettingsDoneButtonId";

@interface SettingsNavigationController () <
    ClearBrowsingDataCoordinatorDelegate,
    GoogleServicesSettingsCoordinatorDelegate,
    ManageSyncSettingsCoordinatorDelegate,
    PasswordDetailsCoordinatorDelegate,
    PasswordsCoordinatorDelegate,
    PrivacySafeBrowsingCoordinatorDelegate,
    SafetyCheckCoordinatorDelegate,
    UIAdaptivePresentationControllerDelegate,
    UINavigationControllerDelegate>

// Google services settings coordinator.
@property(nonatomic, strong)
    GoogleServicesSettingsCoordinator* googleServicesSettingsCoordinator;

// Sync settings coordinator.
@property(nonatomic, strong)
    ManageSyncSettingsCoordinator* manageSyncSettingsCoordinator;

// Saved passwords settings coordinator.
@property(nonatomic, strong) PasswordsCoordinator* savedPasswordsCoordinator;

// Password details coordinator.
@property(nonatomic, strong)
    PasswordDetailsCoordinator* passwordDetailsCoordinator;

@property(nonatomic, strong)
    ClearBrowsingDataCoordinator* clearBrowsingDataCoordinator;

// Safety Check coordinator.
@property(nonatomic, strong) SafetyCheckCoordinator* safetyCheckCoordinator;

// Privacy Safe Browsing coordinator.
@property(nonatomic, strong)
    PrivacySafeBrowsingCoordinator* privacySafeBrowsingCoordinator;

// Privacy Safe Browsing coordinator.
@property(nonatomic, strong)
    InactiveTabsSettingsCoordinator* inactiveTabsSettingsCoordinator;

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
                                         delegate {
  DCHECK(browser);
  SettingsTableViewController* controller = [[SettingsTableViewController alloc]
      initWithBrowser:browser
           dispatcher:[delegate handlerForSettings]];
  controller.applicationCommandsHandler =
      [delegate handlerForApplicationCommands];
  controller.snackbarCommandsHandler = [delegate handlerForSnackbarCommands];
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
  DCHECK(browser);
  AccountsTableViewController* controller =
      [[AccountsTableViewController alloc] initWithBrowser:browser
                                 closeSettingsOnAddAccount:YES];
  controller.applicationCommandsHandler =
      [delegate handlerForApplicationCommands];
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:controller
                             browser:browser
                            delegate:delegate];
  return navigationController;
}

+ (instancetype)
    googleServicesControllerForBrowser:(Browser*)browser
                              delegate:
                                  (id<SettingsNavigationControllerDelegate>)
                                      delegate {
  DCHECK(browser);
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
  DCHECK(browser);
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:nil
                             browser:browser
                            delegate:delegate];
  [navigationController showSyncServices];
  return navigationController;
}

+ (instancetype)
    safetyCheckControllerForBrowser:(Browser*)browser
                           delegate:(id<SettingsNavigationControllerDelegate>)
                                        delegate {
  DCHECK(browser);
  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:nil
                             browser:browser
                            delegate:delegate];

  [navigationController showSafetyCheckAndStartSafetyCheck];

  return navigationController;
}

+ (instancetype)
    safeBrowsingControllerForBrowser:(Browser*)browser
                            delegate:(id<SettingsNavigationControllerDelegate>)
                                         delegate {
  DCHECK(browser);
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
  DCHECK(browser);
  SyncEncryptionPassphraseTableViewController* controller =
      [[SyncEncryptionPassphraseTableViewController alloc]
          initWithBrowser:browser];
  controller.dispatcher = [delegate handlerForSettings];
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
  DCHECK(browser);

  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:nil
                             browser:browser
                            delegate:delegate];
  if (IsPasswordCheckupEnabled()) {
    [navigationController
        showSavedPasswordsAndShowCancelButton:showCancelButton];
  } else {
    [navigationController
        showSavedPasswordsAndStartPasswordCheck:YES
                               showCancelButton:showCancelButton];
  }

  return navigationController;
}

+ (instancetype)
    passwordDetailsControllerForBrowser:(Browser*)browser
                               delegate:
                                   (id<SettingsNavigationControllerDelegate>)
                                       delegate
                             credential:
                                 (password_manager::CredentialUIEntry)credential
                       showCancelButton:(BOOL)showCancelButton {
  CHECK(browser);

  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:nil
                             browser:browser
                            delegate:delegate];
  [navigationController showPasswordDetailsForCredential:credential
                                        showCancelButton:showCancelButton];

  return navigationController;
}

+ (instancetype)
    userFeedbackControllerForBrowser:(Browser*)browser
                            delegate:(id<SettingsNavigationControllerDelegate>)
                                         delegate
                    userFeedbackData:(UserFeedbackData*)userFeedbackData
                             handler:(id<ApplicationCommands>)handler {
  DCHECK(browser);
  DCHECK(ios::provider::IsUserFeedbackSupported());

  UserFeedbackConfiguration* configuration =
      [[UserFeedbackConfiguration alloc] init];
  configuration.data = userFeedbackData;
  configuration.handler = handler;
  configuration.singleSignOnService = GetApplicationContext()->GetSSOService();

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
    importDataControllerForBrowser:(Browser*)browser
                          delegate:
                              (id<SettingsNavigationControllerDelegate>)delegate
                importDataDelegate:
                    (id<ImportDataControllerDelegate>)importDataDelegate
                         fromEmail:(NSString*)fromEmail
                           toEmail:(NSString*)toEmail {
  UIViewController* controller =
      [[ImportDataTableViewController alloc] initWithDelegate:importDataDelegate
                                                    fromEmail:fromEmail
                                                      toEmail:toEmail];

  SettingsNavigationController* navigationController =
      [[SettingsNavigationController alloc]
          initWithRootViewController:controller
                             browser:browser
                            delegate:delegate];

  // Make sure the cancel button is always present, as the Save Passwords screen
  // isn't just shown from Settings.
  [controller navigationItem].leftBarButtonItem =
      [navigationController cancelButton];
  return navigationController;
}

+ (instancetype)
    autofillProfileControllerForBrowser:(Browser*)browser
                               delegate:
                                   (id<SettingsNavigationControllerDelegate>)
                                       delegate {
  DCHECK(browser);
  AutofillProfileTableViewController* controller =
      [[AutofillProfileTableViewController alloc] initWithBrowser:browser];
  controller.dispatcher = [delegate handlerForSettings];

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
  DCHECK(browser);
  AutofillCreditCardTableViewController* controller =
      [[AutofillCreditCardTableViewController alloc] initWithBrowser:browser];
  controller.dispatcher = [delegate handlerForSettings];

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
    defaultBrowserControllerForBrowser:(Browser*)browser
                              delegate:
                                  (id<SettingsNavigationControllerDelegate>)
                                      delegate {
  DCHECK(browser);
  DefaultBrowserSettingsTableViewController* controller =
      [[DefaultBrowserSettingsTableViewController alloc] init];
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
    clearBrowsingDataControllerForBrowser:(Browser*)browser
                                 delegate:
                                     (id<SettingsNavigationControllerDelegate>)
                                         delegate {
  DCHECK(browser);
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
  CHECK(browser);
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

#pragma mark - Lifecycle

- (instancetype)initWithRootViewController:(UIViewController*)rootViewController
                                   browser:(Browser*)browser
                                  delegate:
                                      (id<SettingsNavigationControllerDelegate>)
                                          delegate {
  DCHECK(browser);
  DCHECK(!browser->GetBrowserState()->IsOffTheRecord());
  self = [super initWithRootViewController:rootViewController];
  if (self) {
    _browser = browser;
    _settingsNavigationDelegate = delegate;
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

  // Hardcode navigation bar style for iOS 14 and under to workaround bug that
  // navigation bar height not adjusting consistently across subviews. Should be
  // removed once iOS 14 is deprecated.
  if (!base::ios::IsRunningOnIOS15OrLater()) {
    UINavigationBarAppearance* appearance =
        [[UINavigationBarAppearance alloc] init];
    [appearance configureWithOpaqueBackground];
    self.navigationBar.standardAppearance = appearance;
    self.navigationBar.compactAppearance = appearance;
    self.navigationBar.scrollEdgeAppearance = appearance;
  }

  self.toolbar.translucent = NO;
  self.navigationBar.barTintColor =
      [UIColor colorNamed:kSecondaryBackgroundColor];
  self.toolbar.barTintColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];
  self.view.backgroundColor =
      [UIColor colorNamed:kGroupedPrimaryBackgroundColor];

  self.navigationBar.prefersLargeTitles = YES;
  self.navigationBar.accessibilityIdentifier = @"SettingNavigationBar";

  // Set the NavigationController delegate.
  self.delegate = self;
}

#pragma mark - Public

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
  [self stopSyncSettingsCoordinator];
  [self stopGoogleServicesSettingsCoordinator];
  [self stopPasswordsCoordinator];
  [self stopSafetyCheckCoordinator];
  [self stopClearBrowsingDataCoordinator];
  [self stopPrivacySafeBrowsingCoordinator];

  // Reset the delegate to prevent any queued transitions from attempting to
  // close the settings.
  self.settingsNavigationDelegate = nil;
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

// Creates an autoreleased "Cancel" button that cancels the settings when
// tapped.
- (UIBarButtonItem*)cancelButton {
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(closeSettings)];
  return cancelButton;
}

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

- (void)showSyncServices {
  if ([self.topViewController
          isKindOfClass:[ManageSyncSettingsCoordinator class]]) {
    // The top view controller is already the Sync settings panel.
    // No need to open it.
    return;
  }
  DCHECK(!self.manageSyncSettingsCoordinator);
  self.manageSyncSettingsCoordinator = [[ManageSyncSettingsCoordinator alloc]
      initWithBaseNavigationController:self
                               browser:self.browser];
  self.manageSyncSettingsCoordinator.delegate = self;
  [self.manageSyncSettingsCoordinator start];
}

- (void)showSafetyCheckAndStartSafetyCheck {
  if ([self.topViewController isKindOfClass:[SafetyCheckCoordinator class]]) {
    // The top view controller is already the Safety Check panel.
    // No need to open it.
    return;
  }
  DCHECK(!self.safetyCheckCoordinator);
  self.safetyCheckCoordinator = [[SafetyCheckCoordinator alloc]
      initWithBaseNavigationController:self
                               browser:self.browser];
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

// Stops the underlying Sync settings coordinator if it exists.
- (void)stopSyncSettingsCoordinator {
  [self.manageSyncSettingsCoordinator stop];
  self.manageSyncSettingsCoordinator = nil;
}

// Shows the saved passwords and starts the password check if
// `startPasswordCheck` is true. If `showCancelButton` is true, adds a cancel
// button as the left navigation item. Used when kIOSPasswordCheckup is
// disabled.
- (void)showSavedPasswordsAndStartPasswordCheck:(BOOL)startPasswordCheck
                               showCancelButton:(BOOL)showCancelButton {
  DCHECK(!IsPasswordCheckupEnabled());
  self.savedPasswordsCoordinator = [[PasswordsCoordinator alloc]
      initWithBaseNavigationController:self
                               browser:self.browser];
  self.savedPasswordsCoordinator.delegate = self;
  [self.savedPasswordsCoordinator start];
  if (startPasswordCheck) {
    [self.savedPasswordsCoordinator checkSavedPasswords];
  }
  if (showCancelButton) {
    [self.savedPasswordsCoordinator.viewController navigationItem]
        .leftBarButtonItem = [self cancelButton];
  }
}

// Shows the saved passwords. If `showCancelButton` is true, adds a cancel
// button as the left navigation item. Used when kIOSPasswordCheckup is enabled.
- (void)showSavedPasswordsAndShowCancelButton:(BOOL)showCancelButton {
  DCHECK(IsPasswordCheckupEnabled());
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

- (void)showPasswordDetailsForCredential:
            (password_manager::CredentialUIEntry)credential
                        showCancelButton:(BOOL)showCancelButton {
  CHECK(!self.passwordDetailsCoordinator);
  self.passwordDetailsCoordinator = [[PasswordDetailsCoordinator alloc]
      initWithBaseNavigationController:self
                               browser:self.browser
                            credential:credential
                          reauthModule:[[ReauthenticationModule alloc] init]
                               context:DetailsContext::kOutsideSettings];
  self.passwordDetailsCoordinator.delegate = self;
  self.passwordDetailsCoordinator.showCancelButton = showCancelButton;
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

#pragma mark - GoogleServicesSettingsCoordinatorDelegate

- (void)googleServicesSettingsCoordinatorDidRemove:
    (GoogleServicesSettingsCoordinator*)coordinator {
  DCHECK_EQ(self.googleServicesSettingsCoordinator, coordinator);
  [self stopGoogleServicesSettingsCoordinator];
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

#pragma mark PasswordDetailsCoordinatorDelegate

- (void)passwordDetailsCoordinatorDidRemove:
    (PasswordDetailsCoordinator*)coordinator {
  DCHECK_EQ(self.passwordDetailsCoordinator, coordinator);
  [self.passwordDetailsCoordinator stop];
  self.passwordDetailsCoordinator.delegate = nil;
  self.passwordDetailsCoordinator = nil;
}

- (void)passwordDetailsCancelButtonWasTapped {
  [self closeSettings];
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
  self.currentPresentedViewController = base::mac::ObjCCast<
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

#pragma mark - ApplicationSettingsCommands

// TODO(crbug.com/779791) : Do not pass `baseViewController` through dispatcher.
- (void)showAccountsSettingsFromViewController:
    (UIViewController*)baseViewController {
  AccountsTableViewController* controller =
      [[AccountsTableViewController alloc] initWithBrowser:self.browser
                                 closeSettingsOnAddAccount:NO];
  controller.applicationCommandsHandler =
      [self.settingsNavigationDelegate handlerForApplicationCommands];
  [self pushViewController:controller animated:YES];
}

// TODO(crbug.com/779791) : Do not pass `baseViewController` through dispatcher.
- (void)showGoogleServicesSettingsFromViewController:
    (UIViewController*)baseViewController {
  [self showGoogleServices];
}

// TODO(crbug.com/779791) : Do not pass `baseViewController` through dispatcher.
- (void)showSyncSettingsFromViewController:
    (UIViewController*)baseViewController {
  [self showSyncServices];
}

// TODO(crbug.com/779791) : Do not pass `baseViewController` through dispatcher.
- (void)showSyncPassphraseSettingsFromViewController:
    (UIViewController*)baseViewController {
  SyncEncryptionPassphraseTableViewController* controller =
      [[SyncEncryptionPassphraseTableViewController alloc]
          initWithBrowser:self.browser];
  controller.dispatcher = [self.settingsNavigationDelegate handlerForSettings];
  [self pushViewController:controller animated:YES];
}

// TODO(crbug.com/779791) : Do not pass `baseViewController` through dispatcher.
- (void)showSavedPasswordsSettingsFromViewController:
            (UIViewController*)baseViewController
                                    showCancelButton:(BOOL)showCancelButton
                                  startPasswordCheck:(BOOL)startPasswordCheck {
  if (IsPasswordCheckupEnabled()) {
    [self showSavedPasswordsAndShowCancelButton:showCancelButton];
  } else {
    [self showSavedPasswordsAndStartPasswordCheck:startPasswordCheck
                                 showCancelButton:showCancelButton];
  }
}

// TODO(crbug.com/779791) : Do not pass `baseViewController` through dispatcher.
- (void)showProfileSettingsFromViewController:
    (UIViewController*)baseViewController {
  AutofillProfileTableViewController* controller =
      [[AutofillProfileTableViewController alloc] initWithBrowser:self.browser];
  controller.dispatcher = [self.settingsNavigationDelegate handlerForSettings];
  [self pushViewController:controller animated:YES];
}

- (void)showCreditCardSettings {
  AutofillCreditCardTableViewController* controller =
      [[AutofillCreditCardTableViewController alloc]
          initWithBrowser:self.browser];
  controller.dispatcher = [self.settingsNavigationDelegate handlerForSettings];
  [self pushViewController:controller animated:YES];
}

- (void)showDefaultBrowserSettingsFromViewController:
            (UIViewController*)baseViewController
                                        sourceForUMA:
                                            (DefaultBrowserPromoSource)source {
  DefaultBrowserSettingsTableViewController* controller =
      [[DefaultBrowserSettingsTableViewController alloc] init];
  controller.dispatcher = [self.settingsNavigationDelegate handlerForSettings];
  controller.source = source;
  [self pushViewController:controller animated:YES];
}

- (void)showClearBrowsingDataSettings {
  self.clearBrowsingDataCoordinator = [[ClearBrowsingDataCoordinator alloc]
      initWithBaseNavigationController:self
                               browser:self.browser];
  self.clearBrowsingDataCoordinator.delegate = self;
  [self.clearBrowsingDataCoordinator start];
}

- (void)showSafetyCheckSettingsAndStartSafetyCheck {
  [self showSafetyCheckAndStartSafetyCheck];
}

- (void)showSafeBrowsingSettings {
  [self showSafeBrowsing];
}

@end
