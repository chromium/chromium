// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_platform_key_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecItem.h>
#include <Security/SecKey.h>

#include <string>
#include <string_view>

#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/checked_math.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "crypto/features.h"
#include "crypto/scoped_fake_apple_keychain_v2.h"
#include "crypto/signature_verifier.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/ssl_private_key_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/rsa.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

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

base::apple::ScopedCFTypeRef<SecKeyRef> SecKeyFromPKCS8(
    std::string_view pkcs8) {
  CBS cbs;
  CBS_init(&cbs, reinterpret_cast<const uint8_t*>(pkcs8.data()), pkcs8.size());
  bssl::UniquePtr<EVP_PKEY> openssl_key(EVP_parse_private_key(&cbs));
  if (!openssl_key || CBS_len(&cbs) != 0)
    return base::apple::ScopedCFTypeRef<SecKeyRef>();

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

}  // namespace

class SSLPlatformKeyMacTest : public testing::TestWithParam<TestKey> {};

TEST_P(SSLPlatformKeyMacTest, KeyMatches) {
  base::test::TaskEnvironment task_environment;

  const TestKey& test_key = GetParam();

  // Load test data.
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), test_key.cert_file);
  ASSERT_TRUE(cert);

  std::string pkcs8;
  base::FilePath pkcs8_path =
      GetTestCertsDirectory().AppendASCII(test_key.key_file);
  ASSERT_TRUE(base::ReadFileToString(pkcs8_path, &pkcs8));
  base::apple::ScopedCFTypeRef<SecKeyRef> sec_key = SecKeyFromPKCS8(pkcs8);
  ASSERT_TRUE(sec_key);

  // Make an `SSLPrivateKey` backed by `sec_key`.
  scoped_refptr<SSLPrivateKey> key =
      CreateSSLPrivateKeyForSecKey(cert.get(), sec_key.get());
  ASSERT_TRUE(key);

  // Mac keys from the default provider are expected to support all algorithms.
  EXPECT_EQ(SSLPrivateKey::DefaultAlgorithmPreferences(test_key.type, true),
            key->GetAlgorithmPreferences());

  TestSSLPrivateKeyMatches(key.get(), pkcs8);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SSLPlatformKeyMacTest,
                         testing::ValuesIn(kTestKeys),
                         TestKeyToString);

namespace {

constexpr char kTestKeychainAccessGroup[] = "test-keychain-access-group";
constexpr crypto::SignatureVerifier::SignatureAlgorithm kAcceptableAlgos[] = {
    crypto::SignatureVerifier::ECDSA_SHA256};

const crypto::UnexportableKeyProvider::Config config = {
    .keychain_access_group = kTestKeychainAccessGroup,
};

}  // namespace

// Tests that a SSLPrivateKey can be created from a
// crypto::UnexportableSigningKey.
TEST(UnexportableSSLPlatformKeyMacTest, Convert) {
  crypto::ScopedFakeAppleKeychainV2 scoped_fake_apple_keychain_{
      kTestKeychainAccessGroup};
  base::test::ScopedFeatureList scoped_feature_list_{
      crypto::kEnableMacUnexportableKeys};

  // Create a crypto::UnexportableSigningKey and verify preconditions.
  std::unique_ptr<crypto::UnexportableKeyProvider> provider =
      crypto::GetUnexportableKeyProvider(config);
  ASSERT_TRUE(provider);
  std::unique_ptr<crypto::UnexportableSigningKey> unexportable_key =
      provider->GenerateSigningKeySlowly(kAcceptableAlgos);
  ASSERT_TRUE(unexportable_key);
  SecKeyRef key_ref = unexportable_key->GetSecKeyRef();
  EXPECT_TRUE(key_ref);

  auto ssl_private_key = WrapUnexportableKey(*unexportable_key);
  EXPECT_TRUE(ssl_private_key);
}

}  // namespace net
