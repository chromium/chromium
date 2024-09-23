// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_USER_ENTITY_H_
#define DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_USER_ENTITY_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "components/cbor/values.h"

namespace device {

// Data structure containing a user id, an optional user name,
// and an optional user display name as specified by the CTAP
// spec. Used as required parameter type for AuthenticatorMakeCredential
// request.
class COMPONENT_EXPORT(DEVICE_FIDO) PublicKeyCredentialUserEntity {
 public:
  // Optional parameters used when serializing the request to CBOR.
  struct SerializationOpts {
    // If true, include an empty display name as a member of the public key
    // credential user entity when serializing to CBOR.
    // Empty display names result in CTAP1_ERR_INVALID_LENGTH on some security
    // keys, but iPhones will refuse to make a passkey if they don't receive the
    // display name.
    bool include_empty_display_name = false;
  };

  static std::optional<PublicKeyCredentialUserEntity> CreateFromCBORValue(
      const cbor::Value& cbor);

  PublicKeyCredentialUserEntity();
  explicit PublicKeyCredentialUserEntity(std::vector<uint8_t> id);
  PublicKeyCredentialUserEntity(std::vector<uint8_t> id,
                                std::optional<std::string> name,
                                std::optional<std::string> display_name);
  PublicKeyCredentialUserEntity(const PublicKeyCredentialUserEntity& other);
  PublicKeyCredentialUserEntity(PublicKeyCredentialUserEntity&& other);
  PublicKeyCredentialUserEntity& operator=(
      const PublicKeyCredentialUserEntity& other);
  PublicKeyCredentialUserEntity& operator=(
      PublicKeyCredentialUserEntity&& other);
  bool operator==(const PublicKeyCredentialUserEntity& other) const;
  ~PublicKeyCredentialUserEntity();

  std::vector<uint8_t> id;
  std::optional<std::string> name;
  std::optional<std::string> display_name;

  // Options governing the serialization of the request to CBOR.
  SerializationOpts serialization_options;
};

cbor::Value AsCBOR(const PublicKeyCredentialUserEntity&);

}  // namespace device

#endif  // DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_USER_ENTITY_H_
