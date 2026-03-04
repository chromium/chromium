// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "crypto/apple/test_helpers.h"

#import <Security/Security.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "crypto/evp.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace crypto::apple {

base::apple::ScopedCFTypeRef<SecKeyRef> SecKeyFromPKCS8(
    base::span<const uint8_t> pkcs8) {
  bssl::UniquePtr<EVP_PKEY> openssl_key =
      crypto::evp::PrivateKeyFromBytes(pkcs8);
  if (!openssl_key) {
    return base::apple::ScopedCFTypeRef<SecKeyRef>();
  }

  // `SecKeyCreateWithData` expects PKCS#1 for RSA keys, and a concatenated
  // format for EC keys. See `SecKeyCopyExternalRepresentation` for details.
  CFStringRef key_type;
  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 0)) {
    return base::apple::ScopedCFTypeRef<SecKeyRef>();
  }
  if (EVP_PKEY_id(openssl_key.get()) == EVP_PKEY_RSA) {
    key_type = kSecAttrKeyTypeRSA;
    if (!RSA_marshal_private_key(cbb.get(),
                                 EVP_PKEY_get0_RSA(openssl_key.get()))) {
      return base::apple::ScopedCFTypeRef<SecKeyRef>();
    }
  } else if (EVP_PKEY_id(openssl_key.get()) == EVP_PKEY_EC) {
    key_type = kSecAttrKeyTypeECSECPrimeRandom;
    const EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(openssl_key.get());
    size_t priv_len = EC_KEY_priv2oct(ec_key, nullptr, 0);
    uint8_t* out;
    if (priv_len == 0 ||
        !EC_POINT_point2cbb(cbb.get(), EC_KEY_get0_group(ec_key),
                            EC_KEY_get0_public_key(ec_key),
                            POINT_CONVERSION_UNCOMPRESSED, nullptr) ||
        !CBB_add_space(cbb.get(), &out, priv_len) ||
        EC_KEY_priv2oct(ec_key, out, priv_len) != priv_len) {
      return base::apple::ScopedCFTypeRef<SecKeyRef>();
    }
  } else {
    return base::apple::ScopedCFTypeRef<SecKeyRef>();
  }

  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> attrs(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  CFDictionarySetValue(attrs.get(), kSecAttrKeyClass, kSecAttrKeyClassPrivate);
  CFDictionarySetValue(attrs.get(), kSecAttrKeyType, key_type);

  base::apple::ScopedCFTypeRef<CFDataRef> data(
      CFDataCreate(kCFAllocatorDefault, CBB_data(cbb.get()),
                   base::checked_cast<CFIndex>(CBB_len(cbb.get()))));

  return base::apple::ScopedCFTypeRef<SecKeyRef>(
      SecKeyCreateWithData(data.get(), attrs.get(), nullptr));
}

}  // namespace crypto::apple
