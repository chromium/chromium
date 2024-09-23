// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_keychain_provider.h"

#import "base/functional/callback.h"

PasskeyKeychainProvider::PasskeyKeychainProvider() = default;

PasskeyKeychainProvider::~PasskeyKeychainProvider() = default;

void PasskeyKeychainProvider::FetchKeys(
    NSString* gaia,
    UINavigationController* navigation_controller,
    ReauthenticatePurpose purpose,
    KeyFetchedCallback callback) {
  if (!callback.is_null()) {
    std::move(callback).Run(SharedKeyList());
  }
}

void PasskeyKeychainProvider::MarkKeysAsStale(
    NSString* gaia,
    KeysMarkedAsAsStaleCallback callback) {
  if (!callback.is_null()) {
    std::move(callback).Run();
  }
}
