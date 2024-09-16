// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_keychain_util.h"

#import "base/functional/callback.h"

void FetchSecurityDomainSecret(
    NSString* gaia,
    UINavigationController* navigation_controller,
    PasskeyKeychainProvider::ReauthenticatePurpose purpose,
    FetchKeyCompletionBlock callback) {
  PasskeyKeychainProvider passkeyKeychainProvider;
  passkeyKeychainProvider.FetchKeys(
      gaia, navigation_controller, purpose,
      base::BindOnce(^(const PasskeyKeychainProvider::SharedKeyList& keyList) {
        callback(keyList);
      }));
}
