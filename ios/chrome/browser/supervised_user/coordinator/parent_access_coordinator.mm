// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/supervised_user/coordinator/parent_access_coordinator.h"

#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/supervised_user/coordinator/parent_access_mediator.h"
#import "ios/chrome/browser/supervised_user/ui/parent_access_bottom_sheet_view_controller.h"

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
  SystemIdentityManager* systemIdentityManager =
      GetApplicationContext()->GetSystemIdentityManager();
  _mediator = [[ParentAccessMediator alloc]
      initWithAccountManagerService:ChromeAccountManagerServiceFactory::
                                        GetForProfile(profile)
                    identityManager:IdentityManagerFactory::GetForProfile(
                                        profile)
              systemIdentityManager:systemIdentityManager];
  _viewController = [[ParentAccessBottomSheetViewController alloc] init];
  _viewController.delegate = _mediator;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  _mediator = nil;
  _viewController = nil;
}

#pragma mark - WKScriptMessageHandler

- (void)userContentController:(WKUserContentController*)userContentController
      didReceiveScriptMessage:(WKScriptMessage*)message {
  // TODO(crbug.com/384514294): Processes local approval result in completion
  // callback.
}

@end
