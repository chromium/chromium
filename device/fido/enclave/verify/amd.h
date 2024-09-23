// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_ENCLAVE_VERIFY_AMD_H_
#define DEVICE_FIDO_ENCLAVE_VERIFY_AMD_H_

#include <string>

#include "base/component_export.h"
#include "base/types/expected.h"
#include "device/fido/enclave/verify/attestation_report.h"
#include "third_party/boringssl/src/include/openssl/x509.h"

namespace device::enclave {

// Verifies validity of a matching ARK, ASK certificate pair.
//
// Validate at least a subset of Appendix B.3 of
// https://www.amd.com/content/dam/amd/en/documents/epyc-technical-docs/programmer-references/55766_SEV-KM_API_Specification.pdf
// TODO: Ideally, we'd check everything listed there.
base::expected<void, std::string> COMPONENT_EXPORT(DEVICE_FIDO)
    ValidateArkAskCerts(X509* ark, X509* ask);

// Verifies that the certificate in |signee| contains a valid signature using
// the public key in |signer|.
base::expected<void, std::string> COMPONENT_EXPORT(DEVICE_FIDO)
    VerifyCertSignature(X509* signer, X509* signee);

// Extracts the Hardware ID from the |cert| extensions.
base::expected<std::vector<uint8_t>, std::string> COMPONENT_EXPORT(DEVICE_FIDO)
    GetChipId(X509* cert);

// Builds the TCB Version from the |cert| extensions.
base::expected<TcbVersion, std::string> COMPONENT_EXPORT(DEVICE_FIDO)
    GetTcbVersion(X509* cert);

}  // namespace device::enclave

#endif  // DEVICE_FIDO_ENCLAVE_VERIFY_AMD_H_
