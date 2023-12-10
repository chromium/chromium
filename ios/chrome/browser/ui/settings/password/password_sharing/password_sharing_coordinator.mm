// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_coordinator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/browser/sharing/recipients_fetcher.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/passwords/model/ios_chrome_password_sender_service_factory.h"
#import "ios/chrome/browser/shared/coordinator/alert/alert_coordinator.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/shared/ui/table_view/table_view_utils.h"
#import "ios/chrome/browser/signin/model/identity_manager_factory.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_picker_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/family_promo_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_picker_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_constants.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_view_controller.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_coordinator_delegate.h"
#import "ios/chrome/grit/ios_strings.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util.h"

using password_manager::FetchFamilyMembersRequestStatus;

@interface PasswordSharingCoordinator () <FamilyPickerCoordinatorDelegate,
                                          FamilyPromoCoordinatorDelegate,
                                          PasswordPickerCoordinatorDelegate,
                                          PasswordSharingMediatorDelegate,
                                          SharingStatusCoordinatorDelegate> {
  // The credentials for the password group from which the sharing originated.
  std::vector<password_manager::CredentialUIEntry> _credentials;

  // Service providing a view on user's saved passwords.
  raw_ptr<password_manager::SavedPasswordsPresenter> _savedPasswordsPresenter;
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

// Coordinator for sharing status view.
@property(nonatomic, strong) SharingStatusCoordinator* sharingStatusCoordinator;

// Information about potential password sharing recipients of the user.
@property(nonatomic, strong) NSArray<RecipientInfoForIOSDisplay*>* recipients;

@end

@implementation PasswordSharingCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                   credentials:
                       (const std::vector<password_manager::CredentialUIEntry>&)
                           credentials
       savedPasswordsPresenter:
           (password_manager::SavedPasswordsPresenter*)savedPasswordsPresenter {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _credentials = credentials;
    _savedPasswordsPresenter = savedPasswordsPresenter;
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
       sharedURLLoaderFactory:browserState->GetSharedURLLoaderFactory()
              identityManager:IdentityManagerFactory::GetForBrowserState(
                                  browserState)
      savedPasswordsPresenter:_savedPasswordsPresenter
        passwordSenderService:IOSChromePasswordSenderServiceFactory::
                                  GetForBrowserState(browserState)];

  // With more than 1 credential an additional UI will be presented to select
  // which one should be shared.
  if (_credentials.size() == 1) {
    self.mediator.selectedCredential = _credentials[0];
  }
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
  [self stopPasswordPickerCoordinator];
  [self stopSharingStatusCoordinator];
}

#pragma mark - FamilyPickerCoordinatorDelegate

- (void)familyPickerCoordinatorWasDismissed:
    (FamilyPickerCoordinator*)coordinator {
  [self.delegate passwordSharingCoordinatorDidRemove:self];
}

- (void)familyPickerCoordinator:(FamilyPickerCoordinator*)coordinator
            didSelectRecipients:
                (NSArray<RecipientInfoForIOSDisplay*>*)recipients {
  self.mediator.selectedRecipients = recipients;

  if (self.familyPickerCoordinator == coordinator) {
    [self stopFamilyPickerCoordinator];
  }

  [self startSharingStatusCoordinator];
}

- (void)familyPickerCoordinatorNavigatedBack:
    (FamilyPickerCoordinator*)coordinator {
  if (self.familyPickerCoordinator == coordinator) {
    [self stopFamilyPickerCoordinator];
  }
}

#pragma mark - FamilyPromoCoordinatorDelegate

- (void)familyPromoCoordinatorWasDismissed:
    (FamilyPromoCoordinator*)coordinator {
  if (self.familyPromoCoordinator == coordinator) {
    [self stopFamilyPromoCoordinator];
  }

  [self.delegate passwordSharingCoordinatorDidRemove:self];
}

#pragma mark - PasswordPickerCoordinatorDelegate

- (void)passwordPickerCoordinatorWasDismissed:
    (PasswordPickerCoordinator*)coordinator {
  [self.delegate passwordSharingCoordinatorDidRemove:self];
}

- (void)passwordPickerCoordinator:(PasswordPickerCoordinator*)coordinator
              didSelectCredential:
                  (const password_manager::CredentialUIEntry&)credential {
  self.mediator.selectedCredential = credential;
  [self startFamilyPickerCoordinator];
}

#pragma mark - PasswordSharingMediatorDelegate

- (void)onFetchFamilyMembers:
            (NSArray<RecipientInfoForIOSDisplay*>*)familyMembers
                  withStatus:(const FetchFamilyMembersRequestStatus&)status {
  // TODO(crbug.com/1463882): Add EG tests for the whole flow.
  self.recipients = familyMembers;

  switch (status) {
    case FetchFamilyMembersRequestStatus::kSuccess:
      if (_credentials.size() == 1) {
        [self startFamilyPickerCoordinator];
      } else {
        [self startPasswordPickerCoordinator];
      }
      break;
    case FetchFamilyMembersRequestStatus::kNoFamily:
      [self startFamilyPromoCoordinatorWithType:FamilyPromoType::
                                                    kUserNotInFamilyGroup];
      break;
    case FetchFamilyMembersRequestStatus::kNoOtherFamilyMembers:
      [self startFamilyPromoCoordinatorWithType:
                FamilyPromoType::kUserWithNoOtherFamilyMembers];
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

#pragma mark - SharingStatusCoordinatorDelegate

- (void)sharingStatusCoordinatorWasDismissed:
    (SharingStatusCoordinator*)coordinator {
  [self.delegate passwordSharingCoordinatorDidRemove:self];
}

- (void)startPasswordSharing {
  [self.mediator sendSelectedCredentialToSelectedRecipients];
}

#pragma mark - Private

- (void)startFamilyPickerCoordinator {
  [self.familyPickerCoordinator stop];
  self.familyPickerCoordinator = [[FamilyPickerCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:self.browser
                            recipients:self.recipients];
  self.familyPickerCoordinator.delegate = self;
  self.familyPickerCoordinator.shouldNavigateBack = _credentials.size() > 1;
  [self.familyPickerCoordinator start];
}

- (void)startPasswordPickerCoordinator {
  [self.passwordPickerCoordinator stop];
  self.passwordPickerCoordinator = [[PasswordPickerCoordinator alloc]
      initWithBaseNavigationController:self.navigationController
                               browser:self.browser
                           credentials:_credentials];
  self.passwordPickerCoordinator.delegate = self;
  [self.passwordPickerCoordinator start];
}

- (void)startFamilyPromoCoordinatorWithType:(FamilyPromoType)type {
  [self.familyPromoCoordinator stop];
  self.familyPromoCoordinator = [[FamilyPromoCoordinator alloc]
      initWithFamilyPromoType:type
           baseViewController:self.viewController
                      browser:self.browser];
  self.familyPromoCoordinator.delegate = self;
  [self.familyPromoCoordinator start];
}

- (void)startSharingStatusCoordinator {
  [self.sharingStatusCoordinator stop];
  password_manager::CredentialUIEntry credential =
      self.mediator.selectedCredential;
  self.sharingStatusCoordinator = [[SharingStatusCoordinator alloc]
      initWithBaseViewController:self.navigationController
                         browser:self.browser
                      recipients:self.mediator.selectedRecipients
                         website:base::SysUTF8ToNSString(
                                     password_manager::GetShownOrigin(
                                         credential))
                             URL:credential.GetURL()
               changePasswordURL:credential.GetChangePasswordURL()];
  self.sharingStatusCoordinator.delegate = self;
  [self.sharingStatusCoordinator start];
}

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

- (void)stopPasswordPickerCoordinator {
  [self.passwordPickerCoordinator stop];
  self.passwordPickerCoordinator.delegate = nil;
  self.passwordPickerCoordinator = nil;
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

- (void)stopSharingStatusCoordinator {
  [self.sharingStatusCoordinator stop];
  self.sharingStatusCoordinator.delegate = nil;
  self.sharingStatusCoordinator = nil;
}

@end
