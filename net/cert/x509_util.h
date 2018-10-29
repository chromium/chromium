// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_X509_UTIL_H_
#define NET_CERT_X509_UTIL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "crypto/signature_verifier.h"
#include "net/base/hash_value.h"
#include "net/base/net_export.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace crypto {
class RSAPrivateKey;
}

namespace net {

struct ParseCertificateOptions;
class X509Certificate;

namespace x509_util {

// Supported digest algorithms for signing certificates.
enum DigestAlgorithm { DIGEST_SHA256 };

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
// |subject| is a distinguished name defined in RFC4514 with _only_ a CN
// component, as in:
//   CN=Michael Wong
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
    const std::string& subject,
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

  base::span<const uint8_t> oid;
  bool critical;
  base::span<const uint8_t> contents;
};

// Creates a self-signed certificate from a provided key, using the specified
// hash algorithm.
NET_EXPORT bool CreateSelfSignedCert(
    EVP_PKEY* key,
    DigestAlgorithm alg,
    const std::string& subject,
    uint32_t serial_number,
    base::Time not_valid_before,
    base::Time not_valid_after,
    const std::vector<Extension>& extension_specs,
    std::string* der_cert);

// Returns a CRYPTO_BUFFER_POOL for deduplicating certificates.
NET_EXPORT CRYPTO_BUFFER_POOL* GetBufferPool();

// Creates a CRYPTO_BUFFER in the same pool returned by GetBufferPool.
NET_EXPORT bssl::UniquePtr<CRYPTO_BUFFER> CreateCryptoBuffer(
    const uint8_t* data,
    size_t length);

// Creates a CRYPTO_BUFFER in the same pool returned by GetBufferPool.
NET_EXPORT bssl::UniquePtr<CRYPTO_BUFFER> CreateCryptoBuffer(
    const base::StringPiece& data);

// Overload with no definition, to disallow creating a CRYPTO_BUFFER from a
// char* due to StringPiece implicit ctor.
NET_EXPORT bssl::UniquePtr<CRYPTO_BUFFER> CreateCryptoBuffer(
    const char* invalid_data);

// Compares two CRYPTO_BUFFERs and returns true if they have the same contents.
NET_EXPORT bool CryptoBufferEqual(const CRYPTO_BUFFER* a,
                                  const CRYPTO_BUFFER* b);

// Returns a StringPiece pointing to the data in |buffer|.
NET_EXPORT base::StringPiece CryptoBufferAsStringPiece(
    const CRYPTO_BUFFER* buffer);

// Creates a new X509Certificate from the chain in |buffers|, which must have at
// least one element.
NET_EXPORT scoped_refptr<X509Certificate> CreateX509CertificateFromBuffers(
    const STACK_OF(CRYPTO_BUFFER) * buffers);

// Returns the default ParseCertificateOptions for the net stack.
NET_EXPORT ParseCertificateOptions DefaultParseCertificateOptions();

// On success, returns true and updates |hash| to be the SHA-256 hash of the
// subjectPublicKeyInfo of the certificate in |buffer|. If |buffer| is not a
// valid certificate, returns false and |hash| is in an undefined state.
NET_EXPORT bool CalculateSha256SpkiHash(const CRYPTO_BUFFER* buffer,
                                        HashValue* hash) WARN_UNUSED_RESULT;

// Calls |verifier->VerifyInit|, using the public key from |certificate|,
// checking if the digitalSignature key usage bit is present, and returns true
// on success or false on error.
NET_EXPORT bool SignatureVerifierInitWithCertificate(
    crypto::SignatureVerifier* verifier,
    crypto::SignatureVerifier::SignatureAlgorithm signature_algorithm,
    base::span<const uint8_t> signature,
    const CRYPTO_BUFFER* certificate);

}  // namespace x509_util

}  // namespace net

#endif  // NET_CERT_X509_UTIL_H_
