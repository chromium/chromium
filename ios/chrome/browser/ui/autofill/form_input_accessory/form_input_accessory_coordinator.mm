// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_coordinator.h"

#import <vector>

#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/ios/password_generation_provider.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/passwords/password_tab_helper.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_mediator.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/address_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/fallback_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_all_password_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/security_alert_commands.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_chromium_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FormInputAccessoryCoordinator () <
    AddressCoordinatorDelegate,
    CardCoordinatorDelegate,
    FormInputAccessoryMediatorHandler,
    ManualFillAccessoryViewControllerDelegate,
    PasswordCoordinatorDelegate,
    SecurityAlertCommands>

// Coordinator in charge of the presenting password autofill options as a modal.
@property(nonatomic, strong)
    ManualFillAllPasswordCoordinator* allPasswordCoordinator;

// The Mediator for the input accessory view controller.
@property(nonatomic, strong)
    FormInputAccessoryMediator* formInputAccessoryMediator;

// The View Controller for the input accessory view.
@property(nonatomic, strong)
    FormInputAccessoryViewController* formInputAccessoryViewController;

// The object in charge of interacting with the web view. Used to fill the data
// in the forms.
@property(nonatomic, strong) ManualFillInjectionHandler* injectionHandler;

// Reauthentication Module used for re-authentication.
@property(nonatomic, strong) ReauthenticationModule* reauthenticationModule;

// Modal alert.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// Active Form Input View Controller.
@property(nonatomic, strong) UIViewController* formInputViewController;

// The browser state. May return null after the coordinator has been stopped
// (thus the returned value must be checked for null).
@property(nonatomic, readonly) ChromeBrowserState* browserState;

@end

@implementation FormInputAccessoryCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:self
                             forProtocol:@protocol(SecurityAlertCommands)];
    __weak id<SecurityAlertCommands> securityAlertHandler =
        HandlerForProtocol(dispatcher, SecurityAlertCommands);
    _reauthenticationModule = [[ReauthenticationModule alloc] init];
    _injectionHandler = [[ManualFillInjectionHandler alloc]
          initWithWebStateList:browser->GetWebStateList()
          securityAlertHandler:securityAlertHandler
        reauthenticationModule:_reauthenticationModule];
  }
  return self;
}

- (void)start {
  self.formInputAccessoryViewController =
      [[FormInputAccessoryViewController alloc]
          initWithManualFillAccessoryViewControllerDelegate:self];

  DCHECK(self.browserState);
  auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
      self.browserState, ServiceAccessType::EXPLICIT_ACCESS);

  // There is no personal data manager in OTR (incognito). Get the original
  // one for manual fallback.
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(
          self.browserState->GetOriginalChromeBrowserState());

  __weak id<SecurityAlertCommands> securityAlertHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SecurityAlertCommands);
  self.formInputAccessoryMediator = [[FormInputAccessoryMediator alloc]
            initWithConsumer:self.formInputAccessoryViewController
                     handler:self
                webStateList:self.browser->GetWebStateList()
         personalDataManager:personalDataManager
               passwordStore:passwordStore
        securityAlertHandler:securityAlertHandler
      reauthenticationModule:self.reauthenticationModule];
  self.formInputAccessoryViewController.formSuggestionClient =
      self.formInputAccessoryMediator;
  self.formInputAccessoryViewController.brandingViewControllerDelegate =
      self.formInputAccessoryMediator;
}

- (void)stop {
  [self stopChildren];
  self.formInputAccessoryViewController = nil;
  self.formInputViewController = nil;
  [GetFirstResponder() reloadInputViews];

  [self.formInputAccessoryMediator disconnect];
  self.formInputAccessoryMediator = nil;

  [self.allPasswordCoordinator stop];
  self.allPasswordCoordinator = nil;
}

- (void)reset {
  [self stopChildren];

  [self.formInputAccessoryMediator enableSuggestions];
  [self.formInputAccessoryViewController reset];

  self.formInputViewController = nil;
  [GetFirstResponder() reloadInputViews];
}

#pragma mark - Presenting Children

- (void)stopChildren {
  for (ChromeCoordinator* coordinator in self.childCoordinators) {
    [coordinator stop];
  }
  [self.childCoordinators removeAllObjects];
}

- (void)startPasswordsFromButton:(UIButton*)button
          invokedOnPasswordField:(BOOL)invokedOnPasswordField {
  WebStateList* webStateList = self.browser->GetWebStateList();
  DCHECK(webStateList->GetActiveWebState());
  const GURL& URL = webStateList->GetActiveWebState()->GetLastCommittedURL();
  ManualFillPasswordCoordinator* passwordCoordinator =
      [[ManualFillPasswordCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                             browser:self.browser
                                 URL:URL
                    injectionHandler:self.injectionHandler
              invokedOnPasswordField:invokedOnPasswordField];
  passwordCoordinator.delegate = self;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    [passwordCoordinator presentFromButton:button];
  } else {
    self.formInputViewController = passwordCoordinator.viewController;
    [GetFirstResponder() reloadInputViews];
  }

  [self.childCoordinators addObject:passwordCoordinator];
}

- (void)startCardsFromButton:(UIButton*)button {
  CardCoordinator* cardCoordinator = [[CardCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                injectionHandler:self.injectionHandler];
  cardCoordinator.delegate = self;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    [cardCoordinator presentFromButton:button];
  } else {
    self.formInputViewController = cardCoordinator.viewController;
    [GetFirstResponder() reloadInputViews];
  }

  [self.childCoordinators addObject:cardCoordinator];
}

- (void)startAddressFromButton:(UIButton*)button {
  AddressCoordinator* addressCoordinator = [[AddressCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                injectionHandler:self.injectionHandler];
  addressCoordinator.delegate = self;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    [addressCoordinator presentFromButton:button];
  } else {
    self.formInputViewController = addressCoordinator.viewController;
    [GetFirstResponder() reloadInputViews];
  }

  [self.childCoordinators addObject:addressCoordinator];
}

#pragma mark - FormInputAccessoryMediatorHandler

- (void)resetFormInputView {
  [self reset];
}

#pragma mark - ManualFillAccessoryViewControllerDelegate

- (void)keyboardButtonPressed {
  [self reset];
}

- (void)accountButtonPressed:(UIButton*)sender {
  [self stopChildren];
  [self startAddressFromButton:sender];
  [self.formInputAccessoryViewController lockManualFallbackView];
  [self.formInputAccessoryMediator disableSuggestions];
}

- (void)cardButtonPressed:(UIButton*)sender {
  [self stopChildren];
  [self startCardsFromButton:sender];
  [self.formInputAccessoryViewController lockManualFallbackView];
  [self.formInputAccessoryMediator disableSuggestions];
}

- (void)passwordButtonPressed:(UIButton*)sender {
  [self stopChildren];
  BOOL invokedOnPasswordField =
      [self.formInputAccessoryMediator lastFocusedFieldWasPassword];
  [self startPasswordsFromButton:sender
          invokedOnPasswordField:invokedOnPasswordField];
  [self.formInputAccessoryViewController lockManualFallbackView];
  [self.formInputAccessoryMediator disableSuggestions];
}

#pragma mark - FallbackCoordinatorDelegate

- (void)fallbackCoordinatorDidDismissPopover:
    (FallbackCoordinator*)fallbackCoordinator {
  [self reset];
}

#pragma mark - PasswordCoordinatorDelegate

- (void)openPasswordSettings {
  [self reset];
  [self.navigator openPasswordSettings];
  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kPasswordsAccessorySheet);
}

- (void)openAllPasswordsPicker {
  [self reset];
  [self showConfirmationDialogToUseOtherPassword];
}

- (void)openPasswordSuggestion {
  [self reset];
  if (![self.injectionHandler canUserInjectInPasswordField:YES
                                             requiresHTTPS:NO]) {
    return;
  }
  web::WebState* active_web_state =
      self.browser->GetWebStateList()->GetActiveWebState();
  DCHECK(active_web_state);
  id<PasswordGenerationProvider> generationProvider =
      PasswordTabHelper::FromWebState(active_web_state)
          ->GetPasswordGenerationProvider();
  [generationProvider triggerPasswordGeneration];
}

#pragma mark - CardCoordinatorDelegate

- (void)openCardSettings {
  [self reset];
  [self.navigator openCreditCardSettings];
}

- (void)openAddCreditCard {
  [self reset];
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<BrowserCoordinatorCommands> handler =
      HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
  [handler showAddCreditCard];
}

#pragma mark - AddressCoordinatorDelegate

- (void)openAddressSettings {
  [self reset];
  [self.navigator openAddressSettings];
}

#pragma mark - SecurityAlertCommands

- (void)presentSecurityWarningAlertWithText:(NSString*)body {
  [self stopChildren];
  NSString* alertTitle =
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NOT_SECURE_TITLE);
  NSString* defaultActionTitle =
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NOT_SECURE_OK_BUTTON);

  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:alertTitle
                                          message:body
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* defaultAction =
      [UIAlertAction actionWithTitle:defaultActionTitle
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* action){
                             }];
  [alert addAction:defaultAction];
  UIViewController* presenter = self.baseViewController;
  while (presenter.presentedViewController) {
    presenter = presenter.presentedViewController;
  }
  [presenter presentViewController:alert animated:YES completion:nil];
}

- (void)showSetPasscodeDialog {
  [self stopChildren];
  UIAlertController* alertController = [UIAlertController
      alertControllerWithTitle:l10n_util::GetNSString(
                                   IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_TITLE)
                       message:l10n_util::GetNSString(
                                   IDS_IOS_AUTOFILL_SET_UP_SCREENLOCK_CONTENT)
                preferredStyle:UIAlertControllerStyleAlert];

  __weak id<ApplicationCommands> applicationCommandsHandler =
      HandlerForProtocol(self.browser->GetCommandDispatcher(),
                         ApplicationCommands);
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:GURL(kPasscodeArticleURL)];

  UIAlertAction* learnAction = [UIAlertAction
      actionWithTitle:l10n_util::GetNSString(
                          IDS_IOS_SETTINGS_SET_UP_SCREENLOCK_LEARN_HOW)
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction*) {
                [applicationCommandsHandler openURLInNewTab:command];
              }];
  [alertController addAction:learnAction];
  UIAlertAction* okAction =
      [UIAlertAction actionWithTitle:l10n_util::GetNSString(IDS_OK)
                               style:UIAlertActionStyleDefault
                             handler:nil];
  [alertController addAction:okAction];
  alertController.preferredAction = okAction;

  [self.baseViewController presentViewController:alertController
                                        animated:YES
                                      completion:nil];
}

#pragma mark - CRWResponderInputView

- (UIView*)inputView {
  BOOL isIPad = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
  return isIPad ? nil : self.formInputViewController.view;
}

- (UIView*)inputAccessoryView {
  if (self.formInputAccessoryMediator.inputAccessoryViewActive) {
    return self.formInputAccessoryViewController.view;
  }
  return nil;
}

#pragma mark - Private

- (ChromeBrowserState*)browserState {
  return self.browser ? self.browser->GetBrowserState() : nullptr;
}

// Shows confirmation dialog before opening Other passwords.
- (void)showConfirmationDialogToUseOtherPassword {
  WebStateList* webStateList = self.browser->GetWebStateList();
  const GURL& URL = webStateList->GetActiveWebState()->GetLastCommittedURL();
  std::u16string origin = base::ASCIIToUTF16(
      password_manager::GetShownOrigin(url::Origin::Create(URL)));

  bool useUpdatedStrings = base::FeatureList::IsEnabled(
      password_manager::features::kIOSPasswordUISplit);

  NSString* title = l10n_util::GetNSString(
      useUpdatedStrings ? IDS_IOS_MANUAL_FALLBACK_SELECT_PASSWORD_DIALOG_TITLE
                        : IDS_IOS_CONFIRM_USING_OTHER_PASSWORD_TITLE);
  NSString* message = l10n_util::GetNSStringF(
      useUpdatedStrings ? IDS_IOS_MANUAL_FALLBACK_SELECT_PASSWORD_DIALOG_MESSAGE
                        : IDS_IOS_CONFIRM_USING_OTHER_PASSWORD_DESCRIPTION,
      origin);

  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:title
                         message:message];

  __weak __typeof__(self) weakSelf = self;

  [self.alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                                   action:nil
                                    style:UIAlertActionStyleCancel];

  NSString* actionTitle =
      l10n_util::GetNSString(IDS_IOS_CONFIRM_USING_OTHER_PASSWORD_CONTINUE);
  [self.alertCoordinator addItemWithTitle:actionTitle
                                   action:^{
                                     [weakSelf showAllPasswords];
                                   }
                                    style:UIAlertActionStyleDefault];

  [self.alertCoordinator start];
}

// Opens other passwords.
- (void)showAllPasswords {
  [self reset];
  self.allPasswordCoordinator = [[ManualFillAllPasswordCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                injectionHandler:self.injectionHandler];
  [self.allPasswordCoordinator start];
}

@end
