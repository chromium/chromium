// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util.h"

#include <string.h>

#include <map>
#include <memory>
#include <string_view>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "crypto/openssl_util.h"
#include "crypto/rsa_private_key.h"
#include "crypto/sha2.h"
#include "net/base/hash_value.h"
#include "net/cert/asn1_util.h"
#include "net/cert/time_conversions.h"
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/digest.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/pkcs7.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/stack.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/input.h"
#include "third_party/boringssl/src/pki/name_constraints.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"
#include "third_party/boringssl/src/pki/parse_name.h"
#include "third_party/boringssl/src/pki/parse_values.h"
#include "third_party/boringssl/src/pki/signature_algorithm.h"

namespace net::x509_util {

namespace {

bool AddSignatureAlgorithm(CBB* cbb,
                           base::span<const uint8_t> oid_bytes,
                           bool null_param) {
  // An AlgorithmIdentifier is described in RFC 5280, 4.1.1.2.
  CBB sequence, oid, params;
  if (!CBB_add_asn1(cbb, &sequence, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&sequence, &oid, CBS_ASN1_OBJECT) ||
      !CBB_add_bytes(&oid, oid_bytes.data(), oid_bytes.size()) ||
      (null_param && !CBB_add_asn1(&sequence, &params, CBS_ASN1_NULL)) ||
      !CBB_flush(cbb)) {
    return false;
  }
  return true;
}

bool AddSignatureAlgorithm(CBB* cbb,
                           const EVP_PKEY* pkey,
                           DigestAlgorithm digest_alg) {
  if (digest_alg != DIGEST_SHA256) {
    return false;
  }

  if (EVP_PKEY_id(pkey) == EVP_PKEY_RSA) {
    // See RFC 4055.
    static const uint8_t kSHA256WithRSAEncryption[] = {
        0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b};
    // RSA always has null parameters.
    return AddSignatureAlgorithm(cbb, kSHA256WithRSAEncryption,
                                 /*null_param=*/true);
  } else if (EVP_PKEY_id(pkey) == EVP_PKEY_EC) {
    // 1.2.840.10045.4.3.2
    static const uint8_t kECDSAWithSHA256[] = {0x2a, 0x86, 0x48, 0xce,
                                               0x3d, 0x04, 0x03, 0x02};
    return AddSignatureAlgorithm(cbb, kECDSAWithSHA256,
                                 /*null_param=*/false);
  }
  return false;
}

const EVP_MD* ToEVP(DigestAlgorithm alg) {
  switch (alg) {
    case DIGEST_SHA256:
      return EVP_sha256();
  }
  return nullptr;
}

}  // namespace

// Adds an X.509 Name with the specified distinguished name to |cbb|.
bool AddName(CBB* cbb, std::string_view name) {
  // See RFC 4519.
  static const uint8_t kCommonName[] = {0x55, 0x04, 0x03};
  static const uint8_t kCountryName[] = {0x55, 0x04, 0x06};
  static const uint8_t kOrganizationName[] = {0x55, 0x04, 0x0a};
  static const uint8_t kOrganizationalUnitName[] = {0x55, 0x04, 0x0b};

  std::vector<std::string> attributes = SplitString(
      name, /*separators=*/",", base::WhitespaceHandling::TRIM_WHITESPACE,
      base::SplitResult::SPLIT_WANT_NONEMPTY);

  if (attributes.size() == 0) {
    LOG(ERROR) << "Missing DN or wrong format";
    return false;
  }

  // See RFC 5280, section 4.1.2.4.
  CBB rdns;
  if (!CBB_add_asn1(cbb, &rdns, CBS_ASN1_SEQUENCE)) {
    return false;
  }

  for (const std::string& attribute : attributes) {
    std::vector<std::string> parts =
        SplitString(attribute, /*separators=*/"=",
                    base::WhitespaceHandling::KEEP_WHITESPACE,
                    base::SplitResult::SPLIT_WANT_ALL);
    if (parts.size() != 2) {
      LOG(ERROR) << "Wrong DN format at " + attribute;
      return false;
    }

    const std::string& type_string = parts[0];
    const std::string& value_string = parts[1];
    base::span<const uint8_t> type_bytes;
    if (type_string == "CN") {
      type_bytes = kCommonName;
    } else if (type_string == "C") {
      type_bytes = kCountryName;
    } else if (type_string == "O") {
      type_bytes = kOrganizationName;
    } else if (type_string == "OU") {
      type_bytes = kOrganizationalUnitName;
    } else {
      LOG(ERROR) << "Unrecognized type " + type_string;
      return false;
    }

    CBB rdn, attr, type, value;
    if (!CBB_add_asn1(&rdns, &rdn, CBS_ASN1_SET) ||
        !CBB_add_asn1(&rdn, &attr, CBS_ASN1_SEQUENCE) ||
        !CBB_add_asn1(&attr, &type, CBS_ASN1_OBJECT) ||
        !CBB_add_bytes(&type, type_bytes.data(), type_bytes.size()) ||
        !CBB_add_asn1(&attr, &value, type_string == "C" ?
                          CBS_ASN1_PRINTABLESTRING : CBS_ASN1_UTF8STRING) ||
        !CBB_add_bytes(&value,
                       reinterpret_cast<const uint8_t*>(value_string.data()),
                       value_string.size()) ||
        !CBB_flush(&rdns)) {
      return false;
    }
  }
  if (!CBB_flush(cbb)) {
    return false;
  }
  return true;
}

NET_EXPORT net::CertificateList ConvertToX509CertificatesIgnoreErrors(
    const std::vector<std::vector<uint8_t>>& certs_bytes) {
  net::CertificateList x509_certs;
  for (const auto& cert_uint8 : certs_bytes) {
    scoped_refptr<net::X509Certificate> x509_cert =
        net::X509Certificate::CreateFromBytes(base::as_byte_span(cert_uint8));
    if (x509_cert) {
      x509_certs.push_back(std::move(x509_cert));
    }
  }
  return x509_certs;
}

bssl::ParsedCertificateList ParseAllValidCerts(
    const CertificateList& x509_certs) {
  bssl::ParsedCertificateList parsed_certs;
  for (const auto& x509_cert : x509_certs) {
    std::shared_ptr<const bssl::ParsedCertificate> cert =
        bssl::ParsedCertificate::Create(
            bssl::UpRef(x509_cert->cert_buffer()),
            net::x509_util::DefaultParseCertificateOptions(), nullptr);
    if (cert) {
      parsed_certs.push_back(std::move(cert));
    }
  }

  return parsed_certs;
}

bool CBBAddTime(CBB* cbb, base::Time time) {
  bssl::der::GeneralizedTime generalized_time;
  if (!EncodeTimeAsGeneralizedTime(time, &generalized_time)) {
    return false;
  }

  // Per RFC 5280, 4.1.2.5, times which fit in UTCTime must be encoded as
  // UTCTime rather than GeneralizedTime.
  CBB child;
  uint8_t* out;
  if (generalized_time.InUTCTimeRange()) {
    return CBB_add_asn1(cbb, &child, CBS_ASN1_UTCTIME) &&
           CBB_add_space(&child, &out, bssl::der::kUTCTimeLength) &&
           bssl::der::EncodeUTCTime(generalized_time, out) && CBB_flush(cbb);
  }

  return CBB_add_asn1(cbb, &child, CBS_ASN1_GENERALIZEDTIME) &&
         CBB_add_space(&child, &out, bssl::der::kGeneralizedTimeLength) &&
         bssl::der::EncodeGeneralizedTime(generalized_time, out) &&
         CBB_flush(cbb);
}

bool GetTLSServerEndPointChannelBinding(const X509Certificate& certificate,
                                        std::string* token) {
  static const char kChannelBindingPrefix[] = "tls-server-end-point:";

  std::string_view der_encoded_certificate =
      x509_util::CryptoBufferAsStringPiece(certificate.cert_buffer());

  bssl::der::Input tbs_certificate_tlv;
  bssl::der::Input signature_algorithm_tlv;
  bssl::der::BitString signature_value;
  if (!bssl::ParseCertificate(bssl::der::Input(der_encoded_certificate),
                              &tbs_certificate_tlv, &signature_algorithm_tlv,
                              &signature_value, nullptr)) {
    return false;
  }
  std::optional<bssl::SignatureAlgorithm> signature_algorithm =
      bssl::ParseSignatureAlgorithm(signature_algorithm_tlv);
  if (!signature_algorithm) {
    return false;
  }

  std::optional<bssl::DigestAlgorithm> binding_digest =
      bssl::GetTlsServerEndpointDigestAlgorithm(*signature_algorithm);
  if (!binding_digest) {
    return false;
  }
  const EVP_MD* digest_evp_md = nullptr;
  switch (binding_digest.value()) {
    case bssl::DigestAlgorithm::Md2:
    case bssl::DigestAlgorithm::Md4:
    case bssl::DigestAlgorithm::Md5:
    case bssl::DigestAlgorithm::Sha1:
      // Legacy digests are not supported, and
      // `GetTlsServerEndpointDigestAlgorithm` internally maps MD5 and SHA-1 to
      // SHA-256.
      NOTREACHED_IN_MIGRATION();
      break;

    case bssl::DigestAlgorithm::Sha256:
      digest_evp_md = EVP_sha256();
      break;

    case bssl::DigestAlgorithm::Sha384:
      digest_evp_md = EVP_sha384();
      break;

    case bssl::DigestAlgorithm::Sha512:
      digest_evp_md = EVP_sha512();
      break;
  }
  if (!digest_evp_md)
    return false;

  std::array<uint8_t, EVP_MAX_MD_SIZE> digest;
  unsigned int out_size;
  if (!EVP_Digest(der_encoded_certificate.data(),
                  der_encoded_certificate.size(), digest.data(), &out_size,
                  digest_evp_md, nullptr)) {
    return false;
  }

  token->assign(kChannelBindingPrefix);
  token->append(base::as_string_view(digest).substr(0, out_size));
  return true;
}

// RSA keys created by CreateKeyAndSelfSignedCert will be of this length.
static const uint16_t kRSAKeyLength = 1024;

// Certificates made by CreateKeyAndSelfSignedCert will be signed using this
// digest algorithm.
static const DigestAlgorithm kSignatureDigestAlgorithm = DIGEST_SHA256;

bool CreateKeyAndSelfSignedCert(std::string_view subject,
                                uint32_t serial_number,
                                base::Time not_valid_before,
                                base::Time not_valid_after,
                                std::unique_ptr<crypto::RSAPrivateKey>* key,
                                std::string* der_cert) {
  std::unique_ptr<crypto::RSAPrivateKey> new_key(
      crypto::RSAPrivateKey::Create(kRSAKeyLength));
  if (!new_key)
    return false;

  bool success = CreateSelfSignedCert(new_key->key(), kSignatureDigestAlgorithm,
                                      subject, serial_number, not_valid_before,
                                      not_valid_after, {}, der_cert);
  if (success)
    *key = std::move(new_key);

  return success;
}

Extension::Extension(base::span<const uint8_t> in_oid,
                     bool in_critical,
                     base::span<const uint8_t> in_contents)
    : oid(in_oid), critical(in_critical), contents(in_contents) {}
Extension::~Extension() = default;
Extension::Extension(const Extension&) = default;

bool CreateCert(EVP_PKEY* subject_key,
                DigestAlgorithm digest_alg,
                std::string_view subject,
                uint32_t serial_number,
                base::Time not_valid_before,
                base::Time not_valid_after,
                const std::vector<Extension>& extension_specs,
                std::string_view issuer,
                EVP_PKEY* issuer_key,
                std::string* der_encoded) {
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);

  // See RFC 5280, section 4.1. First, construct the TBSCertificate.
  bssl::ScopedCBB cbb;
  CBB tbs_cert, version, validity;
  uint8_t* tbs_cert_bytes;
  size_t tbs_cert_len;
  if (!CBB_init(cbb.get(), 64) ||
      !CBB_add_asn1(cbb.get(), &tbs_cert, CBS_ASN1_SEQUENCE) ||
      !CBB_add_asn1(&tbs_cert, &version,
                    CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 0) ||
      !CBB_add_asn1_uint64(&version, 2) ||
      !CBB_add_asn1_uint64(&tbs_cert, serial_number) ||
      !AddSignatureAlgorithm(&tbs_cert, issuer_key, digest_alg) ||  // signature
      !AddName(&tbs_cert, issuer) ||
      !CBB_add_asn1(&tbs_cert, &validity, CBS_ASN1_SEQUENCE) ||
      !CBBAddTime(&validity, not_valid_before) ||
      !CBBAddTime(&validity, not_valid_after) ||
      !AddName(&tbs_cert, subject) ||  // subject
      !EVP_marshal_public_key(&tbs_cert,
                              subject_key)) {  // subjectPublicKeyInfo
    return false;
  }

  if (!extension_specs.empty()) {
    CBB outer_extensions, extensions;
    if (!CBB_add_asn1(&tbs_cert, &outer_extensions,
                      3 | CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED) ||
        !CBB_add_asn1(&outer_extensions, &extensions, CBS_ASN1_SEQUENCE)) {
      return false;
    }

    for (const auto& extension_spec : extension_specs) {
      CBB extension, oid, value;
      if (!CBB_add_asn1(&extensions, &extension, CBS_ASN1_SEQUENCE) ||
          !CBB_add_asn1(&extension, &oid, CBS_ASN1_OBJECT) ||
          !CBB_add_bytes(&oid, extension_spec.oid.data(),
                         extension_spec.oid.size()) ||
          (extension_spec.critical && !CBB_add_asn1_bool(&extension, 1)) ||
          !CBB_add_asn1(&extension, &value, CBS_ASN1_OCTETSTRING) ||
          !CBB_add_bytes(&value, extension_spec.contents.data(),
                         extension_spec.contents.size()) ||
          !CBB_flush(&extensions)) {
        return false;
      }
    }

    if (!CBB_flush(&tbs_cert)) {
      return false;
    }
  }

  if (!CBB_finish(cbb.get(), &tbs_cert_bytes, &tbs_cert_len))
    return false;
  bssl::UniquePtr<uint8_t> delete_tbs_cert_bytes(tbs_cert_bytes);

  // Sign the TBSCertificate and write the entire certificate.
  CBB cert, signature;
  bssl::ScopedEVP_MD_CTX ctx;
  uint8_t* sig_out;
  size_t sig_len;
  uint8_t* cert_bytes;
  size_t cert_len;
  if (!CBB_init(cbb.get(), tbs_cert_len) ||
      !CBB_add_asn1(cbb.get(), &cert, CBS_ASN1_SEQUENCE) ||
      !CBB_add_bytes(&cert, tbs_cert_bytes, tbs_cert_len) ||
      !AddSignatureAlgorithm(&cert, issuer_key, digest_alg) ||
      !CBB_add_asn1(&cert, &signature, CBS_ASN1_BITSTRING) ||
      !CBB_add_u8(&signature, 0 /* no unused bits */) ||
      !EVP_DigestSignInit(ctx.get(), nullptr, ToEVP(digest_alg), nullptr,
                          issuer_key) ||
      // Compute the maximum signature length.
      !EVP_DigestSign(ctx.get(), nullptr, &sig_len, tbs_cert_bytes,
                      tbs_cert_len) ||
      !CBB_reserve(&signature, &sig_out, sig_len) ||
      // Actually sign the TBSCertificate.
      !EVP_DigestSign(ctx.get(), sig_out, &sig_len, tbs_cert_bytes,
                      tbs_cert_len) ||
      !CBB_did_write(&signature, sig_len) ||
      !CBB_finish(cbb.get(), &cert_bytes, &cert_len)) {
    return false;
  }
  bssl::UniquePtr<uint8_t> delete_cert_bytes(cert_bytes);
  der_encoded->assign(reinterpret_cast<char*>(cert_bytes), cert_len);
  return true;
}

bool CreateSelfSignedCert(EVP_PKEY* key,
                          DigestAlgorithm digest_alg,
                          std::string_view subject,
                          uint32_t serial_number,
                          base::Time not_valid_before,
                          base::Time not_valid_after,
                          const std::vector<Extension>& extension_specs,
                          std::string* der_encoded) {
  return CreateCert(/*subject_key=*/key, digest_alg, subject, serial_number,
                    not_valid_before, not_valid_after, extension_specs,
                    /*issuer=*/subject, /*issuer_key=*/key, der_encoded);
}

CRYPTO_BUFFER_POOL* GetBufferPool() {
  static CRYPTO_BUFFER_POOL* const kSharedPool = CRYPTO_BUFFER_POOL_new();
  return kSharedPool;
}

bssl::UniquePtr<CRYPTO_BUFFER> CreateCryptoBuffer(
    base::span<const uint8_t> data) {
  return bssl::UniquePtr<CRYPTO_BUFFER>(
      CRYPTO_BUFFER_new(data.data(), data.size(), GetBufferPool()));
}

bssl::UniquePtr<CRYPTO_BUFFER> CreateCryptoBuffer(std::string_view data) {
  return CreateCryptoBuffer(base::as_byte_span(data));
}

bssl::UniquePtr<CRYPTO_BUFFER> CreateCryptoBufferFromStaticDataUnsafe(
    base::span<const uint8_t> data) {
  return bssl::UniquePtr<CRYPTO_BUFFER>(
      CRYPTO_BUFFER_new_from_static_data_unsafe(data.data(), data.size(),
                                                GetBufferPool()));
}

bool CryptoBufferEqual(const CRYPTO_BUFFER* a, const CRYPTO_BUFFER* b) {
  DCHECK(a && b);
  if (a == b)
    return true;
  return CryptoBufferAsSpan(a) == CryptoBufferAsSpan(b);
}

std::string_view CryptoBufferAsStringPiece(const CRYPTO_BUFFER* buffer) {
  return base::as_string_view(CryptoBufferAsSpan(buffer));
}

base::span<const uint8_t> CryptoBufferAsSpan(const CRYPTO_BUFFER* buffer) {
  // SAFETY: CRYPTO_BUFFER_data(buffer) returns a pointer to data that is
  // CRYPTO_BUFFER_len(buffer) bytes in length.
  return UNSAFE_BUFFERS(
      base::make_span(CRYPTO_BUFFER_data(buffer), CRYPTO_BUFFER_len(buffer)));
}

scoped_refptr<X509Certificate> CreateX509CertificateFromBuffers(
    const STACK_OF(CRYPTO_BUFFER) * buffers) {
  if (sk_CRYPTO_BUFFER_num(buffers) == 0) {
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }

  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediate_chain;
  for (size_t i = 1; i < sk_CRYPTO_BUFFER_num(buffers); ++i) {
    intermediate_chain.push_back(
        bssl::UpRef(sk_CRYPTO_BUFFER_value(buffers, i)));
  }
  return X509Certificate::CreateFromBuffer(
      bssl::UpRef(sk_CRYPTO_BUFFER_value(buffers, 0)),
      std::move(intermediate_chain));
}

bool CreateCertBuffersFromPKCS7Bytes(
    base::span<const uint8_t> data,
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>>* handles) {
  crypto::OpenSSLErrStackTracer err_cleaner(FROM_HERE);

  CBS der_data;
  CBS_init(&der_data, data.data(), data.size());
  STACK_OF(CRYPTO_BUFFER)* certs = sk_CRYPTO_BUFFER_new_null();
  bool success =
      PKCS7_get_raw_certificates(certs, &der_data, x509_util::GetBufferPool());
  if (success) {
    for (size_t i = 0; i < sk_CRYPTO_BUFFER_num(certs); ++i) {
      handles->push_back(
          bssl::UniquePtr<CRYPTO_BUFFER>(sk_CRYPTO_BUFFER_value(certs, i)));
    }
  }
  // |handles| took ownership of the individual buffers, so only free the list
  // itself.
  sk_CRYPTO_BUFFER_free(certs);

  return success;
}

bssl::ParseCertificateOptions DefaultParseCertificateOptions() {
  bssl::ParseCertificateOptions options;
  options.allow_invalid_serial_numbers = true;
  return options;
}

bool CalculateSha256SpkiHash(const CRYPTO_BUFFER* buffer, HashValue* hash) {
  std::string_view spki;
  if (!asn1::ExtractSPKIFromDERCert(CryptoBufferAsStringPiece(buffer), &spki)) {
    return false;
  }
  *hash = HashValue(HASH_VALUE_SHA256);
  crypto::SHA256HashString(spki, hash->data(), hash->size());
  return true;
}

bool SignatureVerifierInitWithCertificate(
    crypto::SignatureVerifier* verifier,
    crypto::SignatureVerifier::SignatureAlgorithm signature_algorithm,
    base::span<const uint8_t> signature,
    const CRYPTO_BUFFER* certificate) {
  std::string_view cert_der = x509_util::CryptoBufferAsStringPiece(certificate);

  bssl::der::Input tbs_certificate_tlv;
  bssl::der::Input signature_algorithm_tlv;
  bssl::der::BitString signature_value;
  bssl::ParsedTbsCertificate tbs;
  if (!bssl::ParseCertificate(bssl::der::Input(cert_der), &tbs_certificate_tlv,
                              &signature_algorithm_tlv, &signature_value,
                              nullptr) ||
      !ParseTbsCertificate(tbs_certificate_tlv,
                           DefaultParseCertificateOptions(), &tbs, nullptr)) {
    return false;
  }

  // The key usage extension, if present, must assert the digitalSignature bit.
  if (tbs.extensions_tlv) {
    std::map<bssl::der::Input, bssl::ParsedExtension> extensions;
    if (!ParseExtensions(tbs.extensions_tlv.value(), &extensions)) {
      return false;
    }
    bssl::ParsedExtension key_usage_ext;
    if (ConsumeExtension(bssl::der::Input(bssl::kKeyUsageOid), &extensions,
                         &key_usage_ext)) {
      bssl::der::BitString key_usage;
      if (!bssl::ParseKeyUsage(key_usage_ext.value, &key_usage) ||
          !key_usage.AssertsBit(bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE)) {
        return false;
      }
    }
  }

  return verifier->VerifyInit(signature_algorithm, signature, tbs.spki_tlv);
}

bool HasRsaPkcs1Sha1Signature(const CRYPTO_BUFFER* cert_buffer) {
  bssl::der::Input tbs_certificate_tlv;
  bssl::der::Input signature_algorithm_tlv;
  bssl::der::BitString signature_value;
  if (!bssl::ParseCertificate(bssl::der::Input(CryptoBufferAsSpan(cert_buffer)),
                              &tbs_certificate_tlv, &signature_algorithm_tlv,
                              &signature_value, /*out_errors=*/nullptr)) {
    return false;
  }

  std::optional<bssl::SignatureAlgorithm> signature_algorithm =
      bssl::ParseSignatureAlgorithm(signature_algorithm_tlv);

  return signature_algorithm &&
         *signature_algorithm == bssl::SignatureAlgorithm::kRsaPkcs1Sha1;
}

}  // namespace net::x509_util
