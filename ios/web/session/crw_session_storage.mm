// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/session/crw_session_storage.h"

#import "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "ios/web/common/features.h"
#import "ios/web/navigation/nscoder_util.h"
#import "ios/web/navigation/serializable_user_data_manager_impl.h"
#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"

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
NSString* const kStableIdentifierKey = @"stableIdentifier";

// Deprecated, used for backward compatibility.
// TODO(crbug.com/1278308): Remove this key.
NSString* const kLastCommittedItemIndexDeprecatedKey =
    @"currentNavigationIndex";

// Deprecated, used for backward compatibility for reading the stable
// identifier from the serializable user data as it was stored by the
// external tab helper.
// TODO(crbug.com/1278308): Remove this key.
NSString* const kTabIdKey = @"TabId";
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
      // TODO(crbug.com/1278308): Remove this deprecated key once we remove
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
      // TODO(crbug.com/1278308): Remove this deprecated logic when we
      // remove support for loading legacy sessions.
      _userAgentType = web::UserAgentType::AUTOMATIC;
    }

    _stableIdentifier = [decoder decodeObjectForKey:kStableIdentifierKey];
    if (!_stableIdentifier.length) {
      // Before M99, the stable identifier was managed by a tab helper and
      // saved as part of the serializable user data. To support migration
      // of pre M99 session, read the data from there if not found.
      //
      // TODO(crbug.com/1278306): remove this cast when SerializableUserData
      // has been removed and a simpler alternative has been put in place.
      // For the moment the cast is safe, since SerializableUserData only has
      // one sub-class SerializableUserDataImpl.
      web::SerializableUserDataImpl* userDataImpl =
          static_cast<web::SerializableUserDataImpl*>(_userData.get());
      NSDictionary<NSString*, id<NSCoding>>* data = userDataImpl->data();

      // If "TabId" is set, clear it and initialise the `stableIdentifier`
      // from it (if it is a NSString and non empty, otherwise a new value
      // will be created below).
      id<NSCoding> tabIdValue = [data objectForKey:kTabIdKey];
      if (tabIdValue) {
        NSMutableDictionary<NSString*, id<NSCoding>>* mutableData =
            [data mutableCopy];
        [mutableData removeObjectForKey:kTabIdKey];

        _userData = base::WrapUnique(
            new web::SerializableUserDataImpl([mutableData copy]));

        // If the value is not an NSString or is empty, a random identifier
        // will be generated below.
        _stableIdentifier = base::mac::ObjCCast<NSString>(tabIdValue);
      }
    }

    // If no stable identifier was read, generate a new one (this simplify
    // WebState session restoration code as it can assume that the property
    // is never nil).
    if (!_stableIdentifier.length) {
      _stableIdentifier = [[NSUUID UUID] UUIDString];
    }

    // Force conversion to NSString if `_stableIdentifier` happens to be a
    // NSMutableString (to prevent this value from being mutated).
    _stableIdentifier = [_stableIdentifier copy];
    DCHECK(_stableIdentifier.length);
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
  web::nscoder_util::EncodeString(
      coder, kUserAgentKey, web::GetUserAgentTypeDescription(userAgentType));
  [coder encodeObject:_stableIdentifier forKey:kStableIdentifierKey];
}

@end
