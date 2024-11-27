// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/identity_digest.h"

#include "base/containers/span.h"
#include "net/http/structured_headers.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {

namespace {
const char kSHA256Token[] = "sha-256";
const char kSHA384Token[] = "sha-384";
const char kSHA512Token[] = "sha-512";
}  // namespace

IdentityDigest::IdentityDigest(IntegrityMetadataSet digests)
    : digests_(digests) {}

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

  IntegrityMetadataSet digests;
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
    digests.insert(parsed_digest.ToPair());
  }

  if (digests.empty()) {
    return std::nullopt;
  }
  return IdentityDigest(digests);
}

}  // namespace blink
