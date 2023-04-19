// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_NSS_KEY_UTIL_H_
#define CRYPTO_NSS_KEY_UTIL_H_

#include <secoidt.h>
#include <stdint.h>

#include "base/containers/span.h"
#include "build/build_config.h"
#include "crypto/crypto_export.h"
#include "crypto/scoped_nss_types.h"

typedef struct PK11SlotInfoStr PK11SlotInfo;

namespace crypto {

// Returns a SECItem containing the CKA_ID of the `public_key` or nullptr on
// error.
CRYPTO_EXPORT crypto::ScopedSECItem MakeNssIdFromPublicKey(
    SECKEYPublicKey* public_key);

// Decodes |input| as a SubjectPublicKeyInfo and returns a SECItem containing
// the CKA_ID of that public key or nullptr on error.
CRYPTO_EXPORT ScopedSECItem MakeNssIdFromSpki(base::span<const uint8_t> input);

// Generates a new RSA key pair of size |num_bits| in |slot|. Returns true on
// success and false on failure. If |permanent| is true, the resulting key is
// permanent and is not exportable in plaintext form.
CRYPTO_EXPORT bool GenerateRSAKeyPairNSS(
    PK11SlotInfo* slot,
    uint16_t num_bits,
    bool permanent,
    ScopedSECKEYPublicKey* out_public_key,
    ScopedSECKEYPrivateKey* out_private_key);

// Generates a new EC key pair with curve |named_curve| in |slot|. Returns true
// on success and false on failure. If |permanent| is true, the resulting key is
// permanent and is not exportable in plaintext form.
CRYPTO_EXPORT bool GenerateECKeyPairNSS(
    PK11SlotInfo* slot,
    const SECOidTag named_curve,
    bool permanent,
    ScopedSECKEYPublicKey* out_public_key,
    ScopedSECKEYPrivateKey* out_private_key);

// Imports a private key from |input| into |slot|. |input| is interpreted as a
// DER-encoded PrivateKeyInfo block from PKCS #8. Returns nullptr on error. If
// |permanent| is true, the resulting key is permanent and is not exportable in
// plaintext form.
CRYPTO_EXPORT ScopedSECKEYPrivateKey
ImportNSSKeyFromPrivateKeyInfo(PK11SlotInfo* slot,
                               base::span<const uint8_t> input,
                               bool permanent);

// Decodes |input| as a DER-encoded X.509 SubjectPublicKeyInfo and searches for
// the private key half in the key database. Returns the private key on success
// or nullptr on error.
// Note: This function assumes the CKA_ID for public/private key pairs is
// derived from the public key. NSS does this, but this is not guaranteed by
// PKCS#11, so keys generated outside of NSS may not be found.
CRYPTO_EXPORT ScopedSECKEYPrivateKey
FindNSSKeyFromPublicKeyInfo(base::span<const uint8_t> input);

// Decodes |input| as a DER-encoded X.509 SubjectPublicKeyInfo and searches for
// the private key half in the slot specified by |slot|. Returns the private key
// on success or nullptr on error.
// Note: This function assumes the CKA_ID for public/private key pairs is
// derived from the public key. NSS does this, but this is not guaranteed by
// PKCS#11, so keys generated outside of NSS may not be found.
CRYPTO_EXPORT ScopedSECKEYPrivateKey
FindNSSKeyFromPublicKeyInfoInSlot(base::span<const uint8_t> input,
                                  PK11SlotInfo* slot);

// Decodes |input| as a DER-encoded X.509 SubjectPublicKeyInfo and returns the
// NSS representation of it.
CRYPTO_EXPORT ScopedCERTSubjectPublicKeyInfo
DecodeSubjectPublicKeyInfoNSS(base::span<const uint8_t> input);

}  // namespace crypto

#endif  // CRYPTO_NSS_KEY_UTIL_H_
