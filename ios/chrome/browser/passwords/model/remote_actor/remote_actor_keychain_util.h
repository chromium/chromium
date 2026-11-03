// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_KEYCHAIN_UTIL_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_KEYCHAIN_UTIL_H_

#import <Foundation/Foundation.h>

#import <cstdint>
#import <optional>
#import <vector>

// Keychain account identifier for the Remote Actor symmetric key.
inline NSString* const kRemoteActorKeychainAccount = @"RemoteActorSharedKey";

// Keychain service identifier for the Remote Actor symmetric key.
inline NSString* const kRemoteActorKeychainService =
    @"com.google.chrome.remote_actor";

// Retrieves the symmetric key from the shared Keychain access group using
// default Remote Actor attributes. Returns `std::nullopt` if the key does not
// exist or an error occurs.
//
// Note: This function performs synchronous Keychain IPC (`SecItemCopyMatching`)
// and may block. It must be called on a background sequence that allows
// blocking (e.g., a `base::ThreadPool` task runner with `base::MayBlock()`),
// and must not be called on the main/UI thread.
std::optional<std::vector<uint8_t>> GetRemoteActorSharedSymmetricKey();

// Sets an override for the Keychain access group used by
// `GetRemoteActorSharedSymmetricKey()`. Passing `nil` or an empty string causes
// Keychain queries to omit `kSecAttrAccessGroup`. For testing purposes only.
void SetRemoteActorKeychainAccessGroupOverrideForTesting(  // IN-TEST
    NSString* access_group_override);

// Resets the Keychain access group override back to default.
// For testing purposes only.
void ResetRemoteActorKeychainAccessGroupOverrideForTesting();  // IN-TEST

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_KEYCHAIN_UTIL_H_
