// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_KEYPAIR_H_
#define CRYPTO_KEYPAIR_H_

#include <array>
#include <vector>

#include "base/containers/span.h"
#include "crypto/crypto_export.h"
#include "crypto/subtle_passkey.h"
#include "third_party/boringssl/src/include/openssl/base.h"

namespace crypto::keypair {

// This class wraps an EVP_PKEY containing a private key. Since EVP_PKEY is
// refcounted, PrivateKey is extremely cheap to copy and is intended to be
// passed around by value. All the public constructors are static functions that
// enforce constraints on the type of key they will generate or import; the
// constructor that accepts a raw EVP_PKEY requires a SubtlePassKey to
// discourage client code from dealing in EVP_PKEYs directly.
class CRYPTO_EXPORT PrivateKey {
 public:
  // Directly construct a PrivateKey from an EVP_PKEY. Prefer to use one of the
  // static factory functions below, which do not require a SubtlePassKey.
  PrivateKey(bssl::UniquePtr<EVP_PKEY> key, crypto::SubtlePassKey);
  ~PrivateKey();
  PrivateKey(PrivateKey&& other);
  PrivateKey(const PrivateKey& other);

  PrivateKey& operator=(PrivateKey&& other);
  PrivateKey& operator=(const PrivateKey& other);

  // These functions generate fresh, random RSA private keys of the named sizes
  // with e = 65537.
  // If you believe you need an RSA key of a size other than these, or with a
  // different exponent, please contact a member of //CRYPTO_OWNERS.
  static PrivateKey GenerateRsa2048();
  static PrivateKey GenerateRsa4096();

  // Generates a fresh, random elliptic curve key on the specified curve.
  static PrivateKey GenerateEcP256();
  static PrivateKey GenerateEcP384();
  static PrivateKey GenerateEcP521();

  // Generates a fresh, random Ed25519 key.
  static PrivateKey GenerateEd25519();

  // Imports a PKCS#8 PrivateKeyInfo block. Returns nullopt if the passed-in
  // buffer is not a valid PrivateKeyInfo block, or if there is trailing data in
  // it after the PrivateKeyInfo block.
  static std::optional<PrivateKey> FromPrivateKeyInfo(
      base::span<const uint8_t> pki);

  // Imports an RFC 8032-encoded Ed25519 private key.
  //
  // The encoding used doesn't allow for importing to fail (all input bit
  // strings are potentially valid keys).
  static PrivateKey FromEd25519PrivateKey(base::span<const uint8_t, 32> key);

  // Deliberately not present in this API:
  // ECPrivateKey::CreateFromEncryptedPrivateKeyInfo(): imports a PKCS#8
  // EncryptedPrivateKeyInfo with a hardcoded empty password. There is no reason
  // to ever do this and there is only one client (the GCM code).

  // Exports a PKCS#8 PrivateKeyInfo block.
  std::vector<uint8_t> ToPrivateKeyInfo() const;

  // Exports an Ed25519 private key in RFC 8032 format. It is illegal to call
  // this if !IsEd25519().
  std::array<uint8_t, 32> ToEd25519PrivateKey() const;

  // Computes and exports an X.509 SubjectPublicKeyInfo block corresponding to
  // this key.
  std::vector<uint8_t> ToSubjectPublicKeyInfo() const;

  // Exports an EC public key in X9.62 uncompressed form. It is illegal to call
  // this on a non-EC PrivateKey.
  std::vector<uint8_t> ToUncompressedX962Point() const;

  // Exports an Ed25519 public key in RFC 8032 format. It is illegal to call
  // this if !IsEd25519().
  std::array<uint8_t, 32> ToEd25519PublicKey() const;

  EVP_PKEY* key() { return key_.get(); }
  const EVP_PKEY* key() const { return key_.get(); }

  bool IsRsa() const;
  bool IsEc() const;
  bool IsEd25519() const;

  bool IsEcP256() const;
  bool IsEcP384() const;
  bool IsEcP521() const;

 private:
  explicit PrivateKey(bssl::UniquePtr<EVP_PKEY> key);

  bssl::UniquePtr<EVP_PKEY> key_;
};

class CRYPTO_EXPORT PublicKey {
 public:
  // Construct a PublicKey directly from an EVP_PKEY. Prefer to use one of the
  // static factory functions below, which do not require a SubtlePassKey.
  PublicKey(bssl::UniquePtr<EVP_PKEY> key, crypto::SubtlePassKey);
  ~PublicKey();
  PublicKey(PublicKey&& other);
  PublicKey(const PublicKey& other);

  PublicKey& operator=(PublicKey&& other);
  PublicKey& operator=(const PublicKey& other);

  // Produces the PublicKey corresponding to the given PrivateKey. This is
  // mostly useful in tests but is fine to use in production as well.
  static PublicKey FromPrivateKey(const PrivateKey& key);

  // Imports a PublicKey from an X.509 SubjectPublicKeyInfo. This may return
  // nullopt if the SubjectPublicKeyInfo is ill-formed.
  static std::optional<PublicKey> FromSubjectPublicKeyInfo(
      base::span<const uint8_t> spki);

  // Imports a pair of big-endian big integers (n, e) to form an RSA public key.
  // Returns nullopt if the parameters are invalid for some reason.
  //
  // Note: if you need to serialize and deserialize RSA keys, you should
  // probably use SubjectPublicKeyInfo instead of rolling your own serialization
  // format for the (n, e) pair.
  static std::optional<PublicKey> FromRsaPublicKeyComponents(
      base::span<const uint8_t> n,
      base::span<const uint8_t> e);

  // Imports a big-endian integer point to form an EC public key. Returns
  // nullopt if the point is not on the curve or something else is wrong with
  // it.
  //
  // Note: unless you *only* want an EC key on a fixed curve, you should use
  // SubjectPublicKeyInfo as a serialization format rather than inventing your
  // own format.
  static std::optional<PublicKey> FromEcP256Point(
      base::span<const uint8_t> point);
  static std::optional<PublicKey> FromEcP384Point(
      base::span<const uint8_t> point);
  static std::optional<PublicKey> FromEcP521Point(
      base::span<const uint8_t> point);

  // Imports an Ed25519 public key in RFC 8032 format.
  //
  // Note: the size is hardcoded to 32 here rather than ED25519_PUBLIC_KEY_LEN
  // to avoid pulling curve25519.h into this file. Also, it's impossible for
  // importing to fail.
  static PublicKey FromEd25519PublicKey(base::span<const uint8_t, 32> key);

  // Exports a PublicKey as an X.509 SubjectPublicKeyInfo.
  std::vector<uint8_t> ToSubjectPublicKeyInfo() const;

  // Exports an EC public key in X9.62 uncompressed form. It is illegal to call
  // this on a non-EC PublicKey.
  std::vector<uint8_t> ToUncompressedX962Point() const;

  // Export the components (e, n) of an RSA public key, as big-endian integers.
  // It is illegal to call these on a non-RSA PublicKey.
  std::vector<uint8_t> GetRsaExponent() const;
  std::vector<uint8_t> GetRsaModulus() const;

  EVP_PKEY* key() { return key_.get(); }
  const EVP_PKEY* key() const { return key_.get(); }

  bool IsRsa() const;
  bool IsEc() const;
  bool IsEd25519() const;

  bool IsEcP256() const;
  bool IsEcP384() const;
  bool IsEcP521() const;

 private:
  explicit PublicKey(bssl::UniquePtr<EVP_PKEY> key);

  bssl::UniquePtr<EVP_PKEY> key_;
};

}  // namespace crypto::keypair

#endif  // CRYPTO_KEYPAIR_H_
