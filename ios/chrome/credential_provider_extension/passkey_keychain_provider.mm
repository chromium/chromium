// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/passkey_keychain_provider.h"

#import "base/functional/callback.h"

PasskeyKeychainProvider::PasskeyKeychainProvider() = default;

PasskeyKeychainProvider::~PasskeyKeychainProvider() = default;

void PasskeyKeychainProvider::CheckEnrolled(NSString* gaia,
                                            CheckEnrolledCallback callback) {
  if (!callback.is_null()) {
    std::move(callback).Run(NO, nil);
  }
}

void PasskeyKeychainProvider::Enroll(
    NSString* gaia,
    UINavigationController* navigation_controller,
    EnrollCallback callback) {
  if (!callback.is_null()) {
    std::move(callback).Run(nil);
  }
}

void PasskeyKeychainProvider::FetchKeys(
    NSString* gaia,
    PasskeyKeychainProvider::ReauthenticatePurpose purpose,
    KeysFetchedCallback callback) {
  if (!callback.is_null()) {
    std::move(callback).Run({});
  }
}

void PasskeyKeychainProvider::MarkKeysAsStale(
    NSString* gaia,
    KeysMarkedAsAsStaleCallback callback) {
  if (!callback.is_null()) {
    std::move(callback).Run();
  }
}

void PasskeyKeychainProvider::Reauthenticate(
    NSString* gaia,
    UINavigationController* navigation_controller,
    PasskeyKeychainProvider::ReauthenticatePurpose purpose,
    KeysFetchedCallback callback) {
  if (!callback.is_null()) {
    std::move(callback).Run({});
  }
}
