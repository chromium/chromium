// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/instance_identity_token.h"

#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/values.h"

namespace remoting {

InstanceIdentityToken::InstanceIdentityToken(base::Value::Dict header,
                                             base::Value::Dict payload)
    : header_(std::move(header)), payload_(std::move(payload)) {}

InstanceIdentityToken::~InstanceIdentityToken() = default;

// static
std::optional<InstanceIdentityToken> InstanceIdentityToken::Create(
    std::string_view jwt) {
  // ID Tokens are comprised of three parts separated by periods:
  //   1) Base64 encoded JSON header
  //   2) Base64 encoded JSON payload
  //   3) Signature bytes
  auto parts =
      base::SplitString(jwt, ".", base::WhitespaceHandling::KEEP_WHITESPACE,
                        base::SplitResult::SPLIT_WANT_NONEMPTY);
  // Return early if the token is malformed or missing the signature.
  if (parts.size() != 3) {
    LOG(WARNING) << "Invalid instance identity token: " << jwt;
    return std::nullopt;
  }
  // Parse and validate the header.
  auto encoded_header = parts[0];
  std::string decoded_header;
  if (!base::Base64Decode(encoded_header, &decoded_header,
                          base::Base64DecodePolicy::kForgiving)) {
    LOG(WARNING) << "Failed to decode instance identity token header: "
                 << encoded_header;
    return std::nullopt;
  }
  auto header = base::JSONReader::ReadDict(
      decoded_header, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!header.has_value()) {
    LOG(WARNING) << "Failed to parse instance identity token header: "
                 << decoded_header;
    return std::nullopt;
  }
  if (!header->contains("kid") || !header->contains("alg") ||
      !header->contains("typ")) {
    LOG(WARNING) << "Invalid instance identity token header:\n" << *header;
    return std::nullopt;
  }

  // Parse and validate the payload.
  auto encoded_payload = parts[1];
  std::string decoded_payload;
  if (!base::Base64Decode(encoded_payload, &decoded_payload,
                          base::Base64DecodePolicy::kForgiving)) {
    LOG(WARNING) << "Failed to decode instance identity token payload: "
                 << encoded_payload;
    return std::nullopt;
  }
  auto payload = base::JSONReader::ReadDict(
      decoded_payload, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!payload.has_value()) {
    LOG(WARNING) << "Failed to parse instance identity token payload: "
                 << decoded_payload;
    return std::nullopt;
  }
  if (!payload->contains("iss") || !payload->contains("aud") ||
      !payload->contains("iat") || !payload->contains("exp") ||
      !payload->contains("azp") || !payload->contains("google")) {
    LOG(WARNING) << "Invalid instance identity token payload:\n" << *payload;
    return std::nullopt;
  }
  auto* google_payload = payload->FindDict("google");
  auto* compute_engine_payload =
      google_payload ? google_payload->FindDict("compute_engine") : nullptr;
  if (compute_engine_payload == nullptr ||
      !compute_engine_payload->contains("instance_id") ||
      !compute_engine_payload->contains("instance_name") ||
      !compute_engine_payload->contains("project_id") ||
      !compute_engine_payload->contains("project_number") ||
      !compute_engine_payload->contains("zone")) {
    LOG(WARNING) << "Invalid instance identity token payload:\n" << *payload;
    return std::nullopt;
  }

  return InstanceIdentityToken(std::move(*header), std::move(*payload));
}

std::ostream& operator<<(std::ostream& out,
                         const InstanceIdentityToken& token) {
  out << token.header() << token.payload();
  return out;
}

}  // namespace remoting
