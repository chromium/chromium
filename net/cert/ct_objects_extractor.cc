// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/ct_objects_extractor.h"

#include <string_view>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "crypto/hash.h"
#include "crypto/openssl_util.h"
#include "net/cert/asn1_util.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace net::ct {

namespace {

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
  CHECK_LE(UNSAFE_TODO(CBS_data(&inner) + CBS_len(&inner)),
           UNSAFE_TODO(CBS_data(&outer) + CBS_len(&outer)));

  return !!CBB_add_bytes(out, CBS_data(&outer),
                         CBS_data(&inner) - CBS_data(&outer));
}

// Copies all the bytes in |outer| which are after |inner| to |out|. |inner|
// must be a subset of |outer|.
bool CopyAfter(const CBS& outer, const CBS& inner, CBB* out) {
  CHECK_LE(CBS_data(&outer), CBS_data(&inner));
  CHECK_LE(UNSAFE_TODO(CBS_data(&inner) + CBS_len(&inner)),
           UNSAFE_TODO(CBS_data(&outer) + CBS_len(&outer)));

  return !!CBB_add_bytes(out, UNSAFE_TODO(CBS_data(&inner) + CBS_len(&inner)),
                         UNSAFE_TODO(CBS_data(&outer) + CBS_len(&outer) -
                                     CBS_data(&inner) - CBS_len(&inner)));
}

// Skips |tbs_cert|, which must be a TBSCertificate body, to just before the
// extensions element.
bool SkipTBSCertificateToExtensions(CBS* tbs_cert) {
  constexpr unsigned kVersionTag =
      CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0;
  constexpr unsigned kIssuerUniqueIDTag = CBS_ASN1_CONTEXT_SPECIFIC | 1;
  constexpr unsigned kSubjectUniqueIDTag = CBS_ASN1_CONTEXT_SPECIFIC | 2;
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

  if (!CBB_flush(cbb.get())) {
    return false;
  }

  // Extract the issuer's public key.
  std::string_view issuer_key;
  if (!asn1::ExtractSPKIFromDERCert(
          x509_util::CryptoBufferAsStringPiece(issuer), &issuer_key)) {
    return false;
  }

  // Fill in the SignedEntryData.
  result->type = ct::SignedEntryData::LOG_ENTRY_TYPE_PRECERT;
  result->tbs_certificate.assign(
      base::as_string_view(crypto::CbbAsSpan(cbb.get())));
  result->issuer_key_hash =
      crypto::hash::Sha256(base::as_byte_span(issuer_key));

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

}  // namespace net::ct
