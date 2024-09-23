// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/transport_security_state_generator/cert_util.h"

#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/tools/transport_security_state_generator/spki_hash.h"
#include "third_party/boringssl/src/include/openssl/crypto.h"

using net::transport_security_state::SPKIHash;

namespace {

static const char kPEMBeginBlock[] = "-----BEGIN %s-----";
static const char kPEMEndBlock[] = "-----END %s-----";

// Tries to extract the BASE64 encoded DER structure from |pem_input| by looking
// for the block type in |expected_block_type|. Only attempts the locate the
// first matching block. Other blocks are ignored. Returns true on success and
// copies the der structure to |*der_output|. Returns false on error.
bool ParsePEM(std::string_view pem_input,
              std::string_view expected_block_type,
              std::string* der_output) {
  const std::string& block_start =
      base::StringPrintf(kPEMBeginBlock, expected_block_type.data());
  const std::string& block_end =
      base::StringPrintf(kPEMEndBlock, expected_block_type.data());

  size_t block_start_pos = pem_input.find(block_start);
  if (block_start_pos == std::string::npos)
    return false;
  size_t base64_start_pos = block_start_pos + block_start.size();

  size_t block_end_pos = pem_input.find(block_end, base64_start_pos);
  if (block_end_pos == std::string::npos)
    return false;

  std::string_view base64_encoded =
      pem_input.substr(base64_start_pos, block_end_pos - base64_start_pos);

  if (!base::Base64Decode(base::CollapseWhitespaceASCII(base64_encoded, true),
                          der_output)) {
    return false;
  }

  return true;
}

// Attempts to extract the first entry of type |nid| from |*name|. Returns true
// if the field exists and was extracted. Returns false when the field was not
// found or the data could not be extracted.
bool ExtractFieldFromX509Name(X509_NAME* name, int nid, std::string* field) {
  int index = X509_NAME_get_index_by_NID(name, nid, -1);
  if (index == -1) {
    return false;
  }

  X509_NAME_ENTRY* entry = X509_NAME_get_entry(name, index);
  if (!entry) {
    return false;
  }

  ASN1_STRING* data = X509_NAME_ENTRY_get_data(entry);
  if (!data) {
    return false;
  }

  uint8_t* buffer = nullptr;
  size_t length = ASN1_STRING_to_UTF8(&buffer, data);
  field->assign(reinterpret_cast<const char*>(buffer), length);
  OPENSSL_free(buffer);
  return true;
}

}  // namespace

bssl::UniquePtr<X509> GetX509CertificateFromPEM(std::string_view pem_data) {
  std::string der;
  if (!ParsePEM(pem_data, "CERTIFICATE", &der)) {
    return bssl::UniquePtr<X509>();
  }

  const uint8_t* der_data = reinterpret_cast<const uint8_t*>(der.c_str());
  return bssl::UniquePtr<X509>(
      d2i_X509(nullptr, &der_data, base::checked_cast<long>(der.size())));
}

bool ExtractSubjectNameFromCertificate(X509* certificate, std::string* name) {
  DCHECK(certificate);
  X509_NAME* subject = X509_get_subject_name(certificate);
  if (!subject) {
    return false;
  }

  std::string result;
  // Try extracting the common name first.
  if (!ExtractFieldFromX509Name(subject, NID_commonName, &result) ||
      result.empty()) {
    std::string organization;
    if (!ExtractFieldFromX509Name(subject, NID_organizationName,
                                  &organization)) {
      return false;
    }

    std::string organizational_unit;
    if (!ExtractFieldFromX509Name(subject, NID_organizationalUnitName,
                                  &organizational_unit)) {
      return false;
    }
    result = organization + " " + organizational_unit;
  }

  name->assign(result);
  return true;
}

bool CalculateSPKIHashFromCertificate(X509* certificate, SPKIHash* out_hash) {
  DCHECK(certificate);
  bssl::UniquePtr<EVP_PKEY> key(X509_get_pubkey(certificate));
  if (!key) {
    return false;
  }

  uint8_t* spki_der;
  size_t spki_der_len;
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 0) ||
      !EVP_marshal_public_key(cbb.get(), key.get()) ||
      !CBB_finish(cbb.get(), &spki_der, &spki_der_len)) {
    return false;
  }

  out_hash->CalculateFromBytes(spki_der, spki_der_len);
  OPENSSL_free(spki_der);
  return true;
}

bool CalculateSPKIHashFromKey(std::string_view pem_key, SPKIHash* out_hash) {
  std::string der;
  bool result = ParsePEM(pem_key, "PUBLIC KEY", &der);
  if (!result) {
    return false;
  }

  out_hash->CalculateFromBytes(reinterpret_cast<const uint8_t*>(der.data()),
                               der.size());
  return true;
}
