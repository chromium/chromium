// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/enterprise/managed_profile_creation/managed_profile_creation_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/ios/block_types.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/ui/authentication/enterprise/managed_profile_creation/managed_profile_creation_mediator.h"
#import "ios/chrome/browser/ui/authentication/enterprise/managed_profile_creation/managed_profile_creation_view_controller.h"

@interface ManagedProfileCreationCoordinator () <
    ManagedProfileCreationViewControllerDelegate>
@end

@implementation ManagedProfileCreationCoordinator {
  NSString* _userEmail;
  NSString* _hostedDomain;
  BOOL _skipBrowsingDataMigration;
  ManagedProfileCreationViewController* _viewController;
  ManagedProfileCreationMediator* _mediator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                 userEmail:(NSString*)userEmail
                              hostedDomain:(NSString*)hostedDomain
                                   browser:(Browser*)browser
                 skipBrowsingDataMigration:(BOOL)skipBrowsingDataMigration {
  // TODO(crbug.com/381853288): Add a mediator to listen to the identity
  // changes.
  DCHECK(viewController);
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _userEmail = userEmail;
    _hostedDomain = hostedDomain;
    _skipBrowsingDataMigration = skipBrowsingDataMigration;
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

  ProfileIOS* profile = self.browser->GetProfile()->GetOriginalProfile();
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);

  _mediator = [[ManagedProfileCreationMediator alloc]
        initWithIdentityManager:identityManager
      skipBrowsingDataMigration:_skipBrowsingDataMigration];
  _mediator.consumer = _viewController;

  [self.baseViewController presentViewController:_viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self dismissViewControllerAnimated:YES];
  [super stop];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  [self dismissViewControllerAnimated:YES];
  [self.delegate
      managedProfileCreationCoordinator:self
                              didAccept:YES
               keepBrowsingDataSeparate:_mediator.keepBrowsingDataSeparate];
}

- (void)didTapSecondaryActionButton {
  [self dismissViewControllerAnimated:YES];
  [self.delegate managedProfileCreationCoordinator:self
                                         didAccept:NO
                          keepBrowsingDataSeparate:NO];
}

#pragma mark - ManagedProfileCreationViewControllerDelegate

- (void)showMergeBrowsingDataScreen {
  // TODO(crbug.com/382240108): Actually show a screen where the user can make
  // an educated choice.
  [_mediator setKeepBrowsingDataSeparate:!_mediator.keepBrowsingDataSeparate];
}

#pragma mark - Private

- (void)dismissViewControllerAnimated:(BOOL)animated {
  _mediator.consumer = nil;
  _mediator = nil;
  _viewController.delegate = nil;
  _viewController.managedProfileCreationViewControllerPresentationDelegate =
      nil;
  [_viewController dismissViewControllerAnimated:animated completion:nil];
  _viewController = nil;
}

@end
