// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_mediator.h"

#import "components/password_manager/core/browser/sharing/recipients_fetcher.h"
#import "components/password_manager/core/browser/sharing/recipients_fetcher_impl.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/password_sharing_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_sharing/recipient_info.h"
#import "ios/chrome/common/channel_info.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

using password_manager::FetchFamilyMembersRequestStatus;

@interface PasswordSharingMediator () {
  std::unique_ptr<password_manager::RecipientsFetcher> _recipientsFetcher;
}

@property(nonatomic, weak) id<PasswordSharingMediatorDelegate> delegate;

@end

@implementation PasswordSharingMediator

- (instancetype)initWithDelegate:(id<PasswordSharingMediatorDelegate>)delegate
          SharedURLLoaderFactory:
              (scoped_refptr<network::SharedURLLoaderFactory>)
                  sharedURLLoaderFactory
                 identityManager:(signin::IdentityManager*)identityManager {
  self = [super init];
  if (self) {
    _delegate = delegate;
    _recipientsFetcher =
        std::make_unique<password_manager::RecipientsFetcherImpl>(
            GetChannel(), sharedURLLoaderFactory, identityManager);

    __weak __typeof__(self) weakSelf = self;
    _recipientsFetcher->FetchFamilyMembers(base::BindOnce(
        ^(std::vector<password_manager::RecipientInfo> familyMembers,
          FetchFamilyMembersRequestStatus status) {
          [weakSelf onFetchFamilyMembers:familyMembers withStatus:status];
        }));
  }
  return self;
}

#pragma mark - Private methods

- (void)onFetchFamilyMembers:
            (std::vector<password_manager::RecipientInfo>)familyMembers
                  withStatus:(FetchFamilyMembersRequestStatus)status {
  NSMutableArray<RecipientInfo*>* recipients = [NSMutableArray array];
  for (const password_manager::RecipientInfo& familyMember : familyMembers) {
    [recipients
        addObject:[[RecipientInfo alloc] initWithRecipientInfo:familyMember]];
  }
  [self.delegate onFetchFamilyMembers:recipients withStatus:status];
}

@end
