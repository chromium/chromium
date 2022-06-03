// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/session/crw_session_storage.h"

#include "base/metrics/histogram_functions.h"
#include "ios/web/common/features.h"
#import "ios/web/navigation/nscoder_util.h"
#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"
#import "ios/web/public/session/serializable_user_data_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Serialization keys used in NSCoding functions.
NSString* const kCertificatePolicyCacheStorageKey =
    @"certificatePolicyCacheStorage";
NSString* const kCertificatePolicyCacheStorageDeprecatedKey =
    @"certificatePolicyManager";
NSString* const kItemStoragesKey = @"entries";
NSString* const kHasOpenerKey = @"openedByDOM";
NSString* const kLastCommittedItemIndexKey = @"lastCommittedItemIndex";
NSString* const kUserAgentKey = @"userAgentKey";

// Deprecated, used for backward compatibility.
// TODO(crbug.com/708795): Remove this key.
NSString* const kLastCommittedItemIndexDeprecatedKey =
    @"currentNavigationIndex";

}

@interface CRWSessionStorage () {
  // Backing object for property of same name.
  std::unique_ptr<web::SerializableUserData> _userData;
}

@end

@implementation CRWSessionStorage

#pragma mark - Accessors

- (web::SerializableUserData*)userData {
  return _userData.get();
}

- (void)setSerializableUserData:
    (std::unique_ptr<web::SerializableUserData>)userData {
  _userData = std::move(userData);
}

#pragma mark - NSCoding

- (instancetype)initWithCoder:(nonnull NSCoder*)decoder {
  self = [super init];
  if (self) {
    _hasOpener = [decoder decodeBoolForKey:kHasOpenerKey];

    if ([decoder containsValueForKey:kLastCommittedItemIndexKey]) {
      _lastCommittedItemIndex =
          [decoder decodeIntForKey:kLastCommittedItemIndexKey];
    } else {
      // Backward compatibility.
      _lastCommittedItemIndex =
          [decoder decodeIntForKey:kLastCommittedItemIndexDeprecatedKey];
    }

    _itemStorages = [[NSMutableArray alloc]
        initWithArray:[decoder decodeObjectForKey:kItemStoragesKey]];
    // Prior to M34, 0 was used as "no index" instead of -1; adjust for that.
    if (!_itemStorages.count)
      _lastCommittedItemIndex = -1;
    _certPolicyCacheStorage =
        [decoder decodeObjectForKey:kCertificatePolicyCacheStorageKey];
    if (!_certPolicyCacheStorage) {
      // If the cert policy cache was not found, attempt to decode using the
      // deprecated serialization key.
      // TODO(crbug.com/661633): Remove this deprecated key once we remove
      // support for legacy class conversions.
      _certPolicyCacheStorage = [decoder
          decodeObjectForKey:kCertificatePolicyCacheStorageDeprecatedKey];
    }
    _userData = web::SerializableUserData::Create();
    _userData->Decode(decoder);
    if ([decoder containsValueForKey:kUserAgentKey]) {
      std::string userAgentDescription =
          web::nscoder_util::DecodeString(decoder, kUserAgentKey);
      _userAgentType =
          web::GetUserAgentTypeWithDescription(userAgentDescription);
    } else {
      // Prior to M85, the UserAgent wasn't stored.
      if (web::features::UseWebClientDefaultUserAgent()) {
        _userAgentType = web::UserAgentType::AUTOMATIC;
      } else {
        _userAgentType = web::UserAgentType::MOBILE;
      }
    }
  }
  return self;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeBool:self.hasOpener forKey:kHasOpenerKey];
  [coder encodeInt:self.lastCommittedItemIndex
            forKey:kLastCommittedItemIndexKey];
  [coder encodeObject:self.itemStorages forKey:kItemStoragesKey];
  size_t previous_cert_policy_bytes = web::GetCertPolicyBytesEncoded();
  [coder encodeObject:self.certPolicyCacheStorage
               forKey:kCertificatePolicyCacheStorageKey];
  base::UmaHistogramCounts100000(
      "Session.WebStates.SerializedCertPolicyCacheSize",
      web::GetCertPolicyBytesEncoded() - previous_cert_policy_bytes / 1024);
  if (_userData)
    _userData->Encode(coder);
  web::UserAgentType userAgentType = _userAgentType;
  if (userAgentType == web::UserAgentType::AUTOMATIC &&
      !web::features::UseWebClientDefaultUserAgent()) {
    userAgentType = web::UserAgentType::MOBILE;
  }
  web::nscoder_util::EncodeString(
      coder, kUserAgentKey, web::GetUserAgentTypeDescription(userAgentType));
}

@end
