// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/managed_profile_creation_coordinator.h"

#import "base/apple/foundation_util.h"
#import "base/ios/block_types.h"
#import "base/strings/sys_string_conversions.h"
#import "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/browsing_data_migration_view_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/managed_profile_creation_mediator.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/managed_profile_creation_view_controller.h"
#import "ios/chrome/browser/authentication/ui_bundled/enterprise/managed_profile_creation/managed_profile_learn_more_coordinator.h"
#import "ios/chrome/browser/authentication/ui_bundled/signin/signin_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service.h"
#import "ios/chrome/browser/signin/model/chrome_account_manager_service_factory.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/signin/model/system_identity.h"

@interface ManagedProfileCreationCoordinator () <
    ManagedProfileCreationMediatorDelegate,
    ManagedProfileCreationViewControllerDelegate,
    ManagedProfileLearnMoreCoordinatorDelegate,
    UINavigationControllerDelegate>
@end

@implementation ManagedProfileCreationCoordinator {
  id<SystemIdentity> _identity;
  NSString* _hostedDomain;
  BOOL _skipBrowsingDataMigration;
  BOOL _mergeBrowsingDataByDefault;
  BOOL _browsingDataMigrationDisabledByPolicy;
  BOOL _multiProfileForceMigration;
  ManagedProfileCreationViewController* _viewController;
  // Used to display `_viewController` initially and
  // `_browsingDataMigrationViewController` if the user tries to modify how
  // existing browsing data should be handled.
  UINavigationController* _navigationController;
  ManagedProfileCreationMediator* _mediator;
  BrowsingDataMigrationViewController* _browsingDataMigrationViewController;
  ManagedProfileLearnMoreCoordinator* _learnMoreCoordinator;
}

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                  identity:(id<SystemIdentity>)identity
                              hostedDomain:(NSString*)hostedDomain
                                   browser:(Browser*)browser
                 skipBrowsingDataMigration:(BOOL)skipBrowsingDataMigration
                mergeBrowsingDataByDefault:(BOOL)mergeBrowsingDataByDefault
     browsingDataMigrationDisabledByPolicy:
         (BOOL)browsingDataMigrationDisabledByPolicy
                multiProfileForceMigration:(BOOL)multiProfileForceMigration {
  // TODO(crbug.com/381853288): Add a mediator to listen to the identity
  // changes.
  DCHECK(viewController);
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    // Sign-in related work should be done on regular browser.
    CHECK_EQ(browser->type(), Browser::Type::kRegular,
             base::NotFatalUntil::M145);
    _identity = identity;
    _hostedDomain = hostedDomain;
    _skipBrowsingDataMigration = skipBrowsingDataMigration;
    _mergeBrowsingDataByDefault = mergeBrowsingDataByDefault;
    _browsingDataMigrationDisabledByPolicy =
        browsingDataMigrationDisabledByPolicy;
    _multiProfileForceMigration = multiProfileForceMigration;
  }
  return self;
}

- (void)start {
  _viewController = [[ManagedProfileCreationViewController alloc]
               initWithUserEmail:_identity.userEmail
                    hostedDomain:_hostedDomain
      multiProfileForceMigration:_multiProfileForceMigration];
  _viewController.delegate = self;
  _viewController.managedProfileCreationViewControllerPresentationDelegate =
      self;
  _viewController.modalInPresentation = YES;

  ProfileIOS* profile = self.profile->GetOriginalProfile();
  signin::IdentityManager* identityManager =
      IdentityManagerFactory::GetForProfile(profile);
  ChromeAccountManagerService* accountManagerService =
      ChromeAccountManagerServiceFactory::GetForProfile(profile);

  _mediator = [[ManagedProfileCreationMediator alloc]
                    initWithIdentityManager:identityManager
                      accountManagerService:accountManagerService
                  skipBrowsingDataMigration:_skipBrowsingDataMigration
                 mergeBrowsingDataByDefault:_mergeBrowsingDataByDefault
      browsingDataMigrationDisabledByPolicy:
          _browsingDataMigrationDisabledByPolicy
                                     gaiaID:_identity.gaiaId];
  _mediator.consumer = _viewController;
  _mediator.delegate = self;

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  _navigationController.delegate = self;
  [_navigationController setNavigationBarHidden:YES animated:NO];

  _navigationController.navigationBar.accessibilityIdentifier =
      kManagedProfileCreationNavigationBarAccessibilityIdentifier;
  [self.baseViewController presentViewController:_navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self dismissViewControllerAnimated:YES];
  [self stopLearnMoreCoordinator];
  [super stop];
}

#pragma mark - PromoStyleViewControllerDelegate

- (void)didTapPrimaryActionButton {
  // `dismissViewControllerAnimated` will release the mediator, so grab this
  // value first.
  BOOL browsingDataSeparate = _mediator.browsingDataSeparate;
  [self dismissViewControllerAnimated:YES];
  [self.delegate managedProfileCreationCoordinator:self
                                         didAccept:YES
                              browsingDataSeparate:browsingDataSeparate];
}

- (void)didTapSecondaryActionButton {
  [self dismissViewControllerAnimated:YES];
  [self.delegate managedProfileCreationCoordinator:self
                                         didAccept:NO
                              browsingDataSeparate:NO];
}

- (void)didTapURLInDisclaimer:(NSURL*)URL {
  if ([URL.absoluteString isEqualToString:kManagedProfileLearnMoreURL]) {
    [self showLearnMorePage];
  } else {
    NOTREACHED() << std::string("Unknown URL ")
                 << base::SysNSStringToUTF8(URL.absoluteString);
  }
}

#pragma mark - ManagedProfileCreationViewControllerDelegate

- (void)showMergeBrowsingDataScreen {
  CHECK(!_browsingDataMigrationViewController);
  _browsingDataMigrationViewController =
      [[BrowsingDataMigrationViewController alloc]
             initWithUserEmail:_identity.userEmail
          browsingDataSeparate:_mediator.browsingDataSeparate];
  _browsingDataMigrationViewController.mutator = _mediator;
  [_navigationController pushViewController:_browsingDataMigrationViewController
                                   animated:YES];
}

#pragma mark - UINavigationControllerDelegate

- (void)navigationController:(UINavigationController*)navigationController
      willShowViewController:(UIViewController*)viewController
                    animated:(BOOL)animated {
  BOOL showingMainViewController = [viewController
      isMemberOfClass:[ManagedProfileCreationViewController class]];
  [_navigationController setNavigationBarHidden:showingMainViewController
                                       animated:!showingMainViewController];
  if (showingMainViewController) {
    _browsingDataMigrationViewController.mutator = nil;
    _browsingDataMigrationViewController = nil;
  }
}

#pragma mark - ManagedProfileLearnMoreCoordinatorDelegate

- (void)removeLearnMoreCoordinator:
    (ManagedProfileLearnMoreCoordinator*)coordinator {
  DCHECK(_learnMoreCoordinator);
  DCHECK_EQ(_learnMoreCoordinator, coordinator);
  [self stopLearnMoreCoordinator];
}

#pragma mark - Private

- (void)dismissViewControllerAnimated:(BOOL)animated {
  [_mediator disconnect];
  _mediator = nil;
  _viewController.delegate = nil;
  _viewController.managedProfileCreationViewControllerPresentationDelegate =
      nil;
  [_viewController dismissViewControllerAnimated:animated completion:nil];
  _viewController = nil;
  [_navigationController dismissViewControllerAnimated:animated completion:nil];
  _navigationController = nil;
}

- (void)showLearnMorePage {
  DCHECK(!_learnMoreCoordinator);
  _learnMoreCoordinator = [[ManagedProfileLearnMoreCoordinator alloc]
      initWithBaseViewController:_viewController
                         browser:self.browser
                       userEmail:_identity.userEmail
                    hostedDomain:_hostedDomain];
  _learnMoreCoordinator.delegate = self;
  [_learnMoreCoordinator start];
}

- (void)stopLearnMoreCoordinator {
  [_learnMoreCoordinator stop];
  _learnMoreCoordinator.delegate = nil;
  _learnMoreCoordinator = nil;
}

#pragma mark - ManagedProfileCreationMediatorDelegate

- (void)identityRemovedFromDevice {
  if (_learnMoreCoordinator) {
    [self stopLearnMoreCoordinator];
  }
  [self dismissViewControllerAnimated:YES];
}

@end
