// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_keychain_util.h"

#import "base/functional/callback.h"

namespace {

// Returns the security domain secret from the vault keys.
NSData* GetSecurityDomainSecret(
    const PasskeyKeychainProvider::SharedKeyList& keyList) {
  if (keyList.empty()) {
    return nil;
  }
  // TODO(crbug.com/355041765): Do we need to handle multiple keys?
  return [NSData dataWithBytes:keyList[0].data() length:keyList[0].size()];
}

}  // namespace

void FetchSecurityDomainSecret(
    NSString* gaia,
    UINavigationController* navigation_controller,
    PasskeyKeychainProvider::ReauthenticatePurpose purpose,
    FetchKeyCompletionBlock completion) {
  PasskeyKeychainProvider passkeyKeychainProvider;
  passkeyKeychainProvider.FetchKeys(
      gaia, navigation_controller, purpose,
      base::BindOnce(^(const PasskeyKeychainProvider::SharedKeyList& keyList) {
        completion(GetSecurityDomainSecret(keyList));
      }));
}

void MarkKeysAsStale(NSString* gaia, ProceduralBlock completion) {
  PasskeyKeychainProvider passkeyKeychainProvider;
  passkeyKeychainProvider.MarkKeysAsStale(gaia, base::BindOnce(^() {
                                            completion();
                                          }));
}
