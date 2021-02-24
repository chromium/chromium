// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/token_validator_base.h"

#include <vector>

#include "base/atomic_sequence_num.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/x509_util.h"
#include "net/ssl/client_cert_identity_test_util.h"
#include "net/ssl/test_ssl_private_key.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTokenUrl[] = "https://example.com/token";
const char kTokenValidationUrl[] = "https://example.com/validate";
const char kTokenValidationCertIssuer[] = "*";

base::AtomicSequenceNumber g_serial_number;

std::unique_ptr<net::FakeClientCertIdentity> CreateFakeCert(
    base::Time valid_start,
    base::Time valid_expiry) {
  std::unique_ptr<crypto::RSAPrivateKey> rsa_private_key;
  std::string cert_der;
  net::x509_util::CreateKeyAndSelfSignedCert(
      "CN=subject", g_serial_number.GetNext(), valid_start, valid_expiry,
      &rsa_private_key, &cert_der);

  scoped_refptr<net::X509Certificate> cert =
      net::X509Certificate::CreateFromBytes(cert_der.data(), cert_der.size());
  if (!cert)
    return nullptr;

  scoped_refptr<net::SSLPrivateKey> ssl_private_key =
      net::WrapRSAPrivateKey(rsa_private_key.get());
  if (!ssl_private_key)
    return nullptr;

  return std::make_unique<net::FakeClientCertIdentity>(cert, ssl_private_key);
}

}  // namespace

namespace remoting {

class TestTokenValidator : TokenValidatorBase {
 public:
  explicit TestTokenValidator(const ThirdPartyAuthConfig& config);
  ~TestTokenValidator() override;

  void SelectCertificates(net::ClientCertIdentityList selected_certs);

  void ExpectContinueWithCertificate(
      const net::FakeClientCertIdentity* identity);

 protected:
  void ContinueWithCertificate(
      scoped_refptr<net::X509Certificate> client_cert,
      scoped_refptr<net::SSLPrivateKey> client_private_key) override;

 private:
  void StartValidateRequest(const std::string& token) override {}

  net::X509Certificate* expected_client_cert_ = nullptr;
  net::SSLPrivateKey* expected_private_key_ = nullptr;
};

TestTokenValidator::TestTokenValidator(const ThirdPartyAuthConfig& config) :
    TokenValidatorBase(config, "", nullptr) {
}

TestTokenValidator::~TestTokenValidator() = default;

void TestTokenValidator::SelectCertificates(
    net::ClientCertIdentityList selected_certs) {
  OnCertificatesSelected(nullptr, std::move(selected_certs));
}

void TestTokenValidator::ExpectContinueWithCertificate(
    const net::FakeClientCertIdentity* identity) {
  if (identity) {
    expected_client_cert_ = identity->certificate();
    expected_private_key_ = identity->ssl_private_key();
  } else {
    expected_client_cert_ = nullptr;
    expected_private_key_ = nullptr;
  }
}

void TestTokenValidator::ContinueWithCertificate(
    scoped_refptr<net::X509Certificate> client_cert,
    scoped_refptr<net::SSLPrivateKey> client_private_key) {
  EXPECT_EQ(expected_client_cert_, client_cert.get());
  EXPECT_EQ(expected_private_key_, client_private_key.get());
}

class TokenValidatorBaseTest : public testing::Test {
 public:
  void SetUp() override;
 protected:
  std::unique_ptr<TestTokenValidator> token_validator_;
};

void TokenValidatorBaseTest::SetUp() {
  ThirdPartyAuthConfig config;
  config.token_url = GURL(kTokenUrl);
  config.token_validation_url = GURL(kTokenValidationUrl);
  config.token_validation_cert_issuer = kTokenValidationCertIssuer;
  token_validator_.reset(new TestTokenValidator(config));
}

TEST_F(TokenValidatorBaseTest, TestSelectCertificate) {
  base::Time now = base::Time::Now();

  std::unique_ptr<net::FakeClientCertIdentity> cert_expired_5_minutes_ago =
      CreateFakeCert(now - base::TimeDelta::FromMinutes(10),
                     now - base::TimeDelta::FromMinutes(5));
  ASSERT_TRUE(cert_expired_5_minutes_ago);

  std::unique_ptr<net::FakeClientCertIdentity> cert_start_5min_expire_5min =
      CreateFakeCert(now - base::TimeDelta::FromMinutes(5),
                     now + base::TimeDelta::FromMinutes(5));
  ASSERT_TRUE(cert_start_5min_expire_5min);

  std::unique_ptr<net::FakeClientCertIdentity> cert_start_10min_expire_5min =
      CreateFakeCert(now - base::TimeDelta::FromMinutes(10),
                     now + base::TimeDelta::FromMinutes(5));
  ASSERT_TRUE(cert_start_10min_expire_5min);

  std::unique_ptr<net::FakeClientCertIdentity> cert_start_5min_expire_10min =
      CreateFakeCert(now - base::TimeDelta::FromMinutes(5),
                     now + base::TimeDelta::FromMinutes(10));
  ASSERT_TRUE(cert_start_5min_expire_10min);

  // No certificate.
  token_validator_->ExpectContinueWithCertificate(nullptr);
  token_validator_->SelectCertificates(net::ClientCertIdentityList());
  {
    // One invalid certificate.
    net::ClientCertIdentityList client_certs;
    client_certs.push_back(cert_expired_5_minutes_ago->Copy());
    token_validator_->ExpectContinueWithCertificate(nullptr);
    token_validator_->SelectCertificates(std::move(client_certs));
  }
  {
    // One valid certificate.
    net::ClientCertIdentityList client_certs;
    client_certs.push_back(cert_start_5min_expire_5min->Copy());
    token_validator_->ExpectContinueWithCertificate(
        cert_start_5min_expire_5min.get());
    token_validator_->SelectCertificates(std::move(client_certs));
  }
  {
    // One valid one invalid.
    net::ClientCertIdentityList client_certs;
    client_certs.push_back(cert_expired_5_minutes_ago->Copy());
    client_certs.push_back(cert_start_5min_expire_5min->Copy());
    token_validator_->ExpectContinueWithCertificate(
        cert_start_5min_expire_5min.get());
    token_validator_->SelectCertificates(std::move(client_certs));
  }
  {
    // Two valid certs. Choose latest created.
    net::ClientCertIdentityList client_certs;
    client_certs.push_back(cert_start_10min_expire_5min->Copy());
    client_certs.push_back(cert_start_5min_expire_5min->Copy());
    token_validator_->ExpectContinueWithCertificate(
        cert_start_5min_expire_5min.get());
    token_validator_->SelectCertificates(std::move(client_certs));
  }
  {
    // Two valid certs. Choose latest expires.
    net::ClientCertIdentityList client_certs;
    client_certs.push_back(cert_start_5min_expire_5min->Copy());
    client_certs.push_back(cert_start_5min_expire_10min->Copy());
    token_validator_->ExpectContinueWithCertificate(
        cert_start_5min_expire_10min.get());
    token_validator_->SelectCertificates(std::move(client_certs));
  }
  {
    // Pick the best given all certificates.
    net::ClientCertIdentityList client_certs;
    client_certs.push_back(cert_expired_5_minutes_ago->Copy());
    client_certs.push_back(cert_start_5min_expire_5min->Copy());
    client_certs.push_back(cert_start_5min_expire_10min->Copy());
    client_certs.push_back(cert_start_10min_expire_5min->Copy());
    token_validator_->ExpectContinueWithCertificate(
        cert_start_5min_expire_10min.get());
    token_validator_->SelectCertificates(std::move(client_certs));
  }
}

}  // namespace remoting
