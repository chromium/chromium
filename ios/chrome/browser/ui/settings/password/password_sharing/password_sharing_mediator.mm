// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_mediator.h"

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/password_form.h"
#import "components/password_manager/core/browser/sharing/password_sender_service.h"
#import "components/password_manager/core/browser/sharing/recipients_fetcher.h"
#import "components/password_manager/core/browser/sharing/recipients_fetcher_impl.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/app/tests_hook.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "ios/chrome/common/channel_info.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

using password_manager::FetchFamilyMembersRequestStatus;
using password_manager::RecipientInfo;
using password_manager::RecipientsFetcher;

std::unique_ptr<RecipientsFetcher> CreateRecipientsFetcher(
    scoped_refptr<network::SharedURLLoaderFactory> sharedURLLoaderFactory,
    signin::IdentityManager* identityManager) {
  std::unique_ptr<RecipientsFetcher> test_recipients_fetcher =
      tests_hook::GetOverriddenRecipientsFetcher();
  if (test_recipients_fetcher) {
    return test_recipients_fetcher;
  }
  return std::make_unique<password_manager::RecipientsFetcherImpl>(
      GetChannel(), sharedURLLoaderFactory, identityManager);
}

}  // namespace

@interface PasswordSharingMediator () {
  // Fetches information about the potential sharing recipients of the user.
  std::unique_ptr<RecipientsFetcher> _recipientsFetcher;

  // Sends passwords to specified recipients.
  raw_ptr<password_manager::PasswordSenderService> _passwordSenderService;

  // Service providing a view on user's saved passwords.
  raw_ptr<password_manager::SavedPasswordsPresenter> _savedPasswordsPresenter;
}

@property(nonatomic, weak) id<PasswordSharingMediatorDelegate> delegate;

@end

@implementation PasswordSharingMediator

- (instancetype)initWithDelegate:(id<PasswordSharingMediatorDelegate>)delegate
          sharedURLLoaderFactory:
              (scoped_refptr<network::SharedURLLoaderFactory>)
                  sharedURLLoaderFactory
                 identityManager:(signin::IdentityManager*)identityManager
         savedPasswordsPresenter:
             (password_manager::SavedPasswordsPresenter*)savedPasswordsPresenter
           passwordSenderService:
               (password_manager::PasswordSenderService*)passwordSenderService {
  self = [super init];
  if (self) {
    _delegate = delegate;
    _recipientsFetcher =
        CreateRecipientsFetcher(sharedURLLoaderFactory, identityManager);
    _passwordSenderService = passwordSenderService;
    _savedPasswordsPresenter = savedPasswordsPresenter;

    __weak __typeof__(self) weakSelf = self;
    _recipientsFetcher->FetchFamilyMembers(base::BindOnce(
        ^(std::vector<password_manager::RecipientInfo> familyMembers,
          FetchFamilyMembersRequestStatus status) {
          [weakSelf onFetchFamilyMembers:familyMembers withStatus:status];
        }));
  }
  return self;
}

- (void)sendSelectedCredentialToSelectedRecipients {
  std::vector<password_manager::PasswordForm> passwords =
      _savedPasswordsPresenter->GetCorrespondingPasswordForms(
          self.selectedCredential);
  for (RecipientInfoForIOSDisplay* recipient in self.selectedRecipients) {
    _passwordSenderService->SendPasswords(
        passwords, {.user_id = base::SysNSStringToUTF8(recipient.userID),
                    .public_key = recipient.publicKey});
  }
}

#pragma mark - Private methods

- (void)onFetchFamilyMembers:(std::vector<RecipientInfo>)familyMembers
                  withStatus:(FetchFamilyMembersRequestStatus)status {
  NSMutableArray<RecipientInfoForIOSDisplay*>* familyMembersForIOSDisplay =
      [NSMutableArray array];
  for (const RecipientInfo& familyMember : familyMembers) {
    [familyMembersForIOSDisplay
        addObject:[[RecipientInfoForIOSDisplay alloc]
                      initWithRecipientInfo:familyMember]];
  }
  [self.delegate onFetchFamilyMembers:familyMembersForIOSDisplay
                           withStatus:status];
}

@end
