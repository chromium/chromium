// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_coordinator.h"

#import <vector>

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/functional/bind.h"
#import "base/ios/ios_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/not_fatal_until.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/payments/payments_service_url.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/ios/password_generation_provider.h"
#import "components/plus_addresses/features.h"
#import "components/plus_addresses/grit/plus_addresses_strings.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/model/bottom_sheet/autofill_bottom_sheet_tab_helper.h"
#import "ios/chrome/browser/autofill/model/personal_data_manager_factory.h"
#import "ios/chrome/browser/autofill/ui_bundled/autofill_credit_card_util.h"
#import "ios/chrome/browser/autofill/ui_bundled/branding/branding_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_mediator.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_mediator_handler.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/form_input_accessory_view_controller_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/form_input_accessory/scoped_form_input_accessory_reauth_module_override.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/address_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/card_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/expanded_manual_fill_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/fallback_view_controller.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_all_password_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_all_password_coordinator_delegate.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_all_plus_address_coordinator.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_constants.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/autofill/ui_bundled/manual_fill/manual_fill_password_coordinator.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_constants.h"
#import "ios/chrome/browser/bubble/ui_bundled/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/password_tab_helper.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/security_alert_commands.h"
#import "ios/chrome/browser/shared/public/commands/settings_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// Delay between the time the view is shown, and the time the suggestion label
// is highlighted.
constexpr base::TimeDelta kAutofillSuggestionHighlightDelay = base::Seconds(1);

// Delay between the time the suggestion label is highlighted, and the time the
// autofill suggestion tip is shown.
constexpr base::TimeDelta kAutofillSuggestionTipDelay = base::Seconds(0.5);

// Additional vertical offset for the IPH, so that it doesn't appear below the
// Autofill strip at the top of the keyboard.
const CGFloat kIPHVerticalOffset = -5;

// Return the feature corresponding to the `feature_for_iph` enum.
const base::Feature* FetchIPHFeatureFromEnum(
    SuggestionFeatureForIPH feature_for_iph) {
  switch (feature_for_iph) {
    case SuggestionFeatureForIPH::kAutofillExternalAccountProfile:
      return &feature_engagement::
          kIPHAutofillExternalAccountProfileSuggestionFeature;
    case SuggestionFeatureForIPH::kPlusAddressCreation:
      return &feature_engagement::kIPHPlusAddressCreateSuggestionFeature;
    case SuggestionFeatureForIPH::kUnknown:
      NOTREACHED();
  }
}

}  // namespace

@interface FormInputAccessoryCoordinator () <
    AddressCoordinatorDelegate,
    CardCoordinatorDelegate,
    FormInputAccessoryMediatorHandler,
    FormInputAccessoryViewControllerDelegate,
    ManualFillAllPasswordCoordinatorDelegate,
    ManualFillAllPlusAddressCoordinatorDelegate,
    PasswordCoordinatorDelegate,
    ExpandedManualFillCoordinatorDelegate,
    SecurityAlertCommands>

// The object in charge of interacting with the web view. Used to fill the data
// in the forms.
@property(nonatomic, strong) ManualFillInjectionHandler* injectionHandler;

// Reauthentication Module used for re-authentication.
@property(nonatomic, strong) ReauthenticationModule* reauthenticationModule;

// Active Form Input View Controller.
@property(nonatomic, strong) UIViewController* formInputViewController;

// The profile. May return null after the coordinator has been stopped
// (thus the returned value must be checked for null).
@property(nonatomic, readonly) ProfileIOS* profile;

// Bubble view controller presenter for autofill suggestion tip.
@property(nonatomic, strong) BubbleViewControllerPresenter* bubblePresenter;

// UI tap recognizer used to dismiss bubble presenter.
@property(nonatomic, strong)
    UITapGestureRecognizer* formInputAccessoryTapRecognizer;

// The layout guide installed in the base view controller on which to anchor the
// potential IPH bubble.
@property(nonatomic, strong) UILayoutGuide* layoutGuide;

@end

@implementation FormInputAccessoryCoordinator {
  // Coordinator in charge of the presenting password autofill options as a
  // modal.
  ManualFillAllPasswordCoordinator* _allPasswordCoordinator;

  BrandingCoordinator* _brandingCoordinator;

  // The Mediator for the input accessory view controller.
  FormInputAccessoryMediator* _formInputAccessoryMediator;

  // The View Controller for the input accessory view.
  FormInputAccessoryViewController* _formInputAccessoryViewController;

  // Modal alert.
  AlertCoordinator* _alertCoordinator;

  // Coordinator in charge of presenting the view to show all plus addresses.
  ManualFillAllPlusAddressCoordinator* _allPlusAddressCoordinator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
    [dispatcher startDispatchingToTarget:self
                             forProtocol:@protocol(SecurityAlertCommands)];

    _brandingCoordinator =
        [[BrandingCoordinator alloc] initWithBaseViewController:viewController
                                                        browser:browser];
    _reauthenticationModule = [[ReauthenticationModule alloc] init];
  }
  return self;
}

- (void)start {
  [_brandingCoordinator start];
  _formInputAccessoryViewController = [[FormInputAccessoryViewController alloc]
      initWithFormInputAccessoryViewControllerDelegate:self];
  _formInputAccessoryViewController.brandingViewController =
      _brandingCoordinator.viewController;

  LayoutGuideCenter* layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  _formInputAccessoryViewController.layoutGuideCenter = layoutGuideCenter;

  DCHECK(self.profile);
  auto profilePasswordStore =
      IOSChromeProfilePasswordStoreFactory::GetForBrowserState(
          self.profile, ServiceAccessType::EXPLICIT_ACCESS);
  auto accountPasswordStore =
      IOSChromeAccountPasswordStoreFactory::GetForProfile(
          self.profile, ServiceAccessType::EXPLICIT_ACCESS);

  // There is no personal data manager in OTR (incognito). Get the original
  // one for manual fallback.
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForProfile(
          self.profile->GetOriginalProfile());

  __weak id<SecurityAlertCommands> securityAlertHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SecurityAlertCommands);
  _formInputAccessoryMediator = [[FormInputAccessoryMediator alloc]
            initWithConsumer:_formInputAccessoryViewController
                     handler:self
                webStateList:self.browser->GetWebStateList()
         personalDataManager:personalDataManager
        profilePasswordStore:profilePasswordStore
        accountPasswordStore:accountPasswordStore
        securityAlertHandler:securityAlertHandler
      reauthenticationModule:_reauthenticationModule
           engagementTracker:feature_engagement::TrackerFactory::GetForProfile(
                                 self.browser->GetProfile())];
  _formInputAccessoryViewController.formSuggestionClient =
      _formInputAccessoryMediator;

  self.layoutGuide =
      [layoutGuideCenter makeLayoutGuideNamed:kAutofillFirstSuggestionGuide];
  [self.baseViewController.view addLayoutGuide:self.layoutGuide];

  _injectionHandler = [[ManualFillInjectionHandler alloc]
        initWithWebStateList:self.browser->GetWebStateList()
        securityAlertHandler:securityAlertHandler
      reauthenticationModule:self.reauthenticationModule
        formSuggestionClient:_formInputAccessoryMediator];
}

- (void)stop {
  [self clearPresentedState];
  [self.formInputAccessoryTapRecognizer.view
      removeGestureRecognizer:self.formInputAccessoryTapRecognizer];
  _formInputAccessoryViewController = nil;
  _formInputViewController = nil;
  [GetFirstResponder() reloadInputViews];

  [_formInputAccessoryMediator disconnect];
  _formInputAccessoryMediator = nil;

  [_brandingCoordinator stop];
  _brandingCoordinator = nil;
  [self.layoutGuide.owningView removeLayoutGuide:self.layoutGuide];
  self.layoutGuide = nil;
}

- (void)reset {
  [self stopChildren];
  [self resetInputViews];
  [GetFirstResponder() reloadInputViews];
}

#pragma mark - Presenting Children

- (void)clearPresentedState {
  [self stopChildren];

  [self stopManualFillAllPasswordCoordinator];

  [self dismissAlertCoordinator];
}

- (void)stopChildren {
  _formInputAccessoryMediator.formInputInteractionDelegate = nil;
  for (ChromeCoordinator* coordinator in self.childCoordinators) {
    [coordinator stop];
  }
  [self.childCoordinators removeAllObjects];
}

// Starts the password coordinator and displays its view controller.
- (void)startPasswordsFromButton:(UIButton*)button
        invokedOnObfuscatedField:(BOOL)invokedOnObfuscatedField {
  web::WebState* activeWebState = [self activeWebState];
  if (!activeWebState) {
    return;
  }

  const GURL& URL = activeWebState->GetLastCommittedURL();

  ManualFillPasswordCoordinator* passwordCoordinator =
      [[ManualFillPasswordCoordinator alloc]
             initWithBaseViewController:self.baseViewController
                                browser:self.browser
          manualFillPlusAddressMediator:nil
                                    URL:URL
                       injectionHandler:self.injectionHandler
               invokedOnObfuscatedField:invokedOnObfuscatedField
                 showAutofillFormButton:NO];

  passwordCoordinator.delegate = self;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    [passwordCoordinator presentFromButton:button];
  } else {
    self.formInputViewController = passwordCoordinator.viewController;
    [GetFirstResponder() reloadInputViews];
  }

  [self.childCoordinators addObject:passwordCoordinator];
}

// Starts the card coordinator and displays its view controller.
- (void)startCardsFromButton:(UIButton*)button {
  CardCoordinator* cardCoordinator = [[CardCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                injectionHandler:self.injectionHandler
          reauthenticationModule:self.reauthenticationModule
          showAutofillFormButton:NO];
  cardCoordinator.delegate = self;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    [cardCoordinator presentFromButton:button];
  } else {
    self.formInputViewController = cardCoordinator.viewController;
    [GetFirstResponder() reloadInputViews];
  }

  [self.childCoordinators addObject:cardCoordinator];
}

// Starts the address coordinator and displays its view controller.
- (void)startAddressFromButton:(UIButton*)button {
  AddressCoordinator* addressCoordinator = [[AddressCoordinator alloc]
         initWithBaseViewController:self.baseViewController
                            browser:self.browser
      manualFillPlusAddressMediator:nil
                   injectionHandler:self.injectionHandler
             showAutofillFormButton:NO];
  addressCoordinator.delegate = self;
  if (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET) {
    [addressCoordinator presentFromButton:button];
  } else {
    self.formInputViewController = addressCoordinator.viewController;
    [GetFirstResponder() reloadInputViews];
  }

  [self.childCoordinators addObject:addressCoordinator];
}

// Starts the expanded manual fill coordinator and displays its view controller.
- (void)startManualFillForDataType:(manual_fill::ManualFillDataType)dataType
          invokedOnObfuscatedField:(BOOL)invokedOnObfuscatedField {
  manual_fill::ManualFillDataType focusedFieldDataType = [ManualFillUtil
      manualFillDataTypeFromFillingProduct:
          [_formInputAccessoryMediator currentProviderMainFillingProduct]];
  ExpandedManualFillCoordinator* expandedManualFillCoordinator =
      [[ExpandedManualFillCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                             browser:self.browser
                         forDataType:dataType
                focusedFieldDataType:focusedFieldDataType
              reauthenticationModule:self.reauthenticationModule];

  expandedManualFillCoordinator.injectionHandler = self.injectionHandler;
  expandedManualFillCoordinator.invokedOnObfuscatedField =
      invokedOnObfuscatedField;
  expandedManualFillCoordinator.delegate = self;
  _formInputAccessoryMediator.formInputInteractionDelegate =
      expandedManualFillCoordinator;
  [expandedManualFillCoordinator start];

  self.formInputViewController = expandedManualFillCoordinator.viewController;
  [GetFirstResponder() reloadInputViews];

  [self.childCoordinators addObject:expandedManualFillCoordinator];
}

#pragma mark - FormInputAccessoryMediatorHandler

- (void)resetFormInputView {
  [self reset];
}

- (void)notifyAutofillSuggestionWithIPHSelectedFor:
    (SuggestionFeatureForIPH)featureForIPH {
  // The engagement tracker can change during testing (in feature engagement app
  // interface), therefore we retrive it here instead of storing it in the
  // mediator.
  feature_engagement::Tracker* tracker = self.featureEngagementTracker;
  if (tracker) {
    switch (featureForIPH) {
      case SuggestionFeatureForIPH::kAutofillExternalAccountProfile:
        tracker->NotifyEvent(
            "autofill_external_account_profile_suggestion_accepted");
        break;
      case SuggestionFeatureForIPH::kPlusAddressCreation:
        tracker->NotifyEvent("plus_address_create_suggestion_feature_used");
        break;
      case SuggestionFeatureForIPH::kUnknown:
        NOTREACHED();
    }
  }
}

- (void)showAutofillSuggestionIPHIfNeededFor:
    (SuggestionFeatureForIPH)featureForIPH {
  if (self.bubblePresenter) {
    // Already showing a bubble.
    return;
  }

  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf tryPresentingBubbleFor:featureForIPH];
      }),
      kAutofillSuggestionHighlightDelay);
}

- (void)startManualFillForDataType:(manual_fill::ManualFillDataType)dataType {
  // Currently only payment methods form input accessory may start manual fill
  // directly.
  CHECK_EQ(dataType, manual_fill::ManualFillDataType::kPaymentMethod);
  [self startManualFillForDataType:dataType invokedOnObfuscatedField:NO];
}

#pragma mark - FormInputAccessoryViewControllerDelegate

- (void)formInputAccessoryViewController:
            (FormInputAccessoryViewController*)formInputAccessoryViewController
                  didPressKeyboardButton:(UIButton*)keyboardButton {
  [self reset];
}

- (void)formInputAccessoryViewController:
            (FormInputAccessoryViewController*)formInputAccessoryViewController
                   didPressAccountButton:(UIButton*)accountButton {
  [self stopChildren];
  [self startAddressFromButton:accountButton];
  [self updateKeyboardAccessoryForManualFilling];
}

- (void)formInputAccessoryViewController:
            (FormInputAccessoryViewController*)formInputAccessoryViewController
                didPressCreditCardButton:(UIButton*)creditCardButton {
  [self stopChildren];
  [self startCardsFromButton:creditCardButton];
  [self updateKeyboardAccessoryForManualFilling];
}

- (void)formInputAccessoryViewController:
            (FormInputAccessoryViewController*)formInputAccessoryViewController
                  didPressPasswordButton:(UIButton*)passwordButton {
  [self stopChildren];
  BOOL invokedOnObfuscatedField =
      [_formInputAccessoryMediator lastFocusedFieldWasObfuscated];
  [self startPasswordsFromButton:passwordButton
        invokedOnObfuscatedField:invokedOnObfuscatedField];
  [self updateKeyboardAccessoryForManualFilling];
}

- (void)formInputAccessoryViewController:
            (FormInputAccessoryViewController*)formInputAccessoryViewController
                didPressManualFillButton:(UIButton*)manualFillButton
                             forDataType:
                                 (manual_fill::ManualFillDataType)dataType {
  CHECK(IsKeyboardAccessoryUpgradeEnabled());

  [self stopChildren];
  BOOL invokedOnObfuscatedField =
      [_formInputAccessoryMediator lastFocusedFieldWasObfuscated];
  [self startManualFillForDataType:dataType
          invokedOnObfuscatedField:invokedOnObfuscatedField];

  // TODO(crbug.com/326265397): Hide the keyboard accessory and remove line
  // below.
  [self updateKeyboardAccessoryForManualFilling];
}

- (void)formInputAccessoryViewController:
            (FormInputAccessoryViewController*)formInputAccessoryViewController
            didTapFormInputAccessoryView:(UIView*)formInputAccessoryView {
  [self dismissBubble];
}

- (void)formInputAccessoryViewControllerReset:
    (FormInputAccessoryViewController*)formInputAccessoryViewController {
  CHECK_EQ(_formInputAccessoryViewController, formInputAccessoryViewController);
  [self resetInputViews];
}

#pragma mark - FallbackCoordinatorDelegate

- (void)fallbackCoordinatorDidDismissPopover:
    (FallbackCoordinator*)fallbackCoordinator {
  [self reset];
}

#pragma mark - PasswordCoordinatorDelegate

- (void)openPasswordManager {
  [self reset];
  [self.navigator openPasswordManager];

  UMA_HISTOGRAM_ENUMERATION(
      "PasswordManager.ManagePasswordsReferrer",
      password_manager::ManagePasswordsReferrer::kPasswordsAccessorySheet);
  base::RecordAction(
      base::UserMetricsAction("MobileKeyboardAccessoryOpenPasswordManager"));
}

- (void)openPasswordSettings {
  [self reset];
  [self.navigator openPasswordSettings];
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

  web::WebState* activeWebState = [self activeWebState];
  if (!activeWebState) {
    return;
  }

  id<PasswordGenerationProvider> generationProvider =
      PasswordTabHelper::FromWebState(activeWebState)
          ->GetPasswordGenerationProvider();
  [generationProvider triggerPasswordGeneration];
}

- (void)openPasswordDetailsInEditModeForCredential:
    (password_manager::CredentialUIEntry)credential {
  [self reset];
  [self dispatchCommandToEditPassword:credential];
}

#pragma mark - ManualFillAllPasswordCoordinatorDelegate

- (void)manualFillAllPasswordCoordinatorWantsToBeDismissed:
    (ManualFillAllPasswordCoordinator*)coordinator {
  [self stopManualFillAllPasswordCoordinator];
}

- (void)manualFillAllPasswordCoordinator:
            (ManualFillAllPasswordCoordinator*)coordinator
    didTriggerOpenPasswordDetailsInEditMode:
        (password_manager::CredentialUIEntry)credential {
  [self stopManualFillAllPasswordCoordinator];
  [self dispatchCommandToEditPassword:credential];
}

#pragma mark - ManualFillAllPlusAddressCoordinatorDelegate

- (void)manualFillAllPlusAddressCoordinatorWantsToBeDismissed:
    (ManualFillAllPlusAddressCoordinator*)coordinator {
  [self stopManualFillAllPlusAddressCoordinator];
}

- (void)dismissManualFillAllPlusAddressAndOpenManagePlusAddress {
  [self stopManualFillAllPlusAddressCoordinator];
  [self openManagePlusAddress];
}

#pragma mark - CardCoordinatorDelegate

- (void)cardCoordinatorDidTriggerOpenCardSettings:
    (CardCoordinator*)cardCoordinator {
  [self reset];
  [self.navigator openCreditCardSettings];
}

- (void)cardCoordinatorDidTriggerOpenAddCreditCard:
    (CardCoordinator*)cardCoordinator {
  [self reset];
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<BrowserCoordinatorCommands> handler =
      HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
  [handler showAddCreditCard];
}

- (void)cardCoordinator:(CardCoordinator*)cardCoordinator
    didTriggerOpenCardDetails:(autofill::CreditCard)card
                   inEditMode:(BOOL)editMode {
  [self reset];

  // Check if the card should be edited from the Payments web page.
  if (editMode &&
      [AutofillCreditCardUtil shouldEditCardFromPaymentsWebPage:card]) {
    GURL paymentsURL =
        autofill::payments::GetManageInstrumentUrl(card.instrument_id());
    OpenNewTabCommand* command =
        [OpenNewTabCommand commandWithURLFromChrome:paymentsURL];
    id<ApplicationCommands> applicationHandler = HandlerForProtocol(
        self.browser->GetCommandDispatcher(), ApplicationCommands);
    [applicationHandler openURLInNewTab:command];

    return;
  }

  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<SettingsCommands> settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  [settingsHandler showCreditCardDetails:card inEditMode:editMode];
}

#pragma mark - AddressCoordinatorDelegate

- (void)openAddressDetailsInEditMode:(autofill::AutofillProfile)address
               offerMigrateToAccount:(BOOL)offerMigrateToAccount {
  [self reset];
  CommandDispatcher* dispatcher = self.browser->GetCommandDispatcher();
  id<SettingsCommands> settingsHandler =
      HandlerForProtocol(dispatcher, SettingsCommands);
  [settingsHandler showAddressDetails:std::move(address)
                           inEditMode:YES
                offerMigrateToAccount:offerMigrateToAccount];
}

- (void)openAddressSettings {
  [self reset];
  [self.navigator openAddressSettings];
}

#pragma mark - PlusAddressCoordinatorDelegate

// Opens the create plus address bottom sheet.
- (void)openCreatePlusAddressSheet {
  [self reset];

  web::WebState* activeWebState = [self activeWebState];
  if (!activeWebState) {
    return;
  }

  __weak __typeof(self) weakSelf = self;
  auto callback = base::BindOnce(^(const std::string& plusAddress) {
    [weakSelf.injectionHandler
        userDidPickContent:base::SysUTF8ToNSString(plusAddress)
             passwordField:NO
             requiresHTTPS:NO];
  });

  AutofillBottomSheetTabHelper* tabHelper =
      AutofillBottomSheetTabHelper::FromWebState(activeWebState);
  tabHelper->ShowPlusAddressesBottomSheet(std::move(callback));
}

- (void)openAllPlusAddressesPicker {
  [self reset];

  [self stopManualFillAllPlusAddressCoordinator];

  _allPlusAddressCoordinator = [[ManualFillAllPlusAddressCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                injectionHandler:self.injectionHandler];
  _allPlusAddressCoordinator.manualFillAllPlusAddressCoordinatorDelegate = self;
  [_allPlusAddressCoordinator start];
}

- (void)openManagePlusAddress {
  OpenNewTabCommand* command = [OpenNewTabCommand
      commandWithURLFromChrome:
          GURL(plus_addresses::features::kPlusAddressManagementUrl.Get())];

  id<ApplicationCommands> applicationHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [applicationHandler openURLInNewTab:command];
}

#pragma mark - ExpandedManualFillCoordinatorDelegate

- (void)stopExpandedManualFillCoordinator:
    (ExpandedManualFillCoordinator*)coordinator {
  [self reset];
}

#pragma mark - SecurityAlertCommands

- (void)presentSecurityWarningAlertWithText:(NSString*)body {
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
                             handler:nil];
  [alert addAction:defaultAction];
  UIViewController* presenter = self.baseViewController;
  while (presenter.presentedViewController) {
    presenter = presenter.presentedViewController;
  }
  [presenter presentViewController:alert animated:YES completion:nil];
}

#pragma mark - CRWResponderInputView

- (UIView*)inputView {
  BOOL isIPad = ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET;
  return isIPad ? nil : self.formInputViewController.view;
}

- (UIView*)inputAccessoryView {
  if (_formInputAccessoryMediator.inputAccessoryViewActive) {
    return _formInputAccessoryViewController.view;
  }
  return nil;
}

#pragma mark - Actions

- (void)tapInsideRecognized:(id)sender {
  [self dismissBubble];
}

#pragma mark - Private

// Returns the reauthentication module, which can be an override for testing
// purposes.
- (ReauthenticationModule*)reauthenticationModule {
  id<ReauthenticationProtocol> overrideModule =
      ScopedFormInputAccessoryReauthModuleOverride::Get();
  return overrideModule ? overrideModule : _reauthenticationModule;
}

// Returns the active web state. May return nil.
- (web::WebState*)activeWebState {
  web::WebState* webState =
      self.browser->GetWebStateList()->GetActiveWebState();
  if (!webState) {
    // TODO: b/40940511 - The web state should not be nil, but we have seen
    // cases of it being nil in the wild, so, for now, we handle the nil case
    // gracefully, but still dump the information we need to find the root
    // cause. This can be removed once the root cause has been fixed.
    base::debug::DumpWithoutCrashing();
  }
  return webState;
}

- (void)stopManualFillAllPasswordCoordinator {
  [_allPasswordCoordinator stop];
  _allPasswordCoordinator.manualFillAllPasswordCoordinatorDelegate = nil;
  _allPasswordCoordinator = nil;
}

- (void)stopManualFillAllPlusAddressCoordinator {
  [_allPlusAddressCoordinator stop];
  _allPlusAddressCoordinator.manualFillAllPlusAddressCoordinatorDelegate = nil;
  _allPlusAddressCoordinator = nil;
}

- (void)dismissAlertCoordinator {
  [_alertCoordinator stop];
  _alertCoordinator = nil;
}

- (ProfileIOS*)profile {
  return self.browser ? self.browser->GetProfile() : nullptr;
}

- (feature_engagement::Tracker*)featureEngagementTracker {
  ProfileIOS* profile = self.profile;
  if (!profile) {
    return nullptr;
  }
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);
  CHECK(tracker);
  return tracker;
}

// Shows confirmation dialog before opening Other passwords.
- (void)showConfirmationDialogToUseOtherPassword {
  web::WebState* activeWebState = [self activeWebState];
  if (!activeWebState) {
    return;
  }

  const GURL& URL = activeWebState->GetLastCommittedURL();
  std::u16string origin = base::ASCIIToUTF16(
      password_manager::GetShownOrigin(url::Origin::Create(URL)));

  NSString* title = l10n_util::GetNSString(
      IDS_IOS_MANUAL_FALLBACK_SELECT_PASSWORD_DIALOG_TITLE);
  NSString* message = l10n_util::GetNSStringF(
      IDS_IOS_MANUAL_FALLBACK_SELECT_PASSWORD_DIALOG_MESSAGE, origin);

  _alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:title
                         message:message];
  [self.childCoordinators addObject:_alertCoordinator];

  __weak __typeof__(self) weakSelf = self;

  [_alertCoordinator addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                               action:^{
                                 [weakSelf dismissAlertCoordinator];
                               }
                                style:UIAlertActionStyleCancel];

  NSString* actionTitle =
      l10n_util::GetNSString(IDS_IOS_CONFIRM_USING_OTHER_PASSWORD_CONTINUE);
  [_alertCoordinator addItemWithTitle:actionTitle
                               action:^{
                                 [weakSelf showAllPasswords];
                                 [weakSelf dismissAlertCoordinator];
                               }
                                style:UIAlertActionStyleDefault];

  [_alertCoordinator start];
}

// Opens other passwords.
- (void)showAllPasswords {
  [self reset];
  // The old coordinator could still be alive at this point. Stop it and release
  // it before starting a new one. See crbug.com/40063966.
  [self stopManualFillAllPasswordCoordinator];

  _allPasswordCoordinator = [[ManualFillAllPasswordCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                injectionHandler:self.injectionHandler];
  _allPasswordCoordinator.manualFillAllPasswordCoordinatorDelegate = self;
  [_allPasswordCoordinator start];
}

// Returns a new bubble view controller presenter for password suggestion tip.
- (BubbleViewControllerPresenter*)newBubbleViewControllerPresenterFor:
    (SuggestionFeatureForIPH)featureForIPH {
  NSString* text = nil;
  NSString* voiceOverText = nil;
  switch (featureForIPH) {
    case SuggestionFeatureForIPH::kAutofillExternalAccountProfile:
      text = l10n_util::GetNSString(
          IDS_AUTOFILL_IPH_EXTERNAL_ACCOUNT_PROFILE_SUGGESTION);
      voiceOverText = l10n_util::GetNSString(
          IDS_AUTOFILL_IPH_EXTERNAL_ACCOUNT_PROFILE_SUGGESTION);
      break;
    case SuggestionFeatureForIPH::kPlusAddressCreation:
      text = l10n_util::GetNSString(IDS_PLUS_ADDRESS_CREATE_SUGGESTION_IPH_IOS);
      voiceOverText = l10n_util::GetNSString(
          IDS_PLUS_ADDRESS_CREATE_SUGGESTION_IPH_SCREENREADER_IOS);
      break;
    case SuggestionFeatureForIPH::kUnknown:
      NOTREACHED();
  }

  CHECK(text != nil);
  CHECK(voiceOverText != nil);

  // Prepare the main arguments for the BubbleViewControllerPresenter
  // initializer.
  // Prepare the dismissal callback.
  __weak __typeof(self) weakSelf = self;
  CallbackWithIPHDismissalReasonType dismissalCallback =
      ^(IPHDismissalReasonType IPHDismissalReasonType,
        feature_engagement::Tracker::SnoozeAction snoozeAction) {
        [weakSelf IPHDidDismissWithSnoozeAction:snoozeAction
                                     forFeature:featureForIPH];
      };

  // Create the BubbleViewControllerPresenter.
  BubbleViewControllerPresenter* bubbleViewControllerPresenter =
      [[BubbleViewControllerPresenter alloc]
               initWithText:text
                      title:nil
                      image:nil
             arrowDirection:BubbleArrowDirectionDown
                  alignment:BubbleAlignmentTopOrLeading
                 bubbleType:BubbleViewTypeWithClose
          dismissalCallback:dismissalCallback];
  bubbleViewControllerPresenter.voiceOverAnnouncement = voiceOverText;
  return bubbleViewControllerPresenter;
}

// Checks if the bubble should be presented and acts on it.
- (void)tryPresentingBubbleFor:(SuggestionFeatureForIPH)featureForIPH {
  BubbleViewControllerPresenter* bubblePresenter =
      [self newBubbleViewControllerPresenterFor:featureForIPH];

  // Get the anchor point for the bubble.
  CGRect anchorFrame = self.layoutGuide.layoutFrame;
  CGPoint anchorPoint =
      CGPointMake(CGRectGetMidX(anchorFrame),
                  CGRectGetMinY(anchorFrame) + kIPHVerticalOffset);

  // Discard if it doesn't fit in the view as it is currently shown.
  if (![bubblePresenter canPresentInView:self.baseViewController.view
                             anchorPoint:anchorPoint]) {
    return;
  }

  // Early return if the engagement tracker won't display the IPH.
  feature_engagement::Tracker* tracker = self.featureEngagementTracker;
  const base::Feature* feature = FetchIPHFeatureFromEnum(featureForIPH);
  if (!tracker || !tracker->ShouldTriggerHelpUI(*feature)) {
    return;
  }

  // Present the bubble after the delay.
  self.bubblePresenter = bubblePresenter;
  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf presentBubbleAtAnchorPoint:anchorPoint];
      }),
      kAutofillSuggestionTipDelay);
}

- (void)IPHDidDismissWithSnoozeAction:
            (feature_engagement::Tracker::SnoozeAction)snoozeAction
                           forFeature:(SuggestionFeatureForIPH)featureForIPH {
  feature_engagement::Tracker* tracker = self.featureEngagementTracker;
  if (tracker) {
    const base::Feature* feature = FetchIPHFeatureFromEnum(featureForIPH);
    tracker->DismissedWithSnooze(*feature, snoozeAction);
  }
  self.bubblePresenter = nil;
}

// Actually presents the bubble.
- (void)presentBubbleAtAnchorPoint:(CGPoint)anchorPoint {
  [self.bubblePresenter presentInViewController:self.baseViewController
                                    anchorPoint:anchorPoint];
}

- (void)dismissBubble {
  [self.bubblePresenter dismissAnimated:YES];
  self.bubblePresenter = nil;
}

// Resets `formInputAccessoryViewController` and `formInputViewController` to
// their initial state.
- (void)resetInputViews {
  _formInputAccessoryMediator.suggestionsEnabled = YES;
  [_formInputAccessoryViewController reset];

  _formInputViewController = nil;
}

// Updates the keyboard accessory to the state it should be in when a manual
// fill view is displayed.
- (void)updateKeyboardAccessoryForManualFilling {
  [_formInputAccessoryViewController lockManualFallbackView];
  _formInputAccessoryMediator.suggestionsEnabled = NO;
}

// Creates a SettingsCommend handler and uses it to dispatch a command to show
// the details of a password in edit mode.
- (void)dispatchCommandToEditPassword:
    (password_manager::CredentialUIEntry)credential {
  id<SettingsCommands> settingsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SettingsCommands);
  [settingsHandler showPasswordDetailsForCredential:credential inEditMode:YES];
}

@end
