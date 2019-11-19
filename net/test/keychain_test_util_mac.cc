// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/keychain_test_util_mac.h"

#include <Security/SecCertificate.h>
#include <Security/SecImportExport.h>

#include "base/mac/mac_logging.h"
#include "net/cert/x509_util_mac.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"

namespace net {

namespace {

base::ScopedCFTypeRef<SecIdentityRef> GetSecIdentityRefForCertificate(
    SecCertificateRef cert,
    SecKeychainRef keychain) {
  OSStatus status;
  base::ScopedCFTypeRef<SecIdentityRef> identity;
  status = SecIdentityCreateWithCertificate(keychain, cert,
                                            identity.InitializeInto());
  if (status != noErr) {
    OSSTATUS_LOG(WARNING, status);
    return base::ScopedCFTypeRef<SecIdentityRef>();
  }
  return identity;
}

}  // namespace

ScopedTestKeychain::ScopedTestKeychain() = default;
ScopedTestKeychain::~ScopedTestKeychain() = default;

bool ScopedTestKeychain::Initialize() {
  if (!keychain_dir_.CreateUniqueTempDir())
    return false;
  base::FilePath keychain_path =
      keychain_dir_.GetPath().AppendASCII("test_keychain.keychain");
  return SecKeychainCreate(keychain_path.value().c_str(), 0, "", FALSE, nullptr,
                           keychain_.InitializeInto()) == noErr;
}

base::ScopedCFTypeRef<SecIdentityRef> ImportCertAndKeyToKeychain(
    const X509Certificate* cert,
    const std::string pkcs8,
    SecKeychainRef keychain) {
  // Insert the certificate into the keychain.
  base::ScopedCFTypeRef<SecCertificateRef> sec_cert(
      x509_util::CreateSecCertificateFromX509Certificate(cert));
  if (!sec_cert)
    return base::ScopedCFTypeRef<SecIdentityRef>();
  if (noErr != SecCertificateAddToKeychain(sec_cert, keychain))
    return base::ScopedCFTypeRef<SecIdentityRef>();

  // Import the key into the keychain. Apple doesn't accept unencrypted PKCS#8,
  // but it accepts the low-level RSAPrivateKey and ECPrivateKey types as
  // "kSecFormatOpenSSL", so produce those. There doesn't appear to be a way to
  // tell it which key type we have, so leave this unspecified and have it
  // guess.
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(pkcs8.data()), pkcs8.size());
  bssl::UniquePtr<EVP_PKEY> openssl_key(EVP_parse_private_key(&cbs));
  if (!openssl_key || CBS_len(&cbs) != 0)
    return base::ScopedCFTypeRef<SecIdentityRef>();

  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), 0))
    return base::ScopedCFTypeRef<SecIdentityRef>();
  if (EVP_PKEY_id(openssl_key.get()) == EVP_PKEY_RSA) {
    if (!RSA_marshal_private_key(cbb.get(),
                                 EVP_PKEY_get0_RSA(openssl_key.get())))
      return base::ScopedCFTypeRef<SecIdentityRef>();
  } else if (EVP_PKEY_id(openssl_key.get()) == EVP_PKEY_EC) {
    if (!EC_KEY_marshal_private_key(cbb.get(),
                                    EVP_PKEY_get0_EC_KEY(openssl_key.get()), 0))
      return base::ScopedCFTypeRef<SecIdentityRef>();
  } else {
    return base::ScopedCFTypeRef<SecIdentityRef>();
  }

  uint8_t* encoded;
  size_t encoded_len;
  if (!CBB_finish(cbb.get(), &encoded, &encoded_len))
    return base::ScopedCFTypeRef<SecIdentityRef>();
  bssl::UniquePtr<uint8_t> scoped_encoded(encoded);

  base::ScopedCFTypeRef<CFDataRef> encoded_ref(
      CFDataCreate(kCFAllocatorDefault, encoded, encoded_len));
  SecExternalFormat format = kSecFormatOpenSSL;
  SecExternalItemType item_type = kSecItemTypePrivateKey;
  if (noErr != SecItemImport(encoded_ref, nullptr, &format, &item_type, 0,
                             nullptr, keychain, nullptr)) {
    return base::ScopedCFTypeRef<SecIdentityRef>();
  }

  base::ScopedCFTypeRef<SecIdentityRef> sec_identity =
      GetSecIdentityRefForCertificate(sec_cert, keychain);
  return sec_identity;
}

}  // namespace net
