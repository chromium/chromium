// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/password_suggestion_bottom_sheet_coordinator.h"

#import <optional>

#import "base/feature_list.h"
#import "components/keyed_service/core/service_access_type.h"
#import "components/password_manager/core/browser/features/password_features.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/segmentation_platform/embedder/home_modules/tips_manager/signal_constants.h"
#import "ios/chrome/browser/favicon/model/ios_chrome_favicon_loader_factory.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_account_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_profile_password_store_factory.h"
#import "ios/chrome/browser/passwords/model/password_controller_delegate.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/password_suggestion_bottom_sheet_mediator.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/password_suggestion_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/passwords/ui_bundled/bottom_sheet/scoped_password_suggestion_bottom_sheet_reauth_module_override.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios.h"
#import "ios/chrome/browser/tips_manager/model/tips_manager_ios_factory.h"
#import "ios/chrome/common/ui/reauthentication/reauthentication_module.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

using PasswordSuggestionBottomSheetExitReason::kCouldNotPresent;
using PasswordSuggestionBottomSheetExitReason::kDismissal;
using PasswordSuggestionBottomSheetExitReason::kShowPasswordDetails;
using PasswordSuggestionBottomSheetExitReason::kShowPasswordManager;
using PasswordSuggestionBottomSheetExitReason::kUsePasswordSuggestion;

@interface PasswordSuggestionBottomSheetCoordinator () {
  // The password controller delegate used to open the password manager.
  id<PasswordControllerDelegate> _passwordControllerDelegate;

  // Currently in the process of dismissing the bottom sheet.
  bool _dismissing;
}

// This mediator is used to fetch data related to the bottom sheet.
@property(nonatomic, strong) PasswordSuggestionBottomSheetMediator* mediator;

// This view controller is used to display the bottom sheet.
@property(nonatomic, strong)
    PasswordSuggestionBottomSheetViewController* viewController;

// Module handling reauthentication before accessing sensitive data.
@property(nonatomic, strong) id<ReauthenticationProtocol> reauthModule;

@end

@implementation PasswordSuggestionBottomSheetCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                        params:(const autofill::FormActivityParams&)params
                      delegate:(id<PasswordControllerDelegate>)delegate {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _passwordControllerDelegate = delegate;
    _dismissing = NO;

    WebStateList* webStateList = browser->GetWebStateList();
    const GURL& URL = webStateList->GetActiveWebState()->GetLastCommittedURL();
    self.viewController = [[PasswordSuggestionBottomSheetViewController alloc]
        initWithHandler:self
                    URL:URL];

    ProfileIOS* profile = browser->GetProfile()->GetOriginalProfile();

    auto profilePasswordStore =
        IOSChromeProfilePasswordStoreFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS);
    auto accountPasswordStore =
        IOSChromeAccountPasswordStoreFactory::GetForProfile(
            profile, ServiceAccessType::EXPLICIT_ACCESS);

    self.reauthModule =
        ScopedPasswordSuggestionBottomSheetReauthModuleOverride::Get();
    if (!self.reauthModule) {
      self.reauthModule = [[ReauthenticationModule alloc] init];
    }
    self.mediator = [[PasswordSuggestionBottomSheetMediator alloc]
          initWithWebStateList:webStateList
                 faviconLoader:IOSChromeFaviconLoaderFactory::GetForProfile(
                                   profile)
                   prefService:profile->GetPrefs()
                        params:params
                  reauthModule:_reauthModule
                           URL:URL
          profilePasswordStore:profilePasswordStore
          accountPasswordStore:accountPasswordStore
        sharedURLLoaderFactory:profile->GetSharedURLLoaderFactory()
             engagementTracker:feature_engagement::TrackerFactory::
                                   GetForProfile(self.browser->GetProfile())
                     presenter:self];
    self.viewController.delegate = self.mediator;
    self.mediator.consumer = self.viewController;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  // If the bottom sheet has no suggestion to show, stop the presentation right
  // away.
  if (![self.mediator hasSuggestions]) {
    // Cleanup the coordinator if it couldn't be started.
    [self.browserCoordinatorCommandsHandler dismissPasswordSuggestions];
    // Do not add any logic past this point in this specific context since the
    // the coordinator was torn down at this point hence now unusable.
    return;
  }

  self.viewController.parentViewControllerHeight =
      self.baseViewController.view.frame.size.height;
  __weak __typeof(self) weakSelf = self;
  [self.baseViewController presentViewController:self.viewController
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
  if (!self.viewController.presentingViewController) {
    [self.mediator logExitReason:kCouldNotPresent];
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

#pragma mark - PasswordSuggestionBottomSheetHandler

- (void)displayPasswordManager {
  _dismissing = YES;
  [self.mediator logExitReason:kShowPasswordManager];

  __weak __typeof(self) weakSelf = self;
  [self.viewController.presentingViewController
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
  [self.mediator logExitReason:kShowPasswordDetails];
  std::optional<password_manager::CredentialUIEntry> credential =
      [self.mediator getCredentialForFormSuggestion:formSuggestion];

  __weak __typeof(self) weakSelf = self;
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:^{
                           // TODO(crbug.com/40896839): Add metric for when the
                           // credential is nil.
                           if (credential.has_value()) {
                             [weakSelf
                                 showPasswordDetailsForCredential:credential
                                                                      .value()];
                           }
                           [weakSelf.browserCoordinatorCommandsHandler
                                   dismissPasswordSuggestions];
                         }];
}

- (void)primaryButtonTappedForSuggestion:(FormSuggestion*)formSuggestion
                                 atIndex:(NSInteger)index {
  _dismissing = YES;
  [self.mediator logExitReason:kUsePasswordSuggestion];
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock completion = ^{
    [weakSelf.browserCoordinatorCommandsHandler dismissPasswordSuggestions];
  };
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:^{
                           [weakSelf.mediator didSelectSuggestion:formSuggestion
                                                          atIndex:index
                                                       completion:completion];
                         }];

  // Records the usage of password autofill. This notifies the Tips Manager,
  // which may trigger tips or guidance related to password management features.
  if (IsSegmentationTipsManagerEnabled()) {
    TipsManagerIOS* tipsManager =
        TipsManagerIOSFactory::GetForProfile(self.browser->GetProfile());

    tipsManager->NotifySignal(
        segmentation_platform::tips_manager::signals::kUsedPasswordAutofill);
  }
}

- (void)secondaryButtonTapped {
  // "Use Keyboard" button, which dismisses the bottom sheet.
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
}

- (void)viewDidDisappear {
  if (_dismissing) {
    return;
  }

  // Terminate dismissal if the entrypoint that dismissed the bottom sheet
  // wasn't handled yet (e.g. when swipped away).
  if (base::FeatureList::IsEnabled(
          password_manager::features::kIOSPasswordBottomSheetV2)) {
    // Explicitly refocus the field if the sheet is dismissed without using any
    // of its features. This is only required for V2 where the listeners are
    // detached as soon as the sheet is presented which requires another means
    // to refocus the blurred field once the sheet is dismissed.
    [self.mediator refocus];
  }

  [self.mediator logExitReason:kDismissal];
  [self.mediator onDismissWithoutAnyPasswordAction];

  // Disconnect as a last step of cleaning up the presentation. This should
  // always be kept as the last step.
  [self.mediator disconnect];
  [self.browserCoordinatorCommandsHandler dismissPasswordSuggestions];
}

#pragma mark - PasswordSuggestionBottomSheetPresenter

- (void)endPresentation {
  if (_dismissing) {
    // The bottom sheet was already dismissed by another entrypoint, so no need
    // to do it twice.
    return;
  }

  // Dismiss the bottom sheet, then the presentation will be fully torn down
  // upon calling -viewDidDisappear.
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:NO
                         completion:nil];
}

#pragma mark - Private

- (void)setInitialVoiceOverFocus {
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  self.viewController.aboveTitleView);
}

- (void)displaySavedPasswordList {
  [_passwordControllerDelegate displaySavedPasswordList];
}

- (void)showPasswordDetailsForCredential:
    (password_manager::CredentialUIEntry)credential {
  [_passwordControllerDelegate showPasswordDetailsForCredential:credential];
}

@end
