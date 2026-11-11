// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_payload_parser.h"

#import <vector>

#import "base/base64.h"
#import "base/base64url.h"
#import "base/containers/span.h"
#import "base/containers/to_vector.h"
#import "base/types/expected_macros.h"
#import "crypto/aead.h"
#import "crypto/random.h"
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

namespace remote_actor {

base::expected<proto::RemoteActorPasswordUploadRequest, ParsingError>
ParseRemoteActorPayload(std::string_view base64_payload,
                        std::string_view base64_nonce,
                        base::span<const uint8_t> key) {
  if (base64_payload.size() > kMaxPayloadSizeBytes) {
    return base::unexpected(ParsingError::kPayloadTooLarge);
  }

  if (base64_nonce.size() > kMaxNonceSizeBytes) {
    return base::unexpected(ParsingError::kNonceTooLarge);
  }

  if (key.size() != crypto::aead::KeySizeFor(crypto::aead::AES_256_GCM)) {
    return base::unexpected(ParsingError::kInvalidKeyLength);
  }

  ASSIGN_OR_RETURN(std::vector<uint8_t> ciphertext,
                   DecodeBase64OrUrl(base64_payload),
                   [] { return ParsingError::kInvalidBase64Payload; });

  ASSIGN_OR_RETURN(std::vector<uint8_t> nonce, DecodeBase64OrUrl(base64_nonce),
                   [] { return ParsingError::kInvalidBase64Nonce; });

  if (nonce.size() != crypto::aead::NonceSizeFor(crypto::aead::AES_256_GCM)) {
    return base::unexpected(ParsingError::kDecryptionFailed);
  }

  crypto::Aead aead(crypto::Aead::AES_256_GCM, key);
  ASSIGN_OR_RETURN(std::vector<uint8_t> plaintext,
                   aead.Open(ciphertext, nonce, /*additional_data=*/{}),
                   [] { return ParsingError::kDecryptionFailed; });

  proto::RemoteActorPasswordUploadRequest request;
  if (!request.ParseFromArray(plaintext.data(), plaintext.size())) {
    return base::unexpected(ParsingError::kInvalidProtobuf);
  }

  if (request.gaia_id().empty() || request.domain().empty() ||
      request.task_id().empty()) {
    return base::unexpected(ParsingError::kMissingRequiredField);
  }

  return request;
}

base::expected<proto::RemoteActorPasswordUploadRequest, ParsingError>
ParseRemoteActorPayload(std::string_view base64_payload,
                        std::string_view base64_nonce) {
  ASSIGN_OR_RETURN(std::vector<uint8_t> key, GetSharedSymmetricKey(),
                   [] { return ParsingError::kMissingKeyInKeychain; });

  return ParseRemoteActorPayload(base64_payload, base64_nonce, key);
}

std::optional<std::pair<std::string, std::string>>
CreateEncryptedRemoteActorPayloadForTesting(  // IN-TEST
    const proto::RemoteActorPasswordUploadRequest& request,
    base::span<const uint8_t> key) {
  if (key.size() != crypto::aead::KeySizeFor(crypto::aead::AES_256_GCM)) {
    return std::nullopt;
  }

  std::string proto_bytes;
  if (!request.SerializeToString(&proto_bytes)) {
    return std::nullopt;
  }

  std::vector<uint8_t> nonce = crypto::RandBytesAsVector(
      crypto::aead::NonceSizeFor(crypto::aead::AES_256_GCM));

  crypto::Aead aead(crypto::Aead::AES_256_GCM, key);
  std::vector<uint8_t> ciphertext =
      aead.Seal(base::as_byte_span(proto_bytes), nonce, /*additional_data=*/{});
  if (ciphertext.empty()) {
    return std::nullopt;
  }

  std::string base64_payload = base::Base64Encode(ciphertext);
  std::string base64_nonce = base::Base64Encode(nonce);
  return std::make_pair(base64_payload, base64_nonce);
}

}  // namespace remote_actor
