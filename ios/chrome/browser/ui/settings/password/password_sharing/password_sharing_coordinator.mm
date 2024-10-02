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
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
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
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_coordinator.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/sharing_status_coordinator_delegate.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ios/chrome/grit/ios_strings.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"
#import "ui/base/l10n/l10n_util.h"

using password_manager::FetchFamilyMembersRequestStatus;

@interface PasswordSharingCoordinator () <
    FamilyPickerCoordinatorDelegate,
    FamilyPromoCoordinatorDelegate,
    PasswordPickerCoordinatorDelegate,
    PasswordSharingMediatorDelegate,
    SharingStatusCoordinatorDelegate> {
  // The credentials for the password group from which the sharing originated.
  std::vector<password_manager::CredentialUIEntry> _credentials;

  // Service providing a view on user's saved passwords.
  raw_ptr<password_manager::SavedPasswordsPresenter> _savedPasswordsPresenter;
}

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordSharingMediator* mediator;

// Coordinator for family picker.
@property(nonatomic, strong) FamilyPickerCoordinator* familyPickerCoordinator;

// Coordinator for family promo view.
@property(nonatomic, strong) FamilyPromoCoordinator* familyPromoCoordinator;

// Coordinator for displaying fetching recipients error.
@property(nonatomic, strong)
    AlertCoordinator* fetchingRecipientsErrorCoordinator;

// Coordinator for password picker.
@property(nonatomic, strong)
    PasswordPickerCoordinator* passwordPickerCoordinator;

// Coordinator for sharing status view.
@property(nonatomic, strong) SharingStatusCoordinator* sharingStatusCoordinator;

// Information about potential password sharing recipients of the user.
@property(nonatomic, strong) NSArray<RecipientInfoForIOSDisplay*>* recipients;

@end

@implementation PasswordSharingCoordinator {
  // Contains status of fetching family members for the user.
  FetchFamilyMembersRequestStatus _fetchFamilyMembersRequestStatus;
}

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

  ProfileIOS* profile = self.browser->GetProfile();
  self.mediator = [[PasswordSharingMediator alloc]
             initWithDelegate:self
       sharedURLLoaderFactory:profile->GetSharedURLLoaderFactory()
              identityManager:IdentityManagerFactory::GetForProfile(profile)
      savedPasswordsPresenter:_savedPasswordsPresenter
        passwordSenderService:IOSChromePasswordSenderServiceFactory::
                                  GetForProfile(profile)];

  // With more than 1 credential an additional UI will be presented to select
  // which one should be shared.
  if (_credentials.size() == 1) {
    self.mediator.selectedCredential = _credentials[0];
  }
}

- (void)stop {
  self.mediator = nil;

  [self stopFamilyPickerCoordinator];
  [self stopFamilyPromoCoordinator];
  [self stopFetchingRecipientsErrorCoordinator];
  [self stopPasswordPickerCoordinator];
  [self stopSharingStatusCoordinator];
}

#pragma mark - Public

- (void)showFirstStep {
  switch (_fetchFamilyMembersRequestStatus) {
    case FetchFamilyMembersRequestStatus::kSuccess: {
      if (_credentials.size() == 1) {
        [self startFamilyPickerCoordinator];
      } else {
        [self startPasswordPickerCoordinator];
      }
      break;
    }
    case FetchFamilyMembersRequestStatus::kNoFamily: {
      [self startFamilyPromoCoordinatorWithType:FamilyPromoType::
                                                    kUserNotInFamilyGroup];
      break;
    }
    case FetchFamilyMembersRequestStatus::kNoOtherFamilyMembers: {
      [self startFamilyPromoCoordinatorWithType:
                FamilyPromoType::kUserWithNoOtherFamilyMembers];
      break;
    }
    default: {
      [self startFetchingRecipientsErrorCoordinator];
      break;
    }
  }
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

- (void)passwordPickerWithNavigationController:
            (UINavigationController*)navigationController
                           didSelectCredential:
                               (const password_manager::CredentialUIEntry&)
                                   credential {
  self.mediator.selectedCredential = credential;
  [self startFamilyPickerCoordinatorWithNavigationController:
            navigationController];
}

#pragma mark - PasswordSharingMediatorDelegate

- (void)onFetchFamilyMembers:
            (NSArray<RecipientInfoForIOSDisplay*>*)familyMembers
                  withStatus:(const FetchFamilyMembersRequestStatus&)status {
  self.recipients = familyMembers;
  _fetchFamilyMembersRequestStatus = status;
  [self.delegate shareDataFetched];
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
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                      recipients:self.recipients];
  self.familyPickerCoordinator.delegate = self;
  [self.familyPickerCoordinator start];
}

- (void)startFamilyPickerCoordinatorWithNavigationController:
    (UINavigationController*)navigationController {
  [self.familyPickerCoordinator stop];
  self.familyPickerCoordinator = [[FamilyPickerCoordinator alloc]
      initWithBaseNavigationController:navigationController
                               browser:self.browser
                            recipients:self.recipients];
  self.familyPickerCoordinator.delegate = self;
  [self.familyPickerCoordinator start];
}

- (void)startPasswordPickerCoordinator {
  [self.passwordPickerCoordinator stop];
  self.passwordPickerCoordinator = [[PasswordPickerCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                     credentials:_credentials];
  self.passwordPickerCoordinator.delegate = self;
  [self.passwordPickerCoordinator start];
}

- (void)startFamilyPromoCoordinatorWithType:(FamilyPromoType)type {
  [self.familyPromoCoordinator stop];
  self.familyPromoCoordinator = [[FamilyPromoCoordinator alloc]
      initWithFamilyPromoType:type
           baseViewController:self.baseViewController
                      browser:self.browser];
  self.familyPromoCoordinator.delegate = self;
  [self.familyPromoCoordinator start];
}

- (void)startSharingStatusCoordinator {
  [self.sharingStatusCoordinator stop];
  password_manager::CredentialUIEntry credential =
      self.mediator.selectedCredential;
  self.sharingStatusCoordinator = [[SharingStatusCoordinator alloc]
      initWithBaseViewController:self.baseViewController
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

- (void)startFetchingRecipientsErrorCoordinator {
  NSString* title = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SHARING_FETCHING_RECIPIENTS_ERROR_TITLE);
  NSString* message = l10n_util::GetNSString(
      IDS_IOS_PASSWORD_SHARING_FETCHING_RECIPIENTS_ERROR);
  [self.fetchingRecipientsErrorCoordinator stop];
  self.fetchingRecipientsErrorCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                         browser:self.browser
                           title:title
                         message:message];
  __weak __typeof(self) weakSelf = self;
  [self.fetchingRecipientsErrorCoordinator
      addItemWithTitle:l10n_util::GetNSString(IDS_CANCEL)
                action:^() {
                  [weakSelf stopFetchingRecipientsErrorCoordinator];
                  [weakSelf.delegate
                      passwordSharingCoordinatorDidRemove:weakSelf];
                }
                 style:UIAlertActionStyleCancel];
  [self.fetchingRecipientsErrorCoordinator start];
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

- (void)stopFetchingRecipientsErrorCoordinator {
  [self.fetchingRecipientsErrorCoordinator stop];
  self.fetchingRecipientsErrorCoordinator = nil;
}

- (void)stopSharingStatusCoordinator {
  [self.sharingStatusCoordinator stop];
  self.sharingStatusCoordinator.delegate = nil;
  self.sharingStatusCoordinator = nil;
}

@end
