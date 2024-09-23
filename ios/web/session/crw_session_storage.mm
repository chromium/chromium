// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/session/crw_session_storage.h"

#import "base/apple/foundation_util.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/time/time.h"
#import "ios/web/common/features.h"
#import "ios/web/navigation/nscoder_util.h"
#import "ios/web/public/session/crw_navigation_item_storage.h"
#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"
#import "ios/web/public/session/crw_session_user_data.h"
#import "ios/web/public/session/proto/metadata.pb.h"
#import "ios/web/public/session/proto/navigation.pb.h"
#import "ios/web/public/session/proto/proto_util.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/web_state_id.h"

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
NSString* const kUniqueIdentifierKey = @"uniqueIdentifier";
NSString* const kSerializedUserDataKey = @"serializedUserData";
NSString* const kLastActiveTimeKey = @"lastActiveTime";
NSString* const kCreationTimeKey = @"creationTime";

// Deprecated, used for backward compatibility.
// TODO(crbug.com/40208116): Remove this key.
NSString* const kLastCommittedItemIndexDeprecatedKey =
    @"currentNavigationIndex";

// Deprecated, used for backward compatibility for reading the stable
// identifier from the serializable user data as it was stored by the
// external tab helper.
// TODO(crbug.com/40208116): Remove this key.
NSString* const kTabIdKey = @"TabId";
}

@implementation CRWSessionStorage

- (instancetype)initWithProto:(const web::proto::WebStateStorage&)storage
             uniqueIdentifier:(web::WebStateID)uniqueIdentifier
             stableIdentifier:(NSString*)stableIdentifier {
  if ((self = [super init])) {
    DCHECK(uniqueIdentifier.valid());
    DCHECK(stableIdentifier.length);
    _uniqueIdentifier = uniqueIdentifier;
    _stableIdentifier = stableIdentifier;

    _hasOpener = storage.has_opener();
    _userAgentType = web::UserAgentTypeFromProto(storage.user_agent());
    _certPolicyCacheStorage = [[CRWSessionCertificatePolicyCacheStorage alloc]
        initWithProto:storage.certs_cache()];

    const web::proto::NavigationStorage& navigationStorage =
        storage.navigation();
    _lastCommittedItemIndex = navigationStorage.last_committed_item_index();
    NSMutableArray<CRWNavigationItemStorage*>* itemStorages =
        [[NSMutableArray alloc]
            initWithCapacity:navigationStorage.items_size()];
    for (const web::proto::NavigationItemStorage& itemStorage :
         navigationStorage.items()) {
      [itemStorages addObject:[[CRWNavigationItemStorage alloc]
                                  initWithProto:itemStorage]];
    }
    _itemStorages = [itemStorages copy];

    const web::proto::WebStateMetadataStorage& metadataStorage =
        storage.metadata();
    _creationTime = web::TimeFromProto(metadataStorage.creation_time());
    _lastActiveTime = web::TimeFromProto(metadataStorage.last_active_time());
  }
  return self;
}

- (void)serializeToProto:(web::proto::WebStateStorage&)storage {
  storage.set_has_opener(_hasOpener);
  storage.set_user_agent(web::UserAgentTypeToProto(_userAgentType));
  [_certPolicyCacheStorage serializeToProto:*storage.mutable_certs_cache()];

  web::proto::NavigationStorage* navigationStorage =
      storage.mutable_navigation();
  navigationStorage->set_last_committed_item_index(_lastCommittedItemIndex);
  for (CRWNavigationItemStorage* itemStorage in _itemStorages) {
    [itemStorage serializeToProto:*navigationStorage->add_items()];
  }

  [self serializeMetadataToProto:*storage.mutable_metadata()];
}

- (void)serializeMetadataToProto:
    (web::proto::WebStateMetadataStorage&)metadata {
  web::SerializeTimeToProto(_creationTime, *metadata.mutable_creation_time());
  web::SerializeTimeToProto(_lastActiveTime,
                            *metadata.mutable_last_active_time());
  metadata.set_navigation_item_count(_itemStorages.count);

  if (_lastCommittedItemIndex >= 0) {
    NSUInteger const activePageIndex =
        static_cast<NSUInteger>(_lastCommittedItemIndex);
    if (activePageIndex < _itemStorages.count) {
      CRWNavigationItemStorage* const activePageItem =
          _itemStorages[activePageIndex];
      web::proto::PageMetadataStorage* pageMetadataStorage =
          metadata.mutable_active_page();
      pageMetadataStorage->set_page_title(
          base::UTF16ToUTF8(activePageItem.title));
      GURL pageURL = activePageItem.virtualURL;
      if (!pageURL.is_valid()) {
        pageURL = activePageItem.URL;
      }
      if (pageURL.is_valid()) {
        pageMetadataStorage->set_page_url(pageURL.spec());
      }
    }
  }
}

#pragma mark - NSObject

- (BOOL)isEqual:(NSObject*)object {
  CRWSessionStorage* other = base::apple::ObjCCast<CRWSessionStorage>(object);

  return [other cr_isEqualSameClass:self];
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

    // A few users are crashing because they have a corrupted session (where
    // _itemStorages contains objects that are not CRWNavigationItemStorage).
    // If this happens, consider that the session has no navigations instead.
    // It will result in a tab with no navigation, which will be dropped. It
    // is better than crashing when trying to convert the session to proto.
    // See https://crbug.com/358616893 for details.
    NSObject* itemStoragesObj = [decoder decodeObjectForKey:kItemStoragesKey];
    if ([itemStoragesObj isKindOfClass:[NSArray class]]) {
      NSArray* itemStorages =
          base::apple::ObjCCastStrict<NSArray>(itemStoragesObj);
      for (NSObject* item in itemStorages) {
        if (![item isKindOfClass:[CRWNavigationItemStorage class]]) {
          itemStorages = nil;
          break;
        }
      }
      _itemStorages =
          [[NSMutableArray alloc] initWithArray:(itemStorages ?: @[])];
    } else {
      _itemStorages = [[NSMutableArray alloc] init];
    }

    // Prior to M34, 0 was used as "no index" instead of -1; adjust for that.
    if (!_itemStorages.count)
      _lastCommittedItemIndex = -1;

    // In a respin of M-117, a data corruption was introduced that may cause
    // last_committed_item_index to be out-of-bound. Force the value back in
    // bound to prevent a crash trying to load the session.
    if (_lastCommittedItemIndex != NSNotFound) {
      const int items_size = static_cast<int>(_itemStorages.count);
      if (_lastCommittedItemIndex >= items_size) {
        _lastCommittedItemIndex = items_size - 1;
      }
    }

    _certPolicyCacheStorage =
        [decoder decodeObjectForKey:kCertificatePolicyCacheStorageKey];
    if (!_certPolicyCacheStorage) {
      // If the cert policy cache was not found, attempt to decode using the
      // deprecated serialization key.
      // TODO(crbug.com/40208116): Remove this deprecated key once we remove
      // support for legacy class conversions.
      _certPolicyCacheStorage = [decoder
          decodeObjectForKey:kCertificatePolicyCacheStorageDeprecatedKey];
    }

    id<NSCoding, NSObject> userData =
        [decoder decodeObjectForKey:kSerializedUserDataKey];
    if ([userData isKindOfClass:[CRWSessionUserData class]]) {
      _userData = base::apple::ObjCCastStrict<CRWSessionUserData>(userData);
    } else if ([userData isKindOfClass:[NSDictionary class]]) {
      // Before M99, the user data was serialized by a C++ class that did
      // serialize a NSDictionary<NSString*, id<NSCoding>>* directly.
      // TODO(crbug.com/40208116): Remove this deprecated logic when we remove
      // support for loading legacy sessions.
      NSDictionary<NSString*, id<NSCoding>>* dictionary =
          base::apple::ObjCCastStrict<NSDictionary>(userData);

      _userData = [[CRWSessionUserData alloc] init];
      for (NSString* key in dictionary) {
        [_userData setObject:dictionary[key] forKey:key];
      }
    }

    if ([decoder containsValueForKey:kUserAgentKey]) {
      std::string userAgentDescription =
          web::nscoder_util::DecodeString(decoder, kUserAgentKey);
      _userAgentType =
          web::GetUserAgentTypeWithDescription(userAgentDescription);
    } else {
      // Prior to M85, the UserAgent wasn't stored.
      // TODO(crbug.com/40208116): Remove this deprecated logic when we
      // remove support for loading legacy sessions.
      _userAgentType = web::UserAgentType::AUTOMATIC;
    }

    _stableIdentifier = [decoder decodeObjectForKey:kStableIdentifierKey];
    if (!_stableIdentifier.length) {
      // Before M99, the stable identifier was managed by a tab helper and
      // saved as part of the serializable user data. To support migration
      // of pre M99 session, read the data from there if not found.

      // If "TabId" is set, clear it and initialise the `stableIdentifier`
      // from it (if it is a NSString and non empty, otherwise a new value
      // will be created below).
      id<NSCoding> tabIdValue = [_userData objectForKey:kTabIdKey];
      if (tabIdValue) {
        [_userData removeObjectForKey:kTabIdKey];

        // If the value is not an NSString or is empty, a random identifier
        // will be generated below.
        _stableIdentifier = base::apple::ObjCCast<NSString>(tabIdValue);
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

    // If no unique identifier was read, or it was invalid, generate a
    // new one.
    static_assert(sizeof(_uniqueIdentifier.identifier()) == sizeof(int32_t));
    const int32_t decodedUniqueIdentifier =
        [decoder decodeInt32ForKey:kUniqueIdentifierKey];
    if (web::WebStateID::IsValidValue(decodedUniqueIdentifier)) {
      _uniqueIdentifier =
          web::WebStateID::FromSerializedValue(decodedUniqueIdentifier);
    }

    if ([decoder containsValueForKey:kCreationTimeKey]) {
      _creationTime = base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds([decoder decodeInt64ForKey:kCreationTimeKey]));
    }

    if ([decoder containsValueForKey:kLastActiveTimeKey]) {
      _lastActiveTime = base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds([decoder decodeInt64ForKey:kLastActiveTimeKey]));
    }

    // There was a regression found in M-119 but pre-existing that caused
    // WebState to initialize `GetLastActiveTime()` to base::Time(). This
    // is considered as an infinitely old point in time. Fix the value if
    // found while loading a session written before the initialisation of
    // WebState was fixed (see https://crbug.com/1490604 for details).
    if (_lastActiveTime < _creationTime) {
      _lastActiveTime = _creationTime;
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

  if (_userData) {
    [coder encodeObject:_userData forKey:kSerializedUserDataKey];
  }

  web::UserAgentType userAgentType = _userAgentType;
  web::nscoder_util::EncodeString(
      coder, kUserAgentKey, web::GetUserAgentTypeDescription(userAgentType));
  [coder encodeObject:_stableIdentifier forKey:kStableIdentifierKey];

  if (!_lastActiveTime.is_null()) {
    [coder
        encodeInt64:_lastActiveTime.ToDeltaSinceWindowsEpoch().InMicroseconds()
             forKey:kLastActiveTimeKey];
  }

  if (!_creationTime.is_null()) {
    [coder encodeInt64:_creationTime.ToDeltaSinceWindowsEpoch().InMicroseconds()
                forKey:kCreationTimeKey];
  }

  if (_uniqueIdentifier.valid()) {
    static_assert(sizeof(_uniqueIdentifier.identifier()) == sizeof(int32_t));
    [coder encodeInt32:_uniqueIdentifier.identifier()
                forKey:kUniqueIdentifierKey];
  }
}

#pragma mark Private

- (BOOL)cr_isEqualSameClass:(CRWSessionStorage*)other {
  if (_hasOpener != other.hasOpener) {
    return NO;
  }

  if (_lastCommittedItemIndex != other.lastCommittedItemIndex) {
    return NO;
  }

  if (_userAgentType != other.userAgentType) {
    return NO;
  }

  if (_userData != other.userData && ![_userData isEqual:other.userData]) {
    return NO;
  }

  if (_lastActiveTime != other.lastActiveTime) {
    return NO;
  }

  if (_creationTime != other.creationTime) {
    return NO;
  }

  if (_uniqueIdentifier != other.uniqueIdentifier) {
    return NO;
  }

  if (_stableIdentifier != other.stableIdentifier &&
      ![_stableIdentifier isEqual:other.stableIdentifier]) {
    return NO;
  }

  if (_itemStorages != other.itemStorages &&
      ![_itemStorages isEqual:other.itemStorages]) {
    return NO;
  }

  if (_certPolicyCacheStorage != other.certPolicyCacheStorage &&
      ![_certPolicyCacheStorage isEqual:other.certPolicyCacheStorage]) {
    return NO;
  }

  return YES;
}

@end
