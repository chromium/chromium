// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/credential_details.h"

#import "base/strings/sys_string_conversions.h"
#import "components/affiliations/core/browser/affiliation_utils.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/sync/base/features.h"

@implementation CredentialDetails

- (instancetype)initWithCredential:
    (const password_manager::CredentialUIEntry&)credential {
  self = [super init];
  if (self) {
    _signonRealm = [NSString
        stringWithUTF8String:credential.GetFirstSignonRealm().c_str()];
    _changePasswordURL = credential.GetChangePasswordURL();

    std::vector<password_manager::CredentialUIEntry::DomainInfo> domain_infos =
        credential.GetAffiliatedDomains();
    NSMutableSet<NSString*>* origins = [NSMutableSet set];
    NSInteger websiteCount = domain_infos.size();
    NSMutableArray<NSString*>* websites =
        [NSMutableArray arrayWithCapacity:websiteCount];
    for (const auto& domain_info : domain_infos) {
      [origins addObject:base::SysUTF8ToNSString(domain_info.name)];
      if (affiliations::IsValidAndroidFacetURI(domain_info.signon_realm)) {
        [websites addObject:base::SysUTF8ToNSString(domain_info.name)];
      } else {
        [websites addObject:base::SysUTF8ToNSString(domain_info.url.spec())];
      }
    }
    _websites = websites;
    _origins = [origins allObjects];

    if (!credential.blocked_by_user) {
      _username = base::SysUTF16ToNSString(credential.username);
    }

    _userDisplayName = base::SysUTF16ToNSString(credential.user_display_name);

    if (!credential.federation_origin.IsValid()) {
      _password = base::SysUTF16ToNSString(credential.password);
    } else {
      _federation =
          base::SysUTF8ToNSString(credential.federation_origin.host());
    }

    _creationTime = credential.creation_time;

    _note = base::SysUTF16ToNSString(credential.note);
    _credentialType = credential.blocked_by_user
                          ? CredentialTypeBlocked
                          : CredentialTypeRegularPassword;
    if (_credentialType == CredentialTypeRegularPassword &&
        credential.federation_origin.IsValid()) {
      _credentialType = CredentialTypeFederation;
    }
    if (base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials) &&
        !credential.passkey_credential_id.empty()) {
      _credentialType = CredentialTypePasskey;
    }
  }
  return self;
}

@end
