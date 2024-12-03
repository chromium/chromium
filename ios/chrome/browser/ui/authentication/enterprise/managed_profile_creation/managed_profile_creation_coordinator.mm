// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/enterprise/managed_profile_creation/managed_profile_creation_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/ios/block_types.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/ui/authentication/enterprise/managed_profile_creation/managed_profile_creation_view_controller.h"

@interface ManagedProfileCreationCoordinator () <
    ManagedProfileCreationViewControllerDelegate>
@end

@implementation ManagedProfileCreationCoordinator {
  NSString* _userEmail;
  NSString* _hostedDomain;
  ManagedProfileCreationViewController* _viewController;
}

@synthesize baseViewController = _baseViewController;

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                 userEmail:(NSString*)userEmail
                              hostedDomain:(NSString*)hostedDomain
                                   browser:(Browser*)browser {
  // TODO(crbug.com/381853288): Add a mediator to listen to the identity
  // changes.
  DCHECK(viewController);
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _baseViewController = viewController;
    _userEmail = userEmail;
    _hostedDomain = hostedDomain;
  }
  return self;
}

- (void)start {
  _viewController = [[ManagedProfileCreationViewController alloc]
      initWithUserEmail:_userEmail
           hostedDomain:_hostedDomain];
  _viewController.delegate = self;
  _viewController.managedProfileCreationViewControllerPresentationDelegate =
      self;
  _viewController.modalInPresentation = YES;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  if (_viewController) {
    [_viewController dismissViewControllerAnimated:YES completion:nil];
    _viewController.delegate = nil;
    _viewController.managedProfileCreationViewControllerPresentationDelegate =
        nil;
    _viewController = nil;
  }
  [super stop];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
  [self.delegate managedProfileCreationCoordinator:self didAccept:YES];
}

- (void)didTapSecondaryActionButton {
  [_viewController dismissViewControllerAnimated:YES completion:nil];
  _viewController = nil;
  [self.delegate managedProfileCreationCoordinator:self didAccept:NO];
}

@end
