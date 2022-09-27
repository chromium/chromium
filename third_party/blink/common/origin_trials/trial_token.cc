// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/origin_trials/trial_token.h"

#include "base/base64.h"
#include "base/big_endian.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

namespace {

// Token payloads can be at most 4KB in size, as a guard against trying to parse
// excessively large tokens (see crbug.com/802377). The origin is the only part
// of the payload that is user-supplied. The 4KB payload limit allows for the
// origin to be ~3900 chars. In some cases, 2KB is suggested as the practical
// limit for URLs, e.g.:
// https://stackoverflow.com/questions/417142/what-is-the-maximum-length-of-a-url-in-different-browsers
// This means tokens can contain origins that are nearly twice as long as any
// expected to be seen in the wild.
const size_t kMaxPayloadSize = 4096;
// Encoded tokens can be at most 6KB in size. Based on the 4KB payload limit,
// this allows for the payload, signature, and other format bits, plus the
// Base64 encoding overhead (~4/3 of the input).
const size_t kMaxTokenSize = 6144;

// Version is a 1-byte field at offset 0.
const size_t kVersionOffset = 0;
const size_t kVersionSize = 1;

// These constants define the Version 2 field sizes and offsets.
const size_t kSignatureOffset = kVersionOffset + kVersionSize;
const size_t kSignatureSize = 64;
const size_t kPayloadLengthOffset = kSignatureOffset + kSignatureSize;
const size_t kPayloadLengthSize = 4;
const size_t kPayloadOffset = kPayloadLengthOffset + kPayloadLengthSize;

// Version 3 introduced support to match tokens against third party origins (see
// design doc
// https://docs.google.com/document/d/1xALH9W7rWmX0FpjudhDeS2TNTEOXuPn4Tlc9VmuPdHA
// for more details).
const uint8_t kVersion3 = 3;
// Version 2 is also currently supported. Version 1 was
// introduced in Chrome M50, and removed in M51. There were no experiments
// enabled in the stable M50 release which would have used those tokens.
const uint8_t kVersion2 = 2;

const char* kUsageSubset = "subset";

}  // namespace

TrialToken::~TrialToken() = default;

// static
std::unique_ptr<TrialToken> TrialToken::From(
    base::StringPiece token_text,
    const OriginTrialPublicKey& public_key,
    OriginTrialTokenStatus* out_status) {
  DCHECK(out_status);
  std::string token_payload;
  std::string token_signature;
  uint8_t token_version;
  *out_status = Extract(token_text, public_key, &token_payload,
                        &token_signature, &token_version);
  if (*out_status != OriginTrialTokenStatus::kSuccess) {
    DVLOG(2) << "Malformed origin trial token found (unable to extract)";
    return nullptr;
  }
  std::unique_ptr<TrialToken> token = Parse(token_payload, token_version);
  if (token) {
    token->signature_ = token_signature;
    *out_status = OriginTrialTokenStatus::kSuccess;
    DVLOG(2) << "Well-formed origin trial token found for feature "
             << token->feature_name();
  } else {
    DVLOG(2) << "Malformed origin trial token found (unable to parse)";
    *out_status = OriginTrialTokenStatus::kMalformed;
  }
  return token;
}

OriginTrialTokenStatus TrialToken::IsValid(const url::Origin& origin,
                                           const base::Time& now) const {
  // The order of these checks is intentional. For example, will only report a
  // token as expired if it is valid for the origin.
  if (!ValidateOrigin(origin)) {
    DVLOG(2) << "Origin trial token from different origin";
    return OriginTrialTokenStatus::kWrongOrigin;
  }
  if (!ValidateDate(now)) {
    DVLOG(2) << "Origin trial token expired";
    return OriginTrialTokenStatus::kExpired;
  }
  return OriginTrialTokenStatus::kSuccess;
}

// static
OriginTrialTokenStatus TrialToken::Extract(
    base::StringPiece token_text,
    const OriginTrialPublicKey& public_key,
    std::string* out_token_payload,
    std::string* out_token_signature,
    uint8_t* out_token_version) {
  if (token_text.empty()) {
    return OriginTrialTokenStatus::kMalformed;
  }

  // Protect against attempting to extract arbitrarily large tokens.
  // See crbug.com/802377.
  if (token_text.length() > kMaxTokenSize) {
    return OriginTrialTokenStatus::kMalformed;
  }

  // Token is base64-encoded; decode first.
  std::string token_contents;
  if (!base::Base64Decode(token_text, &token_contents)) {
    return OriginTrialTokenStatus::kMalformed;
  }

  // Only version 2 and 3 currently supported.
  if (token_contents.length() < (kVersionOffset + kVersionSize)) {
    return OriginTrialTokenStatus::kMalformed;
  }
  uint8_t version = token_contents[kVersionOffset];
  if (version != kVersion2 && version != kVersion3) {
    return OriginTrialTokenStatus::kWrongVersion;
  }

  // Token must be large enough to contain a version, signature, and payload
  // length.
  if (token_contents.length() < (kPayloadLengthOffset + kPayloadLengthSize)) {
    return OriginTrialTokenStatus::kMalformed;
  }

  // Extract the length of the signed data (Big-endian).
  uint32_t payload_length;
  base::ReadBigEndian(
      reinterpret_cast<const uint8_t*>(&(token_contents[kPayloadLengthOffset])),
      &payload_length);

  // Validate that the stated length matches the actual payload length.
  if (payload_length != token_contents.length() - kPayloadOffset) {
    return OriginTrialTokenStatus::kMalformed;
  }

  // Extract the version-specific contents of the token.
  const char* token_bytes = token_contents.data();
  base::StringPiece version_piece(token_bytes + kVersionOffset, kVersionSize);
  base::StringPiece signature(token_bytes + kSignatureOffset, kSignatureSize);
  base::StringPiece payload_piece(token_bytes + kPayloadLengthOffset,
                                  kPayloadLengthSize + payload_length);

  // The data which is covered by the signature is (version + length + payload).
  std::string signed_data = base::StrCat({version_piece, payload_piece});

  // Validate the signature on the data before proceeding.
  if (!TrialToken::ValidateSignature(signature, signed_data, public_key)) {
    return OriginTrialTokenStatus::kInvalidSignature;
  }

  // Return the payload and signature, as new strings.
  *out_token_version = version;
  *out_token_payload = token_contents.substr(kPayloadOffset, payload_length);
  *out_token_signature = std::string(signature);
  return OriginTrialTokenStatus::kSuccess;
}

// static
std::unique_ptr<TrialToken> TrialToken::Parse(const std::string& token_payload,
                                              const uint8_t version) {
  // Protect against attempting to parse arbitrarily large tokens. This check is
  // required here because the fuzzer calls Parse() directly, bypassing the size
  // check in Extract().
  // See crbug.com/802377.
  if (token_payload.length() > kMaxPayloadSize) {
    return nullptr;
  }

  absl::optional<base::Value> datadict = base::JSONReader::Read(token_payload);
  if (!datadict || !datadict->is_dict()) {
    return nullptr;
  }

  // Ensure that the origin is a valid (non-opaque) origin URL.
  std::string* origin_string = datadict->FindStringKey("origin");
  if (!origin_string) {
    return nullptr;
  }
  url::Origin origin = url::Origin::Create(GURL(*origin_string));
  if (origin.opaque()) {
    return nullptr;
  }

  // The |isSubdomain| flag is optional. If found, ensure it is a valid boolean.
  bool is_subdomain = false;
  base::Value* is_subdomain_value = datadict->FindKey("isSubdomain");
  if (is_subdomain_value) {
    if (!is_subdomain_value->is_bool()) {
      return nullptr;
    }
    is_subdomain = is_subdomain_value->GetBool();
  }

  // Ensure that the feature name is a valid string.
  std::string* feature_name = datadict->FindStringKey("feature");
  if (!feature_name || feature_name->empty()) {
    return nullptr;
  }

  // Ensure that the expiry timestamp is a valid (positive) integer.
  int expiry_timestamp = datadict->FindIntKey("expiry").value_or(0);
  if (expiry_timestamp <= 0) {
    return nullptr;
  }

  // Initialize optional version 3 fields to default values.
  bool is_third_party = false;
  UsageRestriction usage = UsageRestriction::kNone;

  if (version == kVersion3) {
    // The |isThirdParty| flag is optional. If found, ensure it is a valid
    // boolean.
    base::Value* is_third_party_value = datadict->FindKey("isThirdParty");
    if (is_third_party_value) {
      if (!is_third_party_value->is_bool()) {
        return nullptr;
      }
      is_third_party = is_third_party_value->GetBool();
    }

    // The |usage| field is optional. If found, ensure its value is either empty
    // or "subset".
    std::string* usage_value = datadict->FindStringKey("usage");
    if (usage_value) {
      if (usage_value->empty()) {
        usage = UsageRestriction::kNone;
      } else if (*usage_value == kUsageSubset) {
        usage = UsageRestriction::kSubset;
      } else {
        return nullptr;
      }
    }
  }

  return base::WrapUnique(new TrialToken(
      origin, is_subdomain, *feature_name,
      base::Time::FromDoubleT(expiry_timestamp), is_third_party, usage));
}

bool TrialToken::ValidateOrigin(const url::Origin& origin) const {
  if (match_subdomains_) {
    return origin.scheme() == origin_.scheme() &&
           origin.DomainIs(origin_.host()) && origin.port() == origin_.port();
  }
  return origin == origin_;
}

bool TrialToken::ValidateFeatureName(base::StringPiece feature_name) const {
  return feature_name == feature_name_;
}

bool TrialToken::ValidateDate(const base::Time& now) const {
  return expiry_time_ > now;
}

// static
bool TrialToken::ValidateSignature(base::StringPiece signature,
                                   const std::string& data,
                                   const OriginTrialPublicKey& public_key) {
  // Signature must be 64 bytes long.
  if (signature.length() != 64) {
    return false;
  }

  int result = ED25519_verify(
      reinterpret_cast<const uint8_t*>(data.data()), data.length(),
      reinterpret_cast<const uint8_t*>(signature.data()), public_key.data());
  return (result != 0);
}

TrialToken::TrialToken(const url::Origin& origin,
                       bool match_subdomains,
                       const std::string& feature_name,
                       base::Time expiry_time,
                       bool is_third_party,
                       UsageRestriction usage_restriction)
    : origin_(origin),
      match_subdomains_(match_subdomains),
      feature_name_(feature_name),
      expiry_time_(expiry_time),
      is_third_party_(is_third_party),
      usage_restriction_(usage_restriction) {}

// static
std::unique_ptr<TrialToken> TrialToken::CreateTrialTokenForTesting(
    const url::Origin& origin,
    bool match_subdomains,
    const std::string& feature_name,
    base::Time expiry_time,
    bool is_third_party,
    UsageRestriction usage_restriction) {
  return base::WrapUnique(new TrialToken(origin, match_subdomains, feature_name,
                                         expiry_time, is_third_party,
                                         usage_restriction));
}

}  // namespace blink
