// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_platform_key_nss.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "crypto/nss_crypto_module_delegate.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/x509_util_nss.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/ssl_private_key_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace net {

namespace {

struct TestKey {
  const char* name;
  const char* cert_file;
  const char* key_file;
  int type;
};

const TestKey kTestKeys[] = {
    {"RSA", "client_1.pem", "client_1.pk8", EVP_PKEY_RSA},
    {"ECDSA_P256", "client_4.pem", "client_4.pk8", EVP_PKEY_EC},
    {"ECDSA_P384", "client_5.pem", "client_5.pk8", EVP_PKEY_EC},
    {"ECDSA_P521", "client_6.pem", "client_6.pk8", EVP_PKEY_EC},
};

std::string TestKeyToString(const testing::TestParamInfo<TestKey>& params) {
  return params.param.name;
}

}  // namespace

class SSLPlatformKeyNSSTest : public testing::TestWithParam<TestKey>,
                              public WithTaskEnvironment {};

TEST_P(SSLPlatformKeyNSSTest, KeyMatches) {
  const TestKey& test_key = GetParam();

  std::string pkcs8;
  base::FilePath pkcs8_path =
      GetTestCertsDirectory().AppendASCII(test_key.key_file);
  ASSERT_TRUE(base::ReadFileToString(pkcs8_path, &pkcs8));

  // Import the key into a test NSS database.
  crypto::ScopedTestNSSDB test_db;
  ScopedCERTCertificate nss_cert;
  scoped_refptr<X509Certificate> cert = ImportClientCertAndKeyFromFile(
      GetTestCertsDirectory(), test_key.cert_file, test_key.key_file,
      test_db.slot(), &nss_cert);
  ASSERT_TRUE(cert);
  ASSERT_TRUE(nss_cert);

  // Look up the key.
  scoped_refptr<SSLPrivateKey> key =
      FetchClientCertPrivateKey(cert.get(), nss_cert.get(), nullptr);
  ASSERT_TRUE(key);

  // All NSS keys are expected to have the default preferences.
  EXPECT_EQ(SSLPrivateKey::DefaultAlgorithmPreferences(test_key.type,
                                                       true /* supports PSS */),
            key->GetAlgorithmPreferences());

  TestSSLPrivateKeyMatches(key.get(), pkcs8);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SSLPlatformKeyNSSTest,
                         testing::ValuesIn(kTestKeys),
                         TestKeyToString);

}  // namespace net
