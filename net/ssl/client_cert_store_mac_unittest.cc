// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_store_mac.h"

#include <memory>

#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store_unittest-inl.h"
#include "net/ssl/ssl_private_key.h"

namespace net {

class ClientCertStoreMacTestDelegate {
 public:
  bool SelectClientCerts(const CertificateList& input_certs,
                         const SSLCertRequestInfo& cert_request_info,
                         ClientCertIdentityList* selected_certs) {
    return store_.SelectClientCertsForTesting(
        FakeClientCertIdentityListFromCertificateList(input_certs),
        cert_request_info, selected_certs);
  }

 private:
  ClientCertStoreMac store_;
};

INSTANTIATE_TYPED_TEST_SUITE_P(Mac,
                               ClientCertStoreTest,
                               ClientCertStoreMacTestDelegate);

class ClientCertStoreMacTest : public ::testing::Test {
 protected:
  bool SelectClientCertsGivenPreferred(
      const scoped_refptr<X509Certificate>& preferred_cert,
      const CertificateList& regular_certs,
      const SSLCertRequestInfo& request,
      ClientCertIdentityList* selected_certs) {
    std::unique_ptr<ClientCertIdentity> preferred_identity(
        std::make_unique<FakeClientCertIdentity>(preferred_cert, nullptr));

    return store_.SelectClientCertsGivenPreferredForTesting(
        std::move(preferred_identity),
        FakeClientCertIdentityListFromCertificateList(regular_certs), request,
        selected_certs);
  }

 private:
  ClientCertStoreMac store_;
};

// Verify that the preferred cert gets filtered out when it doesn't match the
// server criteria.
TEST_F(ClientCertStoreMacTest, FilterOutThePreferredCert) {
  scoped_refptr<X509Certificate> cert_1(
      ImportCertFromFile(GetTestCertsDirectory(), "client_1.pem"));
  ASSERT_TRUE(cert_1.get());

  std::vector<std::string> authority_2(
      1, std::string(reinterpret_cast<const char*>(kAuthority2DN),
                     sizeof(kAuthority2DN)));
  EXPECT_FALSE(cert_1->IsIssuedByEncoded(authority_2));

  std::vector<scoped_refptr<X509Certificate> > certs;
  auto request = base::MakeRefCounted<SSLCertRequestInfo>();
  request->cert_authorities = authority_2;

  ClientCertIdentityList selected_certs;
  bool rv = SelectClientCertsGivenPreferred(
      cert_1, certs, *request.get(), &selected_certs);
  EXPECT_TRUE(rv);
  EXPECT_EQ(0u, selected_certs.size());
}

// Verify that the preferred cert takes the first position in the output list,
// when it does not get filtered out.
TEST_F(ClientCertStoreMacTest, PreferredCertGoesFirst) {
  scoped_refptr<X509Certificate> cert_1(
      ImportCertFromFile(GetTestCertsDirectory(), "client_1.pem"));
  ASSERT_TRUE(cert_1.get());
  scoped_refptr<X509Certificate> cert_2(
      ImportCertFromFile(GetTestCertsDirectory(), "client_2.pem"));
  ASSERT_TRUE(cert_2.get());

  std::vector<scoped_refptr<X509Certificate> > certs;
  certs.push_back(cert_2);
  auto request = base::MakeRefCounted<SSLCertRequestInfo>();

  ClientCertIdentityList selected_certs;
  bool rv = SelectClientCertsGivenPreferred(
      cert_1, certs, *request.get(), &selected_certs);
  EXPECT_TRUE(rv);
  ASSERT_EQ(2u, selected_certs.size());
  EXPECT_TRUE(
      selected_certs[0]->certificate()->EqualsExcludingChain(cert_1.get()));
  EXPECT_TRUE(
      selected_certs[1]->certificate()->EqualsExcludingChain(cert_2.get()));
}

}  // namespace net
