// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/credential_provider/archivable_credential.h"

#import "base/check.h"

namespace {

// Keys used to serialize properties.
NSString* const kACFaviconKey = @"favicon";
NSString* const kACPasswordKey = @"password";
NSString* const kACRankKey = @"rank";
NSString* const kACRecordIdentifierKey = @"recordIdentifier";
NSString* const kACServiceIdentifierKey = @"serviceIdentifier";
NSString* const kACServiceNameKey = @"serviceName";
NSString* const kACUserKey = @"user";
NSString* const kACNoteKey = @"note";
NSString* const kACSyncIdKey = @"syncId";
NSString* const kACUserDisplayNameKey = @"userDisplayName";
NSString* const kACUserIdKey = @"userId";
NSString* const kACCredentialIdKey = @"credentialId";
NSString* const kACRpIdKey = @"rpId";
NSString* const kACPrivateKeyKey = @"privateKey";
NSString* const kACEncryptedKey = @"encrypted";
NSString* const kACCreationTimeKey = @"creationTime";

// Returns whether the strings are the same (including if both are nil) or if
// both strings have the same contents.
BOOL stringsAreEqual(NSString* rhs, NSString* lhs) {
  return rhs == lhs || [rhs isEqualToString:lhs];
}

}  // namespace

@implementation NSCoder (ArchivableCredential)

- (id)decodeNSStringForKey:(NSString*)key {
  return [self decodeObjectOfClass:[NSString class] forKey:key];
}

@end

@implementation ArchivableCredential

@synthesize favicon = _favicon;
@synthesize password = _password;
@synthesize rank = _rank;
@synthesize recordIdentifier = _recordIdentifier;
@synthesize serviceIdentifier = _serviceIdentifier;
@synthesize serviceName = _serviceName;
@synthesize username = _username;
@synthesize note = _note;
@synthesize syncId = _syncId;
@synthesize userDisplayName = _userDisplayName;
@synthesize userId = _userId;
@synthesize credentialId = _credentialId;
@synthesize rpId = _rpId;
@synthesize privateKey = _privateKey;
@synthesize encrypted = _encrypted;
@synthesize creationTime = _creationTime;

- (instancetype)initWithFavicon:(NSString*)favicon
                     credential:(id<Credential>)credential {
  if (credential.isPasskey) {
    // Use the passkey initilizer
    self = [self initWithFavicon:credential.favicon
                recordIdentifier:credential.recordIdentifier
                          syncId:credential.syncId
                        username:credential.username
                 userDisplayName:credential.userDisplayName
                          userId:credential.userId
                    credentialId:credential.credentialId
                            rpId:credential.rpId
                      privateKey:credential.privateKey
                       encrypted:credential.encrypted
                    creationTime:credential.creationTime];
  } else {
    // Use the password initializer
    self = [self initWithFavicon:credential.favicon
                        password:credential.password
                            rank:credential.rank
                recordIdentifier:credential.recordIdentifier
               serviceIdentifier:credential.serviceIdentifier
                     serviceName:credential.serviceName
                        username:credential.username
                            note:credential.note];
  }
  return self;
}

- (instancetype)initWithFavicon:(NSString*)favicon
                       password:(NSString*)password
                           rank:(int64_t)rank
               recordIdentifier:(NSString*)recordIdentifier
              serviceIdentifier:(NSString*)serviceIdentifier
                    serviceName:(NSString*)serviceName
                       username:(NSString*)username
                           note:(NSString*)note {
  self = [super init];
  if (self) {
    _favicon = favicon;
    _password = password;
    _rank = rank;
    _recordIdentifier = recordIdentifier;
    _serviceIdentifier = serviceIdentifier;
    _serviceName = serviceName;
    _username = username;
    _note = note;
  }
  return self;
}

- (instancetype)initWithFavicon:(NSString*)favicon
               recordIdentifier:(NSString*)recordIdentifier
                         syncId:(NSString*)syncId
                       username:(NSString*)username
                userDisplayName:(NSString*)userDisplayName
                         userId:(NSString*)userId
                   credentialId:(NSString*)credentialId
                           rpId:(NSString*)rpId
                     privateKey:(NSString*)privateKey
                      encrypted:(NSString*)encrypted
                   creationTime:(int64_t)creationTime {
  CHECK(credentialId.length > 0);
  self = [super init];
  if (self) {
    _favicon = favicon;
    _recordIdentifier = recordIdentifier;
    _syncId = syncId;
    _username = username;
    _userDisplayName = userDisplayName;
    _userId = userId;
    _credentialId = credentialId;
    _rpId = rpId;
    _privateKey = privateKey;
    _encrypted = encrypted;
    _creationTime = creationTime;
  }
  return self;
}

- (BOOL)isPasskey {
  return self.credentialId.length > 0;
}

- (BOOL)isEqual:(id)other {
  if (other == self) {
    return YES;
  } else {
    if (![other isKindOfClass:[ArchivableCredential class]]) {
      return NO;
    }
    ArchivableCredential* otherCredential = (ArchivableCredential*)other;
    return stringsAreEqual(self.favicon, otherCredential.favicon) &&
           stringsAreEqual(self.password, otherCredential.password) &&
           self.rank == otherCredential.rank &&
           stringsAreEqual(self.recordIdentifier,
                           otherCredential.recordIdentifier) &&
           stringsAreEqual(self.serviceIdentifier,
                           otherCredential.serviceIdentifier) &&
           stringsAreEqual(self.serviceName, otherCredential.serviceName) &&
           stringsAreEqual(self.username, otherCredential.username) &&
           stringsAreEqual(self.note, otherCredential.note) &&
           stringsAreEqual(self.syncId, otherCredential.syncId) &&
           stringsAreEqual(self.userDisplayName,
                           otherCredential.userDisplayName) &&
           stringsAreEqual(self.userId, otherCredential.userId) &&
           stringsAreEqual(self.credentialId, otherCredential.credentialId) &&
           stringsAreEqual(self.rpId, otherCredential.rpId) &&
           stringsAreEqual(self.privateKey, otherCredential.privateKey) &&
           stringsAreEqual(self.encrypted, otherCredential.encrypted) &&
           self.creationTime == otherCredential.creationTime;
  }
}

- (NSUInteger)hash {
  // Using record identifier xored with password or passkey id should be enough.
  return self.recordIdentifier.hash ^
         (self.isPasskey ? self.credentialId.hash : self.password.hash);
}

#pragma mark - NSSecureCoding

+ (BOOL)supportsSecureCoding {
  return YES;
}

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeObject:self.favicon forKey:kACFaviconKey];
  [coder encodeObject:self.recordIdentifier forKey:kACRecordIdentifierKey];
  [coder encodeObject:self.username forKey:kACUserKey];
  if (self.isPasskey) {
    [coder encodeObject:self.syncId forKey:kACSyncIdKey];
    [coder encodeObject:self.userDisplayName forKey:kACUserDisplayNameKey];
    [coder encodeObject:self.userId forKey:kACUserIdKey];
    [coder encodeObject:self.credentialId forKey:kACCredentialIdKey];
    [coder encodeObject:self.rpId forKey:kACRpIdKey];
    [coder encodeObject:self.privateKey forKey:kACPrivateKeyKey];
    [coder encodeObject:self.encrypted forKey:kACEncryptedKey];
    [coder encodeInt64:self.creationTime forKey:kACCreationTimeKey];
  } else {
    [coder encodeObject:self.password forKey:kACPasswordKey];
    [coder encodeInt64:self.rank forKey:kACRankKey];
    [coder encodeObject:self.serviceIdentifier forKey:kACServiceIdentifierKey];
    [coder encodeObject:self.serviceName forKey:kACServiceNameKey];
    [coder encodeObject:self.note forKey:kACNoteKey];
  }
}

- (instancetype)initWithCoder:(NSCoder*)coder {
  NSString* credentialId = [coder decodeNSStringForKey:kACCredentialIdKey];

  if (credentialId.length > 0) {
    // Use the passkey initilizer
    return [self
         initWithFavicon:[coder decodeNSStringForKey:kACFaviconKey]
        recordIdentifier:[coder decodeNSStringForKey:kACRecordIdentifierKey]
                  syncId:[coder decodeNSStringForKey:kACSyncIdKey]
                username:[coder decodeNSStringForKey:kACUserKey]
         userDisplayName:[coder decodeNSStringForKey:kACUserDisplayNameKey]
                  userId:[coder decodeNSStringForKey:kACUserIdKey]
            credentialId:credentialId
                    rpId:[coder decodeNSStringForKey:kACRpIdKey]
              privateKey:[coder decodeNSStringForKey:kACPrivateKeyKey]
               encrypted:[coder decodeNSStringForKey:kACEncryptedKey]
            creationTime:[coder decodeInt64ForKey:kACCreationTimeKey]];
  } else {
    // Use the password initializer
    return [self
          initWithFavicon:[coder decodeNSStringForKey:kACFaviconKey]
                 password:[coder decodeNSStringForKey:kACPasswordKey]
                     rank:[coder decodeInt64ForKey:kACRankKey]
         recordIdentifier:[coder decodeNSStringForKey:kACRecordIdentifierKey]
        serviceIdentifier:[coder decodeNSStringForKey:kACServiceIdentifierKey]
              serviceName:[coder decodeNSStringForKey:kACServiceNameKey]
                 username:[coder decodeNSStringForKey:kACUserKey]
                     note:[coder decodeNSStringForKey:kACNoteKey]];
  }
}

@end
