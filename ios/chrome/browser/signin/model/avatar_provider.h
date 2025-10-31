// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_AVATAR_PROVIDER_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_AVATAR_PROVIDER_H_

#import <UIKit/UIKit.h>

enum class IdentityAvatarSize;
@class ResizedAvatarCache;
@protocol SystemIdentity;

namespace signin {

// Contains cache for the avatar for identeties on device, and the default
// avatar, at various size. The cache listen for memory constrain and empty
// itself if needed.
class AvatarProvider {
 public:
  // Returns the identity avatar. If the avatar is not available, it is fetched
  // in background (a notification will be received when it will be available),
  // and the default avatar is returned (see
  // `AccountProfileMapper::Observer::OnIdentityInProfileUpdated()`).
  UIImage* GetIdentityAvatar(id<SystemIdentity> identity,
                             IdentityAvatarSize size);

 private:
  // Returns a ResizedAvatarCache based on `avatar_size`.
  ResizedAvatarCache* GetAvatarCacheForIdentityAvatarSize(
      IdentityAvatarSize avatar_size);

  // ResizedAvatarCache for IdentityAvatarSize::TableViewIcon.
  ResizedAvatarCache* default_table_view_avatar_cache_;
  // ResizedAvatarCache for IdentityAvatarSize::SmallSize.
  ResizedAvatarCache* small_size_avatar_cache_;
  // ResizedAvatarCache for IdentityAvatarSize::Regular.
  ResizedAvatarCache* regular_avatar_cache_;
  // ResizedAvatarCache for IdentityAvatarSize::Large.
  ResizedAvatarCache* large_avatar_cache_;
};

}  // end namespace signin

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_AVATAR_PROVIDER_H_
