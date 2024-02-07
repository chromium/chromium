// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_DESCRIPTOR_H_
#define DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_DESCRIPTOR_H_

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "components/cbor/values.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"

namespace device {

// Data structure containing public key credential type (string) and credential
// id (byte array) as specified in the CTAP spec. Used for exclude_list for
// AuthenticatorMakeCredential command and allow_list parameter for
// AuthenticatorGetAssertion command.
class COMPONENT_EXPORT(DEVICE_FIDO) PublicKeyCredentialDescriptor {
 public:
  static std::optional<PublicKeyCredentialDescriptor> CreateFromCBORValue(
      const cbor::Value& cbor);

  PublicKeyCredentialDescriptor();
  PublicKeyCredentialDescriptor(CredentialType credential_type,
                                std::vector<uint8_t> id);
  PublicKeyCredentialDescriptor(
      CredentialType credential_type,
      std::vector<uint8_t> id,
      base::flat_set<FidoTransportProtocol> transports);
  PublicKeyCredentialDescriptor(const PublicKeyCredentialDescriptor& other);
  PublicKeyCredentialDescriptor(PublicKeyCredentialDescriptor&& other);
  PublicKeyCredentialDescriptor& operator=(
      const PublicKeyCredentialDescriptor& other);
  PublicKeyCredentialDescriptor& operator=(
      PublicKeyCredentialDescriptor&& other);
  bool operator==(const PublicKeyCredentialDescriptor& other) const;
  ~PublicKeyCredentialDescriptor();

  CredentialType credential_type;
  std::vector<uint8_t> id;
  base::flat_set<FidoTransportProtocol> transports;

  // had_other_keys is true if, when parsed from CBOR, this descriptor
  // contained keys other than 'id' and 'type'. This is only used for testing
  // that we don't repeat crbug.com/1270757.
  bool had_other_keys = false;
};

COMPONENT_EXPORT(DEVICE_FIDO)
cbor::Value AsCBOR(const PublicKeyCredentialDescriptor&);

}  // namespace device

#endif  // DEVICE_FIDO_PUBLIC_KEY_CREDENTIAL_DESCRIPTOR_H_
