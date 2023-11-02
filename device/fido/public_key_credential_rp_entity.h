// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_RP_ENTITY_H_
#define DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_RP_ENTITY_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "components/cbor/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

// PublicKeyCredentialRpEntity identifies the web application creating or
// challenging a WebAuthn credential.
//
// https://www.w3.org/TR/webauthn/#dictdef-publickeycredentialrpentity
struct COMPONENT_EXPORT(DEVICE_FIDO) PublicKeyCredentialRpEntity {
 public:
  static absl::optional<PublicKeyCredentialRpEntity> CreateFromCBORValue(
      const cbor::Value& cbor);

  PublicKeyCredentialRpEntity();
  explicit PublicKeyCredentialRpEntity(std::string id);
  PublicKeyCredentialRpEntity(std::string id, absl::optional<std::string> name);
  PublicKeyCredentialRpEntity(const PublicKeyCredentialRpEntity& other);
  PublicKeyCredentialRpEntity(PublicKeyCredentialRpEntity&& other);
  PublicKeyCredentialRpEntity& operator=(
      const PublicKeyCredentialRpEntity& other);
  PublicKeyCredentialRpEntity& operator=(PublicKeyCredentialRpEntity&& other);
  bool operator==(const PublicKeyCredentialRpEntity& other) const;
  ~PublicKeyCredentialRpEntity();

  std::string id;
  absl::optional<std::string> name;
};

cbor::Value AsCBOR(const PublicKeyCredentialRpEntity&);

}  // namespace device

#endif  // DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_RP_ENTITY_H_
