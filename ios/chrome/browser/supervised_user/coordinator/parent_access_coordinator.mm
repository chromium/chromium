// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/coordinator/parent_access_coordinator.h"

#import <MaterialComponents/MaterialSnackbar.h>

#import <optional>

#import "base/functional/bind.h"
#import "components/strings/grit/components_strings.h"
#import "components/supervised_user/core/browser/supervised_user_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/parent_access_commands.h"
#import "ios/chrome/browser/shared/public/commands/snackbar_commands.h"
#import "ios/chrome/browser/shared/ui/util/snackbar_util.h"
#import "ios/chrome/browser/supervised_user/coordinator/parent_access_mediator.h"
#import "ios/chrome/browser/supervised_user/coordinator/parent_access_mediator_delegate.h"
#import "ios/chrome/browser/supervised_user/model/parent_access_tab_helper.h"
#import "ios/chrome/browser/supervised_user/model/parent_access_tab_helper_delegate.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_error_container.h"
#import "ios/chrome/browser/supervised_user/ui/constants.h"
#import "ios/chrome/browser/supervised_user/ui/parent_access_bottom_sheet_view_controller.h"
#import "ios/chrome/browser/supervised_user/ui/parent_access_bottom_sheet_view_controller_presentation_delegate.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

@interface ParentAccessCoordinator () <
    UIAdaptivePresentationControllerDelegate,
    ParentAccessMediatorDelegate,
    ParentAccessTabHelperDelegate,
    ParentAccessBottomSheetViewControllerPresentationDelegate>
@end

@implementation ParentAccessCoordinator {
  ParentAccessApprovalResultCallback _callback;
  ParentAccessBottomSheetViewController* _viewController;
  ParentAccessMediator* _mediator;
  GURL _targetURL;
  supervised_user::FilteringBehaviorReason _filteringBehaviorReason;
  id<SnackbarCommands> _snackbarCommandsHandler;
}

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                     targetURL:(const GURL&)targetURL
       filteringBehaviorReason:
           (supervised_user::FilteringBehaviorReason)filteringBehaviorReason
                    completion:
                        (void (^)(
                            supervised_user::LocalApprovalResult,
                            std::optional<
                                supervised_user::LocalWebApprovalErrorType>))
                            completion {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _targetURL = targetURL;
    _filteringBehaviorReason = filteringBehaviorReason;
    _callback = base::BindOnce(completion);
  }
  return self;
}

- (void)start {
  ProfileIOS* profile = self.profile->GetOriginalProfile();

  // Set up the WebState and its TabHelper.
  web::WebState::CreateParams params = web::WebState::CreateParams(profile);
  std::unique_ptr<web::WebState> webState = web::WebState::Create(params);
  ParentAccessTabHelper::CreateForWebState(webState.get());
  ParentAccessTabHelper* tabHelper =
      ParentAccessTabHelper::FromWebState(webState.get());
  tabHelper->SetDelegate(self);

  GURL parentAccessURL = supervised_user::GetParentAccessURLForIOS(
      GetApplicationContext()->GetApplicationLocale(), _targetURL,
      _filteringBehaviorReason);
  _mediator = [[ParentAccessMediator alloc] initWithWebState:std::move(webState)
                                             parentAccessURL:parentAccessURL];
  _mediator.delegate = self;
  _viewController = [[ParentAccessBottomSheetViewController alloc] init];

  // Do not use the bottom sheet default dismiss button.
  _viewController.showDismissBarButton = NO;

  _viewController.presentationController.delegate = self;
  _viewController.presentationDelegate = self;

  // Set up for a snackbar that will be displayed when the widget fails to load.
  _snackbarCommandsHandler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), SnackbarCommands);

  // Dismiss existing snackbars that would appear on top of the bottom sheet.
  [_snackbarCommandsHandler dismissAllSnackbars];

  // Set the consumer, which starts navigation.
  _mediator.consumer = _viewController;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [_mediator disconnect];
  _mediator = nil;
  [_viewController.presentingViewController dismissViewControllerAnimated:YES
                                                               completion:nil];
  _viewController = nil;
  _snackbarCommandsHandler = nil;
}

#pragma mark - ParentAccessTabHelperDelegate

- (void)hideParentAccessBottomSheetWithResult:
            (supervised_user::LocalApprovalResult)result
                                    errorType:
                                        (std::optional<
                                            supervised_user::
                                                LocalWebApprovalErrorType>)
                                            errorType {
  id<ParentAccessCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ParentAccessCommands);
  if (_callback) {
    std::move(_callback).Run(result, errorType);
  }

  // Dismiss the parent access bottom sheet, which will also stop this
  // coordinator.
  [handler hideParentAccessBottomSheet];
}

#pragma mark - ParentAccessMediatorDelegate

- (void)hideParentAccessBottomSheetOnTimeout {
  [_snackbarCommandsHandler showSnackbarMessage:[self snackbarMessage]];
  [self hideParentAccessBottomSheetWithResult:supervised_user::
                                                  LocalApprovalResult::kError
                                    errorType:supervised_user::
                                                  LocalWebApprovalErrorType::
                                                      kPacpTimeoutExceeded];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self hideParentAccessBottomSheetWithResult:supervised_user::
                                                  LocalApprovalResult::kCanceled
                                    errorType:std::nullopt];
}

#pragma mark - ParentAccessBottomSheetViewControllerPresentationDelegate

- (void)closeBottomSheetRequested:
    (ParentAccessBottomSheetViewController*)controller {
  [self hideParentAccessBottomSheetWithResult:supervised_user::
                                                  LocalApprovalResult::kCanceled
                                    errorType:std::nullopt];
}

#pragma mark - Private

- (MDCSnackbarMessage*)snackbarMessage {
  // Create a "Close" action for the snackbar. Tapping anywhere on the snackbar
  // dismisses it, so an action handler is not required.
  MDCSnackbarMessageAction* action = [[MDCSnackbarMessageAction alloc] init];
  action.title = l10n_util::GetNSString(
      IDS_PARENTAL_LOCAL_APPROVAL_SNACKBAR_GENERIC_ERROR_BACK_BUTTON);
  action.accessibilityIdentifier = kParentAccessSnackbarClose;

  MDCSnackbarMessage* message = CreateSnackbarMessage(l10n_util::GetNSString(
      IDS_PARENTAL_LOCAL_APPROVAL_SNACKBAR_GENERIC_ERROR_TITLE));
  message.action = action;
  message.category = kParentAccessSnackbarCategory;
  return message;
}

@end
