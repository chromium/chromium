// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"

#import "base/strings/sys_string_conversions.h"
#import "components/password_manager/core/browser/affiliation/affiliation_utils.h"
#import "components/password_manager/core/browser/password_ui_utils.h"
#import "components/password_manager/core/browser/ui/credential_ui_entry.h"
#import "components/password_manager/core/browser/well_known_change_password_util.h"
#import "components/sync/base/features.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Helper function that computes the websites displayed to the user
// corresponding to a group of facets.
NSArray<NSString*>* GetWebsitesFromFacets(
    std::vector<password_manager::CredentialFacet> facets) {
  NSInteger websiteCount = facets.size();
  NSMutableArray<NSString*>* websites =
      [NSMutableArray arrayWithCapacity:websiteCount];

  for (const auto& facet : facets) {
    NSString* website;
    auto facetURI = password_manager::FacetURI::FromPotentiallyInvalidSpec(
        facet.signon_realm);
    // Android facets: use Application Name or package name as fallback.
    if (facetURI.IsValidAndroidFacetURI()) {
      if (!facet.display_name.empty()) {
        website = base::SysUTF8ToNSString(facet.display_name);
      } else {
        website = base::SysUTF8ToNSString(facetURI.android_package_name());
      }
    } else {
      // Web facets: use the URL.
      website =
          base::SysUTF8ToNSString(password_manager::GetShownUrl(facet).spec());
    }
    [websites addObject:website];
  }

  return websites;
}

// Helper function that computes the origins corresponding to a group of facets.
NSSet<NSString*>* GetOriginsFromCredential(
    const password_manager::CredentialUIEntry& credential) {
  NSMutableSet<NSString*>* origins = [NSMutableSet set];
  for (const auto& facet : credential.facets) {
    [origins addObject:base::SysUTF8ToNSString(
                           password_manager::GetShownOrigin(facet))];
  }
  return origins;
}

}  // namespace

@implementation PasswordDetails

- (instancetype)initWithCredential:
    (const password_manager::CredentialUIEntry&)credential {
  self = [super init];
  if (self) {
    _signonRealm = [NSString
        stringWithUTF8String:credential.GetFirstSignonRealm().c_str()];
    auto facetUri = password_manager::FacetURI::FromPotentiallyInvalidSpec(
        credential.GetFirstSignonRealm());
    if (facetUri.IsValidAndroidFacetURI() &&
        !credential.GetDisplayName().empty()) {
      _changePasswordURL = password_manager::CreateChangePasswordUrl(
          GURL(credential.GetAffiliatedWebRealm()));
    } else {
      _changePasswordURL =
          password_manager::CreateChangePasswordUrl(credential.GetURL());
    }
    _origins = [GetOriginsFromCredential(credential) allObjects];
    _websites = GetWebsitesFromFacets(credential.facets);

    if (!credential.blocked_by_user) {
      _username = base::SysUTF16ToNSString(credential.username);
    }

    if (credential.federation_origin.opaque()) {
      _password = base::SysUTF16ToNSString(credential.password);
    } else {
      _federation =
          base::SysUTF8ToNSString(credential.federation_origin.host());
    }

    if (base::FeatureList::IsEnabled(syncer::kPasswordNotesWithBackup)) {
      _note = base::SysUTF16ToNSString(credential.note);
    }

    _credentialType = credential.blocked_by_user ? CredentialTypeBlocked
                                                 : CredentialTypeRegular;
    if (_credentialType == CredentialTypeRegular &&
        !credential.federation_origin.opaque()) {
      _credentialType = CredentialTypeFederation;
    }
  }
  return self;
}

@end
