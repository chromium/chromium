// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_USER_ENTITY_H_
#define DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_USER_ENTITY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/component_export.h"
#include "components/cbor/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

// Data structure containing a user id, an optional user name,
// and an optional user display name as specified by the CTAP
// spec. Used as required parameter type for AuthenticatorMakeCredential
// request.
class COMPONENT_EXPORT(DEVICE_FIDO) PublicKeyCredentialUserEntity {
 public:
  static absl::optional<PublicKeyCredentialUserEntity> CreateFromCBORValue(
      const cbor::Value& cbor);

  PublicKeyCredentialUserEntity();
  explicit PublicKeyCredentialUserEntity(std::vector<uint8_t> id);
  PublicKeyCredentialUserEntity(std::vector<uint8_t> id,
                                absl::optional<std::string> name,
                                absl::optional<std::string> display_name);
  PublicKeyCredentialUserEntity(const PublicKeyCredentialUserEntity& other);
  PublicKeyCredentialUserEntity(PublicKeyCredentialUserEntity&& other);
  PublicKeyCredentialUserEntity& operator=(
      const PublicKeyCredentialUserEntity& other);
  PublicKeyCredentialUserEntity& operator=(
      PublicKeyCredentialUserEntity&& other);
  bool operator==(const PublicKeyCredentialUserEntity& other) const;
  ~PublicKeyCredentialUserEntity();

  std::vector<uint8_t> id;
  absl::optional<std::string> name;
  absl::optional<std::string> display_name;
};

cbor::Value AsCBOR(const PublicKeyCredentialUserEntity&);

}  // namespace device

#endif  // DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_USER_ENTITY_H_
