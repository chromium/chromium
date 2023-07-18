// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/enclave_passkey.h"

namespace device {

EnclavePasskey::EnclavePasskey(DiscoverableCredentialMetadata metadata_in,
                               std::vector<uint8_t> private_key_in)
    : metadata(std::move(metadata_in)),
      private_key(std::move(private_key_in)) {}

EnclavePasskey::EnclavePasskey() = default;
EnclavePasskey::EnclavePasskey(const EnclavePasskey& other) = default;
EnclavePasskey::EnclavePasskey(EnclavePasskey&& other) = default;
EnclavePasskey::~EnclavePasskey() = default;
EnclavePasskey& EnclavePasskey::operator=(const EnclavePasskey& other) =
    default;
EnclavePasskey& EnclavePasskey::operator=(EnclavePasskey&& other) = default;

bool EnclavePasskey::operator==(const EnclavePasskey& other) const {
  return metadata == other.metadata && private_key == other.private_key;
}

}  // namespace device
