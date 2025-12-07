// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/model/avatar_provider.h"

#import "ios/chrome/browser/signin/model/constants.h"
#import "ios/chrome/browser/signin/model/resized_avatar_cache.h"

namespace signin {

UIImage* AvatarProvider::GetIdentityAvatar(id<SystemIdentity> identity,
                                           IdentityAvatarSize avatar_size) {
  CHECK(identity, base::NotFatalUntil::M151);
  ResizedAvatarCache* avatar_cache =
      GetAvatarCacheForIdentityAvatarSize(avatar_size);
  CHECK(avatar_cache, base::NotFatalUntil::M151);
  return [avatar_cache resizedAvatarForIdentity:identity];
}

ResizedAvatarCache* AvatarProvider::GetAvatarCacheForIdentityAvatarSize(
    IdentityAvatarSize avatar_size) {
  ResizedAvatarCache* __strong* avatar_cache = nil;
  switch (avatar_size) {
    case IdentityAvatarSize::TableViewIcon:
      avatar_cache = &default_table_view_avatar_cache_;
      break;
    case IdentityAvatarSize::SmallSize:
      avatar_cache = &small_size_avatar_cache_;
      break;
    case IdentityAvatarSize::Regular:
      avatar_cache = &regular_avatar_cache_;
      break;
    case IdentityAvatarSize::Large:
      avatar_cache = &large_avatar_cache_;
      break;
  }
  CHECK(avatar_cache, base::NotFatalUntil::M151);
  if (!*avatar_cache) {
    *avatar_cache =
        [[ResizedAvatarCache alloc] initWithIdentityAvatarSize:avatar_size];
  }
  return *avatar_cache;
}

}  // end namespace signin
