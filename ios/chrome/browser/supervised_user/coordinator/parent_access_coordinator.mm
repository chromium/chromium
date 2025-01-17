// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/coordinator/parent_access_coordinator.h"

#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/parent_access_commands.h"
#import "ios/chrome/browser/supervised_user/coordinator/parent_access_mediator.h"
#import "ios/chrome/browser/supervised_user/ui/parent_access_bottom_sheet_view_controller.h"
#import "ios/web/public/web_state.h"

@interface ParentAccessCoordinator () <UIAdaptivePresentationControllerDelegate>
@end

@implementation ParentAccessCoordinator {
  ParentAccessCallbackCompletion _completion;
  ParentAccessBottomSheetViewController* _viewController;
  ParentAccessMediator* _mediator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                completion:
                                    (ParentAccessCallbackCompletion)completion {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _completion = completion;
  }
  return self;
}

- (void)start {
  ProfileIOS* profile = self.browser->GetProfile()->GetOriginalProfile();
  web::WebState::CreateParams params = web::WebState::CreateParams(profile);
  _mediator = [[ParentAccessMediator alloc]
      initWithWebState:web::WebState::Create(params)];

  _viewController = [[ParentAccessBottomSheetViewController alloc] init];
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

#pragma mark - UIAdaptivePresentationControllerDelegate

- (void)presentationControllerDidDismiss:
    (UIPresentationController*)presentationController {
  id<ParentAccessCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ParentAccessCommands);
  [handler hideParentAccessBottomSheet];
}

#pragma mark - WKScriptMessageHandler

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  // TODO(crbug.com/384514294): Processes local approval result in completion
  // callback.
}

@end
