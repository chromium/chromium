// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_platform_key_util.h"

#include <stddef.h>

#include "base/memory/ref_counted.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/ssl_private_key.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace net {

namespace {

bool GetClientCertInfoFromFile(const char* filename,
                               int* out_type,
                               size_t* out_max_length) {
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), filename);
  if (!cert) {
    ADD_FAILURE() << "Could not read " << filename;
    return false;
  }

  return GetClientCertInfo(cert.get(), out_type, out_max_length);
}

bool GetPublicKeyInfoFromCertificateFile(const char* filename,
                                         int* out_type,
                                         size_t* out_max_length) {
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), filename);
  if (!cert) {
    ADD_FAILURE() << "Could not read " << filename;
    return false;
  }

  std::string_view spki;
  if (!asn1::ExtractSPKIFromDERCert(
          x509_util::CryptoBufferAsStringPiece(cert->cert_buffer()), &spki)) {
    LOG(ERROR) << "Could not extract SPKI from certificate.";
    return false;
  }

  return GetPublicKeyInfo(base::as_byte_span(spki), out_type, out_max_length);
}

size_t BitsToBytes(size_t bits) {
  return (bits + 7) / 8;
}

}  // namespace

TEST(SSLPlatformKeyUtil, GetClientCertInfo) {
  int type;
  size_t max_length;

  ASSERT_TRUE(GetClientCertInfoFromFile("client_1.pem", &type, &max_length));
  EXPECT_EQ(EVP_PKEY_RSA, type);
  EXPECT_EQ(2048u / 8u, max_length);

  ASSERT_TRUE(GetClientCertInfoFromFile("client_4.pem", &type, &max_length));
  EXPECT_EQ(EVP_PKEY_EC, type);
  EXPECT_EQ(ECDSA_SIG_max_len(BitsToBytes(256)), max_length);

  ASSERT_TRUE(GetClientCertInfoFromFile("client_5.pem", &type, &max_length));
  EXPECT_EQ(EVP_PKEY_EC, type);
  EXPECT_EQ(ECDSA_SIG_max_len(BitsToBytes(384)), max_length);

  ASSERT_TRUE(GetClientCertInfoFromFile("client_6.pem", &type, &max_length));
  EXPECT_EQ(EVP_PKEY_EC, type);
  EXPECT_EQ(ECDSA_SIG_max_len(BitsToBytes(521)), max_length);
}

TEST(SSLPlatformKeyUtil, GetPublicKeyInfo) {
  int type;
  size_t max_length;

  ASSERT_TRUE(
      GetPublicKeyInfoFromCertificateFile("client_1.pem", &type, &max_length));
  EXPECT_EQ(EVP_PKEY_RSA, type);
  EXPECT_EQ(2048u / 8u, max_length);

  ASSERT_TRUE(
      GetPublicKeyInfoFromCertificateFile("client_4.pem", &type, &max_length));
  EXPECT_EQ(EVP_PKEY_EC, type);
  EXPECT_EQ(ECDSA_SIG_max_len(BitsToBytes(256)), max_length);

  ASSERT_TRUE(
      GetPublicKeyInfoFromCertificateFile("client_5.pem", &type, &max_length));
  EXPECT_EQ(EVP_PKEY_EC, type);
  EXPECT_EQ(ECDSA_SIG_max_len(BitsToBytes(384)), max_length);

  ASSERT_TRUE(
      GetPublicKeyInfoFromCertificateFile("client_6.pem", &type, &max_length));
  EXPECT_EQ(EVP_PKEY_EC, type);
  EXPECT_EQ(ECDSA_SIG_max_len(BitsToBytes(521)), max_length);
}

}  // namespace net
