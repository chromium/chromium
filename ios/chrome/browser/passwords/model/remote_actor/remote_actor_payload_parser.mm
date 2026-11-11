// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_payload_parser.h"

#import <optional>
#import <string>
#import <vector>

#import "base/base64.h"
#import "base/base64url.h"
#import "base/containers/span.h"
#import "base/containers/to_vector.h"
#import "base/types/expected_macros.h"
#import "crypto/aead.h"
#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_keychain_util.h"

namespace {

std::optional<std::vector<uint8_t>> DecodeBase64OrUrl(std::string_view input) {
  std::string decoded_str;
  if (base::Base64Decode(input, &decoded_str,
                         base::Base64DecodePolicy::kStrict)) {
    return base::ToVector(base::as_byte_span(decoded_str));
  }

  std::string url_decoded_str;
  if (base::Base64UrlDecode(input, base::Base64UrlDecodePolicy::IGNORE_PADDING,
                            &url_decoded_str)) {
    return base::ToVector(base::as_byte_span(url_decoded_str));
  }

  return std::nullopt;
}

}  // namespace

base::expected<remote_actor::proto::RemoteActorPasswordUploadRequest,
               RemoteActorParsingError>
ParseRemoteActorPayload(std::string_view base64_payload,
                        std::string_view base64_nonce,
                        base::span<const uint8_t> key) {
  if (base64_payload.size() > kMaxRemoteActorPayloadSizeBytes) {
    return base::unexpected(RemoteActorParsingError::kPayloadTooLarge);
  }

  if (base64_nonce.size() > kMaxRemoteActorNonceSizeBytes) {
    return base::unexpected(RemoteActorParsingError::kNonceTooLarge);
  }

  if (key.size() != crypto::aead::KeySizeFor(crypto::aead::AES_256_GCM)) {
    return base::unexpected(RemoteActorParsingError::kInvalidKeyLength);
  }

  ASSIGN_OR_RETURN(
      std::vector<uint8_t> ciphertext, DecodeBase64OrUrl(base64_payload),
      [] { return RemoteActorParsingError::kInvalidBase64Payload; });

  ASSIGN_OR_RETURN(std::vector<uint8_t> nonce, DecodeBase64OrUrl(base64_nonce),
                   [] { return RemoteActorParsingError::kInvalidBase64Nonce; });

  if (nonce.size() != crypto::aead::NonceSizeFor(crypto::aead::AES_256_GCM)) {
    return base::unexpected(RemoteActorParsingError::kDecryptionFailed);
  }

  crypto::Aead aead(crypto::Aead::AES_256_GCM, key);
  ASSIGN_OR_RETURN(std::vector<uint8_t> plaintext,
                   aead.Open(ciphertext, nonce, /*additional_data=*/{}),
                   [] { return RemoteActorParsingError::kDecryptionFailed; });

  remote_actor::proto::RemoteActorPasswordUploadRequest request;
  if (!request.ParseFromArray(plaintext.data(), plaintext.size())) {
    return base::unexpected(RemoteActorParsingError::kInvalidProtobuf);
  }

  if (request.gaia_id().empty() || request.domain().empty() ||
      request.task_id().empty()) {
    return base::unexpected(RemoteActorParsingError::kMissingRequiredField);
  }

  return request;
}

base::expected<remote_actor::proto::RemoteActorPasswordUploadRequest,
               RemoteActorParsingError>
ParseRemoteActorPayload(std::string_view base64_payload,
                        std::string_view base64_nonce) {
  ASSIGN_OR_RETURN(
      std::vector<uint8_t> key, GetRemoteActorSharedSymmetricKey(),
      [] { return RemoteActorParsingError::kMissingKeyInKeychain; });

  ASSIGN_OR_RETURN(
      remote_actor::proto::RemoteActorPasswordUploadRequest request,
      ParseRemoteActorPayload(base64_payload, base64_nonce, key));

  DeleteRemoteActorSharedSymmetricKey();
  return request;
}
