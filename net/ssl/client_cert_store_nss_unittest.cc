// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_store_nss.h"

#include <cert.h>
#include <certt.h>
#include <pk11pub.h>

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string_view_util.h"
#include "base/test/task_environment.h"
#include "crypto/keypair.h"
#include "crypto/nss_key_util.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store_unittest-inl.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "net/ssl/ssl_private_key_test_util.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "third_party/boringssl/src/pki/extended_key_usage.h"
#include "third_party/boringssl/src/pki/pem.h"

namespace net {

namespace {

void SaveIdentitiesAndQuitCallback(ClientCertIdentityList* out_identities,
                                   base::OnceClosure quit_closure,
                                   ClientCertIdentityList in_identities) {
  *out_identities = std::move(in_identities);
  std::move(quit_closure).Run();
}

void SavePrivateKeyAndQuitCallback(scoped_refptr<SSLPrivateKey>* out_key,
                                   base::OnceClosure quit_closure,
                                   scoped_refptr<SSLPrivateKey> in_key) {
  *out_key = std::move(in_key);
  std::move(quit_closure).Run();
}

}  // namespace

class ClientCertStoreNSSTestDelegate {
 public:
  bool SelectClientCerts(const CertificateList& input_certs,
                         const SSLCertRequestInfo& cert_request_info,
                         ClientCertIdentityList* selected_identities) {
    *selected_identities =
        FakeClientCertIdentityListFromCertificateList(input_certs);

    // Filters |selected_identities| using the logic being used to filter the
    // system store when GetClientCerts() is called.
    crypto::EnsureNSSInit();
    ClientCertStoreNSS::FilterCertsOnWorkerThread(selected_identities,
                                                  cert_request_info);
    return true;
  }
};

INSTANTIATE_TYPED_TEST_SUITE_P(NSSNew,
                               ClientCertStoreTest,
                               ClientCertStoreNSSTestDelegate);

// Tests that ClientCertStoreNSS attempts to build a certificate chain by
// querying NSS before return a certificate.
TEST(ClientCertStoreNSSTest, BuildsCertificateChain) {
  base::test::TaskEnvironment task_environment;

  // Avoid using client_1.pem as a test. Even when we inspect `test_db`,
  // ClientCertSourceNSS will consider CAs in the default slot for
  // path-building. The default slot may contain older versions of the CA
  // certificates in this chain either if the dev has imported an older version
  // of this credential test, or if the machine was impacted by
  // https://crbug.com/546722950.
  //
  // Generating a new chain instead means the CA names are randomized and we
  // will not collide.
  crypto::ScopedTestNSSDB test_db;
  auto key = crypto::keypair::PrivateKey::GenerateEcP256();
  ASSERT_TRUE(crypto::ImportNSSKeyFromPrivateKeyInfo(
      test_db.slot(), key.ToPrivateKeyInfo(), /*permanent=*/true));
  auto [leaf, intermediate, root] = CertBuilder::CreateSimpleChain3();
  leaf->SetExtendedKeyUsages({{bssl::kClientAuth}});
  leaf->SetKey(bssl::UpRef(key.key()));
  ScopedCERTCertificate leaf_nss =
      ImportClientCertToSlot(leaf->GetX509Certificate(), test_db.slot());
  ASSERT_TRUE(leaf_nss);
  ScopedCERTCertificate intermediate_nss = ImportClientCertToSlot(
      intermediate->GetX509Certificate(), test_db.slot());
  ASSERT_TRUE(intermediate_nss);

  auto store = std::make_unique<ClientCertStoreNSS>(
      ClientCertStoreNSS::PasswordDelegateFactory());

  // The test key is an EC key.
  std::vector<uint16_t> expected = SSLPrivateKey::DefaultAlgorithmPreferences(
      EVP_PKEY_EC, true /* supports PSS */);

  {
    // Request certificates matching `intermediate`.
    auto request = base::MakeRefCounted<SSLCertRequestInfo>();
    request->cert_authorities.emplace_back(base::as_string_view(
        x509_util::SECItemAsSpan(intermediate_nss->derSubject)));

    ClientCertIdentityList selected_identities;
    base::RunLoop loop;
    store->GetClientCerts(
        request, base::BindOnce(SaveIdentitiesAndQuitCallback,
                                &selected_identities, loop.QuitClosure()));
    loop.Run();

    // The result be `leaf` with no intermediates.
    ASSERT_EQ(1u, selected_identities.size());
    scoped_refptr<X509Certificate> selected_cert =
        selected_identities[0]->certificate();
    EXPECT_TRUE(x509_util::CryptoBufferEqual(leaf->GetCertBuffer(),
                                             selected_cert->cert_buffer()));
    ASSERT_EQ(0u, selected_cert->intermediate_buffers().size());

    scoped_refptr<SSLPrivateKey> ssl_private_key;
    base::RunLoop key_loop;
    selected_identities[0]->AcquirePrivateKey(
        base::BindOnce(SavePrivateKeyAndQuitCallback, &ssl_private_key,
                       key_loop.QuitClosure()));
    key_loop.Run();

    ASSERT_TRUE(ssl_private_key);
    EXPECT_EQ(expected, ssl_private_key->GetAlgorithmPreferences());
    TestSSLPrivateKeyMatches(ssl_private_key.get(), key.ToPrivateKeyInfo());
  }

  {
    // Request certificates matching `root`.
    auto request = base::MakeRefCounted<SSLCertRequestInfo>();
    request->cert_authorities.emplace_back(base::as_string_view(
        x509_util::SECItemAsSpan(intermediate_nss->derIssuer)));

    ClientCertIdentityList selected_identities;
    base::RunLoop loop;
    store->GetClientCerts(
        request, base::BindOnce(SaveIdentitiesAndQuitCallback,
                                &selected_identities, loop.QuitClosure()));
    loop.Run();

    // The result be `leaf` with `intermediate` as an intermediate.
    ASSERT_EQ(1u, selected_identities.size());
    scoped_refptr<X509Certificate> selected_cert =
        selected_identities[0]->certificate();
    EXPECT_TRUE(x509_util::CryptoBufferEqual(leaf->GetCertBuffer(),
                                             selected_cert->cert_buffer()));
    ASSERT_EQ(1u, selected_cert->intermediate_buffers().size());
    EXPECT_TRUE(x509_util::CryptoBufferEqual(
        intermediate->GetCertBuffer(),
        selected_cert->intermediate_buffers()[0].get()));

    scoped_refptr<SSLPrivateKey> ssl_private_key;
    base::RunLoop key_loop;
    selected_identities[0]->AcquirePrivateKey(
        base::BindOnce(SavePrivateKeyAndQuitCallback, &ssl_private_key,
                       key_loop.QuitClosure()));
    key_loop.Run();
    ASSERT_TRUE(ssl_private_key);
    EXPECT_EQ(expected, ssl_private_key->GetAlgorithmPreferences());
    TestSSLPrivateKeyMatches(ssl_private_key.get(), key.ToPrivateKeyInfo());
  }
}

TEST(ClientCertStoreNSSTest, SubjectPrintableStringContainingUTF8) {
  base::test::TaskEnvironment task_environment;

  crypto::ScopedTestNSSDB test_db;
  base::FilePath certs_dir =
      GetTestNetDataDirectory().AppendASCII("parse_certificate_unittest");

  ASSERT_TRUE(ImportSensitiveKeyFromFile(
      certs_dir, "v3_certificate_template.pk8", test_db.slot()));
  std::string pkcs8_key;
  ASSERT_TRUE(base::ReadFileToString(
      certs_dir.AppendASCII("v3_certificate_template.pk8"), &pkcs8_key));

  std::string file_data;
  ASSERT_TRUE(base::ReadFileToString(
      certs_dir.AppendASCII(
          "subject_printable_string_containing_utf8_client_cert.pem"),
      &file_data));

  bssl::PEMTokenizer pem_tokenizer(file_data, {"CERTIFICATE"});
  ASSERT_TRUE(pem_tokenizer.GetNext());
  std::string cert_der(pem_tokenizer.data());
  ASSERT_FALSE(pem_tokenizer.GetNext());

  ScopedCERTCertificate cert(
      x509_util::CreateCERTCertificateFromBytes(base::as_byte_span(cert_der)));
  ASSERT_TRUE(cert);

  ASSERT_TRUE(ImportClientCertToSlot(cert.get(), test_db.slot()));

  auto store = std::make_unique<ClientCertStoreNSS>(
      ClientCertStoreNSS::PasswordDelegateFactory());

  // These test keys are RSA keys.
  std::vector<uint16_t> expected = SSLPrivateKey::DefaultAlgorithmPreferences(
      EVP_PKEY_RSA, true /* supports PSS */);

  constexpr uint8_t kAuthorityDN[] = {
      0x30, 0x45, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
      0x02, 0x41, 0x55, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08,
      0x0c, 0x0a, 0x53, 0x6f, 0x6d, 0x65, 0x2d, 0x53, 0x74, 0x61, 0x74, 0x65,
      0x31, 0x21, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x18, 0x49,
      0x6e, 0x74, 0x65, 0x72, 0x6e, 0x65, 0x74, 0x20, 0x57, 0x69, 0x64, 0x67,
      0x69, 0x74, 0x73, 0x20, 0x50, 0x74, 0x79, 0x20, 0x4c, 0x74, 0x64};
  auto request = base::MakeRefCounted<SSLCertRequestInfo>();
  request->cert_authorities.emplace_back(
      reinterpret_cast<const char*>(kAuthorityDN), sizeof(kAuthorityDN));

  ClientCertIdentityList selected_identities;
  base::RunLoop loop;
  store->GetClientCerts(
      request, base::BindOnce(SaveIdentitiesAndQuitCallback,
                              &selected_identities, loop.QuitClosure()));
  loop.Run();

  // The result be |cert| with no intermediates.
  ASSERT_EQ(1u, selected_identities.size());
  scoped_refptr<X509Certificate> selected_cert =
      selected_identities[0]->certificate();
  EXPECT_TRUE(x509_util::IsSameCertificate(cert.get(), selected_cert.get()));
  EXPECT_EQ(0u, selected_cert->intermediate_buffers().size());

  scoped_refptr<SSLPrivateKey> ssl_private_key;
  base::RunLoop key_loop;
  selected_identities[0]->AcquirePrivateKey(base::BindOnce(
      SavePrivateKeyAndQuitCallback, &ssl_private_key, key_loop.QuitClosure()));
  key_loop.Run();

  ASSERT_TRUE(ssl_private_key);
  EXPECT_EQ(expected, ssl_private_key->GetAlgorithmPreferences());
  TestSSLPrivateKeyMatches(ssl_private_key.get(), pkcs8_key);
}

// TODO(mattm): is it possible to unittest slot unlocking?

}  // namespace net
