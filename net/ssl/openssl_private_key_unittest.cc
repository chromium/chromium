// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/openssl_private_key.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "crypto/openssl_util.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/ssl_private_key_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
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

class OpenSSLPrivateKeyTest : public testing::TestWithParam<TestKey>,
                              public WithTaskEnvironment {};

TEST_P(OpenSSLPrivateKeyTest, KeyMatches) {
  const TestKey& test_key = GetParam();

  std::string pkcs8;
  base::FilePath pkcs8_path =
      GetTestCertsDirectory().AppendASCII(test_key.key_file);
  ASSERT_TRUE(base::ReadFileToString(pkcs8_path, &pkcs8));

  // Create an EVP_PKEY from the PKCS#8 buffer.
  crypto::OpenSSLErrStackTracer err_tracer(FROM_HERE);
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(pkcs8.data()), pkcs8.size());
  bssl::UniquePtr<EVP_PKEY> openssl_key(EVP_parse_private_key(&cbs));
  ASSERT_TRUE(openssl_key);
  EXPECT_EQ(0u, CBS_len(&cbs));

  scoped_refptr<SSLPrivateKey> private_key =
      WrapOpenSSLPrivateKey(std::move(openssl_key));
  ASSERT_TRUE(private_key);
  net::TestSSLPrivateKeyMatches(private_key.get(), pkcs8);
}

INSTANTIATE_TEST_SUITE_P(All,
                         OpenSSLPrivateKeyTest,
                         testing::ValuesIn(kTestKeys),
                         TestKeyToString);

}  // namespace net
