// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_coordinator.h"

#import <vector>

#import "base/bind.h"
#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/utf_string_conversions.h"
#import "base/threading/sequenced_task_runner_handle.h"
#import "base/time/time.h"
#import "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/core/common/autofill_features.h"
#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/feature_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/manage_passwords_referrer.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/common/password_manager_features.h"
#import "components/password_manager/ios/password_generation_provider.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/feature_engagement/tracker_factory.h"
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
#import "ios/chrome/browser/ui/bubble/bubble_features.h"
#import "ios/chrome/browser/ui/bubble/bubble_view_controller_presenter.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/commands/security_alert_commands.h"
#import "ios/chrome/browser/ui/main/layout_guide_util.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/util/util_swift.h"
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

namespace {
// Delay between the time the view is shown, and the time the suggestion label
// is highlighted.
constexpr base::TimeDelta kPasswordSuggestionHighlightDelay = base::Seconds(1);

// Delay between the time the suggestion label is highlighted, and the time the
// password suggestion tip is shown.
constexpr base::TimeDelta kPasswordSuggestionTipDelay = base::Seconds(0.5);

// Additional vertical offset for the IPH, so that it doesn't appear below the
// Autofill strip at the top of the keyboard.
const CGFloat kIPHVerticalOffset = -5;

// Returns BubbleViewType param from kBubbleRichIPH feature flag.
BubbleViewType BubbleTypeFromFeature() {
  std::string bubbleTypeName = base::GetFieldTrialParamValueByFeature(
      kBubbleRichIPH, kBubbleRichIPHParameterName);
  if (bubbleTypeName == kBubbleRichIPHParameterExplicitDismissal) {
    return BubbleViewTypeWithClose;
  } else if (bubbleTypeName == kBubbleRichIPHParameterRich) {
    return BubbleViewTypeRich;
  } else if (bubbleTypeName == kBubbleRichIPHParameterRichWithSnooze) {
    return BubbleViewTypeRichWithSnooze;
  } else {
    return BubbleViewTypeDefault;
  }
}
}  // namespace

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

// Bubble view controller presenter for password suggestion tip.
@property(nonatomic, strong) BubbleViewControllerPresenter* bubblePresenter;

// UI tap recognizer used to dismiss bubble presenter.
@property(nonatomic, strong)
    UITapGestureRecognizer* formInputAccessoryTapRecognizer;

// The layout guide installed in the base view controller on which to anchor the
// potential IPH bubble.
@property(nonatomic, strong) UILayoutGuide* layoutGuide;

// The browser state. May return null after the coordinator has been stopped
// (thus the returned value must be checked for null).
@property(nonatomic, readonly) ChromeBrowserState* browserState;

// The tracker for feature engagement. May return null after the coordinator has
// been stopped (thus the returned value must be checked for null).
@property(nonatomic, readonly)
    feature_engagement::Tracker* featureEngagementTracker;

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
    _formInputAccessoryTapRecognizer = [[UITapGestureRecognizer alloc]
        initWithTarget:self
                action:@selector(tapInsideRecognized:)];
    _formInputAccessoryTapRecognizer.cancelsTouchesInView = NO;
  }
  return self;
}

- (void)start {
  self.formInputAccessoryViewController =
      [[FormInputAccessoryViewController alloc]
          initWithManualFillAccessoryViewControllerDelegate:self];
  LayoutGuideCenter* layoutGuideCenter =
      LayoutGuideCenterForBrowser(self.browser);
  self.formInputAccessoryViewController.layoutGuideCenter = layoutGuideCenter;

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
  [self.formInputAccessoryViewController.view
      addGestureRecognizer:self.formInputAccessoryTapRecognizer];

  self.layoutGuide =
      [layoutGuideCenter makeLayoutGuideNamed:kAutofillFirstSuggestionGuide];
  [self.baseViewController.view addLayoutGuide:self.layoutGuide];
}

- (void)stop {
  [self stopChildren];
  [self.formInputAccessoryTapRecognizer.view
      removeGestureRecognizer:self.formInputAccessoryTapRecognizer];
  self.formInputAccessoryViewController = nil;
  self.formInputViewController = nil;
  [GetFirstResponder() reloadInputViews];

  [self.formInputAccessoryMediator disconnect];
  self.formInputAccessoryMediator = nil;

  [self.allPasswordCoordinator stop];
  self.allPasswordCoordinator = nil;

  [self.layoutGuide.owningView removeLayoutGuide:self.layoutGuide];
  self.layoutGuide = nil;
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

#pragma mark - Actions

- (void)tapInsideRecognized:(id)sender {
  [self.bubblePresenter dismissAnimated:YES];
  self.bubblePresenter = nil;
}

#pragma mark - FormInputAccessoryMediatorHandler

- (void)resetFormInputView {
  [self reset];
}

- (void)notifyPasswordSuggestionsShown {
  // The engagement tracker can change during testing (in feature engagement app
  // interface), therefore we retrive it here instead of storing it in the
  // mediator.
  feature_engagement::Tracker* tracker = self.featureEngagementTracker;
  if (tracker) {
    tracker->NotifyEvent(feature_engagement::events::kPasswordSuggestionsShown);
  }
}

- (void)notifyPasswordSuggestionSelected {
  // The engagement tracker can change during testing (in feature engagement app
  // interface), therefore we retrive it here instead of storing it in the
  // mediator.
  feature_engagement::Tracker* tracker = self.featureEngagementTracker;
  if (tracker) {
    tracker->NotifyEvent(
        feature_engagement::events::kPasswordSuggestionSelected);
  }
}

- (void)showPasswordSuggestionIPHIfNeeded {
  if (self.bubblePresenter) {
    // Already showing a bubble.
    return;
  }

  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf tryPresentingBubble];
        [weakSelf notifyPasswordSuggestionsShown];
      }),
      kPasswordSuggestionHighlightDelay);
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

- (feature_engagement::Tracker*)featureEngagementTracker {
  ChromeBrowserState* browserState = self.browserState;
  if (!browserState)
    return nullptr;
  feature_engagement::Tracker* tracker =
      feature_engagement::TrackerFactory::GetForBrowserState(browserState);
  DCHECK(tracker);
  return tracker;
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

// Returns a new bubble view controller presenter for password suggestion tip.
- (BubbleViewControllerPresenter*)newBubbleViewControllerPresenter {
  // Prepare the main arguments for the BubbleViewControllerPresenter
  // initializer.
  NSString* text = l10n_util::GetNSString(IDS_IOS_PASSWORD_SUGGESTIONS_TIP);
  NSString* title =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SUGGESTIONS_TIP_TITLE);
  UIImage* image = [UIImage imageNamed:@"password_suggestion_icon"];
  BubbleViewType bubbleType = BubbleTypeFromFeature();

  // Prepare the dismissal callback.
  __weak __typeof(self) weakSelf = self;
  ProceduralBlockWithSnoozeAction dismissalCallback =
      ^(feature_engagement::Tracker::SnoozeAction snoozeAction) {
        [weakSelf IPHDidDismissWithSnoozeAction:snoozeAction];
      };

  // Create the BubbleViewControllerPresenter.
  BubbleViewControllerPresenter* bubbleViewControllerPresenter =
      [[BubbleViewControllerPresenter alloc]
               initWithText:text
                      title:title
                      image:image
             arrowDirection:BubbleArrowDirectionDown
                  alignment:BubbleAlignmentLeading
                 bubbleType:bubbleType
          dismissalCallback:dismissalCallback];
  bubbleViewControllerPresenter.voiceOverAnnouncement =
      l10n_util::GetNSString(IDS_IOS_PASSWORD_SUGGESTIONS_TIP_VOICEOVER);
  return bubbleViewControllerPresenter;
}

- (void)IPHDidDismissWithSnoozeAction:
    (feature_engagement::Tracker::SnoozeAction)snoozeAction {
  feature_engagement::Tracker* tracker = self.featureEngagementTracker;
  if (tracker) {
    const base::Feature& feature =
        feature_engagement::kIPHPasswordSuggestionsFeature;
    tracker->DismissedWithSnooze(feature, snoozeAction);
  }
  self.bubblePresenter = nil;
}

// Checks if the bubble should be presented and acts on it.
- (void)tryPresentingBubble {
  BubbleViewControllerPresenter* bubblePresenter =
      [self newBubbleViewControllerPresenter];

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
  const base::Feature& feature =
      feature_engagement::kIPHPasswordSuggestionsFeature;
  if (!tracker || !tracker->ShouldTriggerHelpUI(feature)) {
    return;
  }

  // Show the highlight suggestion now.
  [self.formInputAccessoryViewController animateSuggestionLabel];

  // Present the bubble after the delay.
  self.bubblePresenter = bubblePresenter;
  __weak __typeof(self) weakSelf = self;
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, base::BindOnce(^{
        [weakSelf presentBubbleAtAnchorPoint:anchorPoint];
      }),
      kPasswordSuggestionTipDelay);
}

// Actually presents the bubble.
- (void)presentBubbleAtAnchorPoint:(CGPoint)anchorPoint {
  [self.bubblePresenter presentInViewController:self.baseViewController
                                           view:self.baseViewController.view
                                    anchorPoint:anchorPoint];
}

@end
