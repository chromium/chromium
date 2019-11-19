// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CREDENTIAL_MANAGEMENT_H_
#define DEVICE_FIDO_CREDENTIAL_MANAGEMENT_H_

#include "base/component_export.h"
#include "base/optional.h"
#include "device/fido/fido_constants.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_rp_entity.h"
#include "device/fido/public_key_credential_user_entity.h"

namespace cbor {
class Value;
}

namespace device {

namespace pin {
struct EmptyResponse;
}

// https://drafts.fidoalliance.org/fido-2/latest/fido-client-to-authenticator-protocol-v2.0-wd-20190409.html#authenticatorCredentialManagement

enum class CredentialManagementRequestKey : uint8_t {
  kSubCommand = 0x01,
  kSubCommandParams = 0x02,
  kPinProtocol = 0x03,
  kPinAuth = 0x04,
};

enum class CredentialManagementRequestParamKey : uint8_t {
  kRPIDHash = 0x01,
  kCredentialID = 0x02,
};

enum class CredentialManagementResponseKey : uint8_t {
  kExistingResidentCredentialsCount = 0x01,
  kMaxPossibleRemainingResidentCredentialsCount = 0x02,
  kRP = 0x03,
  kRPIDHash = 0x04,
  kTotalRPs = 0x05,
  kUser = 0x06,
  kCredentialID = 0x07,
  kPublicKey = 0x08,
  kTotalCredentials = 0x09,
  kCredProtect = 0x0a,
};

enum class CredentialManagementSubCommand : uint8_t {
  kGetCredsMetadata = 0x01,
  kEnumerateRPsBegin = 0x02,
  kEnumerateRPsGetNextRP = 0x03,
  kEnumerateCredentialsBegin = 0x04,
  kEnumerateCredentialsGetNextCredential = 0x05,
  kDeleteCredential = 0x06,
};

// CredentialManagementPreviewRequestAdapter wraps any credential management
// request struct in order to replace the authenticatorCredentialManagement
// command byte returned by the static EncodeAsCBOR() method (0x0a) with its
// vendor-specific preview equivalent (0x41).
template <class T>
class CredentialManagementPreviewRequestAdapter {
 public:
  static std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
  EncodeAsCBOR(const CredentialManagementPreviewRequestAdapter<T>& request) {
    auto result = T::EncodeAsCBOR(request.wrapped_request_);
    DCHECK_EQ(result.first,
              CtapRequestCommand::kAuthenticatorCredentialManagement);
    result.first =
        CtapRequestCommand::kAuthenticatorCredentialManagementPreview;
    return result;
  }

  CredentialManagementPreviewRequestAdapter(T request)
      : wrapped_request_(std::move(request)) {}

 private:
  T wrapped_request_;
};

// CredentialManagementRequest is an authenticatorCredentialManagement(0x0a)
// CTAP2 request. Instances can be obtained via one of the subcommand-specific
// static factory methods.
struct CredentialManagementRequest {
  static std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
  EncodeAsCBOR(const CredentialManagementRequest&);

  enum Version {
    kDefault,
    kPreview,
  };

  static CredentialManagementRequest ForGetCredsMetadata(
      Version version,
      base::span<const uint8_t> pin_token);
  static CredentialManagementRequest ForEnumerateRPsBegin(
      Version version,
      base::span<const uint8_t> pin_token);
  static CredentialManagementRequest ForEnumerateRPsGetNext(Version version);
  static CredentialManagementRequest ForEnumerateCredentialsBegin(
      Version version,
      base::span<const uint8_t> pin_token,
      std::array<uint8_t, kRpIdHashLength> rp_id_hash);
  static CredentialManagementRequest ForEnumerateCredentialsGetNext(
      Version version);
  static CredentialManagementRequest ForDeleteCredential(
      Version version,
      base::span<const uint8_t> pin_token,
      const PublicKeyCredentialDescriptor& credential_id);

  CredentialManagementRequest(CredentialManagementRequest&&);
  CredentialManagementRequest& operator=(CredentialManagementRequest&&);
  ~CredentialManagementRequest();

  Version version;
  CredentialManagementSubCommand subcommand;
  base::Optional<cbor::Value::MapValue> params;
  base::Optional<std::array<uint8_t, 16>> pin_auth;

 private:
  CredentialManagementRequest() = delete;
  CredentialManagementRequest(Version version,
                              CredentialManagementSubCommand subcommand,
                              base::Optional<cbor::Value::MapValue> params,
                              base::Optional<std::array<uint8_t, 16>> pin_auth);
  CredentialManagementRequest(const CredentialManagementRequest&) = delete;
  CredentialManagementRequest& operator=(const CredentialManagementRequest&) =
      delete;
};

struct CredentialsMetadataResponse {
  static base::Optional<CredentialsMetadataResponse> Parse(
      const base::Optional<cbor::Value>& cbor_response);

  size_t num_existing_credentials;
  size_t num_estimated_remaining_credentials;

 private:
  CredentialsMetadataResponse() = default;
};

struct EnumerateRPsResponse {
  static base::Optional<EnumerateRPsResponse> Parse(
      bool expect_rp_count,
      const base::Optional<cbor::Value>& cbor_response);

  // StringFixupPredicate indicates which fields of an EnumerateRPsResponse may
  // contain truncated UTF-8 strings. See
  // |Ctap2DeviceOperation::CBORPathPredicate|.
  static bool StringFixupPredicate(const std::vector<const cbor::Value*>& path);

  EnumerateRPsResponse(EnumerateRPsResponse&&);
  EnumerateRPsResponse& operator=(EnumerateRPsResponse&&);
  ~EnumerateRPsResponse();

  base::Optional<PublicKeyCredentialRpEntity> rp;
  base::Optional<std::array<uint8_t, kRpIdHashLength>> rp_id_hash;
  size_t rp_count;

 private:
  EnumerateRPsResponse(
      base::Optional<PublicKeyCredentialRpEntity> rp,
      base::Optional<std::array<uint8_t, kRpIdHashLength>> rp_id_hash,
      size_t rp_count);
  EnumerateRPsResponse(const EnumerateRPsResponse&) = delete;
  EnumerateRPsResponse& operator=(const EnumerateRPsResponse&) = delete;
};

struct EnumerateCredentialsResponse {
  static base::Optional<EnumerateCredentialsResponse> Parse(
      bool expect_credential_count,
      const base::Optional<cbor::Value>& cbor_response);

  // StringFixupPredicate indicates which fields of an
  // EnumerateCredentialsResponse may contain truncated UTF-8 strings. See
  // |Ctap2DeviceOperation::CBORPathPredicate|.
  static bool StringFixupPredicate(const std::vector<const cbor::Value*>& path);

  EnumerateCredentialsResponse(EnumerateCredentialsResponse&&);
  EnumerateCredentialsResponse& operator=(EnumerateCredentialsResponse&&);
  ~EnumerateCredentialsResponse();

  PublicKeyCredentialUserEntity user;
  PublicKeyCredentialDescriptor credential_id;
  // For convenience, also return the serialized |credential_id| so that the UI
  // doesn't have to do CBOR serialization. (It only cares about the opaque byte
  // string.)
  std::vector<uint8_t> credential_id_cbor_bytes;
  size_t credential_count;

 private:
  EnumerateCredentialsResponse(PublicKeyCredentialUserEntity user,
                               PublicKeyCredentialDescriptor credential_id,
                               size_t credential_count);
  EnumerateCredentialsResponse(const EnumerateCredentialsResponse&) = delete;
  EnumerateCredentialsResponse& operator=(EnumerateCredentialsResponse&) =
      delete;
};

struct COMPONENT_EXPORT(DEVICE_FIDO) AggregatedEnumerateCredentialsResponse {
  AggregatedEnumerateCredentialsResponse(PublicKeyCredentialRpEntity rp);
  AggregatedEnumerateCredentialsResponse(
      AggregatedEnumerateCredentialsResponse&&);
  AggregatedEnumerateCredentialsResponse& operator=(
      AggregatedEnumerateCredentialsResponse&&);
  ~AggregatedEnumerateCredentialsResponse();

  PublicKeyCredentialRpEntity rp;
  std::vector<EnumerateCredentialsResponse> credentials;

 private:
  AggregatedEnumerateCredentialsResponse(
      const AggregatedEnumerateCredentialsResponse&) = delete;
};

using DeleteCredentialResponse = pin::EmptyResponse;

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const CredentialManagementRequest&);

}  // namespace device

#endif  // DEVICE_FIDO_CREDENTIAL_MANAGEMENT_H_
