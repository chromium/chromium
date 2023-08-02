// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/archivable_credential.h"

namespace {

// Keys used to serialize properties.
NSString* const kACFaviconKey = @"favicon";
NSString* const kACKeychainIdentifierKey = @"keychainIdentifier";
NSString* const kACRankKey = @"rank";
NSString* const kACRecordIdentifierKey = @"recordIdentifier";
NSString* const kACServiceIdentifierKey = @"serviceIdentifier";
NSString* const kACServiceNameKey = @"serviceName";
NSString* const kACUserKey = @"user";
NSString* const kNoteKey = @"note";

}  // namespace

@implementation ArchivableCredential

@synthesize favicon = _favicon;
@synthesize keychainIdentifier = _keychainIdentifier;
@synthesize rank = _rank;
@synthesize recordIdentifier = _recordIdentifier;
@synthesize serviceIdentifier = _serviceIdentifier;
@synthesize serviceName = _serviceName;
@synthesize user = _user;
@synthesize note = _note;

- (instancetype)initWithFavicon:(NSString*)favicon
             keychainIdentifier:(NSString*)keychainIdentifier
                           rank:(int64_t)rank
               recordIdentifier:(NSString*)recordIdentifier
              serviceIdentifier:(NSString*)serviceIdentifier
                    serviceName:(NSString*)serviceName
                           user:(NSString*)user
                           note:(NSString*)note {
  self = [super init];
  if (self) {
    _favicon = favicon;
    _keychainIdentifier = keychainIdentifier;
    _rank = rank;
    _recordIdentifier = recordIdentifier;
    _serviceIdentifier = serviceIdentifier;
    _serviceName = serviceName;
    _user = user;
    _note = note;
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
    return [self.favicon isEqualToString:otherCredential.favicon] &&
           [self.keychainIdentifier
               isEqualToString:otherCredential.keychainIdentifier] &&
           self.rank == otherCredential.rank &&
           [self.recordIdentifier
               isEqualToString:otherCredential.recordIdentifier] &&
           [self.serviceIdentifier
               isEqualToString:otherCredential.serviceIdentifier] &&
           [self.serviceName isEqualToString:otherCredential.serviceName] &&
           [self.user isEqualToString:otherCredential.user] &&
           [self.note isEqualToString:otherCredential.note];
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
  [coder encodeObject:self.note forKey:kNoteKey];
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  return
      [self initWithFavicon:[coder decodeObjectForKey:kACFaviconKey]
          keychainIdentifier:[coder decodeObjectForKey:kACKeychainIdentifierKey]
                        rank:[coder decodeInt64ForKey:kACRankKey]
            recordIdentifier:[coder decodeObjectForKey:kACRecordIdentifierKey]
           serviceIdentifier:[coder decodeObjectForKey:kACServiceIdentifierKey]
                 serviceName:[coder decodeObjectForKey:kACServiceNameKey]
                        user:[coder decodeObjectForKey:kACUserKey]
                        note:[coder decodeObjectForKey:kNoteKey]];
}

@end
