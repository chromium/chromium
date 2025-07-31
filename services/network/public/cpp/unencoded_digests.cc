// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/unencoded_digests.h"

#include <optional>
#include <vector>

#include "base/base64.h"
#include "base/containers/contains.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/integrity_metadata.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "url/gurl.h"

namespace network {

namespace {

constexpr char kUnencodedDigest[] = "unencoded-digest";
constexpr char kSha256Label[] = "sha-256";
constexpr char kSha512Label[] = "sha-512";

const size_t kSha256DigestLength = 32;
const size_t kSha512DigestLength = 64;

}  // namespace

mojom::UnencodedDigestsPtr ParseUnencodedDigestsFromHeaders(
    const net::HttpResponseHeaders& headers) {
  auto parsed_headers = mojom::UnencodedDigests::New();

  std::optional<std::string> header =
      headers.GetNormalizedHeader(kUnencodedDigest);
  if (!header) {
    return parsed_headers;
  }

  std::optional<net::structured_headers::Dictionary> dictionary =
      net::structured_headers::ParseDictionary(*header);
  if (!dictionary) {
    parsed_headers->issues.emplace_back(
        mojom::UnencodedDigestIssue::kMalformedDictionary);
    return parsed_headers;
  }

  for (const auto& entry : dictionary.value()) {
    network::IntegrityMetadata parsed_digest;
    size_t expected_length = 0;

    if (entry.first == kSha256Label) {
      parsed_digest.algorithm = mojom::IntegrityAlgorithm::kSha256;
      expected_length = kSha256DigestLength;
    } else if (entry.first == kSha512Label) {
      parsed_digest.algorithm = mojom::IntegrityAlgorithm::kSha512;
      expected_length = kSha512DigestLength;
    } else {
      // We only accept SHA-256 and SHA-512; skip over digests using unknown
      // algorithm tokens.
      parsed_headers->issues.emplace_back(
          mojom::UnencodedDigestIssue::kUnknownAlgorithm);
      continue;
    }

    // Skip entries that cannot be parsed as byte sequences.
    if (entry.second.member_is_inner_list || entry.second.member.empty() ||
        !entry.second.member[0].item.is_byte_sequence()) {
      parsed_headers->issues.emplace_back(
          mojom::UnencodedDigestIssue::kIncorrectDigestType);
      continue;
    }

    // Store the digest after converting it from a string into a vector.
    std::string digest_string = entry.second.member[0].item.GetString();
    if (digest_string.size() != expected_length) {
      parsed_headers->issues.emplace_back(
          mojom::UnencodedDigestIssue::kIncorrectDigestLength);
      continue;
    }
    parsed_digest.value.assign(digest_string.begin(), digest_string.end());

    parsed_headers->digests.emplace_back(std::move(parsed_digest));
  }

  return parsed_headers;
}

void ReportUnencodedDigestIssuesToDevtools(
    const mojom::UnencodedDigestsPtr& digests,
    const raw_ptr<mojom::DevToolsObserver> devtools_observer,
    const std::string& devtools_request_id,
    const GURL& request_url) {
  if (!devtools_observer || devtools_request_id.empty()) {
    return;
  }

  for (const mojom::UnencodedDigestIssue issue : digests->issues) {
    devtools_observer->OnUnencodedDigestError(devtools_request_id, request_url,
                                              issue);
  }
}

}  // namespace network
