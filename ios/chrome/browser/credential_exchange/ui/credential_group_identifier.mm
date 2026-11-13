// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/ui/credential_group_identifier.h"

#import "base/apple/foundation_util.h"
#import "base/hash/hash.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"

@implementation CredentialGroupIdentifier

- (instancetype)initWithGroup:(const password_manager::AffiliatedGroup&)group {
  self = [super init];
  if (self) {
    _affiliatedGroup = group;
  }
  return self;
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[CredentialGroupIdentifier class]]) {
    return NO;
  }
  CredentialGroupIdentifier* other =
      base::apple::ObjCCast<CredentialGroupIdentifier>(object);

  return self.affiliatedGroup == other.affiliatedGroup;
}

- (NSUInteger)hash {
  const password_manager::AffiliatedGroup& group = self.affiliatedGroup;
  size_t hash = base::FastHash(group.GetDisplayName());
  for (const auto& cred : group.GetCredentials()) {
    hash = base::HashCombine(hash, cred.GetFirstSignonRealm());
  }
  return static_cast<NSUInteger>(hash);
}

- (NSString*)description {
  return [NSString stringWithFormat:@"<%@: %p, name: %@>", self.class, self,
                                    base::SysUTF8ToNSString(
                                        self.affiliatedGroup.GetDisplayName())];
}

@end
