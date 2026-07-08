// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/credential_exchange/ui/credential_group_identifier.h"

#import <string>

#import "base/apple/foundation_util.h"
#import "base/containers/flat_set.h"
#import "base/hash/hash.h"
#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/ui/affiliated_group.h"

namespace {
base::flat_set<std::string> GetUniqueSignonRealms(
    const password_manager::AffiliatedGroup& group) {
  std::vector<std::string> realms;
  for (const auto& cred : group.GetCredentials()) {
    realms.push_back(cred.GetFirstSignonRealm());
  }
  return base::flat_set<std::string>(std::move(realms));
}
}  // namespace

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

  if (self.affiliatedGroup != other.affiliatedGroup) {
    return NO;
  }

  // To prevent NSDiffableDataSource crashes when two distinct groups
  // evaluate to equal (due to CredentialUIEntry operator== ignoring
  // signon_realm for web credentials), we must also verify their exact
  // signon_realms match. This guarantees that distinct AffiliatedGroups
  // returned by PasswordsGrouper are never treated as duplicates by the
  // snapshot.
  return GetUniqueSignonRealms(self.affiliatedGroup) ==
         GetUniqueSignonRealms(other.affiliatedGroup);
}

- (NSUInteger)hash {
  size_t hash = base::FastHash(self.affiliatedGroup.GetDisplayName());
  for (const auto& realm : GetUniqueSignonRealms(self.affiliatedGroup)) {
    hash = base::HashCombine(hash, realm);
  }
  return static_cast<NSUInteger>(hash);
}

- (NSString*)description {
  return [NSString stringWithFormat:@"<%@: %p, name: %@>", self.class, self,
                                    base::SysUTF8ToNSString(
                                        self.affiliatedGroup.GetDisplayName())];
}

@end
