// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_platform_key_android.h"

#include <string>

#include "base/android/android_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "net/android/keystore.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_platform_key_util.h"
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

using base::android::ScopedJavaLocalRef;

bool ReadTestFile(const char* filename, std::string* pkcs8) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  base::FilePath file_path = certs_dir.AppendASCII(filename);
  return base::ReadFileToString(file_path, pkcs8);
}

// Retrieve a JNI local ref from encoded PKCS#8 data.
ScopedJavaLocalRef<jobject> GetPKCS8PrivateKeyJava(
    const char* algorithm,
    const std::string& pkcs8_key) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jbyteArray> bytes =
      base::android::ToJavaByteArray(env, pkcs8_key);
  ScopedJavaLocalRef<jstring> algorithm_java =
      base::android::ConvertUTF8ToJavaString(env, algorithm);

  ScopedJavaLocalRef<jobject> key(
      android::Java_AndroidKeyStoreTestUtil_createPrivateKeyFromPKCS8(
          env, algorithm_java, bytes));

  return key;
}

struct TestKey {
  const char* name;
  const char* cert_file;
  const char* key_file;
  int type;
  const char* android_key_type;
};

const TestKey kTestKeys[] = {
    {"RSA", "client_1.pem", "client_1.pk8", EVP_PKEY_RSA, "RSA"},
    {"ECDSA_P256", "client_p256.pem", "client_p256.pk8", EVP_PKEY_EC, "EC"},
    {"ECDSA_P384", "client_p384.pem", "client_p384.pk8", EVP_PKEY_EC, "EC"},
    {"ECDSA_P521", "client_p521.pem", "client_p521.pk8", EVP_PKEY_EC, "EC"},
    {"MLDSA44", "client_mldsa44.pem", "client_mldsa44.pk8", EVP_PKEY_ML_DSA_44,
     "ML-DSA-44"},
    {"MLDSA65", "client_mldsa65.pem", "client_mldsa65.pk8", EVP_PKEY_ML_DSA_65,
     "ML-DSA-65"},
    {"MLDSA87", "client_mldsa87.pem", "client_mldsa87.pk8", EVP_PKEY_ML_DSA_87,
     "ML-DSA-87"},
};

std::string TestKeyToString(const testing::TestParamInfo<TestKey>& params) {
  return params.param.name;
}

}  // namespace

class SSLPlatformKeyAndroidTest : public testing::TestWithParam<TestKey>,
                                  public WithTaskEnvironment {};

TEST_P(SSLPlatformKeyAndroidTest, Matches) {
  const TestKey& test_key = GetParam();
  if (base::android::android_info::sdk_int_full() <
          base::android::android_info::SDK_VERSION_FULL_BAKLAVA_1 &&
      (test_key.type == EVP_PKEY_ML_DSA_65 ||
       test_key.type == EVP_PKEY_ML_DSA_87)) {
    GTEST_SKIP() << "Android added ML-DSA-65 in API level 36.1";
  }
  // TODO(crbug.com/536164653): When ML-DSA-44 is shipped, add the appropriate
  // SDK check here. For now, we just skip the test.
  if (test_key.type == EVP_PKEY_ML_DSA_44) {
    GTEST_SKIP() << "Android has not yet added ML-DSA-44 in a release";
  }

  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), test_key.cert_file);
  ASSERT_TRUE(cert);

  std::string key_bytes;
  ASSERT_TRUE(ReadTestFile(test_key.key_file, &key_bytes));
  ScopedJavaLocalRef<jobject> java_key =
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

TEST_P(SSLPlatformKeyAndroidTest, MatchesPublicKey) {
  const TestKey& test_key = GetParam();
  if (base::android::android_info::sdk_int_full() <
          base::android::android_info::SDK_VERSION_FULL_BAKLAVA_1 &&
      (test_key.type == EVP_PKEY_ML_DSA_65 ||
       test_key.type == EVP_PKEY_ML_DSA_87)) {
    GTEST_SKIP() << "Android added ML-DSA-65 in API level 36.1";
  }
  // TODO(crbug.com/536164653): When ML-DSA-44 is shipped, add the appropriate
  // SDK check here. For now, we just skip the test.
  if (test_key.type == EVP_PKEY_ML_DSA_44) {
    GTEST_SKIP() << "Android has not yet added ML-DSA-44 in a release";
  }

  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), test_key.cert_file);
  ASSERT_TRUE(cert);

  std::string key_bytes;
  ASSERT_TRUE(ReadTestFile(test_key.key_file, &key_bytes));
  ScopedJavaLocalRef<jobject> java_key =
      GetPKCS8PrivateKeyJava(test_key.android_key_type, key_bytes);
  ASSERT_FALSE(java_key.is_null());

  bssl::UniquePtr<EVP_PKEY> pubkey = net::GetClientCertPublicKey(cert.get());
  ASSERT_TRUE(pubkey);

  scoped_refptr<SSLPrivateKey> key =
      WrapJavaPrivateKey(std::move(pubkey), java_key);
  ASSERT_TRUE(key);

  EXPECT_EQ(SSLPrivateKey::DefaultAlgorithmPreferences(test_key.type,
                                                       true /* supports_pss */),
            key->GetAlgorithmPreferences());

  TestSSLPrivateKeyMatches(key.get(), key_bytes);
}

TEST(SSLPlatformKeyAndroidInvalidTest, UnsupportedKeyType) {
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), "client_x25519.pem");
  ASSERT_TRUE(cert);

  ScopedJavaLocalRef<jobject> java_key;
  std::string key_bytes;
  if (base::android::android_info::sdk_int() >=
      base::android::android_info::SDK_VERSION_S) {
    ASSERT_TRUE(ReadTestFile("client_x25519.pk8", &key_bytes));
    java_key = GetPKCS8PrivateKeyJava("XDH", key_bytes);
  } else {
    // Prior to Android S, Android did not support importing X25519 keys.
    // However, `WrapJavaPrivateKey` only inspects the certificate's public key
    // when checking the key type, so we can still test rejection by pairing the
    // certificate with an arbitrary RSA key.
    ASSERT_TRUE(ReadTestFile("client_1.pk8", &key_bytes));
    java_key = GetPKCS8PrivateKeyJava("RSA", key_bytes);
  }
  ASSERT_FALSE(java_key.is_null());

  scoped_refptr<SSLPrivateKey> key = WrapJavaPrivateKey(cert.get(), java_key);
  EXPECT_FALSE(key);
}

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

DEFINE_JNI(AndroidKeyStoreTestUtil)
