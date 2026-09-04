// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/ssl_platform_key_win.h"

#include <array>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/notreached.h"
#include "base/strings/cstring_view.h"
#include "base/test/task_environment.h"
#include "crypto/evp.h"
#include "crypto/openssl_util.h"
#include "crypto/scoped_capi_types.h"
#include "crypto/scoped_cng_types.h"
#include "crypto/sign.h"
#include "crypto/unexportable_key.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/ssl_private_key_test_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"
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
    {.name = "RSA",
     .cert_file = "client_1.pem",
     .key_file = "client_1.pk8",
     .type = EVP_PKEY_RSA},
    {.name = "P256",
     .cert_file = "client_p256.pem",
     .key_file = "client_p256.pk8",
     .type = EVP_PKEY_EC},
    {.name = "P384",
     .cert_file = "client_p384.pem",
     .key_file = "client_p384.pk8",
     .type = EVP_PKEY_EC},
    {.name = "P521",
     .cert_file = "client_p521.pem",
     .key_file = "client_p521.pk8",
     .type = EVP_PKEY_EC},
    {.name = "RSA1024",
     .cert_file = "client_rsa1024.pem",
     .key_file = "client_rsa1024.pk8",
     .type = EVP_PKEY_RSA},
    {.name = "MLDSA44",
     .cert_file = "client_mldsa44.pem",
     .key_file = "client_mldsa44.pk8",
     .type = EVP_PKEY_ML_DSA_44},
    {.name = "MLDSA65",
     .cert_file = "client_mldsa65.pem",
     .key_file = "client_mldsa65.pk8",
     .type = EVP_PKEY_ML_DSA_65},
    {.name = "MLDSA87",
     .cert_file = "client_mldsa87.pem",
     .key_file = "client_mldsa87.pk8",
     .type = EVP_PKEY_ML_DSA_87},
};

std::string TestParamsToString(const testing::TestParamInfo<TestKey>& params) {
  return params.param.name;
}

// Appends |bn| to |cbb|, represented as |len| bytes in little-endian order,
// zero-padded as needed. Returns true on success and false if |len| is too
// small.
bool AddBIGNUMLittleEndian(CBB* cbb, const BIGNUM* bn, size_t len) {
  uint8_t* ptr;
  return CBB_add_space(cbb, &ptr, len) && BN_bn2le_padded(ptr, len, bn);
}

// Converts the PKCS#8 PrivateKeyInfo structure serialized in |pkcs8| to a
// private key BLOB, suitable for import with CAPI using Microsoft Base
// Cryptographic Provider.
bool PKCS8ToBLOBForCAPI(base::span<const uint8_t> pkcs8,
                        std::vector<uint8_t>* blob) {
  bssl::UniquePtr<EVP_PKEY> key = crypto::evp::PrivateKeyFromBytes(pkcs8);
  if (!key || EVP_PKEY_id(key.get()) != EVP_PKEY_RSA) {
    return false;
  }
  const RSA* rsa = EVP_PKEY_get0_RSA(key.get());

  // See
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa375601(v=vs.85).aspx
  PUBLICKEYSTRUC header = {0};
  header.bType = PRIVATEKEYBLOB;
  header.bVersion = 2;
  header.aiKeyAlg = CALG_RSA_SIGN;

  RSAPUBKEY rsapubkey = {0};
  rsapubkey.magic = 0x32415352;
  rsapubkey.bitlen = RSA_bits(rsa);
  rsapubkey.pubexp = BN_get_word(RSA_get0_e(rsa));

  bssl::ScopedCBB cbb;
  if (!CBB_init(cbb.get(), sizeof(header) + sizeof(rsapubkey) + pkcs8.size()) ||
      !CBB_add_bytes(cbb.get(), reinterpret_cast<const uint8_t*>(&header),
                     sizeof(header)) ||
      !CBB_add_bytes(cbb.get(), reinterpret_cast<const uint8_t*>(&rsapubkey),
                     sizeof(rsapubkey)) ||
      !AddBIGNUMLittleEndian(cbb.get(), RSA_get0_n(rsa),
                             rsapubkey.bitlen / 8) ||
      !AddBIGNUMLittleEndian(cbb.get(), RSA_get0_p(rsa),
                             rsapubkey.bitlen / 16) ||
      !AddBIGNUMLittleEndian(cbb.get(), RSA_get0_q(rsa),
                             rsapubkey.bitlen / 16) ||
      !AddBIGNUMLittleEndian(cbb.get(), RSA_get0_dmp1(rsa),
                             rsapubkey.bitlen / 16) ||
      !AddBIGNUMLittleEndian(cbb.get(), RSA_get0_dmq1(rsa),
                             rsapubkey.bitlen / 16) ||
      !AddBIGNUMLittleEndian(cbb.get(), RSA_get0_iqmp(rsa),
                             rsapubkey.bitlen / 16) ||
      !AddBIGNUMLittleEndian(cbb.get(), RSA_get0_d(rsa),
                             rsapubkey.bitlen / 8)) {
    return false;
  }

  *blob = base::ToVector(crypto::CbbAsSpan(cbb.get()));
  return true;
}

// Appends |bn| to |cbb|, represented as |len| bytes in big-endian order,
// zero-padded as needed. Returns true on success and false if |len| is too
// small.
bool AddBIGNUMBigEndian(CBB* cbb, const BIGNUM* bn, size_t len) {
  uint8_t* ptr;
  return CBB_add_space(cbb, &ptr, len) && BN_bn2bin_padded(ptr, len, bn);
}

// Converts the PKCS#8 PrivateKeyInfo structure serialized in |pkcs8| to a
// private key BLOB, suitable for import with CNG using the Microsoft Software
// KSP, and sets |*blob_type| to the type of the BLOB.
bool PKCS8ToBLOBForCNG(base::span<const uint8_t> pkcs8,
                       LPCWSTR* blob_type,
                       std::vector<uint8_t>* blob) {
  bssl::UniquePtr<EVP_PKEY> key = crypto::evp::PrivateKeyFromBytes(pkcs8);
  if (!key) {
    return false;
  }

  const int key_type = EVP_PKEY_id(key.get());
  if (key_type == EVP_PKEY_RSA) {
    // See
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa375531(v=vs.85).aspx.
    const RSA* rsa = EVP_PKEY_get0_RSA(key.get());
    BCRYPT_RSAKEY_BLOB header = {0};
    header.Magic = BCRYPT_RSAFULLPRIVATE_MAGIC;
    header.BitLength = RSA_bits(rsa);
    header.cbPublicExp = BN_num_bytes(RSA_get0_e(rsa));
    header.cbModulus = BN_num_bytes(RSA_get0_n(rsa));
    header.cbPrime1 = BN_num_bytes(RSA_get0_p(rsa));
    header.cbPrime2 = BN_num_bytes(RSA_get0_q(rsa));

    base::span<const uint8_t> header_bytes = base::byte_span_from_ref(header);
    bssl::ScopedCBB cbb;
    if (!CBB_init(cbb.get(), header_bytes.size() + pkcs8.size()) ||
        !CBB_add_bytes(cbb.get(), header_bytes.data(), header_bytes.size()) ||
        !AddBIGNUMBigEndian(cbb.get(), RSA_get0_e(rsa), header.cbPublicExp) ||
        !AddBIGNUMBigEndian(cbb.get(), RSA_get0_n(rsa), header.cbModulus) ||
        !AddBIGNUMBigEndian(cbb.get(), RSA_get0_p(rsa), header.cbPrime1) ||
        !AddBIGNUMBigEndian(cbb.get(), RSA_get0_q(rsa), header.cbPrime2) ||
        !AddBIGNUMBigEndian(cbb.get(), RSA_get0_dmp1(rsa), header.cbPrime1) ||
        !AddBIGNUMBigEndian(cbb.get(), RSA_get0_dmq1(rsa), header.cbPrime2) ||
        !AddBIGNUMBigEndian(cbb.get(), RSA_get0_iqmp(rsa), header.cbPrime1) ||
        !AddBIGNUMBigEndian(cbb.get(), RSA_get0_d(rsa), header.cbModulus)) {
      return false;
    }

    *blob_type = BCRYPT_RSAFULLPRIVATE_BLOB;
    *blob = base::ToVector(crypto::CbbAsSpan(cbb.get()));
    return true;
  }

  if (key_type == EVP_PKEY_EC) {
    // See
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa375520(v=vs.85).aspx.
    const EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(key.get());
    const EC_GROUP* group = EC_KEY_get0_group(ec_key);
    bssl::UniquePtr<BIGNUM> x(BN_new());
    bssl::UniquePtr<BIGNUM> y(BN_new());
    if (!EC_POINT_get_affine_coordinates_GFp(
            group, EC_KEY_get0_public_key(ec_key), x.get(), y.get(), nullptr)) {
      return false;
    }

    BCRYPT_ECCKEY_BLOB header = {0};
    switch (EC_GROUP_get_curve_name(EC_KEY_get0_group(ec_key))) {
      case NID_X9_62_prime256v1:
        header.dwMagic = BCRYPT_ECDSA_PRIVATE_P256_MAGIC;
        break;
      case NID_secp384r1:
        header.dwMagic = BCRYPT_ECDSA_PRIVATE_P384_MAGIC;
        break;
      case NID_secp521r1:
        header.dwMagic = BCRYPT_ECDSA_PRIVATE_P521_MAGIC;
        break;
      default:
        return false;
    }
    header.cbKey = (EC_GROUP_get_degree(group) + 7) / 8;

    base::span<const uint8_t> header_bytes = base::byte_span_from_ref(header);
    bssl::ScopedCBB cbb;
    if (!CBB_init(cbb.get(), header_bytes.size() + header.cbKey * 3) ||
        !CBB_add_bytes(cbb.get(), header_bytes.data(), header_bytes.size()) ||
        !AddBIGNUMBigEndian(cbb.get(), x.get(), header.cbKey) ||
        !AddBIGNUMBigEndian(cbb.get(), y.get(), header.cbKey) ||
        !AddBIGNUMBigEndian(cbb.get(), EC_KEY_get0_private_key(ec_key),
                            header.cbKey)) {
      return false;
    }

    *blob_type = BCRYPT_ECCPRIVATE_BLOB;
    *blob = base::ToVector(crypto::CbbAsSpan(cbb.get()));
    return true;
  }

  if (key_type == EVP_PKEY_ML_DSA_44 || key_type == EVP_PKEY_ML_DSA_65 ||
      key_type == EVP_PKEY_ML_DSA_87) {
    // See
    // https://learn.microsoft.com/en-us/windows/win32/seccng/bcrypt/ns-bcrypt-bcrypt_pqdsa_key_blob
    base::wcstring_view parameter_set;
    switch (key_type) {
      case EVP_PKEY_ML_DSA_44:
        parameter_set = BCRYPT_MLDSA_PARAMETER_SET_44;
        break;
      case EVP_PKEY_ML_DSA_65:
        parameter_set = BCRYPT_MLDSA_PARAMETER_SET_65;
        break;
      case EVP_PKEY_ML_DSA_87:
        parameter_set = BCRYPT_MLDSA_PARAMETER_SET_87;
        break;
      default:
        NOTREACHED();
    }

    std::array<uint8_t, 32> seed;  // ML-DSA seeds are always 32 bytes.
    size_t seed_len = seed.size();
    CHECK(EVP_PKEY_get_private_seed(key.get(), seed.data(), &seed_len));
    CHECK(seed_len == seed.size());

    base::span<const uint8_t> parameter_set_bytes =
        base::byte_span_with_nul_from_cstring_view(parameter_set);
    BCRYPT_PQDSA_KEY_BLOB header = {};
    header.dwMagic = BCRYPT_MLDSA_PRIVATE_SEED_MAGIC;
    header.cbParameterSet = parameter_set_bytes.size();
    header.cbKey = seed.size();

    *blob_type = BCRYPT_PQDSA_PRIVATE_SEED_BLOB;
    blob->clear();
    base::span<const uint8_t> header_bytes = base::byte_span_from_ref(header);
    blob->reserve(header_bytes.size() + parameter_set_bytes.size() +
                  seed.size());
    blob->append_range(header_bytes);
    blob->append_range(parameter_set_bytes);
    blob->append_range(seed);
    return true;
  }

  return false;
}

}  // namespace

class SSLPlatformKeyWinTest
    : public testing::TestWithParam<TestKey>,
      public WithTaskEnvironment {
 public:
  const TestKey& GetTestKey() const { return GetParam(); }
};

TEST_P(SSLPlatformKeyWinTest, KeyMatchesCNG) {
  const TestKey& test_key = GetTestKey();

  // Load test data.
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), test_key.cert_file);
  ASSERT_TRUE(cert);

  base::FilePath pkcs8_path =
      GetTestCertsDirectory().AppendASCII(test_key.key_file);
  std::optional<std::vector<uint8_t>> pkcs8 = base::ReadFileToBytes(pkcs8_path);
  ASSERT_TRUE(pkcs8);

  // Import the key into CNG. Per MSDN's documentation on NCryptImportKey, if a
  // key name is not supplied (via the pParameterList parameter for the BLOB
  // types we use), the Microsoft Software KSP will treat the key as ephemeral.
  //
  // https://msdn.microsoft.com/en-us/library/windows/desktop/aa376276(v=vs.85).aspx
  crypto::ScopedNCryptProvider prov;
  SECURITY_STATUS status = NCryptOpenStorageProvider(
      crypto::ScopedNCryptProvider::Receiver(prov).get(),
      MS_KEY_STORAGE_PROVIDER, 0);
  ASSERT_FALSE(FAILED(status)) << status;

  // ML-DSA support was only added to Windows 11 in the October 2025 update.
  // Check for ML-DSA support and skip the test if we cannot run it.
  if (test_key.type == EVP_PKEY_ML_DSA_44 ||
      test_key.type == EVP_PKEY_ML_DSA_65 ||
      test_key.type == EVP_PKEY_ML_DSA_87) {
    status = NCryptIsAlgSupported(prov.get(), BCRYPT_MLDSA_ALGORITHM,
                                  NCRYPT_SILENT_FLAG);
    if (status == NTE_NOT_SUPPORTED) {
      GTEST_SKIP() << "Software storage provider does not support ML-DSA";
    }
    ASSERT_FALSE(FAILED(status)) << status;
  }

  LPCWSTR blob_type;
  std::vector<uint8_t> blob;
  ASSERT_TRUE(PKCS8ToBLOBForCNG(*pkcs8, &blob_type, &blob));
  crypto::ScopedNCryptKey ncrypt_key;
  status = NCryptImportKey(prov.get(), /*hImportKey=*/0, blob_type,
                           /*pParameterList=*/nullptr,
                           crypto::ScopedNCryptKey::Receiver(ncrypt_key).get(),
                           blob.data(), blob.size(), NCRYPT_SILENT_FLAG);
  ASSERT_FALSE(FAILED(status)) << status;

  scoped_refptr<SSLPrivateKey> key =
      WrapCNGPrivateKey(cert.get(), std::move(ncrypt_key));
  ASSERT_TRUE(key);

  EXPECT_EQ(SSLPrivateKey::DefaultAlgorithmPreferences(test_key.type,
                                                       /*supports_pss=*/true),
            key->GetAlgorithmPreferences());
  EXPECT_EQ("CNG: Microsoft Software Key Storage Provider",
            key->GetProviderName());
  TestSSLPrivateKeyMatches(key.get(), *pkcs8);
}

TEST_P(SSLPlatformKeyWinTest, KeyMatchesCAPI) {
  const TestKey& test_key = GetTestKey();
  if (test_key.type != EVP_PKEY_RSA) {
    GTEST_SKIP() << "CAPI only supports RSA keys";
  }

  // Load test data.
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), test_key.cert_file);
  ASSERT_TRUE(cert);

  base::FilePath pkcs8_path =
      GetTestCertsDirectory().AppendASCII(test_key.key_file);
  std::optional<std::vector<uint8_t>> pkcs8 = base::ReadFileToBytes(pkcs8_path);
  ASSERT_TRUE(pkcs8);

  // Import the key into CAPI. Use CRYPT_VERIFYCONTEXT for an ephemeral key.
  crypto::ScopedHCRYPTPROV prov;
  ASSERT_NE(FALSE,
            CryptAcquireContext(crypto::ScopedHCRYPTPROV::Receiver(prov).get(),
                                nullptr, nullptr, PROV_RSA_AES,
                                CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
      << GetLastError();

  std::vector<uint8_t> blob;
  ASSERT_TRUE(PKCS8ToBLOBForCAPI(*pkcs8, &blob));

  crypto::ScopedHCRYPTKEY hcryptkey;
  ASSERT_NE(FALSE,
            CryptImportKey(prov.get(), blob.data(), blob.size(),
                           /*hPubKey=*/0, /*dwFlags=*/0,
                           crypto::ScopedHCRYPTKEY::Receiver(hcryptkey).get()))
      << GetLastError();
  // Release |hcryptkey| so it does not outlive |prov|.
  hcryptkey.reset();

  scoped_refptr<SSLPrivateKey> key =
      WrapCAPIPrivateKey(cert.get(), std::move(prov), AT_SIGNATURE);
  ASSERT_TRUE(key);

  std::vector<uint16_t> expected = {
      SSL_SIGN_RSA_PKCS1_SHA256,
      SSL_SIGN_RSA_PKCS1_SHA384,
      SSL_SIGN_RSA_PKCS1_SHA512,
      SSL_SIGN_RSA_PKCS1_SHA1,
  };
  EXPECT_EQ(expected, key->GetAlgorithmPreferences());
  EXPECT_EQ("CAPI: Microsoft Enhanced RSA and AES Cryptographic Provider",
            key->GetProviderName());
  TestSSLPrivateKeyMatches(key.get(), *pkcs8);
}

INSTANTIATE_TEST_SUITE_P(All,
                         SSLPlatformKeyWinTest,
                         testing::ValuesIn(kTestKeys),
                         TestParamsToString);

TEST(SSLPlatformKeyWinInvalidTest, UnsupportedKeyTypeCNG) {
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), "client_x25519.pem");
  ASSERT_TRUE(cert);

  // CNG does not support importing X25519 keys via Microsoft Software KSP.
  // However, `WrapCNGPrivateKey` only inspects the certificate's public key
  // when checking the key type, so test rejection by pairing the certificate
  // with an arbitrary other key.
  base::FilePath pkcs8_path =
      GetTestCertsDirectory().AppendASCII("client_1.pk8");
  std::optional<std::vector<uint8_t>> pkcs8 = base::ReadFileToBytes(pkcs8_path);
  ASSERT_TRUE(pkcs8);

  crypto::ScopedNCryptProvider prov;
  SECURITY_STATUS status = NCryptOpenStorageProvider(
      crypto::ScopedNCryptProvider::Receiver(prov).get(),
      MS_KEY_STORAGE_PROVIDER, 0);
  ASSERT_FALSE(FAILED(status)) << status;

  LPCWSTR blob_type;
  std::vector<uint8_t> blob;
  ASSERT_TRUE(PKCS8ToBLOBForCNG(*pkcs8, &blob_type, &blob));
  crypto::ScopedNCryptKey ncrypt_key;
  status = NCryptImportKey(prov.get(), /*hImportKey=*/0, blob_type,
                           /*pParameterList=*/nullptr,
                           crypto::ScopedNCryptKey::Receiver(ncrypt_key).get(),
                           blob.data(), blob.size(), NCRYPT_SILENT_FLAG);
  ASSERT_FALSE(FAILED(status)) << status;

  scoped_refptr<SSLPrivateKey> key =
      WrapCNGPrivateKey(cert.get(), std::move(ncrypt_key));
  EXPECT_FALSE(key);
}

TEST(SSLPlatformKeyWinInvalidTest, UnsupportedKeyTypeCAPI) {
  scoped_refptr<X509Certificate> cert =
      ImportCertFromFile(GetTestCertsDirectory(), "client_x25519.pem");
  ASSERT_TRUE(cert);

  // CAPI only supports RSA keys. However, `WrapCAPIPrivateKey` only inspects
  // the certificate's public key when checking the key type, so test rejection
  // by pairing the certificate with an arbitrary RSA key.
  base::FilePath pkcs8_path =
      GetTestCertsDirectory().AppendASCII("client_1.pk8");
  std::optional<std::vector<uint8_t>> pkcs8 = base::ReadFileToBytes(pkcs8_path);
  ASSERT_TRUE(pkcs8);

  crypto::ScopedHCRYPTPROV prov;
  ASSERT_NE(FALSE,
            CryptAcquireContext(crypto::ScopedHCRYPTPROV::Receiver(prov).get(),
                                nullptr, nullptr, PROV_RSA_AES,
                                CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
      << GetLastError();

  std::vector<uint8_t> blob;
  ASSERT_TRUE(PKCS8ToBLOBForCAPI(*pkcs8, &blob));

  crypto::ScopedHCRYPTKEY hcryptkey;
  ASSERT_NE(FALSE,
            CryptImportKey(prov.get(), blob.data(), blob.size(),
                           /*hPubKey=*/0, /*dwFlags=*/0,
                           crypto::ScopedHCRYPTKEY::Receiver(hcryptkey).get()))
      << GetLastError();
  hcryptkey.reset();

  scoped_refptr<SSLPrivateKey> key =
      WrapCAPIPrivateKey(cert.get(), std::move(prov), AT_SIGNATURE);
  EXPECT_FALSE(key);
}

class UnexportableSSLPlatformKeyWinTest : public testing::TestWithParam<bool> {
 protected:
  bool UseHardwareBackedKeys() { return GetParam(); }
};

TEST_P(UnexportableSSLPlatformKeyWinTest, WrapUnexportableKeySlowly) {
  auto provider = UseHardwareBackedKeys()
                      ? crypto::GetUnexportableKeyProvider({})
                      : crypto::GetMicrosoftSoftwareUnexportableKeyProvider();
  if (!provider) {
    GTEST_SKIP() << "Platform keys are not supported.";
  }

  const crypto::sign::SignatureKind algorithms[] = {
      crypto::sign::ECDSA_SHA256, crypto::sign::RSA_PKCS1_SHA256};
  auto key = provider->GenerateSigningKeySlowly(algorithms);
  if (!key) {
    // Could be hitting crbug.com/41494935. Fine to skip the test as the
    // UnexportableKeyProvider logic is covered in another test suite.
    GTEST_SKIP()
        << "Workaround for https://issues.chromium.org/issues/41494935";
  }

  auto ssl_private_key = WrapUnexportableKeySlowly(*key);
  ASSERT_TRUE(ssl_private_key);
}

INSTANTIATE_TEST_SUITE_P(All,
                         UnexportableSSLPlatformKeyWinTest,
                         testing::Bool());

}  // namespace net
