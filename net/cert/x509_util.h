// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_X509_UTIL_H_
#define NET_CERT_X509_UTIL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_span.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "crypto/signature_verifier.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"

namespace crypto {
class RSAPrivateKey;
}

namespace net {

namespace x509_util {

// Convert a vector of bytes into X509Certificate objects.
// This will silently drop all input that does not parse, so be careful using
// this.
NET_EXPORT net::CertificateList ConvertToX509CertificatesIgnoreErrors(
    const std::vector<std::vector<uint8_t>>& certs_bytes);

// Parse all certificiates with default parsing options. Return those that
// parse.
// This will silently drop all certs with parsing errors, so be careful using
// this.
NET_EXPORT bssl::ParsedCertificateList ParseAllValidCerts(
    const CertificateList& x509_certs);

// Supported digest algorithms for signing certificates.
enum DigestAlgorithm { DIGEST_SHA256 };

// Adds a RFC 5280 Time value to the given CBB.
NET_EXPORT bool CBBAddTime(CBB* cbb, base::Time time);

// Adds an X.509 name to |cbb|. The name is determined by parsing |name| as
// a comma-separated list of type=value pairs, such as "O=Organization,
// CN=Common Name".
//
// WARNING: This function does not implement the full RFC 4514 syntax for
// distinguished names. It should only be used if |name| is a constant
// value, rather than programmatically constructed. If programmatic support
// is needed, this input should be replaced with a richer type.
NET_EXPORT bool AddName(CBB* cbb, std::string_view name);

// Generate a 'tls-server-end-point' channel binding based on the specified
// certificate. Channel bindings are based on RFC 5929.
NET_EXPORT_PRIVATE bool GetTLSServerEndPointChannelBinding(
    const X509Certificate& certificate,
    std::string* token);

// Creates a public-private keypair and a self-signed certificate.
// Subject, serial number and validity period are given as parameters.
// The certificate is signed by the private key in |key|. The key length and
// signature algorithm may be updated periodically to match best practices.
//
// |subject| specifies the subject and issuer names as in AddName()
//
// SECURITY WARNING
//
// Using self-signed certificates has the following security risks:
// 1. Encryption without authentication and thus vulnerable to
//    man-in-the-middle attacks.
// 2. Self-signed certificates cannot be revoked.
//
// Use this certificate only after the above risks are acknowledged.
NET_EXPORT bool CreateKeyAndSelfSignedCert(
    std::string_view subject,
    uint32_t serial_number,
    base::Time not_valid_before,
    base::Time not_valid_after,
    std::unique_ptr<crypto::RSAPrivateKey>* key,
    std::string* der_cert);

struct NET_EXPORT Extension {
  Extension(base::span<const uint8_t> oid,
            bool critical,
            base::span<const uint8_t> contents);
  ~Extension();
  Extension(const Extension&);

  base::raw_span<const uint8_t> oid;
  bool critical;
  base::raw_span<const uint8_t> contents;
};

// Create a certificate signed by |issuer_key| and write it to |der_encoded|.
//
// |subject| and |issuer| specify names as in AddName(). If you want to create
// a self-signed certificate, see |CreateSelfSignedCert|.
NET_EXPORT bool CreateCert(EVP_PKEY* subject_key,
                           DigestAlgorithm digest_alg,
                           std::string_view subject,
                           uint32_t serial_number,
                           base::Time not_valid_before,
                           base::Time not_valid_after,
                           const std::vector<Extension>& extension_specs,
                           std::string_view issuer,
                           EVP_PKEY* issuer_key,
                           std::string* der_encoded);

// Creates a self-signed certificate from a provided key, using the specified
// hash algorithm.
//
// |subject| specifies the subject and issuer names as in AddName().
NET_EXPORT bool CreateSelfSignedCert(
    EVP_PKEY* key,
    DigestAlgorithm alg,
    std::string_view subject,
    uint32_t serial_number,
    base::Time not_valid_before,
    base::Time not_valid_after,
    const std::vector<Extension>& extension_specs,
    std::string* der_cert);

// Returns a CRYPTO_BUFFER_POOL for deduplicating certificates.
NET_EXPORT CRYPTO_BUFFER_POOL* GetBufferPool();

// Creates a CRYPTO_BUFFER in the same pool returned by GetBufferPool.
NET_EXPORT bssl::UniquePtr<CRYPTO_BUFFER> CreateCryptoBuffer(
    base::span<const uint8_t> data);

// Creates a CRYPTO_BUFFER in the same pool returned by GetBufferPool.
NET_EXPORT bssl::UniquePtr<CRYPTO_BUFFER> CreateCryptoBuffer(
    std::string_view data);

// Overload with no definition, to disallow creating a CRYPTO_BUFFER from a
// char* due to std::string_view implicit ctor.
NET_EXPORT bssl::UniquePtr<CRYPTO_BUFFER> CreateCryptoBuffer(
    const char* invalid_data);

// Creates a CRYPTO_BUFFER in the same pool returned by GetBufferPool backed by
// |data| without copying. |data| must be immutable and last for the lifetime
// of the address space.
NET_EXPORT bssl::UniquePtr<CRYPTO_BUFFER>
CreateCryptoBufferFromStaticDataUnsafe(base::span<const uint8_t> data);

// Compares two CRYPTO_BUFFERs and returns true if they have the same contents.
NET_EXPORT bool CryptoBufferEqual(const CRYPTO_BUFFER* a,
                                  const CRYPTO_BUFFER* b);

// Returns a std::string_view pointing to the data in |buffer|.
NET_EXPORT std::string_view CryptoBufferAsStringPiece(
    const CRYPTO_BUFFER* buffer);

// Returns a span pointing to the data in |buffer|.
NET_EXPORT base::span<const uint8_t> CryptoBufferAsSpan(
    const CRYPTO_BUFFER* buffer);

// Creates a new X509Certificate from the chain in |buffers|, which must have at
// least one element.
NET_EXPORT scoped_refptr<X509Certificate> CreateX509CertificateFromBuffers(
    const STACK_OF(CRYPTO_BUFFER) * buffers);

// Parses certificates from a PKCS#7 SignedData structure, appending them to
// |handles|. Returns true on success (in which case zero or more elements were
// added to |handles|) and false on error (in which case |handles| is
// unmodified).
NET_EXPORT bool CreateCertBuffersFromPKCS7Bytes(
    base::span<const uint8_t> data,
    std::vector<bssl::UniquePtr<CRYPTO_BUFFER>>* handles);

// Returns the default ParseCertificateOptions for the net stack.
NET_EXPORT bssl::ParseCertificateOptions DefaultParseCertificateOptions();

// On success, returns true and updates |hash| to be the SHA-256 hash of the
// subjectPublicKeyInfo of the certificate in |buffer|. If |buffer| is not a
// valid certificate, returns false and |hash| is in an undefined state.
[[nodiscard]] NET_EXPORT bool CalculateSha256SpkiHash(
    const CRYPTO_BUFFER* buffer,
    HashValue* hash);

// Calls |verifier->VerifyInit|, using the public key from |certificate|,
// checking if the digitalSignature key usage bit is present, and returns true
// on success or false on error.
NET_EXPORT bool SignatureVerifierInitWithCertificate(
    crypto::SignatureVerifier* verifier,
    crypto::SignatureVerifier::SignatureAlgorithm signature_algorithm,
    base::span<const uint8_t> signature,
    const CRYPTO_BUFFER* certificate);

// Returns true if the signature on the certificate is RSASSA-PKCS1-v1_5 with
// SHA-1.
NET_EXPORT_PRIVATE bool HasRsaPkcs1Sha1Signature(
    const CRYPTO_BUFFER* cert_buffer);

}  // namespace x509_util

}  // namespace net

#endif  // NET_CERT_X509_UTIL_H_
