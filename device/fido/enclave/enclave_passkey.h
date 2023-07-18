// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_ENCLAVE_PASSKEY_H_
#define DEVICE_FIDO_ENCLAVE_ENCLAVE_PASSKEY_H_

#include <vector>

#include "base/component_export.h"
#include "device/fido/discoverable_credential_metadata.h"

namespace device {

// A passkey that includes the encrypted private key data that can be provided
// to a cloud enclave authenticator service and used to generate an assertion.
struct COMPONENT_EXPORT(DEVICE_FIDO) EnclavePasskey {
  EnclavePasskey(DiscoverableCredentialMetadata metadata_in,
                 std::vector<uint8_t> private_key_in);

  EnclavePasskey();
  EnclavePasskey(const EnclavePasskey&);
  EnclavePasskey(EnclavePasskey&&);
  ~EnclavePasskey();
  EnclavePasskey& operator=(const EnclavePasskey& other);
  EnclavePasskey& operator=(EnclavePasskey&& other);
  bool operator==(const EnclavePasskey& other) const;

  DiscoverableCredentialMetadata metadata;
  std::vector<uint8_t> private_key;
};

}  // namespace device

#endif  // DEVICE_FIDO_ENCLAVE_ENCLAVE_PASSKEY_H_
