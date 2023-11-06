// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SSL_CLIENT_CERT_STORE_UNITTEST_INL_H_
#define NET_SSL_CLIENT_CERT_STORE_UNITTEST_INL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "net/cert/x509_util.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/pem.h"

namespace net {

namespace {

// "CN=B CA" - DER encoded DN of the issuer of client_1.pem
const unsigned char kAuthority1DN[] = {
    0x30, 0x0f, 0x31, 0x0d, 0x30, 0x0b, 0x06, 0x03, 0x55,
    0x04, 0x03, 0x0c, 0x04, 0x42, 0x20, 0x43, 0x41,
};

// "CN=E CA" - DER encoded DN of the issuer of client_2.pem
const unsigned char kAuthority2DN[] = {
    0x30, 0x0f, 0x31, 0x0d, 0x30, 0x0b, 0x06, 0x03, 0x55,
    0x04, 0x03, 0x0c, 0x04, 0x45, 0x20, 0x43, 0x41,
};

// "CN=C Root CA" - DER encoded DN of the issuer of client_1_ca.pem,
// client_2_ca.pem, and client_3_ca.pem.
const unsigned char kAuthorityRootDN[] = {
    0x30, 0x14, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x03,
    0x0c, 0x09, 0x43, 0x20, 0x52, 0x6f, 0x6f, 0x74, 0x20, 0x43, 0x41,
};

}  // namespace

// Use a templated test to provide common testcases for all the platform
// implementations of ClientCertStore. These cases test the client cert
// filtering behavior.
//
// NOTE: If any test cases are added, removed, or renamed, the
// REGISTER_TYPED_TEST_SUITE_P macro at the bottom of this file must be updated.
//
// The type T provided as the third argument to INSTANTIATE_TYPED_TEST_SUITE_P
// by the platform implementation should implement this method: bool
// SelectClientCerts(const CertificateList& input_certs,
//                        const SSLCertRequestInfo& cert_request_info,
//                        ClientCertIdentityList* selected_identities);
template <typename T>
class ClientCertStoreTest : public TestWithTaskEnvironment {
 public:
  T delegate_;
};

TYPED_TEST_SUITE_P(ClientCertStoreTest);

TYPED_TEST_P(ClientCertStoreTest, EmptyQuery) {
  CertificateList certs;
  auto request = base::MakeRefCounted<SSLCertRequestInfo>();

  ClientCertIdentityList selected_identities;
  bool rv = this->delegate_.SelectClientCerts(certs, *request.get(),
                                              &selected_identities);
  EXPECT_TRUE(rv);
  EXPECT_EQ(0u, selected_identities.size());
}

// Verify that CertRequestInfo with empty |cert_authorities| matches all
// issuers, rather than no issuers.
TYPED_TEST_P(ClientCertStoreTest, AllIssuersAllowed) {
  scoped_refptr<X509Certificate> cert(
      ImportCertFromFile(GetTestCertsDirectory(), "client_1.pem"));
  ASSERT_TRUE(cert.get());

  std::vector<scoped_refptr<X509Certificate> > certs;
  certs.push_back(cert);
  auto request = base::MakeRefCounted<SSLCertRequestInfo>();

  ClientCertIdentityList selected_identities;
  bool rv = this->delegate_.SelectClientCerts(certs, *request.get(),
                                              &selected_identities);
  EXPECT_TRUE(rv);
  ASSERT_EQ(1u, selected_identities.size());
  EXPECT_TRUE(
      selected_identities[0]->certificate()->EqualsExcludingChain(cert.get()));
}

// Verify that certificates are correctly filtered against CertRequestInfo with
// |cert_authorities| containing only |authority_1_DN|.
TYPED_TEST_P(ClientCertStoreTest, CertAuthorityFiltering) {
  scoped_refptr<X509Certificate> cert_1(
      ImportCertFromFile(GetTestCertsDirectory(), "client_1.pem"));
  ASSERT_TRUE(cert_1.get());
  scoped_refptr<X509Certificate> cert_2(
      ImportCertFromFile(GetTestCertsDirectory(), "client_2.pem"));
  ASSERT_TRUE(cert_2.get());

  std::vector<std::string> authority_1(
      1, std::string(reinterpret_cast<const char*>(kAuthority1DN),
                     sizeof(kAuthority1DN)));
  std::vector<std::string> authority_2(
      1, std::string(reinterpret_cast<const char*>(kAuthority2DN),
                     sizeof(kAuthority2DN)));
  EXPECT_TRUE(cert_1->IsIssuedByEncoded(authority_1));
  EXPECT_FALSE(cert_1->IsIssuedByEncoded(authority_2));
  EXPECT_TRUE(cert_2->IsIssuedByEncoded(authority_2));
  EXPECT_FALSE(cert_2->IsIssuedByEncoded(authority_1));

  std::vector<scoped_refptr<X509Certificate> > certs;
  certs.push_back(cert_1);
  certs.push_back(cert_2);
  auto request = base::MakeRefCounted<SSLCertRequestInfo>();
  request->cert_authorities = authority_1;

  ClientCertIdentityList selected_identities;
  bool rv = this->delegate_.SelectClientCerts(certs, *request.get(),
                                              &selected_identities);
  EXPECT_TRUE(rv);
  ASSERT_EQ(1u, selected_identities.size());
  EXPECT_TRUE(selected_identities[0]->certificate()->EqualsExcludingChain(
      cert_1.get()));
}

TYPED_TEST_P(ClientCertStoreTest, PrintableStringContainingUTF8) {
  base::FilePath certs_dir =
      GetTestNetDataDirectory().AppendASCII("parse_certificate_unittest");

  std::string file_data;
  ASSERT_TRUE(base::ReadFileToString(
      certs_dir.AppendASCII(
          "subject_printable_string_containing_utf8_client_cert.pem"),
      &file_data));

  bssl::PEMTokenizer pem_tokenizer(file_data, {"CERTIFICATE"});
  ASSERT_TRUE(pem_tokenizer.GetNext());
  std::string cert_der(pem_tokenizer.data());
  ASSERT_FALSE(pem_tokenizer.GetNext());

  bssl::UniquePtr<CRYPTO_BUFFER> cert_handle =
      x509_util::CreateCryptoBuffer(cert_der);
  ASSERT_TRUE(cert_handle);

  X509Certificate::UnsafeCreateOptions options;
  options.printable_string_is_utf8 = true;
  scoped_refptr<X509Certificate> cert =
      X509Certificate::CreateFromBufferUnsafeOptions(std::move(cert_handle), {},
                                                     options);
  ASSERT_TRUE(cert);

  auto request = base::MakeRefCounted<SSLCertRequestInfo>();

  ClientCertIdentityList selected_identities;
  bool rv = this->delegate_.SelectClientCerts({cert}, *request.get(),
                                              &selected_identities);
  EXPECT_TRUE(rv);
  ASSERT_EQ(1u, selected_identities.size());
  EXPECT_TRUE(
      selected_identities[0]->certificate()->EqualsExcludingChain(cert.get()));
}

REGISTER_TYPED_TEST_SUITE_P(ClientCertStoreTest,
                            EmptyQuery,
                            AllIssuersAllowed,
                            CertAuthorityFiltering,
                            PrintableStringContainingUTF8);

}  // namespace net

#endif  // NET_SSL_CLIENT_CERT_STORE_UNITTEST_INL_H_
