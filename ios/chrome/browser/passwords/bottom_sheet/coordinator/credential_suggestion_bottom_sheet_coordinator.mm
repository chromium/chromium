// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/credential_suggestion_bottom_sheet_coordinator.h"

#import <optional>

#import "base/apple/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/not_fatal_until.h"
#import "components/autofill/ios/form_util/form_activity_params.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/first_run/public/best_features_item.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/credential_suggestion_bottom_sheet_mediator.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/passkey_suggestion_bottom_sheet_mediator.h"
#import "ios/chrome/browser/passwords/bottom_sheet/coordinator/password_suggestion_bottom_sheet_exit_reason.h"
#import "ios/chrome/browser/passwords/bottom_sheet/public/scoped_credential_suggestion_bottom_sheet_reauth_module_override.h"
#import "ios/chrome/browser/passwords/bottom_sheet/ui/credential_suggestion_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/password_controller_delegate.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"
#import "ios/chrome/browser/welcome_back/model/features.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

using PasswordSuggestionBottomSheetExitReason::kCouldNotPresent;
using PasswordSuggestionBottomSheetExitReason::kDismissal;
using PasswordSuggestionBottomSheetExitReason::kShowPasswordDetails;
using PasswordSuggestionBottomSheetExitReason::kShowPasswordManager;
using PasswordSuggestionBottomSheetExitReason::kUsePasswordSuggestion;

@implementation CredentialSuggestionBottomSheetCoordinator {
  // The password controller delegate used to open the password manager.
  __weak id<PasswordControllerDelegate> _passwordControllerDelegate;

  // Currently in the process of dismissing the bottom sheet.
  bool _dismissing;

  // Module handling reauthentication before accessing sensitive data.
  id<ReauthenticationProtocol> _reauthModule;

  // This mediator is used to fetch data related to the bottom sheet.
  CredentialSuggestionBottomSheetMediatorBase* _mediator;

  // This view controller is used to display the bottom sheet.
  CredentialSuggestionBottomSheetViewController* _viewController;

  // The navigation controller containing the Suggestion.
  UINavigationController* _navigationController;

  // Form activity parameters giving the context around the sheet trigger.
  std::optional<autofill::FormActivityParams> _params;

  // Information of the passkey request.
  std::optional<webauthn::IOSPasskeyClient::RequestInfo> _requestInfo;
}

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                        params:(const autofill::FormActivityParams&)params
                      delegate:(id<PasswordControllerDelegate>)delegate {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _passwordControllerDelegate = delegate;
    _dismissing = NO;
    _params = params;
  }
  return self;
}

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                   requestInfo:
                       (webauthn::IOSPasskeyClient::RequestInfo)requestInfo
                      delegate:(id<PasswordControllerDelegate>)delegate {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _passwordControllerDelegate = delegate;
    _dismissing = NO;
    _requestInfo = std::move(requestInfo);
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  if (!_params.has_value() && !_requestInfo.has_value()) {
    // Cleanup the coordinator if it couldn't be started.
    [self.browserCoordinatorCommandsHandler dismissPasswordSuggestions];
    // Do not add any logic past this point in this specific context since the
    // the coordinator was torn down at this point hence now unusable.
    return;
  }

  WebStateList* webStateList = self.browser->GetWebStateList();
  const GURL& URL = webStateList->GetActiveWebState()->GetLastCommittedURL();

  ProfileIOS* profile = self.browser->GetProfile()->GetOriginalProfile();

  auto profilePasswordStore =
      IOSChromeProfilePasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);
  auto accountPasswordStore =
      IOSChromeAccountPasswordStoreFactory::GetForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS);

  _reauthModule =
      ScopedCredentialSuggestionBottomSheetReauthModuleOverride::Get();
  if (!_reauthModule) {
    _reauthModule = [[ReauthenticationModule alloc] init];
  }

  FaviconLoader* faviconLoader =
      IOSChromeFaviconLoaderFactory::GetForProfile(profile);
  PrefService* prefService = profile->GetPrefs();
  scoped_refptr<network::SharedURLLoaderFactory> sharedURLLoaderFactory =
      profile->GetSharedURLLoaderFactory();
  feature_engagement::Tracker* engagementTracker =
      feature_engagement::TrackerFactory::GetForProfile(profile);

  if (_params.has_value()) {
    _mediator = [[CredentialSuggestionBottomSheetMediator alloc]
          initWithWebStateList:webStateList
                 faviconLoader:faviconLoader
                   prefService:prefService
                        params:*_params
                  reauthModule:_reauthModule
          profilePasswordStore:profilePasswordStore
          accountPasswordStore:accountPasswordStore
        sharedURLLoaderFactory:sharedURLLoaderFactory
             engagementTracker:engagementTracker];
  } else {
    CHECK(_requestInfo.has_value());

    _mediator = [[PasskeySuggestionBottomSheetMediator alloc]
        initWithWebStateList:webStateList
                 requestInfo:std::move(*_requestInfo)];
  }
  _mediator.presenter = self;

  _viewController = [[CredentialSuggestionBottomSheetViewController alloc]
      initWithHandler:self
                  URL:URL];

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];

  _viewController.delegate = _mediator;
  _mediator.consumer = _viewController;

  // If the bottom sheet has no suggestion to show, stop the presentation right
  // away.
  if (![_mediator hasSuggestions]) {
    // Cleanup the coordinator if it couldn't be started.
    [self.browserCoordinatorCommandsHandler dismissPasswordSuggestions];
    // Do not add any logic past this point in this specific context since the
    // the coordinator was torn down at this point hence now unusable.
    return;
  }

  _viewController.parentViewControllerHeight =
      self.baseViewController.view.frame.size.height;
  __weak __typeof(self) weakSelf = self;
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:^{
                                        [weakSelf setInitialVoiceOverFocus];
                                      }];

  // Dismiss right away if the presentation failed to avoid having a zombie
  // coordinator. This is the best proxy we have to know whether the view
  // controller for the bottom sheet could really be presented as the completion
  // block is only called when presentation really happens, and we can't get any
  // error message or signal. Based on what we could test, we know that
  // presentingViewController is only set if the view controller can be
  // presented, where it is left to nil if the presentation is rejected for
  // various reasons (having another view controller already presented is one of
  // them). One should not think they can know all the reasons why the
  // presentation fails.
  //
  // Keep this line at the end of -start because the
  // delegate will likely -stop the coordinator when closing suggestions, so the
  // coordinator should be in the most up to date state where it can be safely
  // stopped.
  if (!_navigationController.presentingViewController) {
    [_mediator logExitReason:kCouldNotPresent];
    [self.browserCoordinatorCommandsHandler dismissPasswordSuggestions];
  }
}

- (void)stop {
  [_mediator disconnect];
  _mediator.consumer = nil;
  _mediator = nil;
  _viewController.delegate = nil;
  _viewController = nil;
  [super stop];
}

#pragma mark - CredentialSuggestionBottomSheetHandler

- (void)displayPasswordManager {
  _dismissing = YES;
  [_mediator logExitReason:kShowPasswordManager];

  __weak __typeof(self) weakSelf = self;
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:^{
                           [weakSelf displaySavedPasswordList];
                           [weakSelf.browserCoordinatorCommandsHandler
                                   dismissPasswordSuggestions];
                         }];
}

- (void)displayPasswordDetailsForFormSuggestion:
    (FormSuggestion*)formSuggestion {
  _dismissing = YES;

  CredentialSuggestionBottomSheetMediator*
      credentialSuggestionBottomSheetMediator =
          base::apple::ObjCCastStrict<CredentialSuggestionBottomSheetMediator>(
              _mediator);

  [credentialSuggestionBottomSheetMediator logExitReason:kShowPasswordDetails];
  std::optional<password_manager::CredentialUIEntry> credential =
      [credentialSuggestionBottomSheetMediator
          getCredentialForFormSuggestion:formSuggestion];

  __weak __typeof(self) weakSelf = self;
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:^{
                           if (credential.has_value()) {
                             [weakSelf
                                 showPasswordDetailsForCredential:credential
                                                                      .value()];
                           }
                           base::UmaHistogramBoolean("IOS.PasswordBottomSheet."
                                                     "Details.ValidCredential",
                                                     credential.has_value());
                           [weakSelf.browserCoordinatorCommandsHandler
                                   dismissPasswordSuggestions];
                         }];
}

- (void)primaryButtonTappedForSuggestion:(FormSuggestion*)formSuggestion
                                 atIndex:(NSInteger)index {
  if (_dismissing) {
    // Do not handle an action if the view controller is already being
    // dismissed. Only one action is allowed on the sheet.
    return;
  }
  // Disable user interactions on the root view of the view controller so any
  // further user action isn't allowed. Only one action is allowed on the sheet.
  _viewController.view.userInteractionEnabled = NO;

  _dismissing = YES;
  [_mediator logExitReason:kUsePasswordSuggestion];
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock completion = ^{
    [weakSelf.browserCoordinatorCommandsHandler dismissPasswordSuggestions];
  };
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:^{
                           [weakSelf didSelectSuggestion:formSuggestion
                                                 atIndex:index
                                              completion:completion];
                         }];

  // Dismiss the soft keyboard right after starting the animation so it doesn't
  // flicker.
  [self dismissSoftKeyboard];

  // Records the usage of password autofill. This notifies the Tips Manager,
  // which may trigger tips or guidance related to password management features.
  TipsManagerIOS* tipsManager =
      TipsManagerIOSFactory::GetForProfile(self.profile);

  if (tipsManager) {
    tipsManager->NotifySignal(
        segmentation_platform::tips_manager::signals::kUsedPasswordAutofill);
  }

  // Notify Welcome Back to remove Save and Autofill Passwords from the eligible
  // features.
  if (IsWelcomeBackEnabled()) {
    MarkWelcomeBackFeatureUsed(BestFeaturesItemType::kSaveAndAutofillPasswords);
  }
}

- (void)secondaryButtonTapped {
  // "Use Keyboard" button, which dismisses the bottom sheet.
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
}

- (void)viewDidDisappear {
  if (_dismissing) {
    return;
  }

  CredentialSuggestionBottomSheetMediator*
      credentialSuggestionBottomSheetMediator =
          base::apple::ObjCCast<CredentialSuggestionBottomSheetMediator>(
              _mediator);
  if (credentialSuggestionBottomSheetMediator) {
    // Terminate dismissal if the entrypoint that dismissed the bottom sheet
    // wasn't handled yet (e.g. when swipped away).
    // Explicitly refocus the field if the sheet is dismissed without using any
    // of its features. The listeners are detached as soon as the sheet is
    // presented which requires another means to refocus the blurred field once
    // the sheet is dismissed.
    [credentialSuggestionBottomSheetMediator refocus];
  }

  [_mediator logExitReason:kDismissal];
  [_mediator onDismissWithoutAnyCredentialAction];

  // Disconnect as a last step of cleaning up the presentation. This should
  // always be kept as the last step.
  [_mediator disconnect];
  [self.browserCoordinatorCommandsHandler dismissPasswordSuggestions];
}

#pragma mark - CredentialSuggestionBottomSheetPresenter

- (void)endPresentation {
  if (_dismissing) {
    // The bottom sheet was already dismissed by another entrypoint, so no need
    // to do it twice.
    return;
  }

  // Dismiss the bottom sheet, then the presentation will be fully torn down
  // upon calling -viewDidDisappear.
  [_navigationController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:nil];
}

#pragma mark - Private

- (void)setInitialVoiceOverFocus {
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  _viewController.aboveTitleView);
}

- (void)displaySavedPasswordList {
  [_passwordControllerDelegate displaySavedPasswordList];
}

// Sends the information about which suggestion from the bottom sheet was
// selected by the user, which is expected to fill the relevant fields.
- (void)didSelectSuggestion:(FormSuggestion*)formSuggestion
                    atIndex:(NSInteger)index
                 completion:(ProceduralBlock)completion {
  [_mediator didSelectSuggestion:formSuggestion
                         atIndex:index
                      completion:completion];
}

- (void)showPasswordDetailsForCredential:
    (password_manager::CredentialUIEntry)credential {
  [_passwordControllerDelegate showPasswordDetailsForCredential:credential];
}

// Dismisses the soft keyboard. Make sure to only call this when there is an
// active webstate.
- (void)dismissSoftKeyboard {
  web::WebState* activeWebState =
      self.browser->GetWebStateList()->GetActiveWebState();
  CHECK(activeWebState);
  if (activeWebState) {
    [activeWebState->GetView() endEditing:NO];
  }
}

@end
