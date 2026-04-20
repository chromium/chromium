// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/crypto_private_key.h"

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "crypto/keypair.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/ssl_private_key_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

struct TestKey {
  const char* name;
  const char* cert_file;
  const char* key_file;
};

const TestKey kTestKeys[] = {
    {"RSA", "client_1.pem", "client_1.pk8"},
    {"ECDSA_P256", "client_4.pem", "client_4.pk8"},
    {"ECDSA_P384", "client_5.pem", "client_5.pk8"},
    {"ECDSA_P521", "client_6.pem", "client_6.pk8"},
    {"ED25519", "client_8.pem", "client_8.pk8"},
};

std::string TestKeyToString(const testing::TestParamInfo<TestKey>& params) {
  return params.param.name;
}

}  // namespace

class CryptoPrivateKeyTest : public testing::TestWithParam<TestKey>,
                             public WithTaskEnvironment {};

TEST_P(CryptoPrivateKeyTest, KeyMatches) {
  const TestKey& test_key = GetParam();

  base::FilePath pkcs8_path =
      GetTestCertsDirectory().AppendASCII(test_key.key_file);
  std::optional<std::vector<uint8_t>> pkcs8 = base::ReadFileToBytes(pkcs8_path);
  ASSERT_TRUE(pkcs8);

  std::optional<crypto::keypair::PrivateKey> key =
      crypto::keypair::PrivateKey::FromPrivateKeyInfo(*pkcs8);
  ASSERT_TRUE(key);

  scoped_refptr<SSLPrivateKey> private_key =
      WrapCryptoPrivateKey(std::move(*key));
  ASSERT_TRUE(private_key);
  net::TestSSLPrivateKeyMatches(private_key.get(), *pkcs8);
}

INSTANTIATE_TEST_SUITE_P(All,
                         CryptoPrivateKeyTest,
                         testing::ValuesIn(kTestKeys),
                         TestKeyToString);

}  // namespace net
