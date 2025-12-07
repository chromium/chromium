// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/types.h"

#include "components/sync/protocol/webauthn_credential_specifics.pb.h"

namespace device::enclave {

EnclaveIdentity::EnclaveIdentity() = default;
EnclaveIdentity::EnclaveIdentity(const EnclaveIdentity&) = default;
EnclaveIdentity::EnclaveIdentity(EnclaveIdentity&&) = default;
EnclaveIdentity& EnclaveIdentity::operator=(const EnclaveIdentity&) = default;
EnclaveIdentity& EnclaveIdentity::operator=(EnclaveIdentity&&) = default;
EnclaveIdentity::~EnclaveIdentity() = default;

ClientSignature::ClientSignature() = default;
ClientSignature::~ClientSignature() = default;
ClientSignature::ClientSignature(const ClientSignature&) = default;
ClientSignature::ClientSignature(ClientSignature&&) = default;

ClaimedPIN::ClaimedPIN(std::vector<uint8_t> in_pin_claim,
                       std::vector<uint8_t> in_wrapped_pin)
    : pin_claim(std::move(in_pin_claim)),
      wrapped_pin(std::move(in_wrapped_pin)) {}
ClaimedPIN::~ClaimedPIN() = default;

CredentialRequest::CredentialRequest() = default;
CredentialRequest::~CredentialRequest() = default;
CredentialRequest::CredentialRequest(CredentialRequest&&) = default;
}  // namespace device::enclave
