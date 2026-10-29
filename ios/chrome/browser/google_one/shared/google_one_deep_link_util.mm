// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/google_one/shared/google_one_deep_link_util.h"

#import "base/strings/sys_string_conversions.h"
#import "google_apis/gaia/gaia_id.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/signin/model/system_identity.h"
#import "ios/chrome/browser/signin/model/system_identity_manager.h"
#import "ios/public/provider/chrome/browser/google_one/google_one_api.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"
#import "url/url_constants.h"

namespace {

// Callback passed to `SystemIdentityManager::IterateOverIdentities` that
// populates `identities` with all system identities on the device.
SystemIdentityManager::IteratorResult IdentitiesOnDevice(
    NSMutableArray<id<SystemIdentity>>* identities,
    id<SystemIdentity> identity) {
  [identities addObject:identity];
  return SystemIdentityManager::IteratorResult::kContinueIteration;
}

}  // namespace

bool IsGoogleOneDeepLinkURL(const GURL& url, GURL* out_url) {
  if (!url.is_valid() || !url.has_host() || url.host() != "one.google.com") {
    return false;
  }
  GURL::Replacements replacements;
  replacements.SetSchemeStr(url::kHttpsScheme);
  GURL https_url = url.ReplaceComponents(replacements);
  if (ios::provider::CanHandleGoogleOneURL(net::NSURLWithGURL(https_url))) {
    if (out_url) {
      *out_url = https_url;
    }
    return true;
  }
  return false;
}

NSString* GoogleOneAccountFromURL(const GURL& url) {
  if (!url.is_valid()) {
    return nil;
  }
  return ios::provider::GoogleOneEmailFromURL(net::NSURLWithGURL(url));
}

id<SystemIdentity> FindIdentityForGoogleOneAccount(NSString* account_param) {
  if (account_param.length == 0) {
    return nil;
  }
  std::string account_std = base::SysNSStringToUTF8(account_param);
  NSMutableArray<id<SystemIdentity>>* identities =
      [[NSMutableArray alloc] init];
  // TODO(crbug.com/576575308): Filter out identities that do not match
  // `kRestrictAccountsToPatterns`.
  GetApplicationContext()->GetSystemIdentityManager()->IterateOverIdentities(
      base::BindRepeating(&IdentitiesOnDevice, identities));
  for (id<SystemIdentity> identity in identities) {
    if ([account_param caseInsensitiveCompare:identity.userEmail] ==
            NSOrderedSame ||
        identity.gaiaId.ToString() == account_std) {
      return identity;
    }
  }
  return nil;
}

GaiaId FindGaiaIdForGoogleOneAccount(NSString* account_param) {
  id<SystemIdentity> identity = FindIdentityForGoogleOneAccount(account_param);
  return identity ? identity.gaiaId : GaiaId();
}
