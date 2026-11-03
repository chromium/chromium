// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_TEST_REMOTE_ACTOR_TEST_UTIL_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_TEST_REMOTE_ACTOR_TEST_UTIL_H_

#import <Foundation/Foundation.h>

#import <cstdint>
#import <optional>
#import <string>
#import <utility>

#import "base/containers/span.h"
#import "ios/chrome/browser/passwords/model/remote_actor/proto/remote_actor_password_upload_request.pb.h"

// Encrypts and serializes `request` with `key` for testing. Returns a pair of
// {`base64_payload`, `base64_nonce`}, or `std::nullopt` on failure.
std::optional<std::pair<std::string, std::string>>
CreateEncryptedRemoteActorPayloadForTesting(  // IN-TEST
    const remote_actor::proto::RemoteActorPasswordUploadRequest& request,
    base::span<const uint8_t> key);

// Stores `key` in the Keychain under `access_group`, `account`, and `service`
// for testing or mock purposes. Returns `true` if stored successfully.
bool SetRemoteActorSharedSymmetricKeyForTesting(  // IN-TEST
    base::span<const uint8_t> key,
    NSString* access_group,
    NSString* account,
    NSString* service);

// Deletes the symmetric key identified by `access_group`, `account`, and
// `service` from the Keychain. Returns `true` if deleted successfully or if the
// key did not exist.
bool DeleteRemoteActorSharedSymmetricKeyForTesting(  // IN-TEST
    NSString* access_group,
    NSString* account,
    NSString* service);

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_TEST_REMOTE_ACTOR_TEST_UTIL_H_
