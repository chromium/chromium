// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/archivable_credential.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Keys used to serialize properties.
NSString* const kACFaviconKey = @"favicon";
NSString* const kACKeychainIdentifierKey = @"keychainIdentifier";
NSString* const kACRankKey = @"rank";
NSString* const kACRecordIdentifierKey = @"recordIdentifier";
NSString* const kACServiceIdentifierKey = @"serviceIdentifier";
NSString* const kACServiceNameKey = @"serviceName";
NSString* const kACUserKey = @"user";
NSString* const kACValidationIdentifierKey = @"validationIdentifier";

}  // namespace

@implementation ArchivableCredential

@synthesize favicon = _favicon;
@synthesize keychainIdentifier = _keychainIdentifier;
@synthesize rank = _rank;
@synthesize recordIdentifier = _recordIdentifier;
@synthesize serviceIdentifier = _serviceIdentifier;
@synthesize serviceName = _serviceName;
@synthesize user = _user;
@synthesize validationIdentifier = _validationIdentifier;

- (instancetype)initWithFavicon:(NSString*)favicon
             keychainIdentifier:(NSString*)keychainIdentifier
                           rank:(int64_t)rank
               recordIdentifier:(NSString*)recordIdentifier
              serviceIdentifier:(NSString*)serviceIdentifier
                    serviceName:(NSString*)serviceName
                           user:(NSString*)user
           validationIdentifier:(NSString*)validationIdentifier {
  self = [super init];
  if (self) {
    _favicon = favicon;
    _keychainIdentifier = keychainIdentifier;
    _rank = rank;
    _recordIdentifier = recordIdentifier;
    _serviceIdentifier = serviceIdentifier;
    _serviceName = serviceName;
    _user = user;
    _validationIdentifier = validationIdentifier;
  }
  return self;
}

- (BOOL)isEqual:(id)other {
  if (other == self) {
    return YES;
  } else {
    if (![other isKindOfClass:[ArchivableCredential class]]) {
      return NO;
    }
    ArchivableCredential* otherCredential = (ArchivableCredential*)other;
    return
        [self.favicon isEqual:otherCredential.favicon] &&
        [self.keychainIdentifier isEqual:otherCredential.keychainIdentifier] &&
        self.rank == otherCredential.rank &&
        [self.recordIdentifier isEqual:otherCredential.recordIdentifier] &&
        [self.serviceIdentifier isEqual:otherCredential.serviceIdentifier] &&
        [self.serviceName isEqual:otherCredential.serviceName] &&
        [self.user isEqual:otherCredential.user] &&
        [self.validationIdentifier
            isEqual:otherCredential.validationIdentifier];
  }
}

- (NSUInteger)hash {
  // Using record identifier xored with keychain identifier should be enough.
  return self.recordIdentifier.hash ^ self.keychainIdentifier.hash;
}

#pragma mark - NSSecureCoding

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeObject:self.favicon forKey:kACFaviconKey];
  [coder encodeObject:self.keychainIdentifier forKey:kACKeychainIdentifierKey];
  [coder encodeInt64:self.rank forKey:kACRankKey];
  [coder encodeObject:self.recordIdentifier forKey:kACRecordIdentifierKey];
  [coder encodeObject:self.serviceIdentifier forKey:kACServiceIdentifierKey];
  [coder encodeObject:self.serviceName forKey:kACServiceNameKey];
  [coder encodeObject:self.user forKey:kACUserKey];
  [coder encodeObject:self.validationIdentifier
               forKey:kACValidationIdentifierKey];
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  return [self
           initWithFavicon:[coder decodeObjectForKey:kACFaviconKey]
        keychainIdentifier:[coder decodeObjectForKey:kACKeychainIdentifierKey]
                      rank:[coder decodeInt64ForKey:kACRankKey]
          recordIdentifier:[coder decodeObjectForKey:kACRecordIdentifierKey]
         serviceIdentifier:[coder decodeObjectForKey:kACServiceIdentifierKey]
               serviceName:[coder decodeObjectForKey:kACServiceNameKey]
                      user:[coder decodeObjectForKey:kACUserKey]
      validationIdentifier:[coder
                               decodeObjectForKey:kACValidationIdentifierKey]];
}

@end
