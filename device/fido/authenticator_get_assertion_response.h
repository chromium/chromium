// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_AUTHENTICATOR_GET_ASSERTION_RESPONSE_H_
#define DEVICE_FIDO_AUTHENTICATOR_GET_ASSERTION_RESPONSE_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/optional.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_user_entity.h"
#include "device/fido/response_data.h"

namespace device {

// Represents response from authenticators for AuthenticatorGetAssertion and
// AuthenticatorGetNextAssertion requests.
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#authenticatorGetAssertion
class COMPONENT_EXPORT(DEVICE_FIDO) AuthenticatorGetAssertionResponse
    : public ResponseData {
 public:
  static base::Optional<AuthenticatorGetAssertionResponse>
  CreateFromU2fSignResponse(
      base::span<const uint8_t, kRpIdHashLength> relying_party_id_hash,
      base::span<const uint8_t> u2f_data,
      base::span<const uint8_t> key_handle);

  AuthenticatorGetAssertionResponse(AuthenticatorData authenticator_data,
                                    std::vector<uint8_t> signature);
  AuthenticatorGetAssertionResponse(AuthenticatorGetAssertionResponse&& that);
  AuthenticatorGetAssertionResponse& operator=(
      AuthenticatorGetAssertionResponse&& other);
  ~AuthenticatorGetAssertionResponse() override;

  // ResponseData:
  const std::array<uint8_t, kRpIdHashLength>& GetRpIdHash() const override;

  AuthenticatorGetAssertionResponse& SetCredential(
      PublicKeyCredentialDescriptor credential);
  AuthenticatorGetAssertionResponse& SetUserEntity(
      PublicKeyCredentialUserEntity user_entity);
  AuthenticatorGetAssertionResponse& SetNumCredentials(uint8_t num_credentials);

  const base::Optional<PublicKeyCredentialDescriptor>& credential() const {
    return credential_;
  }
  const AuthenticatorData& auth_data() const { return authenticator_data_; }
  const std::vector<uint8_t>& signature() const { return signature_; }
  const base::Optional<PublicKeyCredentialUserEntity>& user_entity() const {
    return user_entity_;
  }
  const base::Optional<uint8_t>& num_credentials() const {
    return num_credentials_;
  }

  base::Optional<std::array<uint8_t, kLargeBlobKeyLength>> large_blob_key()
      const {
    return large_blob_key_;
  }
  void set_large_blob_key(
      const base::span<const uint8_t, kLargeBlobKeyLength> large_blob_key);
  base::Optional<std::vector<uint8_t>> large_blob() const {
    return large_blob_;
  }
  void set_large_blob(base::Optional<std::vector<uint8_t>> large_blob) {
    large_blob_ = std::move(large_blob);
  }
  bool large_blob_written() const { return large_blob_written_; }
  void set_large_blob_written(bool large_blob_written) {
    large_blob_written_ = large_blob_written;
  }

  // hmac_secret contains the output of the hmac_secret extension.
  base::Optional<base::span<const uint8_t>> hmac_secret() const;
  void set_hmac_secret(std::vector<uint8_t>);

  // hmac_secret_not_evaluated will be true in cases where the
  // |FidoAuthenticator| was unable to process the extension, even though it
  // supports hmac_secret in general. This is intended for a case of Windows,
  // where some versions of webauthn.dll can only express the extension for
  // makeCredential, not getAssertion.
  bool hmac_secret_not_evaluated() const;
  void set_hmac_secret_not_evaluated(bool);

 private:
  base::Optional<PublicKeyCredentialDescriptor> credential_;
  AuthenticatorData authenticator_data_;
  std::vector<uint8_t> signature_;
  base::Optional<PublicKeyCredentialUserEntity> user_entity_;
  base::Optional<uint8_t> num_credentials_;
  base::Optional<std::vector<uint8_t>> hmac_secret_;
  bool hmac_secret_not_evaluated_ = false;

  // The large blob key associated to the credential. This value is only
  // returned if the assertion request contains the largeBlobKey extension on a
  // capable authenticator and the credential has an associated large blob key.
  base::Optional<std::array<uint8_t, kLargeBlobKeyLength>> large_blob_key_;

  // The large blob associated with the credential.
  base::Optional<std::vector<uint8_t>> large_blob_;

  // Whether a large blob was successfully written as part of this GetAssertion
  // request.
  bool large_blob_written_ = false;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorGetAssertionResponse);
};

}  // namespace device

#endif  // DEVICE_FIDO_AUTHENTICATOR_GET_ASSERTION_RESPONSE_H_
