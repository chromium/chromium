// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_platform_key_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "net/android/keystore.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/ssl_private_key_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "net/android/net_tests_jni/AndroidKeyStoreTestUtil_jni.h"

namespace net {

namespace {

typedef base::android::ScopedJavaLocalRef<jobject> ScopedJava;

bool ReadTestFile(const char* filename, std::string* pkcs8) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  base::FilePath file_path = certs_dir.AppendASCII(filename);
  return base::ReadFileToString(file_path, pkcs8);
}

// Retrieve a JNI local ref from encoded PKCS#8 data.
ScopedJava GetPKCS8PrivateKeyJava(android::PrivateKeyType key_type,
                                  const std::string& pkcs8_key) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jbyteArray> bytes =
      base::android::ToJavaByteArray(env, pkcs8_key);

  ScopedJava key(Java_AndroidKeyStoreTestUtil_createPrivateKeyFromPKCS8(
      env, key_type, bytes));

  return key;
}

struct TestKey {
  const char* name;
  const char* cert_file;
  const char* key_file;
  int type;
  android::PrivateKeyType android_key_type;
};

const TestKey kTestKeys[] = {
    {"RSA", "client_1.pem", "client_1.pk8", EVP_PKEY_RSA,
     android::PRIVATE_KEY_TYPE_RSA},
    {"ECDSA_P256", "client_4.pem", "client_4.pk8", EVP_PKEY_EC,
     android::PRIVATE_KEY_TYPE_ECDSA},
    {"ECDSA_P384", "client_5.pem", "client_5.pk8", EVP_PKEY_EC,
     android::PRIVATE_KEY_TYPE_ECDSA},
    {"ECDSA_P521", "client_6.pem", "client_6.pk8", EVP_PKEY_EC,
     android::PRIVATE_KEY_TYPE_ECDSA},
};

std::string TestKeyToString(const testing::TestParamInfo<TestKey>& params) {
  return params.param.name;
}

}  // namespace

class SSLPlatformKeyAndroidTest : public testing::TestWithParam<TestKey>,
                                  public WithTaskEnvironment {};

TEST_P(SSLPlatformKeyAndroidTest, Matches) {
  const TestKey& test_key = GetParam();

  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), test_key.cert_file);
  ASSERT_TRUE(cert);

  std::string key_bytes;
  ASSERT_TRUE(ReadTestFile(test_key.key_file, &key_bytes));
  ScopedJava java_key =
      GetPKCS8PrivateKeyJava(test_key.android_key_type, key_bytes);
  ASSERT_FALSE(java_key.is_null());

  scoped_refptr<SSLPrivateKey> key = WrapJavaPrivateKey(cert.get(), java_key);
  ASSERT_TRUE(key);

  EXPECT_EQ(SSLPrivateKey::DefaultAlgorithmPreferences(test_key.type,
                                                       true /* supports_pss */),
            key->GetAlgorithmPreferences());

  TestSSLPrivateKeyMatches(key.get(), key_bytes);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SSLPlatformKeyAndroidTest,
                         testing::ValuesIn(kTestKeys),
                         TestKeyToString);

TEST(SSLPlatformKeyAndroidSigAlgTest, SignatureAlgorithmsToJavaKeyTypes) {
  const struct {
    std::vector<uint16_t> algorithms;
    std::vector<std::string> expected_key_types;
  } kTests[] = {
      {{SSL_SIGN_RSA_PKCS1_SHA256, SSL_SIGN_RSA_PSS_RSAE_SHA384,
        SSL_SIGN_ECDSA_SECP256R1_SHA256, SSL_SIGN_RSA_PKCS1_SHA512,
        SSL_SIGN_ED25519},
       {"RSA", "EC"}},
      {{SSL_SIGN_RSA_PSS_RSAE_SHA256}, {"RSA"}},
      {{SSL_SIGN_RSA_PKCS1_SHA256}, {"RSA"}},
      {{SSL_SIGN_ECDSA_SECP256R1_SHA256}, {"EC"}},
      {{SSL_SIGN_ECDSA_SECP384R1_SHA384}, {"EC"}},
      // Android doesn't document a Java key type corresponding to Ed25519, so
      // for now we ignore it.
      {{SSL_SIGN_ED25519}, {}},
      // Unknown algorithm.
      {{0xffff}, {}},
      // Test the empty list.
      {{}, {}},
  };
  for (const auto& t : kTests) {
    EXPECT_EQ(SignatureAlgorithmsToJavaKeyTypes(t.algorithms),
              t.expected_key_types);
  }
}

}  // namespace net
