// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/openssl_private_key.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "crypto/evp.h"
#include "crypto/openssl_util.h"
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
  const char* key_file;
};

const TestKey kTestKeys[] = {
    {"RSA", "client_1.pk8"},           {"ECDSA_P256", "client_p256.pk8"},
    {"ECDSA_P384", "client_p384.pk8"}, {"ECDSA_P521", "client_p521.pk8"},
    {"ED25519", "client_ed25519.pk8"}, {"MLDSA44", "client_mldsa44.pk8"},
    {"MLDSA65", "client_mldsa65.pk8"}, {"MLDSA87", "client_mldsa87.pk8"},
};

std::string TestKeyToString(const testing::TestParamInfo<TestKey>& params) {
  return params.param.name;
}

}  // namespace

class OpenSSLPrivateKeyTest : public testing::TestWithParam<TestKey>,
                              public WithTaskEnvironment {};

TEST_P(OpenSSLPrivateKeyTest, KeyMatches) {
  const TestKey& test_key = GetParam();

  base::FilePath pkcs8_path =
      GetTestCertsDirectory().AppendASCII(test_key.key_file);
  std::optional<std::vector<uint8_t>> pkcs8 = base::ReadFileToBytes(pkcs8_path);
  ASSERT_TRUE(pkcs8);

  // Create an EVP_PKEY from the PKCS#8 buffer.
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  bssl::UniquePtr<EVP_PKEY> openssl_key =
      crypto::evp::PrivateKeyFromBytes(*pkcs8);
  ASSERT_TRUE(openssl_key);

  scoped_refptr<SSLPrivateKey> private_key =
      WrapOpenSSLPrivateKey(std::move(openssl_key));
  ASSERT_TRUE(private_key);
  net::TestSSLPrivateKeyMatches(private_key.get(), *pkcs8);
}

INSTANTIATE_TEST_SUITE_P(All,
                         OpenSSLPrivateKeyTest,
                         testing::ValuesIn(kTestKeys),
                         TestKeyToString);

TEST(OpenSSLPrivateKeyInvalidTest, UnsupportedKeyType) {
  base::FilePath pkcs8_path =
      GetTestCertsDirectory().AppendASCII("client_x25519.pk8");
  std::optional<std::vector<uint8_t>> pkcs8 = base::ReadFileToBytes(pkcs8_path);
  ASSERT_TRUE(pkcs8);

  // Create an EVP_PKEY from the PKCS#8 buffer.
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  bssl::UniquePtr<EVP_PKEY> openssl_key =
      crypto::evp::PrivateKeyFromBytes(*pkcs8);
  ASSERT_TRUE(openssl_key);

  scoped_refptr<SSLPrivateKey> private_key =
      WrapOpenSSLPrivateKey(std::move(openssl_key));
  EXPECT_FALSE(private_key);
}

}  // namespace net
