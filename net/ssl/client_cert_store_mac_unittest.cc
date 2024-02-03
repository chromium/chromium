// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ssl/client_cert_store_mac.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "net/cert/x509_util_apple.h"
#include "net/ssl/client_cert_identity_mac.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/client_cert_store_unittest-inl.h"
#include "net/ssl/ssl_private_key.h"
#include "net/test/cert_builder.h"
#include "third_party/boringssl/src/pki/extended_key_usage.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"

namespace net {

namespace {

std::vector<std::unique_ptr<ClientCertIdentityMac>>
ClientCertIdentityMacListFromCertificateList(const CertificateList& certs) {
  // This doesn't quite construct a real `ClientCertIdentityMac` because the
  // `SecIdentityRef` is null. This means `SelectClientCertsForTesting` must
  // turn off the KeyChain query. If this becomes an issue, change
  // client_cert_store_unittest-inl.h to pass in the key data.
  //
  // Actually constructing a `SecIdentityRef` without persisting it is not
  // currently possible with macOS's non-deprecated APIs, but it is possible
  // with deprecated APIs using `SecKeychainCreate` and `SecItemImport`. See git
  // history for net/test/keychain_test_util_mac.cc.
  std::vector<std::unique_ptr<ClientCertIdentityMac>> identities;
  identities.reserve(certs.size());
  for (const auto& cert : certs) {
    identities.push_back(std::make_unique<ClientCertIdentityMac>(
        cert, base::apple::ScopedCFTypeRef<SecIdentityRef>()));
  }
  return identities;
}

std::string InputVectorToString(std::vector<bssl::der::Input> vec) {
  std::string r = "{";
  std::string sep;
  for (const auto& element : vec) {
    r += sep;
    r += base::HexEncode(element);
    sep = ',';
  }
  r += '}';
  return r;
}

std::string KeyUsageVectorToString(std::vector<bssl::KeyUsageBit> vec) {
  std::string r = "{";
  std::string sep;
  for (const auto& element : vec) {
    r += sep;
    r += base::NumberToString(static_cast<int>(element));
    sep = ',';
  }
  r += '}';
  return r;
}

}  // namespace

class ClientCertStoreMacTestDelegate {
 public:
  bool SelectClientCerts(const CertificateList& input_certs,
                         const SSLCertRequestInfo& cert_request_info,
                         ClientCertIdentityList* selected_certs) {
    return store_.SelectClientCertsForTesting(
        ClientCertIdentityMacListFromCertificateList(input_certs),
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
  bool SelectClientCerts(const CertificateList& input_certs,
                         const SSLCertRequestInfo& cert_request_info,
                         ClientCertIdentityList* selected_certs) {
    return store_.SelectClientCertsForTesting(
        ClientCertIdentityMacListFromCertificateList(input_certs),
        cert_request_info, selected_certs);
  }

  bool SelectClientCertsGivenPreferred(
      const scoped_refptr<X509Certificate>& preferred_cert,
      const CertificateList& regular_certs,
      const SSLCertRequestInfo& request,
      ClientCertIdentityList* selected_certs) {
    auto preferred_identity = std::make_unique<ClientCertIdentityMac>(
        preferred_cert, base::apple::ScopedCFTypeRef<SecIdentityRef>());

    return store_.SelectClientCertsGivenPreferredForTesting(
        std::move(preferred_identity),
        ClientCertIdentityMacListFromCertificateList(regular_certs), request,
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

TEST_F(ClientCertStoreMacTest, CertSupportsClientAuth) {
  base::FilePath certs_dir = GetTestCertsDirectory();
  std::unique_ptr<CertBuilder> builder =
      CertBuilder::FromFile(certs_dir.AppendASCII("ok_cert.pem"), nullptr);
  ASSERT_TRUE(builder);

  struct {
    bool expected_result;
    std::vector<bssl::KeyUsageBit> key_usages;
    std::vector<bssl::der::Input> ekus;
  } cases[] = {
      {true, {}, {}},
      {true, {bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE}, {}},
      {true,
       {bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE,
        bssl::KEY_USAGE_BIT_KEY_CERT_SIGN},
       {}},
      {false, {bssl::KEY_USAGE_BIT_NON_REPUDIATION}, {}},
      {false, {bssl::KEY_USAGE_BIT_KEY_ENCIPHERMENT}, {}},
      {false, {bssl::KEY_USAGE_BIT_DATA_ENCIPHERMENT}, {}},
      {false, {bssl::KEY_USAGE_BIT_KEY_AGREEMENT}, {}},
      {false, {bssl::KEY_USAGE_BIT_KEY_CERT_SIGN}, {}},
      {false, {bssl::KEY_USAGE_BIT_CRL_SIGN}, {}},
      {false, {bssl::KEY_USAGE_BIT_ENCIPHER_ONLY}, {}},
      {false, {bssl::KEY_USAGE_BIT_DECIPHER_ONLY}, {}},
      {true, {}, {bssl::der::Input(bssl::kAnyEKU)}},
      {true, {}, {bssl::der::Input(bssl::kClientAuth)}},
      {true,
       {},
       {bssl::der::Input(bssl::kServerAuth),
        bssl::der::Input(bssl::kClientAuth)}},
      {true,
       {},
       {bssl::der::Input(bssl::kClientAuth),
        bssl::der::Input(bssl::kServerAuth)}},
      {false, {}, {bssl::der::Input(bssl::kServerAuth)}},
      {true,
       {bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE},
       {bssl::der::Input(bssl::kClientAuth)}},
      {false,
       {bssl::KEY_USAGE_BIT_KEY_CERT_SIGN},
       {bssl::der::Input(bssl::kClientAuth)}},
      {false,
       {bssl::KEY_USAGE_BIT_DIGITAL_SIGNATURE},
       {bssl::der::Input(bssl::kServerAuth)}},
  };

  for (const auto& testcase : cases) {
    SCOPED_TRACE(testcase.expected_result);
    SCOPED_TRACE(KeyUsageVectorToString(testcase.key_usages));
    SCOPED_TRACE(InputVectorToString(testcase.ekus));

    if (testcase.key_usages.empty())
      builder->EraseExtension(bssl::der::Input(bssl::kKeyUsageOid));
    else
      builder->SetKeyUsages(testcase.key_usages);

    if (testcase.ekus.empty())
      builder->EraseExtension(bssl::der::Input(bssl::kExtKeyUsageOid));
    else
      builder->SetExtendedKeyUsages(testcase.ekus);

    auto request = base::MakeRefCounted<SSLCertRequestInfo>();
    ClientCertIdentityList selected_certs;
    bool rv = SelectClientCerts({builder->GetX509Certificate()}, *request.get(),
                                &selected_certs);
    EXPECT_TRUE(rv);
    if (testcase.expected_result) {
      ASSERT_EQ(1U, selected_certs.size());
      EXPECT_TRUE(selected_certs[0]->certificate()->EqualsExcludingChain(
          builder->GetX509Certificate().get()));
    } else {
      EXPECT_TRUE(selected_certs.empty());
    }
  }
}

}  // namespace net
