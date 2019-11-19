// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/test/cert_test_util.h"

#include <pk11pub.h>
#include <secmodt.h>
#include <string.h>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "crypto/ec_private_key.h"
#include "crypto/nss_key_util.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/cert_type.h"
#include "net/cert/x509_util_nss.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace net {

bool ImportSensitiveKeyFromFile(const base::FilePath& dir,
                                const std::string& key_filename,
                                PK11SlotInfo* slot) {
  base::FilePath key_path = dir.AppendASCII(key_filename);
  std::string key_pkcs8;
  bool success = base::ReadFileToString(key_path, &key_pkcs8);
  if (!success) {
    LOG(ERROR) << "Failed to read file " << key_path.value();
    return false;
  }

  std::vector<uint8_t> key_vector(key_pkcs8.begin(), key_pkcs8.end());

  // Prior to NSS 3.30, NSS cannot import unencrypted ECDSA private keys. Detect
  // such keys and encrypt with an empty password before importing. Once our
  // minimum version is raised to NSS 3.30, this logic can be removed. See
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1295121
  CBS cbs;
  CBS_init(&cbs, key_vector.data(), key_vector.size());
  bssl::UniquePtr<EVP_PKEY> evp_pkey(EVP_parse_private_key(&cbs));
  if (!evp_pkey) {
    LOG(ERROR) << "Could not parse private key from file " << key_path.value();
    return false;
  }
  if (EVP_PKEY_id(evp_pkey.get()) == EVP_PKEY_EC) {
    std::unique_ptr<crypto::ECPrivateKey> ec_private_key =
        crypto::ECPrivateKey::CreateFromPrivateKeyInfo(key_vector);
    std::vector<uint8_t> encrypted;
    if (!ec_private_key ||
        !ec_private_key->ExportEncryptedPrivateKey(&encrypted)) {
      LOG(ERROR) << "Error importing private key from file "
                 << key_path.value();
      return false;
    }

    SECItem encrypted_item = {siBuffer, encrypted.data(),
                              static_cast<unsigned>(encrypted.size())};
    SECKEYEncryptedPrivateKeyInfo epki;
    memset(&epki, 0, sizeof(epki));
    crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
    if (SEC_QuickDERDecodeItem(
            arena.get(), &epki,
            SEC_ASN1_GET(SECKEY_EncryptedPrivateKeyInfoTemplate),
            &encrypted_item) != SECSuccess) {
      LOG(ERROR) << "Error importing private key from file "
                 << key_path.value();
      return false;
    }

    // NSS uses the serialized public key in X9.62 form as the "public value"
    // for key ID purposes.
    EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(ec_private_key->key());
    bssl::ScopedCBB cbb;
    uint8_t* public_value;
    size_t public_value_len;
    if (!CBB_init(cbb.get(), 0) ||
        !EC_POINT_point2cbb(cbb.get(), EC_KEY_get0_group(ec_key),
                            EC_KEY_get0_public_key(ec_key),
                            POINT_CONVERSION_UNCOMPRESSED, nullptr) ||
        !CBB_finish(cbb.get(), &public_value, &public_value_len)) {
      LOG(ERROR) << "Error importing private key from file "
                 << key_path.value();
      return false;
    }
    bssl::UniquePtr<uint8_t> scoped_public_value(public_value);
    SECItem public_item = {siBuffer, public_value,
                           static_cast<unsigned>(public_value_len)};

    SECItem password_item = {siBuffer, nullptr, 0};
    if (PK11_ImportEncryptedPrivateKeyInfo(
            slot, &epki, &password_item, nullptr /* nickname */, &public_item,
            PR_TRUE /* permanent */, PR_TRUE /* private */, ecKey,
            KU_DIGITAL_SIGNATURE, nullptr /* wincx */) != SECSuccess) {
      LOG(ERROR) << "Error importing private key from file "
                 << key_path.value();
      return false;
    }
    return true;
  }

  crypto::ScopedSECKEYPrivateKey private_key(
      crypto::ImportNSSKeyFromPrivateKeyInfo(slot, key_vector,
                                             true /* permanent */));
  LOG_IF(ERROR, !private_key) << "Could not create key from file "
                              << key_path.value();
  return !!private_key;
}

bool ImportClientCertToSlot(CERTCertificate* nss_cert, PK11SlotInfo* slot) {
  std::string nickname =
      x509_util::GetDefaultUniqueNickname(nss_cert, USER_CERT, slot);
  SECStatus rv = PK11_ImportCert(slot, nss_cert, CK_INVALID_HANDLE,
                                 nickname.c_str(), PR_FALSE);
  if (rv != SECSuccess) {
    LOG(ERROR) << "Could not import cert";
    return false;
  }
  return true;
}

ScopedCERTCertificate ImportClientCertToSlot(
    const scoped_refptr<X509Certificate>& cert,
    PK11SlotInfo* slot) {
  ScopedCERTCertificate nss_cert =
      x509_util::CreateCERTCertificateFromX509Certificate(cert.get());
  if (!nss_cert)
    return nullptr;

  if (!ImportClientCertToSlot(nss_cert.get(), slot))
    return nullptr;

  return nss_cert;
}

scoped_refptr<X509Certificate> ImportClientCertAndKeyFromFile(
    const base::FilePath& dir,
    const std::string& cert_filename,
    const std::string& key_filename,
    PK11SlotInfo* slot,
    ScopedCERTCertificate* nss_cert) {
  if (!ImportSensitiveKeyFromFile(dir, key_filename, slot)) {
    LOG(ERROR) << "Could not import private key from file " << key_filename;
    return nullptr;
  }

  scoped_refptr<X509Certificate> cert(ImportCertFromFile(dir, cert_filename));

  if (!cert.get()) {
    LOG(ERROR) << "Failed to parse cert from file " << cert_filename;
    return nullptr;
  }

  *nss_cert = ImportClientCertToSlot(cert, slot);
  if (!*nss_cert)
    return nullptr;

  // |cert| continues to point to the original X509Certificate before the
  // import to |slot|. However this should not make a difference as NSS handles
  // state globally.
  return cert;
}

scoped_refptr<X509Certificate> ImportClientCertAndKeyFromFile(
    const base::FilePath& dir,
    const std::string& cert_filename,
    const std::string& key_filename,
    PK11SlotInfo* slot) {
  ScopedCERTCertificate nss_cert;
  return ImportClientCertAndKeyFromFile(dir, cert_filename, key_filename, slot,
                                        &nss_cert);
}

ScopedCERTCertificate ImportCERTCertificateFromFile(
    const base::FilePath& certs_dir,
    const std::string& cert_file) {
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(certs_dir, cert_file);
  if (!cert)
    return nullptr;
  return x509_util::CreateCERTCertificateFromX509Certificate(cert.get());
}

ScopedCERTCertificateList CreateCERTCertificateListFromFile(
    const base::FilePath& certs_dir,
    const std::string& cert_file,
    int format) {
  CertificateList certs =
      CreateCertificateListFromFile(certs_dir, cert_file, format);
  ScopedCERTCertificateList nss_certs;
  for (const auto& cert : certs) {
    ScopedCERTCertificate nss_cert =
        x509_util::CreateCERTCertificateFromX509Certificate(cert.get());
    if (!nss_cert)
      return {};
    nss_certs.push_back(std::move(nss_cert));
  }
  return nss_certs;
}

}  // namespace net
