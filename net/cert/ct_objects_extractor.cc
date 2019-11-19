// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_objects_extractor.h"

#include <string.h>

#include "base/hash/sha1.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "crypto/sha2.h"
#include "net/cert/asn1_util.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace net {

namespace ct {

namespace {

// The wire form of the OID 1.3.6.1.4.1.11129.2.4.2. See Section 3.3 of
// RFC6962.
const uint8_t kEmbeddedSCTOid[] = {0x2B, 0x06, 0x01, 0x04, 0x01,
                                   0xD6, 0x79, 0x02, 0x04, 0x02};

// The wire form of the OID 1.3.6.1.4.1.11129.2.4.5 - OCSP SingleExtension for
// X.509v3 Certificate Transparency Signed Certificate Timestamp List, see
// Section 3.3 of RFC6962.
const uint8_t kOCSPExtensionOid[] = {0x2B, 0x06, 0x01, 0x04, 0x01,
                                     0xD6, 0x79, 0x02, 0x04, 0x05};

// The wire form of the OID 1.3.6.1.5.5.7.48.1.1. See RFC 6960.
const uint8_t kOCSPBasicResponseOid[] = {0x2b, 0x06, 0x01, 0x05, 0x05,
                                         0x07, 0x30, 0x01, 0x01};

// The wire form of the OID 1.3.14.3.2.26.
const uint8_t kSHA1Oid[] = {0x2b, 0x0e, 0x03, 0x02, 0x1a};

// The wire form of the OID 2.16.840.1.101.3.4.2.1.
const uint8_t kSHA256Oid[] = {0x60, 0x86, 0x48, 0x01, 0x65,
                              0x03, 0x04, 0x02, 0x01};

bool StringEqualToCBS(const std::string& value1, const CBS* value2) {
  if (CBS_len(value2) != value1.size())
    return false;
  return memcmp(value1.data(), CBS_data(value2), CBS_len(value2)) == 0;
}

bool SkipElements(CBS* cbs, int count) {
  for (int i = 0; i < count; i++) {
    if (!CBS_get_any_asn1_element(cbs, nullptr, nullptr, nullptr))
      return false;
  }
  return true;
}

bool SkipOptionalElement(CBS* cbs, unsigned tag) {
  CBS unused;
  return !CBS_peek_asn1_tag(cbs, tag) || CBS_get_asn1(cbs, &unused, tag);
}

// Copies all the bytes in |outer| which are before |inner| to |out|. |inner|
// must be a subset of |outer|.
bool CopyBefore(const CBS& outer, const CBS& inner, CBB* out) {
  CHECK_LE(CBS_data(&outer), CBS_data(&inner));
  CHECK_LE(CBS_data(&inner) + CBS_len(&inner),
           CBS_data(&outer) + CBS_len(&outer));

  return !!CBB_add_bytes(out, CBS_data(&outer),
                         CBS_data(&inner) - CBS_data(&outer));
}

// Copies all the bytes in |outer| which are after |inner| to |out|. |inner|
// must be a subset of |outer|.
bool CopyAfter(const CBS& outer, const CBS& inner, CBB* out) {
  CHECK_LE(CBS_data(&outer), CBS_data(&inner));
  CHECK_LE(CBS_data(&inner) + CBS_len(&inner),
           CBS_data(&outer) + CBS_len(&outer));

  return !!CBB_add_bytes(
      out, CBS_data(&inner) + CBS_len(&inner),
      CBS_data(&outer) + CBS_len(&outer) - CBS_data(&inner) - CBS_len(&inner));
}

// Skips |tbs_cert|, which must be a TBSCertificate body, to just before the
// extensions element.
bool SkipTBSCertificateToExtensions(CBS* tbs_cert) {
  constexpr unsigned kVersionTag =
      CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0;
  constexpr unsigned kIssuerUniqueIDTag =
      CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 1;
  constexpr unsigned kSubjectUniqueIDTag =
      CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 2;
  return SkipOptionalElement(tbs_cert, kVersionTag) &&
         SkipElements(tbs_cert,
                      6 /* serialNumber through subjectPublicKeyInfo */) &&
         SkipOptionalElement(tbs_cert, kIssuerUniqueIDTag) &&
         SkipOptionalElement(tbs_cert, kSubjectUniqueIDTag);
}

// Looks for the extension with the specified OID in |extensions|, which must
// contain the contents of a SEQUENCE of X.509 extension structures. If found,
// returns true and sets |*out| to the full extension element.
bool FindExtensionElement(const CBS& extensions,
                          const uint8_t* oid,
                          size_t oid_len,
                          CBS* out) {
  CBS extensions_copy = extensions;
  CBS result;
  CBS_init(&result, nullptr, 0);
  bool found = false;
  while (CBS_len(&extensions_copy) > 0) {
    CBS extension_element;
    if (!CBS_get_asn1_element(&extensions_copy, &extension_element,
                              CBS_ASN1_SEQUENCE)) {
      return false;
    }

    CBS copy = extension_element;
    CBS extension, extension_oid;
    if (!CBS_get_asn1(&copy, &extension, CBS_ASN1_SEQUENCE) ||
        !CBS_get_asn1(&extension, &extension_oid, CBS_ASN1_OBJECT)) {
      return false;
    }

    if (CBS_mem_equal(&extension_oid, oid, oid_len)) {
      if (found)
        return false;
      found = true;
      result = extension_element;
    }
  }
  if (!found)
    return false;

  *out = result;
  return true;
}

// Finds the SignedCertificateTimestampList in an extension with OID |oid| in
// |x509_exts|. If found, returns true and sets |*out_sct_list| to the encoded
// SCT list.
bool ParseSCTListFromExtensions(const CBS& extensions,
                                const uint8_t* oid,
                                size_t oid_len,
                                std::string* out_sct_list) {
  CBS extension_element, extension, extension_oid, value, sct_list;
  if (!FindExtensionElement(extensions, oid, oid_len, &extension_element) ||
      !CBS_get_asn1(&extension_element, &extension, CBS_ASN1_SEQUENCE) ||
      !CBS_get_asn1(&extension, &extension_oid, CBS_ASN1_OBJECT) ||
      // Skip the optional critical element.
      !SkipOptionalElement(&extension, CBS_ASN1_BOOLEAN) ||
      // The extension value is stored in an OCTET STRING.
      !CBS_get_asn1(&extension, &value, CBS_ASN1_OCTETSTRING) ||
      CBS_len(&extension) != 0 ||
      // The extension value itself is an OCTET STRING containing the
      // serialized SCT list.
      !CBS_get_asn1(&value, &sct_list, CBS_ASN1_OCTETSTRING) ||
      CBS_len(&value) != 0) {
    return false;
  }

  DCHECK(CBS_mem_equal(&extension_oid, oid, oid_len));
  *out_sct_list = std::string(
      reinterpret_cast<const char*>(CBS_data(&sct_list)), CBS_len(&sct_list));
  return true;
}

// Finds the SingleResponse in |responses| which matches |issuer| and
// |cert_serial_number|. On success, returns true and sets
// |*out_single_response| to the body of the SingleResponse starting at the
// |certStatus| field.
bool FindMatchingSingleResponse(CBS* responses,
                                const CRYPTO_BUFFER* issuer,
                                const std::string& cert_serial_number,
                                CBS* out_single_response) {
  base::StringPiece issuer_spki;
  if (!asn1::ExtractSPKIFromDERCert(
          x509_util::CryptoBufferAsStringPiece(issuer), &issuer_spki))
    return false;

  // In OCSP, only the key itself is under hash.
  base::StringPiece issuer_spk;
  if (!asn1::ExtractSubjectPublicKeyFromSPKI(issuer_spki, &issuer_spk))
    return false;

  // ExtractSubjectPublicKeyFromSPKI does not remove the initial octet encoding
  // the number of unused bits in the ASN.1 BIT STRING so we do it here. For
  // public keys, the bitstring is in practice always byte-aligned.
  if (issuer_spk.empty() || issuer_spk[0] != 0)
    return false;
  issuer_spk.remove_prefix(1);

  // TODO(ekasper): add SHA-384 to crypto/sha2.h and here if it proves
  // necessary.
  // TODO(ekasper): only compute the hashes on demand.
  std::string issuer_key_sha256_hash = crypto::SHA256HashString(issuer_spk);
  std::string issuer_key_sha1_hash =
      base::SHA1HashString(issuer_spk.as_string());

  while (CBS_len(responses) > 0) {
    CBS single_response, cert_id;
    if (!CBS_get_asn1(responses, &single_response, CBS_ASN1_SEQUENCE) ||
        !CBS_get_asn1(&single_response, &cert_id, CBS_ASN1_SEQUENCE)) {
      return false;
    }

    CBS hash_algorithm, hash, serial_number, issuer_name_hash, issuer_key_hash;
    if (!CBS_get_asn1(&cert_id, &hash_algorithm, CBS_ASN1_SEQUENCE) ||
        !CBS_get_asn1(&hash_algorithm, &hash, CBS_ASN1_OBJECT) ||
        !CBS_get_asn1(&cert_id, &issuer_name_hash, CBS_ASN1_OCTETSTRING) ||
        !CBS_get_asn1(&cert_id, &issuer_key_hash, CBS_ASN1_OCTETSTRING) ||
        !CBS_get_asn1(&cert_id, &serial_number, CBS_ASN1_INTEGER) ||
        CBS_len(&cert_id) != 0) {
      return false;
    }

    // Check the serial number matches.
    if (!StringEqualToCBS(cert_serial_number, &serial_number))
      continue;

    // Check if the issuer_key_hash matches.
    // TODO(ekasper): also use the issuer name hash in matching.
    if (CBS_mem_equal(&hash, kSHA1Oid, sizeof(kSHA1Oid))) {
      if (StringEqualToCBS(issuer_key_sha1_hash, &issuer_key_hash)) {
        *out_single_response = single_response;
        return true;
      }
    } else if (CBS_mem_equal(&hash, kSHA256Oid, sizeof(kSHA256Oid))) {
      if (StringEqualToCBS(issuer_key_sha256_hash, &issuer_key_hash)) {
        *out_single_response = single_response;
        return true;
      }
    }
  }

  return false;
}

}  // namespace

bool ExtractEmbeddedSCTList(const CRYPTO_BUFFER* cert, std::string* sct_list) {
  CBS cert_cbs;
  CBS_init(&cert_cbs, CRYPTO_BUFFER_data(cert), CRYPTO_BUFFER_len(cert));
  CBS cert_body, tbs_cert, extensions_wrap, extensions;
  if (!CBS_get_asn1(&cert_cbs, &cert_body, CBS_ASN1_SEQUENCE) ||
      CBS_len(&cert_cbs) != 0 ||
      !CBS_get_asn1(&cert_body, &tbs_cert, CBS_ASN1_SEQUENCE) ||
      !SkipTBSCertificateToExtensions(&tbs_cert) ||
      // Extract the extensions list.
      !CBS_get_asn1(&tbs_cert, &extensions_wrap,
                    CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 3) ||
      !CBS_get_asn1(&extensions_wrap, &extensions, CBS_ASN1_SEQUENCE) ||
      CBS_len(&extensions_wrap) != 0 || CBS_len(&tbs_cert) != 0) {
    return false;
  }

  return ParseSCTListFromExtensions(extensions, kEmbeddedSCTOid,
                                    sizeof(kEmbeddedSCTOid), sct_list);
}

bool GetPrecertSignedEntry(const CRYPTO_BUFFER* leaf,
                           const CRYPTO_BUFFER* issuer,
                           SignedEntryData* result) {
  result->Reset();

  // Parse the TBSCertificate.
  CBS cert_cbs;
  CBS_init(&cert_cbs, CRYPTO_BUFFER_data(leaf), CRYPTO_BUFFER_len(leaf));
  CBS cert_body, tbs_cert;
  if (!CBS_get_asn1(&cert_cbs, &cert_body, CBS_ASN1_SEQUENCE) ||
      CBS_len(&cert_cbs) != 0 ||
      !CBS_get_asn1(&cert_body, &tbs_cert, CBS_ASN1_SEQUENCE)) {
    return false;
  }

  CBS tbs_cert_copy = tbs_cert;
  if (!SkipTBSCertificateToExtensions(&tbs_cert))
    return false;

  // Start filling in a new TBSCertificate. Copy everything parsed or skipped
  // so far to the |new_tbs_cert|.
  bssl::ScopedCBB cbb;
  CBB new_tbs_cert;
  if (!CBB_init(cbb.get(), CBS_len(&tbs_cert_copy)) ||
      !CBB_add_asn1(cbb.get(), &new_tbs_cert, CBS_ASN1_SEQUENCE) ||
      !CopyBefore(tbs_cert_copy, tbs_cert, &new_tbs_cert)) {
    return false;
  }

  // Parse the extensions list and find the SCT extension.
  //
  // XXX(rsleevi): We could generate precerts for certs without the extension
  // by leaving the TBSCertificate as-is. The reference implementation does not
  // do this.
  constexpr unsigned kExtensionsTag =
      CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 3;
  CBS extensions_wrap, extensions, sct_extension;
  if (!CBS_get_asn1(&tbs_cert, &extensions_wrap, kExtensionsTag) ||
      !CBS_get_asn1(&extensions_wrap, &extensions, CBS_ASN1_SEQUENCE) ||
      CBS_len(&extensions_wrap) != 0 || CBS_len(&tbs_cert) != 0 ||
      !FindExtensionElement(extensions, kEmbeddedSCTOid,
                            sizeof(kEmbeddedSCTOid), &sct_extension)) {
    return false;
  }

  // Add extensions to the TBSCertificate. Copy all extensions except the
  // embedded SCT extension.
  CBB new_extensions_wrap, new_extensions;
  if (!CBB_add_asn1(&new_tbs_cert, &new_extensions_wrap, kExtensionsTag) ||
      !CBB_add_asn1(&new_extensions_wrap, &new_extensions, CBS_ASN1_SEQUENCE) ||
      !CopyBefore(extensions, sct_extension, &new_extensions) ||
      !CopyAfter(extensions, sct_extension, &new_extensions)) {
    return false;
  }

  uint8_t* new_tbs_cert_der;
  size_t new_tbs_cert_len;
  if (!CBB_finish(cbb.get(), &new_tbs_cert_der, &new_tbs_cert_len))
    return false;
  bssl::UniquePtr<uint8_t> scoped_new_tbs_cert_der(new_tbs_cert_der);

  // Extract the issuer's public key.
  base::StringPiece issuer_key;
  if (!asn1::ExtractSPKIFromDERCert(
          x509_util::CryptoBufferAsStringPiece(issuer), &issuer_key)) {
    return false;
  }

  // Fill in the SignedEntryData.
  result->type = ct::SignedEntryData::LOG_ENTRY_TYPE_PRECERT;
  result->tbs_certificate.assign(
      reinterpret_cast<const char*>(new_tbs_cert_der), new_tbs_cert_len);
  crypto::SHA256HashString(issuer_key, result->issuer_key_hash.data,
                           sizeof(result->issuer_key_hash.data));

  return true;
}

bool GetX509SignedEntry(const CRYPTO_BUFFER* leaf, SignedEntryData* result) {
  DCHECK(leaf);

  result->Reset();
  result->type = ct::SignedEntryData::LOG_ENTRY_TYPE_X509;
  result->leaf_certificate =
      std::string(x509_util::CryptoBufferAsStringPiece(leaf));
  return true;
}

bool ExtractSCTListFromOCSPResponse(const CRYPTO_BUFFER* issuer,
                                    const std::string& cert_serial_number,
                                    base::StringPiece ocsp_response,
                                    std::string* sct_list) {
  // The input is an OCSPResponse. See RFC2560, section 4.2.1. The SCT list is
  // in the extensions field of the SingleResponse which matches the input
  // certificate.
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(ocsp_response.data()),
           ocsp_response.size());

  // Parse down to the ResponseBytes. The ResponseBytes is optional, but if it's
  // missing, this can't include an SCT list.
  CBS sequence, tagged_response_bytes, response_bytes, response_type, response;
  if (!CBS_get_asn1(&cbs, &sequence, CBS_ASN1_SEQUENCE) || CBS_len(&cbs) != 0 ||
      !SkipElements(&sequence, 1 /* responseStatus */) ||
      !CBS_get_asn1(&sequence, &tagged_response_bytes,
                    CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0) ||
      CBS_len(&sequence) != 0 ||
      !CBS_get_asn1(&tagged_response_bytes, &response_bytes,
                    CBS_ASN1_SEQUENCE) ||
      CBS_len(&tagged_response_bytes) != 0 ||
      !CBS_get_asn1(&response_bytes, &response_type, CBS_ASN1_OBJECT) ||
      !CBS_get_asn1(&response_bytes, &response, CBS_ASN1_OCTETSTRING) ||
      CBS_len(&response_bytes) != 0) {
    return false;
  }

  // The only relevant ResponseType is id-pkix-ocsp-basic.
  if (!CBS_mem_equal(&response_type, kOCSPBasicResponseOid,
                     sizeof(kOCSPBasicResponseOid))) {
    return false;
  }

  // Parse the ResponseData out of the BasicOCSPResponse. Ignore the rest.
  constexpr unsigned kVersionTag =
      CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0;
  CBS basic_response, response_data, responses;
  if (!CBS_get_asn1(&response, &basic_response, CBS_ASN1_SEQUENCE) ||
      CBS_len(&response) != 0 ||
      !CBS_get_asn1(&basic_response, &response_data, CBS_ASN1_SEQUENCE)) {
    return false;
  }

  // Extract the list of SingleResponses from the ResponseData.
  if (!SkipOptionalElement(&response_data, kVersionTag) ||
      !SkipElements(&response_data, 2 /* responderID, producedAt */) ||
      !CBS_get_asn1(&response_data, &responses, CBS_ASN1_SEQUENCE)) {
    return false;
  }

  CBS single_response;
  if (!FindMatchingSingleResponse(&responses, issuer, cert_serial_number,
                                  &single_response)) {
    return false;
  }

  // Parse the extensions out of the SingleResponse.
  constexpr unsigned kNextUpdateTag =
      CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0;
  constexpr unsigned kSingleExtensionsTag =
      CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 1;
  CBS extensions_wrap, extensions;
  if (!SkipElements(&single_response, 2 /* certStatus, thisUpdate */) ||
      !SkipOptionalElement(&single_response, kNextUpdateTag) ||
      !CBS_get_asn1(&single_response, &extensions_wrap, kSingleExtensionsTag) ||
      !CBS_get_asn1(&extensions_wrap, &extensions, CBS_ASN1_SEQUENCE) ||
      CBS_len(&extensions_wrap) != 0) {
    return false;
  }

  return ParseSCTListFromExtensions(extensions, kOCSPExtensionOid,
                                    sizeof(kOCSPExtensionOid), sct_list);
}

}  // namespace ct

}  // namespace net
