// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/credential_management.h"

#include "base/logging.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/pin.h"
#include "third_party/boringssl/src/include/openssl/hmac.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace device {

namespace {
std::array<uint8_t, 16> MakePINAuth(base::span<const uint8_t> pin_token,
                                    base::span<const uint8_t> pin_auth_bytes) {
  DCHECK(!pin_token.empty() && !pin_auth_bytes.empty());
  std::array<uint8_t, SHA256_DIGEST_LENGTH> hmac;
  unsigned hmac_len;
  CHECK(HMAC(EVP_sha256(), pin_token.data(), pin_token.size(),
             pin_auth_bytes.data(), pin_auth_bytes.size(), hmac.data(),
             &hmac_len));
  DCHECK_EQ(hmac.size(), static_cast<size_t>(hmac_len));
  std::array<uint8_t, 16> pin_auth;
  std::copy(hmac.begin(), hmac.begin() + 16, pin_auth.begin());
  return pin_auth;
}
}  // namespace

CredentialManagementRequest::CredentialManagementRequest(
    Version version_,
    CredentialManagementSubCommand subcommand_,
    base::Optional<cbor::Value::MapValue> params_,
    base::Optional<std::array<uint8_t, 16>> pin_auth_)
    : version(version_),
      subcommand(subcommand_),
      params(std::move(params_)),
      pin_auth(std::move(pin_auth_)) {}
CredentialManagementRequest::CredentialManagementRequest(
    CredentialManagementRequest&&) = default;
CredentialManagementRequest& CredentialManagementRequest::operator=(
    CredentialManagementRequest&&) = default;
CredentialManagementRequest::~CredentialManagementRequest() = default;

// static
CredentialManagementRequest CredentialManagementRequest::ForGetCredsMetadata(
    Version version,
    base::span<const uint8_t> pin_token) {
  return CredentialManagementRequest(
      version, CredentialManagementSubCommand::kGetCredsMetadata,
      /*params=*/base::nullopt,
      MakePINAuth(pin_token,
                  {{static_cast<uint8_t>(
                      CredentialManagementSubCommand::kGetCredsMetadata)}}));
}

// static
CredentialManagementRequest CredentialManagementRequest::ForEnumerateRPsBegin(
    Version version,
    base::span<const uint8_t> pin_token) {
  return CredentialManagementRequest(
      version, CredentialManagementSubCommand::kEnumerateRPsBegin,
      /*params=*/base::nullopt,
      MakePINAuth(pin_token,
                  {{static_cast<uint8_t>(
                      CredentialManagementSubCommand::kEnumerateRPsBegin)}}));
}

// static
CredentialManagementRequest CredentialManagementRequest::ForEnumerateRPsGetNext(
    Version version) {
  return CredentialManagementRequest(
      version, CredentialManagementSubCommand::kEnumerateRPsGetNextRP,
      /*params=*/base::nullopt,
      /*pin_auth=*/base::nullopt);
}

// static
CredentialManagementRequest
CredentialManagementRequest::ForEnumerateCredentialsBegin(
    Version version,
    base::span<const uint8_t> pin_token,
    std::array<uint8_t, kRpIdHashLength> rp_id_hash) {
  cbor::Value::MapValue params_map;
  params_map.emplace(
      static_cast<int>(CredentialManagementRequestParamKey::kRPIDHash),
      std::move(rp_id_hash));
  base::Optional<std::vector<uint8_t>> pin_auth_bytes =
      cbor::Writer::Write(cbor::Value(params_map));
  DCHECK(pin_auth_bytes);
  pin_auth_bytes->insert(
      pin_auth_bytes->begin(),
      static_cast<uint8_t>(
          CredentialManagementSubCommand::kEnumerateCredentialsBegin));
  return CredentialManagementRequest(
      version, CredentialManagementSubCommand::kEnumerateCredentialsBegin,
      std::move(params_map), MakePINAuth(pin_token, *pin_auth_bytes));
}

// static
CredentialManagementRequest
CredentialManagementRequest::ForEnumerateCredentialsGetNext(Version version) {
  return CredentialManagementRequest(
      version,
      CredentialManagementSubCommand::kEnumerateCredentialsGetNextCredential,
      /*params=*/base::nullopt, /*pin_auth=*/base::nullopt);
}

// static
CredentialManagementRequest CredentialManagementRequest::ForDeleteCredential(
    Version version,
    base::span<const uint8_t> pin_token,
    const PublicKeyCredentialDescriptor& credential_id) {
  cbor::Value::MapValue params_map;
  params_map.emplace(
      static_cast<int>(CredentialManagementRequestParamKey::kCredentialID),
      AsCBOR(credential_id));
  base::Optional<std::vector<uint8_t>> pin_auth_bytes =
      cbor::Writer::Write(cbor::Value(params_map));
  DCHECK(pin_auth_bytes);
  pin_auth_bytes->insert(
      pin_auth_bytes->begin(),
      static_cast<uint8_t>(CredentialManagementSubCommand::kDeleteCredential));
  return CredentialManagementRequest(
      version, CredentialManagementSubCommand::kDeleteCredential,
      std::move(params_map), MakePINAuth(pin_token, *pin_auth_bytes));
}

// static
base::Optional<CredentialsMetadataResponse> CredentialsMetadataResponse::Parse(
    const base::Optional<cbor::Value>& cbor_response) {
  CredentialsMetadataResponse response;

  if (!cbor_response || !cbor_response->is_map()) {
    return base::nullopt;
  }
  const cbor::Value::MapValue& response_map = cbor_response->GetMap();

  auto it = response_map.find(cbor::Value(static_cast<int>(
      CredentialManagementResponseKey::kExistingResidentCredentialsCount)));
  if (it == response_map.end() || !it->second.is_unsigned()) {
    return base::nullopt;
  }
  const int64_t existing_count = it->second.GetUnsigned();
  if (existing_count > std::numeric_limits<size_t>::max()) {
    return base::nullopt;
  }
  response.num_existing_credentials = static_cast<size_t>(existing_count);

  it = response_map.find(cbor::Value(
      static_cast<int>(CredentialManagementResponseKey::
                           kMaxPossibleRemainingResidentCredentialsCount)));
  if (it == response_map.end() || !it->second.is_unsigned()) {
    return base::nullopt;
  }
  const int64_t remaining_count = it->second.GetUnsigned();
  if (remaining_count > std::numeric_limits<size_t>::max()) {
    return base::nullopt;
  }
  response.num_estimated_remaining_credentials =
      static_cast<size_t>(remaining_count);

  return response;
}

// static
base::Optional<EnumerateRPsResponse> EnumerateRPsResponse::Parse(
    bool expect_rp_count,
    const base::Optional<cbor::Value>& cbor_response) {
  if (!cbor_response) {
    // Some authenticators send an empty response if there are no RPs (though
    // the spec doesn't say that).
    return EnumerateRPsResponse(base::nullopt, base::nullopt, 0);
  }
  if (!cbor_response->is_map() || cbor_response->GetMap().empty()) {
    return base::nullopt;
  }
  const cbor::Value::MapValue& response_map = cbor_response->GetMap();

  size_t rp_count = 0;
  auto it = response_map.find(cbor::Value(
      static_cast<int>(CredentialManagementResponseKey::kTotalRPs)));
  if (!expect_rp_count && it != response_map.end()) {
    return base::nullopt;
  }
  if (expect_rp_count) {
    if (it == response_map.end() || !it->second.is_unsigned() ||
        it->second.GetUnsigned() > std::numeric_limits<size_t>::max()) {
      return base::nullopt;
    }
    rp_count = static_cast<size_t>(it->second.GetUnsigned());
    if (rp_count == 0) {
      if (response_map.size() != 1) {
        return base::nullopt;
      }
      return EnumerateRPsResponse(base::nullopt, base::nullopt, 0);
    }
  }

  it = response_map.find(
      cbor::Value(static_cast<int>(CredentialManagementResponseKey::kRP)));
  if (it == response_map.end()) {
    return base::nullopt;
  }
  auto opt_rp = PublicKeyCredentialRpEntity::CreateFromCBORValue(it->second);
  if (!opt_rp) {
    return base::nullopt;
  }

  it = response_map.find(cbor::Value(
      static_cast<int>(CredentialManagementResponseKey::kRPIDHash)));
  if (it == response_map.end() || !it->second.is_bytestring()) {
    return base::nullopt;
  }
  const std::vector<uint8_t>& rp_id_hash_bytes = it->second.GetBytestring();
  if (rp_id_hash_bytes.size() != kRpIdHashLength) {
    return base::nullopt;
  }
  std::array<uint8_t, kRpIdHashLength> rp_id_hash;
  std::copy_n(rp_id_hash_bytes.begin(), kRpIdHashLength, rp_id_hash.begin());

  return EnumerateRPsResponse(std::move(*opt_rp), std::move(rp_id_hash),
                              rp_count);
}

// static
bool EnumerateRPsResponse::StringFixupPredicate(
    const std::vector<const cbor::Value*>& path) {
  // In the rp field (0x04), "name" may be truncated.
  if (path.size() != 2 || !path[0]->is_unsigned() ||
      path[0]->GetUnsigned() !=
          static_cast<int>(CredentialManagementResponseKey::kRP) ||
      !path[1]->is_string()) {
    return false;
  }
  return path[1]->GetString() == "name";
}

EnumerateRPsResponse::EnumerateRPsResponse(EnumerateRPsResponse&&) = default;
EnumerateRPsResponse& EnumerateRPsResponse::operator=(EnumerateRPsResponse&&) =
    default;
EnumerateRPsResponse::~EnumerateRPsResponse() = default;
EnumerateRPsResponse::EnumerateRPsResponse(
    base::Optional<PublicKeyCredentialRpEntity> rp_,
    base::Optional<std::array<uint8_t, kRpIdHashLength>> rp_id_hash_,
    size_t rp_count_)
    : rp(std::move(rp_)),
      rp_id_hash(std::move(rp_id_hash_)),
      rp_count(rp_count_) {}

//  static
base::Optional<EnumerateCredentialsResponse>
EnumerateCredentialsResponse::Parse(
    bool expect_credential_count,
    const base::Optional<cbor::Value>& cbor_response) {
  if (!cbor_response || !cbor_response->is_map()) {
    // Note that some authenticators may send an empty response if they don't
    // have a credential for a given RP ID hash (though the spec doesn't say
    // that). However, that case should not be reached from
    // CredentialManagementHandler.
    return base::nullopt;
  }
  const cbor::Value::MapValue& response_map = cbor_response->GetMap();

  auto it = response_map.find(
      cbor::Value(static_cast<int>(CredentialManagementResponseKey::kUser)));
  if (it == response_map.end()) {
    return base::nullopt;
  }
  auto opt_user =
      PublicKeyCredentialUserEntity::CreateFromCBORValue(it->second);
  if (!opt_user) {
    return base::nullopt;
  }

  it = response_map.find(cbor::Value(
      static_cast<int>(CredentialManagementResponseKey::kCredentialID)));
  if (it == response_map.end()) {
    return base::nullopt;
  }
  auto opt_credential_id =
      PublicKeyCredentialDescriptor::CreateFromCBORValue(it->second);
  if (!opt_credential_id) {
    return base::nullopt;
  }

  // Ignore the public key's value.
  it = response_map.find(cbor::Value(
      static_cast<int>(CredentialManagementResponseKey::kPublicKey)));
  if (it == response_map.end() || !it->second.is_map()) {
    return base::nullopt;
  }

  size_t credential_count = 0;
  if (!expect_credential_count) {
    if (response_map.find(cbor::Value(static_cast<int>(
            CredentialManagementResponseKey::kTotalCredentials))) !=
        response_map.end()) {
      return base::nullopt;
    }
  } else {
    it = response_map.find(cbor::Value(
        static_cast<int>(CredentialManagementResponseKey::kTotalCredentials)));
    if (it == response_map.end() || !it->second.is_unsigned() ||
        it->second.GetUnsigned() > std::numeric_limits<size_t>::max()) {
      return base::nullopt;
    }
    credential_count = static_cast<size_t>(it->second.GetUnsigned());
  }
  return EnumerateCredentialsResponse(
      std::move(*opt_user), std::move(*opt_credential_id), credential_count);
}

// static
bool EnumerateCredentialsResponse::StringFixupPredicate(
    const std::vector<const cbor::Value*>& path) {
  // In the user field (0x06), "name" or "displayName" may be truncated.
  if (path.size() != 2 || !path[0]->is_unsigned() ||
      path[0]->GetUnsigned() !=
          static_cast<int>(CredentialManagementResponseKey::kUser) ||
      !path[1]->is_string()) {
    return false;
  }
  const std::string& user_key = path[1]->GetString();
  return user_key == "name" || user_key == "displayName";
}

EnumerateCredentialsResponse::EnumerateCredentialsResponse(
    EnumerateCredentialsResponse&&) = default;
EnumerateCredentialsResponse& EnumerateCredentialsResponse::operator=(
    EnumerateCredentialsResponse&&) = default;
EnumerateCredentialsResponse::~EnumerateCredentialsResponse() = default;
EnumerateCredentialsResponse::EnumerateCredentialsResponse(
    PublicKeyCredentialUserEntity user_,
    PublicKeyCredentialDescriptor credential_id_,
    size_t credential_count_)
    : user(std::move(user_)),
      credential_id(std::move(credential_id_)),
      credential_count(credential_count_) {
  credential_id_cbor_bytes = *cbor::Writer::Write(AsCBOR(credential_id));
}

AggregatedEnumerateCredentialsResponse::AggregatedEnumerateCredentialsResponse(
    PublicKeyCredentialRpEntity rp_)
    : rp(std::move(rp_)), credentials() {}
AggregatedEnumerateCredentialsResponse::AggregatedEnumerateCredentialsResponse(
    AggregatedEnumerateCredentialsResponse&&) = default;
AggregatedEnumerateCredentialsResponse& AggregatedEnumerateCredentialsResponse::
operator=(AggregatedEnumerateCredentialsResponse&&) = default;
AggregatedEnumerateCredentialsResponse::
    ~AggregatedEnumerateCredentialsResponse() = default;

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const CredentialManagementRequest& request) {
  cbor::Value::MapValue request_map;
  request_map.emplace(
      static_cast<int>(CredentialManagementRequestKey::kSubCommand),
      static_cast<int>(request.subcommand));
  if (request.params) {
    request_map.emplace(
        static_cast<int>(CredentialManagementRequestKey::kSubCommandParams),
        *request.params);
  }
  if (request.pin_auth) {
    request_map.emplace(
        static_cast<int>(CredentialManagementRequestKey::kPinProtocol),
        static_cast<int>(pin::kProtocolVersion));
    request_map.emplace(
        static_cast<int>(CredentialManagementRequestKey::kPinAuth),
        *request.pin_auth);
  }
  return {request.version == CredentialManagementRequest::kPreview
              ? CtapRequestCommand::kAuthenticatorCredentialManagementPreview
              : CtapRequestCommand::kAuthenticatorCredentialManagement,
          cbor::Value(std::move(request_map))};
}

}  // namespace device
