// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_PARAMS_H_
#define DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_PARAMS_H_

#include <string>
#include <tuple>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "components/cbor/values.h"
#include "device/fido/fido_constants.h"

namespace device {

// Data structure containing public key credential type(string) and
// cryptographic algorithm(integer) as specified by the CTAP spec. Used as a
// request parameter for AuthenticatorMakeCredential.
class COMPONENT_EXPORT(DEVICE_FIDO) PublicKeyCredentialParams {
 public:
  struct COMPONENT_EXPORT(DEVICE_FIDO) CredentialInfo {
    bool operator==(const CredentialInfo& other) const;
    CredentialType type = CredentialType::kPublicKey;
    int algorithm = base::strict_cast<int>(CoseAlgorithmIdentifier::kCoseEs256);
  };

  static base::Optional<PublicKeyCredentialParams> CreateFromCBORValue(
      const cbor::Value& cbor_value);

  explicit PublicKeyCredentialParams(
      std::vector<CredentialInfo> credential_params);
  PublicKeyCredentialParams(const PublicKeyCredentialParams& other);
  PublicKeyCredentialParams(PublicKeyCredentialParams&& other);
  PublicKeyCredentialParams& operator=(const PublicKeyCredentialParams& other);
  PublicKeyCredentialParams& operator=(PublicKeyCredentialParams&& other);
  ~PublicKeyCredentialParams();

  const std::vector<CredentialInfo>& public_key_credential_params() const {
    return public_key_credential_params_;
  }

 private:
  std::vector<CredentialInfo> public_key_credential_params_;
};

cbor::Value AsCBOR(const PublicKeyCredentialParams&);

}  // namespace device

#endif  // DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_PARAMS_H_
