// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/ui/net_util.h"

#import "base/strings/sys_string_conversions.h"
#import "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace credential_provider_extension {

BOOL SecureHostsMatch(NSString* requestedHost, NSString* credentialHost) {
  if (requestedHost.length == 0 || credentialHost.length == 0) {
    return NO;
  }
  if ([requestedHost isEqualToString:credentialHost]) {
    return YES;
  }

  std::string reqDomain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          base::SysNSStringToUTF8(requestedHost),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
  std::string credDomain =
      net::registry_controlled_domains::GetDomainAndRegistry(
          base::SysNSStringToUTF8(credentialHost),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);

  if (reqDomain.empty() || credDomain.empty()) {
    return NO;
  }

  return reqDomain == credDomain;
}

}  // namespace credential_provider_extension
