// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_coordinator.h"

#import "components/password_manager/core/browser/sharing/recipients_fetcher.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/identity_manager_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "ios/chrome/grit/ios_strings.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util.h"

using password_manager::FetchFamilyMembersRequestStatus;

@interface PasswordSharingCoordinator () <FamilyPickerCoordinatorDelegate,
                                          FamilyPromoCoordinatorDelegate,
                                          PasswordSharingMediatorDelegate> {
  // The credentials for the password group from which the sharing originated.
  std::vector<password_manager::CredentialUIEntry> _credentials;
}

// The navigation controller displaying the view controller.
@property(nonatomic, strong)
    TableViewNavigationController* navigationController;

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordSharingViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordSharingMediator* mediator;

// Coordinator for family picker.
@property(nonatomic, strong) FamilyPickerCoordinator* familyPickerCoordinator;

// Coordinator for family promo view.
@property(nonatomic, strong) FamilyPromoCoordinator* familyPromoCoordinator;

// Coordinator to display modal alerts to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// Coordinator for password picker.
@property(nonatomic, strong)
    PasswordPickerCoordinator* passwordPickerCoordinator;

@end

@implementation PasswordSharingCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                   credentials:
                       (const std::vector<password_manager::CredentialUIEntry>&)
                           credentials {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _credentials = credentials;
  }
  return self;
}

- (void)start {
  [super start];

  self.viewController = [[PasswordSharingViewController alloc]
      initWithStyle:ChromeTableViewStyle()];
  self.navigationController =
      [[TableViewNavigationController alloc] initWithTable:self.viewController];
  [self.navigationController
      setModalPresentationStyle:UIModalPresentationFormSheet];
  self.navigationController.navigationBar.prefersLargeTitles = NO;

  UISheetPresentationController* sheetPresentationController =
      self.navigationController.sheetPresentationController;
  if (sheetPresentationController) {
    sheetPresentationController.detents =
        @[ [UISheetPresentationControllerDetent mediumDetent] ];
  }

  [self.baseViewController presentViewController:self.navigationController
                                        animated:YES
                                      completion:nil];

  ChromeBrowserState* browserState = self.browser->GetBrowserState();
  self.mediator = [[PasswordSharingMediator alloc]
            initWithDelegate:self
      SharedURLLoaderFactory:browserState->GetSharedURLLoaderFactory()
             identityManager:IdentityManagerFactory::GetForBrowserState(
                                 browserState)];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
  self.navigationController = nil;
  self.mediator = nil;

  [self stopFamilyPickerCoordinator];
  [self stopFamilyPromoCoordinator];
  [self stopAlertCoordinator];
}

#pragma mark - FamilyPickerCoordinatorDelegate

- (void)familyPickerCoordinatorWasDismissed:
    (FamilyPickerCoordinator*)coordinator {
  if (self.familyPickerCoordinator == coordinator) {
    [self stopFamilyPickerCoordinator];
  }

  [self.delegate passwordSharingCoordinatorDidRemove:self];
}

#pragma mark - FamilyPromoCoordinatorDelegate

- (void)familyPromoCoordinatorWasDismissed:
    (FamilyPromoCoordinator*)coordinator {
  if (self.familyPromoCoordinator == coordinator) {
    [self stopFamilyPromoCoordinator];
  }

  [self.delegate passwordSharingCoordinatorDidRemove:self];
}

#pragma mark - PasswordSharingMediatorDelegate

- (void)onFetchFamilyMembers:
            (NSArray<RecipientInfoForIOSDisplay*>*)familyMembers
                  withStatus:(const FetchFamilyMembersRequestStatus&)status {
  // TODO(crbug.com/1463882): Add EG tests for the whole flow.
  switch (status) {
    case FetchFamilyMembersRequestStatus::kSuccess:
      if (_credentials.size() == 1) {
        [self.familyPickerCoordinator stop];
        self.familyPickerCoordinator = [[FamilyPickerCoordinator alloc]
            initWithBaseViewController:self.viewController
                               browser:self.browser
                            recipients:familyMembers];
        self.familyPickerCoordinator.delegate = self;
        [self.familyPickerCoordinator start];
      } else {
        self.passwordPickerCoordinator = [[PasswordPickerCoordinator alloc]
            initWithBaseViewController:self.viewController
                               browser:self.browser
                           credentials:_credentials];
        [self.passwordPickerCoordinator start];
      }
      break;
    case FetchFamilyMembersRequestStatus::kNoFamily:
      [self.familyPromoCoordinator stop];
      self.familyPromoCoordinator = [[FamilyPromoCoordinator alloc]
          initWithBaseViewController:self.viewController
                             browser:self.browser];
      self.familyPromoCoordinator.delegate = self;
      [self.familyPromoCoordinator start];
      break;
    case FetchFamilyMembersRequestStatus::kUnknown:
    case FetchFamilyMembersRequestStatus::kNetworkError:
    case FetchFamilyMembersRequestStatus::kPendingRequest:
      __weak __typeof(self) weakSelf = self;
      [self.viewController.presentingViewController
          dismissViewControllerAnimated:YES
                             completion:^() {
                               [weakSelf startAlertCoordinator];
                             }];
      break;
  }
}

#pragma mark - Private

- (void)startAlertCoordinator {
  NSString* title = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SHARING_FETCHING_RECIPIENTS_ERROR_TITLE);
  NSString* message = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SHARING_FETCHING_RECIPIENTS_ERROR);
  [self.alertCoordinator stop];
  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:title
                         message:message];
  __weak __typeof(self) weakSelf = self;
  [self.alertCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^() {
                  [weakSelf stopAlertCoordinator];
                  [weakSelf.delegate
                      passwordSharingCoordinatorDidRemove:weakSelf];
                }
                 style:UIAlertActionStyleCancel];
  [self.alertCoordinator start];
}

- (void)stopFamilyPickerCoordinator {
  [self.familyPickerCoordinator stop];
  self.familyPickerCoordinator.delegate = nil;
  self.familyPickerCoordinator = nil;
}

- (void)stopFamilyPromoCoordinator {
  [self.familyPromoCoordinator stop];
  self.familyPromoCoordinator.delegate = nil;
  self.familyPromoCoordinator = nil;
}

- (void)stopAlertCoordinator {
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

@end
