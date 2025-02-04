// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/coordinator/parent_access_coordinator.h"

#import "base/functional/bind.h"
#import "components/supervised_user/core/browser/supervised_user_utils.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/parent_access_commands.h"
#import "ios/chrome/browser/supervised_user/coordinator/parent_access_mediator.h"
#import "ios/chrome/browser/supervised_user/coordinator/parent_access_mediator_delegate.h"
#import "ios/chrome/browser/supervised_user/model/parent_access_tab_helper.h"
#import "ios/chrome/browser/supervised_user/model/parent_access_tab_helper_delegate.h"
#import "ios/chrome/browser/supervised_user/model/supervised_user_error_container.h"
#import "ios/chrome/browser/supervised_user/ui/parent_access_bottom_sheet_view_controller.h"
#import "ios/web/public/web_state.h"

@interface ParentAccessCoordinator () <UIAdaptivePresentationControllerDelegate,
                                       ParentAccessMediatorDelegate,
                                       ParentAccessTabHelperDelegate>
@end

@implementation ParentAccessCoordinator {
  ParentAccessApprovalResultCallback _callback;
  ParentAccessBottomSheetViewController* _viewController;
  ParentAccessMediator* _mediator;
  GURL _targetURL;
  supervised_user::FilteringBehaviorReason _filteringBehaviorReason;
}

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                     targetURL:(const GURL&)targetURL
       filteringBehaviorReason:
           (supervised_user::FilteringBehaviorReason)filteringBehaviorReason
                    completion:(void (^)(supervised_user::LocalApprovalResult))
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
  ProfileIOS* profile = self.browser->GetProfile()->GetOriginalProfile();

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
}

#pragma mark - ParentAccessTabHelperDelegate

- (void)hideParentAccessBottomSheetWithResult:
    (supervised_user::LocalApprovalResult)result {
  id<ParentAccessCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ParentAccessCommands);
  if (_callback) {
    std::move(_callback).Run(result);
  }

  // Dismiss the parent access bottom sheet, which will also stop this
  // coordinator.
  [handler hideParentAccessBottomSheet];
}

#pragma mark - ParentAccessMediatorDelegate

- (void)hideParentAccessBottomSheetOnTimeout {
  [self hideParentAccessBottomSheetWithResult:
            supervised_user::LocalApprovalResult::kCanceled];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  [self hideParentAccessBottomSheetWithResult:
            supervised_user::LocalApprovalResult::kDeclined];
}

@end
