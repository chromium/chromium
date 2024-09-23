// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/enclave/verify/amd.h"

#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "device/fido/enclave/verify/attestation_report.h"
#include "third_party/boringssl/src/include/openssl/asn1.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "third_party/boringssl/src/include/openssl/obj_mac.h"
#include "third_party/boringssl/src/include/openssl/x509.h"

namespace device::enclave {
namespace {
// The keys in the key-value map of X509 certificates are Object Identifiers
// (OIDs) which have a global registry. The present OIDs are taken from
// Table 8 of
// https://www.amd.com/content/dam/amd/en/documents/epyc-technical-docs/specifications/57230.pdf
const char BL_SPL[] = "1.3.6.1.4.1.3704.1.3.1";
const char TEE_SPL[] = "1.3.6.1.4.1.3704.1.3.2";
const char SNP_SPL[] = "1.3.6.1.4.1.3704.1.3.3";
const char UCODE_SPL[] = "1.3.6.1.4.1.3704.1.3.8";
const char HW_ID[] = "1.3.6.1.4.1.3704.1.4";

base::expected<const ASN1_OCTET_STRING*, std::string> GetExtension(
    X509* cert,
    std::string_view ext_name) {
  bssl::UniquePtr<ASN1_OBJECT> obj(OBJ_txt2obj(ext_name.data(), 1));
  if (!obj) {
    return base::unexpected("Could not parse ASN1_OBJECT from string");
  }
  int pos = X509_get_ext_by_OBJ(cert, obj.get(), -1);
  if (pos < 0) {
    return base::unexpected(base::StringPrintf(
        "Could not find <%s> in the extensions.", ext_name.data()));
  }
  const X509_EXTENSION* extension = X509_get_ext(cert, pos);
  const ASN1_OCTET_STRING* octet_string = X509_EXTENSION_get_data(extension);
  if (ASN1_STRING_length(octet_string) == 0) {
    return base::unexpected(base::StringPrintf(
        "<%s> returned empty string from the extensions.", ext_name.data()));
  }
  return octet_string;
}

}  // namespace

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

base::expected<std::vector<uint8_t>, std::string> GetChipId(X509* cert) {
  auto extension = GetExtension(cert, HW_ID);
  if (!extension.has_value()) {
    return base::unexpected(extension.error());
  }
  std::vector<uint8_t> res(ASN1_STRING_get0_data(extension.value()),
                           ASN1_STRING_get0_data(extension.value()) +
                               ASN1_STRING_length(extension.value()));
  return res;
}

base::expected<TcbVersion, std::string> GetTcbVersion(X509* cert) {
  TcbVersion tcb_version{};

  // TODO: Document the reasoning behind ignoring the all but the last byte
  // below.
  auto bl_spl = GetExtension(cert, BL_SPL);
  if (!bl_spl.has_value()) {
    return base::unexpected(bl_spl.error());
  }
  tcb_version.boot_loader = ASN1_STRING_get0_data(
      bl_spl.value())[ASN1_STRING_length(bl_spl.value()) - 1];

  auto tee_spl = GetExtension(cert, TEE_SPL);
  if (!tee_spl.has_value()) {
    return base::unexpected(tee_spl.error());
  }
  tcb_version.tee = ASN1_STRING_get0_data(
      tee_spl.value())[ASN1_STRING_length(tee_spl.value()) - 1];

  auto snp_spl = GetExtension(cert, SNP_SPL);
  if (!snp_spl.has_value()) {
    return base::unexpected(snp_spl.error());
  }
  tcb_version.snp = ASN1_STRING_get0_data(
      snp_spl.value())[ASN1_STRING_length(snp_spl.value()) - 1];

  auto ucode_spl = GetExtension(cert, UCODE_SPL);
  if (!ucode_spl.has_value()) {
    return base::unexpected(ucode_spl.error());
  }
  tcb_version.microcode = ASN1_STRING_get0_data(
      ucode_spl.value())[ASN1_STRING_length(ucode_spl.value()) - 1];

  return tcb_version;
}

}  // namespace device::enclave
