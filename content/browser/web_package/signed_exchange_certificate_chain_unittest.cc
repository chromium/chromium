// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_certificate_chain.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/optional.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "components/cbor/values.h"
#include "components/cbor/writer.h"
#include "content/browser/web_package/signed_exchange_test_utils.h"
#include "content/public/common/content_paths.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

cbor::Value CBORByteString(base::StringPiece str) {
  return cbor::Value(str, cbor::Value::Type::BYTE_STRING);
}

scoped_refptr<net::X509Certificate> LoadCertificate(
    const std::string& cert_file) {
  base::FilePath dir_path;
  base::PathService::Get(content::DIR_TEST_DATA, &dir_path);
  dir_path = dir_path.AppendASCII("sxg");

  return net::CreateCertificateChainFromFile(
      dir_path, cert_file, net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
}

}  // namespace

TEST(SignedExchangeCertificateParseTest, Empty) {
  auto parsed = SignedExchangeCertificateChain::Parse(
      base::span<const uint8_t>(), nullptr);
  EXPECT_FALSE(parsed);
}

TEST(SignedExchangeCertificateParseTest, EmptyChain) {
  cbor::Value::ArrayValue cbor_array;
  cbor_array.push_back(cbor::Value(u8"\U0001F4DC\u26D3"));

  auto serialized = cbor::Writer::Write(cbor::Value(std::move(cbor_array)));
  ASSERT_TRUE(serialized.has_value());

  auto parsed = SignedExchangeCertificateChain::Parse(
      base::make_span(*serialized), nullptr);
  EXPECT_FALSE(parsed);
}

TEST(SignedExchangeCertificateParseTest, MissingCert) {
  cbor::Value::MapValue cbor_map;
  cbor_map[cbor::Value("sct")] = CBORByteString("SCT");
  cbor_map[cbor::Value("ocsp")] = CBORByteString("OCSP");

  cbor::Value::ArrayValue cbor_array;
  cbor_array.push_back(cbor::Value(u8"\U0001F4DC\u26D3"));
  cbor_array.push_back(cbor::Value(std::move(cbor_map)));

  auto serialized = cbor::Writer::Write(cbor::Value(std::move(cbor_array)));
  ASSERT_TRUE(serialized.has_value());

  auto parsed = SignedExchangeCertificateChain::Parse(
      base::make_span(*serialized), nullptr);
  EXPECT_FALSE(parsed);
}

TEST(SignedExchangeCertificateParseTest, OneCert) {
  net::CertificateList certs;
  ASSERT_TRUE(
      net::LoadCertificateFiles({"subjectAltName_sanity_check.pem"}, &certs));
  ASSERT_EQ(1U, certs.size());
  base::StringPiece cert_der =
      net::x509_util::CryptoBufferAsStringPiece(certs[0]->cert_buffer());

  cbor::Value::MapValue cbor_map;
  cbor_map[cbor::Value("sct")] = CBORByteString("SCT");
  cbor_map[cbor::Value("cert")] = CBORByteString(cert_der);
  cbor_map[cbor::Value("ocsp")] = CBORByteString("OCSP");

  cbor::Value::ArrayValue cbor_array;
  cbor_array.push_back(cbor::Value(u8"\U0001F4DC\u26D3"));
  cbor_array.push_back(cbor::Value(std::move(cbor_map)));

  auto serialized = cbor::Writer::Write(cbor::Value(std::move(cbor_array)));
  ASSERT_TRUE(serialized.has_value());

  auto parsed = SignedExchangeCertificateChain::Parse(
      base::make_span(*serialized), nullptr);
  ASSERT_TRUE(parsed);
  EXPECT_EQ(cert_der, net::x509_util::CryptoBufferAsStringPiece(
                          parsed->cert()->cert_buffer()));
  ASSERT_EQ(0U, parsed->cert()->intermediate_buffers().size());
  EXPECT_EQ(parsed->ocsp(), base::make_optional<std::string>("OCSP"));
  EXPECT_EQ(parsed->sct(), base::make_optional<std::string>("SCT"));
}

TEST(SignedExchangeCertificateParseTest, MissingOCSPInFirstCert) {
  net::CertificateList certs;
  ASSERT_TRUE(
      net::LoadCertificateFiles({"subjectAltName_sanity_check.pem"}, &certs));
  ASSERT_EQ(1U, certs.size());
  base::StringPiece cert_der =
      net::x509_util::CryptoBufferAsStringPiece(certs[0]->cert_buffer());

  cbor::Value::MapValue cbor_map;
  cbor_map[cbor::Value("sct")] = CBORByteString("SCT");
  cbor_map[cbor::Value("cert")] = CBORByteString(cert_der);

  cbor::Value::ArrayValue cbor_array;
  cbor_array.push_back(cbor::Value(u8"\U0001F4DC\u26D3"));
  cbor_array.push_back(cbor::Value(std::move(cbor_map)));

  auto serialized = cbor::Writer::Write(cbor::Value(std::move(cbor_array)));
  ASSERT_TRUE(serialized.has_value());

  auto parsed = SignedExchangeCertificateChain::Parse(
      base::make_span(*serialized), nullptr);
  EXPECT_FALSE(parsed);
}

TEST(SignedExchangeCertificateParseTest, TwoCerts) {
  net::CertificateList certs;
  ASSERT_TRUE(net::LoadCertificateFiles(
      {"subjectAltName_sanity_check.pem", "root_ca_cert.pem"}, &certs));
  ASSERT_EQ(2U, certs.size());
  base::StringPiece cert1_der =
      net::x509_util::CryptoBufferAsStringPiece(certs[0]->cert_buffer());
  base::StringPiece cert2_der =
      net::x509_util::CryptoBufferAsStringPiece(certs[1]->cert_buffer());

  cbor::Value::MapValue cbor_map1;
  cbor_map1[cbor::Value("sct")] = CBORByteString("SCT");
  cbor_map1[cbor::Value("cert")] = CBORByteString(cert1_der);
  cbor_map1[cbor::Value("ocsp")] = CBORByteString("OCSP");

  cbor::Value::MapValue cbor_map2;
  cbor_map2[cbor::Value("cert")] = CBORByteString(cert2_der);

  cbor::Value::ArrayValue cbor_array;
  cbor_array.push_back(cbor::Value(u8"\U0001F4DC\u26D3"));
  cbor_array.push_back(cbor::Value(std::move(cbor_map1)));
  cbor_array.push_back(cbor::Value(std::move(cbor_map2)));

  auto serialized = cbor::Writer::Write(cbor::Value(std::move(cbor_array)));
  ASSERT_TRUE(serialized.has_value());

  auto parsed = SignedExchangeCertificateChain::Parse(
      base::make_span(*serialized), nullptr);
  ASSERT_TRUE(parsed);
  EXPECT_EQ(cert1_der, net::x509_util::CryptoBufferAsStringPiece(
                           parsed->cert()->cert_buffer()));
  ASSERT_EQ(1U, parsed->cert()->intermediate_buffers().size());
  EXPECT_EQ(cert2_der, net::x509_util::CryptoBufferAsStringPiece(
                           parsed->cert()->intermediate_buffers()[0].get()));
  EXPECT_EQ(parsed->ocsp(), base::make_optional<std::string>("OCSP"));
  EXPECT_EQ(parsed->sct(), base::make_optional<std::string>("SCT"));
}

TEST(SignedExchangeCertificateParseTest, HavingOCSPInSecondCert) {
  net::CertificateList certs;
  ASSERT_TRUE(net::LoadCertificateFiles(
      {"subjectAltName_sanity_check.pem", "root_ca_cert.pem"}, &certs));
  ASSERT_EQ(2U, certs.size());
  base::StringPiece cert1_der =
      net::x509_util::CryptoBufferAsStringPiece(certs[0]->cert_buffer());
  base::StringPiece cert2_der =
      net::x509_util::CryptoBufferAsStringPiece(certs[1]->cert_buffer());

  cbor::Value::MapValue cbor_map1;
  cbor_map1[cbor::Value("sct")] = CBORByteString("SCT");
  cbor_map1[cbor::Value("cert")] = CBORByteString(cert1_der);
  cbor_map1[cbor::Value("ocsp")] = CBORByteString("OCSP1");

  cbor::Value::MapValue cbor_map2;
  cbor_map2[cbor::Value("cert")] = CBORByteString(cert2_der);
  cbor_map2[cbor::Value("ocsp")] = CBORByteString("OCSP2");

  cbor::Value::ArrayValue cbor_array;
  cbor_array.push_back(cbor::Value(u8"\U0001F4DC\u26D3"));
  cbor_array.push_back(cbor::Value(std::move(cbor_map1)));
  cbor_array.push_back(cbor::Value(std::move(cbor_map2)));

  auto serialized = cbor::Writer::Write(cbor::Value(std::move(cbor_array)));
  ASSERT_TRUE(serialized.has_value());

  auto parsed = SignedExchangeCertificateChain::Parse(
      base::make_span(*serialized), nullptr);
  EXPECT_FALSE(parsed);
}

TEST(SignedExchangeCertificateParseTest, ParseGoldenFile) {
  base::FilePath path;
  base::PathService::Get(content::DIR_TEST_DATA, &path);
  path =
      path.AppendASCII("sxg").AppendASCII("test.example.org.public.pem.cbor");
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(path, &contents));

  auto parsed = SignedExchangeCertificateChain::Parse(
      base::as_bytes(base::make_span(contents)), nullptr);
  ASSERT_TRUE(parsed);
}

TEST(SignedExchangeCertificateChainTest, IgnoreErrorsSPKIList) {
  SignedExchangeCertificateChain::IgnoreErrorsSPKIList ignore_nothing("");
  SignedExchangeCertificateChain::IgnoreErrorsSPKIList ignore_ecdsap256(
      kPEMECDSAP256SPKIHash);
  SignedExchangeCertificateChain::IgnoreErrorsSPKIList ignore_ecdsap384(
      kPEMECDSAP384SPKIHash);
  SignedExchangeCertificateChain::IgnoreErrorsSPKIList ignore_both(
      std::string(kPEMECDSAP256SPKIHash) + "," + kPEMECDSAP384SPKIHash);

  scoped_refptr<net::X509Certificate> cert_ecdsap256 =
      LoadCertificate("prime256v1-sha256.public.pem");
  scoped_refptr<net::X509Certificate> cert_ecdsap384 =
      LoadCertificate("secp384r1-sha256.public.pem");

  EXPECT_FALSE(ignore_nothing.ShouldIgnoreErrorsInternal(cert_ecdsap256));
  EXPECT_FALSE(ignore_nothing.ShouldIgnoreErrorsInternal(cert_ecdsap384));
  EXPECT_TRUE(ignore_ecdsap256.ShouldIgnoreErrorsInternal(cert_ecdsap256));
  EXPECT_FALSE(ignore_ecdsap256.ShouldIgnoreErrorsInternal(cert_ecdsap384));
  EXPECT_FALSE(ignore_ecdsap384.ShouldIgnoreErrorsInternal(cert_ecdsap256));
  EXPECT_TRUE(ignore_ecdsap384.ShouldIgnoreErrorsInternal(cert_ecdsap384));
  EXPECT_TRUE(ignore_both.ShouldIgnoreErrorsInternal(cert_ecdsap256));
  EXPECT_TRUE(ignore_both.ShouldIgnoreErrorsInternal(cert_ecdsap384));
}

}  // namespace content
