// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_signature_header_field.h"

#include "base/functional/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class SignedExchangeSignatureHeaderFieldTest : public ::testing::Test {
 protected:
  SignedExchangeSignatureHeaderFieldTest() {}
};

TEST_F(SignedExchangeSignatureHeaderFieldTest, ParseSignature) {
  const char hdr_string[] =
      "sig1;"
      " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
      "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY=*;"
      " integrity=\"mi-draft2\";"
      " validity-url=\"https://example.com/resource.validity.1511128380\";"
      " cert-url=\"https://example.com/oldcerts\";"
      " cert-sha256=*W7uB969dFW3Mb5ZefPS9Tq5ZbH5iSmOILpjv2qEArmI=*;"
      " date=1511128380; expires=1511733180,"
      "sig2;"
      " sig=*MEQCIGjZRqTRf9iKNkGFyzRMTFgwf/BrY2ZNIP/dykhUV0aYAiBTXg+8wujoT4n/W+"
      "cNgb7pGqQvIUGYZ8u8HZJ5YH26Qg==*;"
      " integrity=\"mi-draft2\";"
      " validity-url=\"https://example.com/resource.validity.1511128380\";"
      " cert-url=\"https://example.com/newcerts\";"
      " cert-sha256=*J/lEm9kNRODdCmINbvitpvdYKNQ+YgBj99DlYp4fEXw=*;"
      " date=1511128380; expires=1511733180";

  const uint8_t decoded_sig1[] = {
      0x30, 0x45, 0x02, 0x21, 0x00, 0xd7, 0x94, 0x8d, 0xa0, 0x37, 0x74, 0x4d,
      0x06, 0x58, 0x05, 0x8a, 0xe4, 0x4d, 0x16, 0x96, 0x57, 0x70, 0x32, 0x1a,
      0x52, 0x95, 0xfa, 0x1c, 0x81, 0x30, 0x71, 0x91, 0x1c, 0xd1, 0xc6, 0x58,
      0x2c, 0x02, 0x20, 0x6b, 0xd0, 0xec, 0x54, 0xe3, 0x0c, 0xfa, 0x0e, 0x58,
      0xa7, 0x01, 0x01, 0x74, 0x65, 0xb7, 0xb1, 0x2f, 0x9b, 0xbe, 0x79, 0x80,
      0x24, 0x98, 0x92, 0x33, 0x08, 0x6e, 0x05, 0xda, 0xa9, 0xe5, 0x46};
  const net::SHA256HashValue decoded_cert_sha256_1 = {
      {0x5b, 0xbb, 0x81, 0xf7, 0xaf, 0x5d, 0x15, 0x6d, 0xcc, 0x6f, 0x96,
       0x5e, 0x7c, 0xf4, 0xbd, 0x4e, 0xae, 0x59, 0x6c, 0x7e, 0x62, 0x4a,
       0x63, 0x88, 0x2e, 0x98, 0xef, 0xda, 0xa1, 0x00, 0xae, 0x62}};
  const uint8_t decoded_sig2[] = {
      0x30, 0x44, 0x02, 0x20, 0x68, 0xd9, 0x46, 0xa4, 0xd1, 0x7f, 0xd8, 0x8a,
      0x36, 0x41, 0x85, 0xcb, 0x34, 0x4c, 0x4c, 0x58, 0x30, 0x7f, 0xf0, 0x6b,
      0x63, 0x66, 0x4d, 0x20, 0xff, 0xdd, 0xca, 0x48, 0x54, 0x57, 0x46, 0x98,
      0x02, 0x20, 0x53, 0x5e, 0x0f, 0xbc, 0xc2, 0xe8, 0xe8, 0x4f, 0x89, 0xff,
      0x5b, 0xe7, 0x0d, 0x81, 0xbe, 0xe9, 0x1a, 0xa4, 0x2f, 0x21, 0x41, 0x98,
      0x67, 0xcb, 0xbc, 0x1d, 0x92, 0x79, 0x60, 0x7d, 0xba, 0x42};
  const net::SHA256HashValue decoded_cert_sha256_2 = {
      {0x27, 0xf9, 0x44, 0x9b, 0xd9, 0x0d, 0x44, 0xe0, 0xdd, 0x0a, 0x62,
       0x0d, 0x6e, 0xf8, 0xad, 0xa6, 0xf7, 0x58, 0x28, 0xd4, 0x3e, 0x62,
       0x00, 0x63, 0xf7, 0xd0, 0xe5, 0x62, 0x9e, 0x1f, 0x11, 0x7c}};

  auto signatures = SignedExchangeSignatureHeaderField::ParseSignature(
      hdr_string, nullptr /* devtools_proxy */);
  ASSERT_TRUE(signatures.has_value());
  ASSERT_EQ(signatures->size(), 2u);

  EXPECT_EQ(signatures->at(0).label, "sig1");
  EXPECT_EQ(signatures->at(0).sig,
            std::string(reinterpret_cast<const char*>(decoded_sig1),
                        sizeof(decoded_sig1)));
  EXPECT_EQ(signatures->at(0).integrity, "mi-draft2");
  EXPECT_EQ(signatures->at(0).validity_url.url,
            "https://example.com/resource.validity.1511128380");
  EXPECT_EQ(signatures->at(0).cert_url, "https://example.com/oldcerts");
  EXPECT_EQ(signatures->at(0).cert_sha256, decoded_cert_sha256_1);
  EXPECT_EQ(signatures->at(0).date, 1511128380ul);
  EXPECT_EQ(signatures->at(0).expires, 1511733180ul);

  EXPECT_EQ(signatures->at(1).label, "sig2");
  EXPECT_EQ(signatures->at(1).sig,
            std::string(reinterpret_cast<const char*>(decoded_sig2),
                        sizeof(decoded_sig2)));
  EXPECT_EQ(signatures->at(1).integrity, "mi-draft2");
  EXPECT_EQ(signatures->at(1).validity_url.url,
            "https://example.com/resource.validity.1511128380");
  EXPECT_EQ(signatures->at(1).cert_url, "https://example.com/newcerts");
  EXPECT_EQ(signatures->at(1).cert_sha256, decoded_cert_sha256_2);
  EXPECT_EQ(signatures->at(1).date, 1511128380ul);
  EXPECT_EQ(signatures->at(1).expires, 1511733180ul);
}

TEST_F(SignedExchangeSignatureHeaderFieldTest, IncompleteSignature) {
  const char hdr_string[] =
      "sig1;"
      " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
      "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY=*;"
      // no integrity= param
      " validity-url=\"https://example.com/resource.validity.1511128380\";"
      " cert-url=\"https://example.com/oldcerts\";"
      " cert-sha256=*W7uB969dFW3Mb5ZefPS9Tq5ZbH5iSmOILpjv2qEArmI=*;"
      " date=1511128380; expires=1511733180";
  auto signatures = SignedExchangeSignatureHeaderField::ParseSignature(
      hdr_string, nullptr /* devtools_proxy */);
  EXPECT_FALSE(signatures.has_value());
}

TEST_F(SignedExchangeSignatureHeaderFieldTest, DuplicatedParam) {
  const char hdr_string[] =
      "sig1;"
      " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
      "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY=*;"
      " integrity=\"mi-draft2\";"
      " validity-url=\"https://example.com/resource.validity.1511128380\";"
      " cert-url=\"https://example.com/oldcerts\";"
      " cert-url=\"https://example.com/oldcerts\";"
      " cert-sha256=*W7uB969dFW3Mb5ZefPS9Tq5ZbH5iSmOILpjv2qEArmI=*;"
      " date=1511128380; expires=1511733180";
  auto signatures = SignedExchangeSignatureHeaderField::ParseSignature(
      hdr_string, nullptr /* devtools_proxy */);
  EXPECT_FALSE(signatures.has_value());
}

TEST_F(SignedExchangeSignatureHeaderFieldTest, InvalidCertURL) {
  const char hdr_string[] =
      "sig1;"
      " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
      "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY=*;"
      " integrity=\"mi-draft2\";"
      " validity-url=\"https://example.com/resource.validity.1511128380\";"
      " cert-url=\"https:://example.com/oldcerts\";"
      " cert-sha256=*W7uB969dFW3Mb5ZefPS9Tq5ZbH5iSmOILpjv2qEArmI=*;"
      " date=1511128380; expires=1511733180";
  auto signatures = SignedExchangeSignatureHeaderField::ParseSignature(
      hdr_string, nullptr /* devtools_proxy */);
  EXPECT_FALSE(signatures.has_value());
}

TEST_F(SignedExchangeSignatureHeaderFieldTest, CertURLWithFragment) {
  const char hdr_string[] =
      "sig1;"
      " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
      "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY=*;"
      " integrity=\"mi-draft2\";"
      " validity-url=\"https://example.com/resource.validity.1511128380\";"
      " cert-url=\"https://example.com/oldcerts#test\";"
      " cert-sha256=*W7uB969dFW3Mb5ZefPS9Tq5ZbH5iSmOILpjv2qEArmI=*;"
      " date=1511128380; expires=1511733180";
  auto signatures = SignedExchangeSignatureHeaderField::ParseSignature(
      hdr_string, nullptr /* devtools_proxy */);
  EXPECT_FALSE(signatures.has_value());
}

TEST_F(SignedExchangeSignatureHeaderFieldTest, CertURLHttpShouldFail) {
  const char hdr_string[] =
      "sig1;"
      " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
      "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY=*;"
      " integrity=\"mi-draft2\";"
      " validity-url=\"https://example.com/resource.validity.1511128380\";"
      " cert-url=\"http://example.com/oldcerts#test\";"
      " cert-sha256=*W7uB969dFW3Mb5ZefPS9Tq5ZbH5iSmOILpjv2qEArmI=*;"
      " date=1511128380; expires=1511733180";
  auto signatures = SignedExchangeSignatureHeaderField::ParseSignature(
      hdr_string, nullptr /* devtools_proxy */);
  EXPECT_FALSE(signatures.has_value());
}

TEST_F(SignedExchangeSignatureHeaderFieldTest, RelativeCertURL) {
  const char hdr_string[] =
      "sig1;"
      " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
      "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY=*;"
      " integrity=\"mi-draft2\";"
      " validity-url=\"https://example.com/resource.validity.1511128380\";"
      " cert-url=\"oldcerts\";"
      " cert-sha256=*W7uB969dFW3Mb5ZefPS9Tq5ZbH5iSmOILpjv2qEArmI=*;"
      " date=1511128380; expires=1511733180";
  auto signatures = SignedExchangeSignatureHeaderField::ParseSignature(
      hdr_string, nullptr /* devtools_proxy */);
  EXPECT_FALSE(signatures.has_value());
}

TEST_F(SignedExchangeSignatureHeaderFieldTest, InvalidValidityUrl) {
  const char hdr_string[] =
      "sig1;"
      " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
      "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY=*;"
      " integrity=\"mi-draft2\";"
      " validity-url=\"https:://example.com/resource.validity.1511128380\";"
      " cert-url=\"https://example.com/oldcerts\";"
      " cert-sha256=*W7uB969dFW3Mb5ZefPS9Tq5ZbH5iSmOILpjv2qEArmI=*;"
      " date=1511128380; expires=1511733180";
  auto signatures = SignedExchangeSignatureHeaderField::ParseSignature(
      hdr_string, nullptr /* devtools_proxy */);
  EXPECT_FALSE(signatures.has_value());
}

TEST_F(SignedExchangeSignatureHeaderFieldTest, ValidityUrlWithFragment) {
  const char hdr_string[] =
      "sig1;"
      " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
      "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY=*;"
      " integrity=\"mi-draft2\";"
      " validity-url=\"https://example.com/resource.validity.1511128380#test\";"
      " cert-url=\"https://example.com/oldcerts\";"
      " cert-sha256=*W7uB969dFW3Mb5ZefPS9Tq5ZbH5iSmOILpjv2qEArmI=*;"
      " date=1511128380; expires=1511733180";
  auto signatures = SignedExchangeSignatureHeaderField::ParseSignature(
      hdr_string, nullptr /* devtools_proxy */);
  EXPECT_FALSE(signatures.has_value());
}

TEST_F(SignedExchangeSignatureHeaderFieldTest, ValidityUrlHttpShouldFail) {
  const char hdr_string[] =
      "sig1;"
      " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
      "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY=*;"
      " integrity=\"mi-draft2\";"
      " validity-url=\"http://example.com/resource.validity.1511128380#test\";"
      " cert-url=\"https://example.com/oldcerts\";"
      " cert-sha256=*W7uB969dFW3Mb5ZefPS9Tq5ZbH5iSmOILpjv2qEArmI=*;"
      " date=1511128380; expires=1511733180";
  auto signatures = SignedExchangeSignatureHeaderField::ParseSignature(
      hdr_string, nullptr /* devtools_proxy */);
  EXPECT_FALSE(signatures.has_value());
}

TEST_F(SignedExchangeSignatureHeaderFieldTest, RelativeValidityUrl) {
  const char hdr_string[] =
      "sig1;"
      " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
      "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY=*;"
      " integrity=\"mi-draft2\";"
      " validity-url=\"resource.validity.1511128380\";"
      " cert-url=\"https://example.com/oldcerts\";"
      " cert-sha256=*W7uB969dFW3Mb5ZefPS9Tq5ZbH5iSmOILpjv2qEArmI=*;"
      " date=1511128380; expires=1511733180";
  auto signatures = SignedExchangeSignatureHeaderField::ParseSignature(
      hdr_string, nullptr /* devtools_proxy */);
  EXPECT_FALSE(signatures.has_value());
}

TEST_F(SignedExchangeSignatureHeaderFieldTest, InvalidCertSHA256) {
  const char hdr_string[] =
      "sig1;"
      " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
      "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY=*;"
      " integrity=\"mi-draft2\";"
      " validity-url=\"https://example.com/resource.validity.1511128380\";"
      " cert-url=\"https://example.com/oldcerts\";"
      " cert-sha256=*W7uB969dFW3Mb5ZefPS9;"
      " date=1511128380; expires=1511733180";
  auto signatures = SignedExchangeSignatureHeaderField::ParseSignature(
      hdr_string, nullptr /* devtools_proxy */);
  EXPECT_FALSE(signatures.has_value());
}

TEST_F(SignedExchangeSignatureHeaderFieldTest, OpenQuoteAtEnd) {
  const char hdr_string[] = "sig1; sig=\"";
  auto signatures = SignedExchangeSignatureHeaderField::ParseSignature(
      hdr_string, nullptr /* devtools_proxy */);
  EXPECT_FALSE(signatures.has_value());
}

TEST_F(SignedExchangeSignatureHeaderFieldTest, AsteriskInTheMiddleOfBinary) {
  const char hdr_string[] =
      "sig1;"
      " sig=*MEUCIQDXlI2gN3RNBlgFiuRNFpZXcDIaUpX6HIEwcZEc0cZYLAIga9DsVOMM+"
      "g5YpwEBdGW3sS+bvnmAJJiSMwhuBdqp5UY=*;"
      " integrity=\"mi-draft2\";"
      " validity-url=\"https://example.com/resource.validity.1511128380\";"
      " cert-url=\"https://example.com/oldcerts\";"
      " cert-sha256=*W7uB969dFW3Mb5ZefP*S9Tq5ZbH5iSmOILpjv2qEArmI=*;"
      " date=1511128380; expires=1511733180";
  auto signatures = SignedExchangeSignatureHeaderField::ParseSignature(
      hdr_string, nullptr /* devtools_proxy */);
  EXPECT_FALSE(signatures.has_value());
}

}  // namespace content
