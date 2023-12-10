// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/resized_avatar_cache.h"

#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/signin_util.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_api.h"

@interface ResizedAvatarCache ()

// Size of resized avatar.
@property(nonatomic, assign) CGSize expectedSize;
// Default avatar at `self.expectedSize` size.
@property(nonatomic, strong) UIImage* defaultResizedAvatar;
// Retains resized images. Key is Chrome Identity.
@property(nonatomic, strong)
    NSCache<id<SystemIdentity>, UIImage*>* resizedImages;
// Holds weak references to the cached avatar image from the
// ChromeIdentityService. Key is Chrome Identity.
@property(nonatomic, strong)
    NSMapTable<id<SystemIdentity>, UIImage*>* originalImages;

@end

@implementation ResizedAvatarCache

- (instancetype)initWithSize:(CGSize)size {
  self = [super init];
  if (self) {
    _expectedSize = size;
    _resizedImages = [[NSCache alloc] init];
    _originalImages = [NSMapTable strongToWeakObjectsMapTable];
    [[NSNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(notificicationReceived:)
               name:UIApplicationDidReceiveMemoryWarningNotification
             object:nil];
  }
  return self;
}

- (instancetype)initWithIdentityAvatarSize:(IdentityAvatarSize)avatarSize {
  CGSize size = GetSizeForIdentityAvatarSize(avatarSize);
  return [self initWithSize:size];
}

- (UIImage*)resizedAvatarForIdentity:(id<SystemIdentity>)identity {
  SystemIdentityManager* system_identity_manager =
      GetApplicationContext()->GetSystemIdentityManager();

  UIImage* image =
      system_identity_manager->GetCachedAvatarForIdentity(identity);
  if (!image) {
    // No cached image, trigger a fetch, which will notify all observers.
    system_identity_manager->FetchAvatarForIdentity(identity);
    return self.defaultResizedAvatar;
  }

  // If the currently used image has already been resized, use it.
  if ([_resizedImages objectForKey:identity] &&
      [_originalImages objectForKey:identity] == image)
    return [_resizedImages objectForKey:identity];

  [_originalImages setObject:image forKey:identity];

  // Resize the profile image if it is not of the expected size.
  if (!CGSizeEqualToSize(image.size, self.expectedSize)) {
    image = ResizeImage(image, self.expectedSize, ProjectionMode::kAspectFit);
  }
  [_resizedImages setObject:image forKey:identity];
  return image;
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
        ResizeImage(image, self.expectedSize, ProjectionMode::kAspectFit);
  }
  return _defaultResizedAvatar;
}

@end
