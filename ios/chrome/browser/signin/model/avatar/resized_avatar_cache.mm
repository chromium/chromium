// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/avatar/resized_avatar_cache.h"

#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/browser/signin/model/system_identity_manager_observer_bridge.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"

@interface ResizedAvatarCache () <SystemIdentityManagerObserving>

// Default avatar at `_expectedSize` size.
@property(nonatomic, strong) UIImage* defaultResizedAvatar;

@end

@implementation ResizedAvatarCache {
  // Size of resized avatar.
  CGSize _expectedSize;
  // Retains resized images. Key is Chrome Identity.
  NSCache<NSString*, UIImage*>* _avatarForGaiaID;
  // List of gaia ids, in `_avatarForGaiaID`. NSCache cannot return keys in the
  // cache. Since NSCache can remove values, it is possible to a gaia id in
  // `_knownGaiaIDs` but not in `_avatarForGaiaID`.
  NSMutableSet<NSString*>* _knownGaiaIDs;
  std::unique_ptr<SystemIdentityManagerObserverBridge>
      _systemIdentityManagerObserverBridge;
}

- (instancetype)initWithIdentityAvatarSize:(IdentityAvatarSize)avatarSize {
  self = [super init];
  if (self) {
    _expectedSize = GetSizeForIdentityAvatarSize(avatarSize);
    _avatarForGaiaID = [[NSCache alloc] init];
    _knownGaiaIDs = [NSMutableSet set];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(notificicationReceived:)
               name:UIApplicationDidReceiveMemoryWarningNotification
             object:nil];
    SystemIdentityManager* systemIdentityManager =
        GetApplicationContext()->GetSystemIdentityManager();
    _systemIdentityManagerObserverBridge =
        std::make_unique<SystemIdentityManagerObserverBridge>(
            systemIdentityManager, self);
  }
  return self;
}

- (UIImage*)resizedAvatarForIdentity:(id<SystemIdentity>)identity {
  CHECK(identity, base::NotFatalUntil::M150);

  NSString* gaiaIDString = identity.gaiaId.ToNSString();
  UIImage* avatar = [_avatarForGaiaID objectForKey:gaiaIDString];
  if (avatar) {
    return avatar;
  }

  SystemIdentityManager* system_identity_manager =
      GetApplicationContext()->GetSystemIdentityManager();
  avatar = system_identity_manager->GetCachedAvatarForIdentity(identity);
  if (!avatar) {
    // No cached image, trigger a fetch, which will notify all observers.
    system_identity_manager->FetchAvatarForIdentity(identity);
    return self.defaultResizedAvatar;
  }

  // Resize the profile image if it is not of the expected size.
  if (!CGSizeEqualToSize(avatar.size, _expectedSize)) {
    avatar = ResizeImage(avatar, _expectedSize, ProjectionMode::kAspectFit);
  }
  [_avatarForGaiaID setObject:avatar forKey:gaiaIDString];
  [_knownGaiaIDs addObject:gaiaIDString];
  return avatar;
}

- (void)notificicationReceived:(NSNotification*)notification {
  if ([notification.name
          isEqual:UIApplicationDidReceiveMemoryWarningNotification]) {
    self.defaultResizedAvatar = nil;
  }
}

#pragma mark - Properties

- (UIImage*)defaultResizedAvatar {
  if (!_defaultResizedAvatar) {
    UIImage* image = ios::provider::GetSigninDefaultAvatar();
    _defaultResizedAvatar =
        ResizeImage(image, _expectedSize, ProjectionMode::kAspectFit);
  }
  return _defaultResizedAvatar;
}

#pragma mark - SystemIdentityManagerObserving

- (void)onIdentityListChanged {
  NSMutableSet<NSString*>* gaiaIDsToRemove = [_knownGaiaIDs mutableCopy];
  SystemIdentityManager* system_identity_manager =
      GetApplicationContext()->GetSystemIdentityManager();
  SystemIdentityManager::IdentityIteratorCallback callback =
      base::BindRepeating(
          [](NSMutableSet* gaiaIDsToRemove, id<SystemIdentity> identity) {
            [gaiaIDsToRemove removeObject:identity.gaiaId.ToNSString()];
            return SystemIdentityManager::IteratorResult::kContinueIteration;
          },
          gaiaIDsToRemove);
  system_identity_manager->IterateOverIdentities(callback);
  // Clear gaia ids that don't exist anymore.
  for (NSString* gaiaIDString in gaiaIDsToRemove) {
    [_avatarForGaiaID removeObjectForKey:gaiaIDString];
    [_knownGaiaIDs removeObject:gaiaIDString];
  }
}

- (void)onIdentityUpdated:(id<SystemIdentity>)identity {
  // Forget about the current cached avatar. It will be recomputed automatically
  // on the next request.
  NSString* gaiaIDString = identity.gaiaId.ToNSString();
  [_avatarForGaiaID removeObjectForKey:gaiaIDString];
  [_knownGaiaIDs removeObject:gaiaIDString];
}

@end
