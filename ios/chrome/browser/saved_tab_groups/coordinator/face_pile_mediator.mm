// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/saved_tab_groups/coordinator/face_pile_mediator.h"

#import "base/scoped_observation.h"
#import "base/strings/sys_string_conversions.h"
#import "components/data_sharing/public/data_sharing_service.h"
#import "components/signin/public/identity_manager/account_info.h"
#import "components/signin/public/identity_manager/identity_manager.h"
#import "ios/chrome/browser/data_sharing/model/data_sharing_service_observer_bridge.h"
#import "ios/chrome/browser/saved_tab_groups/coordinator/face_pile_configuration.h"
#import "ios/chrome/browser/saved_tab_groups/ui/face_pile_consumer.h"
#import "ios/chrome/browser/share_kit/model/share_kit_avatar_configuration.h"
#import "ios/chrome/browser/share_kit/model/share_kit_service.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

using ScopedDataSharingSyncObservation =
    base::ScopedObservation<data_sharing::DataSharingService,
                            data_sharing::DataSharingService::Observer>;

@interface FacePileMediator () <DataSharingServiceObserverDelegate>
@end

@implementation FacePileMediator {
  // Services required by the mediator.
  raw_ptr<data_sharing::DataSharingService> _dataSharingService;
  raw_ptr<ShareKitService, DanglingUntriaged> _shareKitService;
  raw_ptr<signin::IdentityManager, DanglingUntriaged> _identityManager;

  // Settings specific to this mediator instance.
  FacePileConfiguration* _configuration;

  // Handles observing changes from services.
  std::unique_ptr<DataSharingServiceObserverBridge> _dataSharingServiceObserver;
  std::unique_ptr<ScopedDataSharingSyncObservation>
      _scopedDataSharingServiceObservation;
}

- (instancetype)
    initWithConfiguration:(FacePileConfiguration*)configuration
       dataSharingService:(data_sharing::DataSharingService*)dataSharingService
          shareKitService:(ShareKitService*)shareKitService
          identityManager:(signin::IdentityManager*)identityManager {
  self = [super init];
  if (self) {
    _configuration = configuration;
    _dataSharingService = dataSharingService;
    _shareKitService = shareKitService;
    _identityManager = identityManager;
    CHECK(_configuration);
    CHECK(_dataSharingService);
    CHECK(_shareKitService);
    CHECK(_identityManager);

    _dataSharingServiceObserver =
        std::make_unique<DataSharingServiceObserverBridge>(self);
    _scopedDataSharingServiceObservation =
        std::make_unique<ScopedDataSharingSyncObservation>(
            _dataSharingServiceObserver.get());
    _scopedDataSharingServiceObservation->Observe(_dataSharingService);
  }
  return self;
}

- (void)setConsumer:(id<FacePileConsumer>)consumer {
  _consumer = consumer;

  [self updateConsumer];
}

- (void)disconnect {
  _scopedDataSharingServiceObservation.reset();
  _dataSharingServiceObserver.reset();
}

#pragma mark DataSharingServiceObserverDelegate

- (void)dataSharingServiceInitialized {
  [self updateConsumer];
}

- (void)dataSharingServiceDidChangeGroup:
            (const data_sharing::GroupData&)groupData
                                  atTime:(base::Time)eventTime {
  if (groupData.group_token.group_id != _configuration.groupID) {
    return;
  }

  [self updateConsumer];
}

- (void)dataSharingServiceDestroyed {
  _scopedDataSharingServiceObservation.reset();
  _dataSharingServiceObserver.reset();
  _dataSharingService = nullptr;
}

#pragma mark - Private

// Updates the consumer for the current group data.
- (void)updateConsumer {
  if (!_consumer || !_dataSharingService->IsGroupDataModelLoaded()) {
    return;
  }

  [self.consumer setSharedButtonWhenEmpty:_configuration.showsEmptyState];

  CoreAccountInfo account =
      _identityManager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  if (account.IsEmpty()) {
    return;
  }

  std::optional<data_sharing::GroupData> groupData =
      _dataSharingService->ReadGroup(_configuration.groupID);
  if (!groupData) {
    return;
  }

  // Configure the consumer.
  [self.consumer setAvatarSize:_configuration.avatarSize];
  [self.consumer setFacePileBackgroundColor:_configuration.backgroundColor];

  ShareKitAvatarConfiguration* ownerConfig;
  ShareKitAvatarConfiguration* secondConfig;
  ShareKitAvatarConfiguration* thirdConfig;
  BOOL foundSelf = NO;
  // Determine if a third avatar is needed for 3 members.
  BOOL needThirdFace = groupData->members.size() == 3;

  for (data_sharing::GroupMember member : groupData->members) {
    // Optimization: Stop if all necessary configs are found.
    if (ownerConfig && secondConfig && foundSelf &&
        (!needThirdFace || thirdConfig)) {
      break;
    }

    BOOL isOwner = (member.role == data_sharing::MemberRole::kOwner);
    BOOL isSelf = (member.gaia_id == account.gaia);
    foundSelf = foundSelf || isSelf;

    if (!isOwner && !isSelf && secondConfig &&
        (!needThirdFace || thirdConfig)) {
      // Optimization: Skip non-owner/non-self if second/third configs are set.
      continue;
    }

    // Create avatar config for current member.
    ShareKitAvatarConfiguration* config =
        [[ShareKitAvatarConfiguration alloc] init];
    config.avatarUrl = net::NSURLWithGURL(member.avatar_url);
    config.displayName = base::SysUTF8ToNSString(member.display_name);
    config.avatarSize =
        CGSizeMake(_configuration.avatarSize, _configuration.avatarSize);

    // Assign config to appropriate avatar slot.
    if (isOwner) {
      ownerConfig = config;
    } else if (isSelf) {
      // Shift second to third if needed, then assign self to second.
      if (needThirdFace) {
        thirdConfig = secondConfig;
      }
      secondConfig = config;
    } else if (!secondConfig) {
      secondConfig = config;
    } else if (needThirdFace && !thirdConfig) {
      thirdConfig = config;
    }
  }
  CHECK(ownerConfig);

  NSMutableArray<id<ShareKitAvatarPrimitive>>* faces = [NSMutableArray array];
  [faces addObject:_shareKitService->AvatarImage(ownerConfig)];
  if (secondConfig) {
    [faces addObject:_shareKitService->AvatarImage(secondConfig)];
  }
  if (thirdConfig) {
    [faces addObject:_shareKitService->AvatarImage(thirdConfig)];
  }

  [self.consumer updateWithFaces:faces totalNumber:groupData->members.size()];
}

@end
