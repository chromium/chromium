// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/identity_digest.h"

#include "base/containers/span.h"
#include "base/notreached.h"
#include "net/http/structured_headers.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {

namespace {

const char kSHA256Token[] = "sha-256";
const char kSHA384Token[] = "sha-384";
const char kSHA512Token[] = "sha-512";

HashAlgorithm GetHashAlgorithm(IntegrityAlgorithm integrity) {
  switch (integrity) {
    case IntegrityAlgorithm::kSha256:
      return kHashAlgorithmSha256;
    case IntegrityAlgorithm::kSha384:
      return kHashAlgorithmSha384;
    case IntegrityAlgorithm::kSha512:
      return kHashAlgorithmSha512;

    // We don't parse signature algorithms, so we should never generate
    // a parsed `Identity-Digest` header with such a prefix:
    case IntegrityAlgorithm::kEd25519:
      NOTREACHED();
  }
}

}  // namespace

IdentityDigest::IdentityDigest(IntegrityMetadataSet integrity_metadata)
    : integrity_metadata_(integrity_metadata) {
  CHECK(integrity_metadata.signatures.empty());
}

std::optional<IdentityDigest> IdentityDigest::Create(
    const HTTPHeaderMap& headers) {
  AtomicString header_value = headers.Get(http_names::kIdentityDigest);
  if (header_value == g_null_atom) {
    return std::nullopt;
  }

  std::optional<net::structured_headers::Dictionary> dictionary =
      net::structured_headers::ParseDictionary(header_value.Utf8());
  if (!dictionary) {
    return std::nullopt;
  }

  IntegrityMetadataSet integrity_metadata;
  for (const auto& entry : dictionary.value()) {
    IntegrityMetadata parsed_digest;
    size_t expected_digest_length = 0;
    if (entry.first == kSHA256Token) {
      parsed_digest.SetAlgorithm(IntegrityAlgorithm::kSha256);
      expected_digest_length = 32;
    } else if (entry.first == kSHA384Token) {
      parsed_digest.SetAlgorithm(IntegrityAlgorithm::kSha384);
      expected_digest_length = 48;
    } else if (entry.first == kSHA512Token) {
      parsed_digest.SetAlgorithm(IntegrityAlgorithm::kSha512);
      expected_digest_length = 64;
    } else {
      // Skip over entries with unknown algorithm tokens.
      //
      // TODO(https://crbug.com/381044049): Emit errors.
      continue;
    }

    if (entry.second.member_is_inner_list || entry.second.member.empty() ||
        !entry.second.member[0].item.is_byte_sequence()) {
      // Likewise skip over entries with unparsable digests.
      //
      // TODO(https://crbug.com/381044049): Emit errors.
      continue;
    }

    std::string digest = entry.second.member[0].item.GetString();
    if (digest.length() != expected_digest_length) {
      // Skip entries with digests of incorrect length.
      //
      // TODO(https://crbug.com/381044049): Emit errors.
      continue;
    }

    // Store the byte sequence as a base64-encoded digest, matching CSP and
    // SRI's existing `IntegrityMetadata` implementation.
    parsed_digest.SetDigest(Base64Encode(base::as_byte_span(digest)));
    integrity_metadata.hashes.insert(parsed_digest.ToPair());
  }

  if (integrity_metadata.hashes.empty()) {
    return std::nullopt;
  }
  return IdentityDigest(integrity_metadata);
}

bool IdentityDigest::DoesMatch(WTF::SegmentedBuffer* data) {
  for (const IntegrityMetadata& digest : integrity_metadata_.hashes) {
    HashAlgorithm algorithm = GetHashAlgorithm(digest.Algorithm());
    DigestValue computed_digest;
    if (!ComputeDigest(algorithm, data, computed_digest)) {
      // TODO(https://crbug.com/381044049): Emit errors.
      return false;
    }

    // Convert the stored digest into a `DigestValue`
    Vector<char> digest_bytes;
    Base64Decode(digest.Digest(), digest_bytes);
    DigestValue expected_digest(base::as_byte_span(digest_bytes));

    // If any specified digest doesn't match the digest computed over |data|,
    // matching fails.
    if (computed_digest != expected_digest) {
      // TODO(https://crbug.com/381044049): Emit errors.
      return false;
    }
  }

  // If no digest failed to match (or if we didn't have any digests in the
  // first place), matching succeeded.
  return true;
}

}  // namespace blink
