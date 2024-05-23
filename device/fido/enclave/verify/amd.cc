// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/amd.h"

#include "base/strings/stringprintf.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/obj_mac.h"
#include "third_party/boringssl/src/include/openssl/x509.h"

namespace device::enclave {

base::expected<void, std::string> VerifyCertSignature(X509* signer,
                                                      X509* signee) {
  auto signature_nid = X509_get_signature_nid(signee);
  if (signature_nid != NID_rsassaPss) {
    return base::unexpected("Unsupported signature algorithm.");
  }

  bssl::UniquePtr<EVP_PKEY> verifying_key(X509_get_pubkey(signer));
  if (!verifying_key) {
    return base::unexpected("Could not parse RSA public key.");
  }
  if (!X509_verify(signee, verifying_key.get())) {
    return base::unexpected("Signature verification failed.");
  }

  return base::expected<void, std::string>();
}

base::expected<void, std::string> ValidateArkAskCerts(X509* ark, X509* ask) {
  if (X509_get_version(ark) != X509_VERSION_3) {
    return base::unexpected("Unexpected version of ARK cert.");
  }
  if (X509_get_version(ask) != X509_VERSION_3) {
    return base::unexpected("Unexpected version of ASK cert.");
  }
  return VerifyCertSignature(ark, ask);
}

}  // namespace device::enclave
