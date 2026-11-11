// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_PAYLOAD_PARSER_H_
#define IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_PAYLOAD_PARSER_H_

#import <cstddef>
#import <cstdint>
#import <string_view>

#import "base/containers/span.h"
#import "base/types/expected.h"
#import "ios/chrome/browser/passwords/model/remote_actor/proto/remote_actor_password_upload_request.pb.h"

// Maximum allowed size for the incoming Base64-encoded payload (16 KB).
inline constexpr size_t kMaxRemoteActorPayloadSizeBytes = 16 * 1024;

// Maximum allowed size for the incoming Base64-encoded nonce (256 bytes).
inline constexpr size_t kMaxRemoteActorNonceSizeBytes = 256;

// Error outcomes when parsing and decrypting an incoming Remote Actor request.
enum class RemoteActorParsingError {
  kPayloadTooLarge,
  kNonceTooLarge,
  kInvalidBase64Payload,
  kInvalidBase64Nonce,
  kMissingKeyInKeychain,
  kInvalidKeyLength,
  kDecryptionFailed,
  kInvalidProtobuf,
  kMissingRequiredField,
};

// Parses and decrypts `base64_payload` with the 12-byte `base64_nonce` using
// the 32-byte AES-256 `key`.
base::expected<remote_actor::proto::RemoteActorPasswordUploadRequest,
               RemoteActorParsingError>
ParseRemoteActorPayload(std::string_view base64_payload,
                        std::string_view base64_nonce,
                        base::span<const uint8_t> key);

// Parses and decrypts `base64_payload` with the 12-byte `base64_nonce` by
// retrieving the symmetric key from the shared Keychain.
//
// Note: Because this function calls `GetRemoteActorSharedSymmetricKey()`, it
// performs synchronous Keychain IPC and must be called on a background sequence
// that allows blocking (e.g., with `base::MayBlock()`).
base::expected<remote_actor::proto::RemoteActorPasswordUploadRequest,
               RemoteActorParsingError>
ParseRemoteActorPayload(std::string_view base64_payload,
                        std::string_view base64_nonce);

#endif  // IOS_CHROME_BROWSER_PASSWORDS_MODEL_REMOTE_ACTOR_REMOTE_ACTOR_PAYLOAD_PARSER_H_
