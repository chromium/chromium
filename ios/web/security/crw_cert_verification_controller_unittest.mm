// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/security/crw_cert_verification_controller.h"

#include "base/mac/foundation_util.h"
#import "base/test/ios/wait_util.h"
#include "ios/web/public/test/web_test.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/security/wk_web_view_security_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_ios_and_mac.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {

namespace {
// Generated cert filename.
const char kCertFileName[] = "ok_cert.pem";
// Test hostname for cert verification.
NSString* const kHostName = @"www.example.com";
}  // namespace

// Test fixture to test CRWCertVerificationController class.
class CRWCertVerificationControllerTest : public web::WebTest {
 protected:
  void SetUp() override {
    web::WebTest::SetUp();

    controller_ = [[CRWCertVerificationController alloc]
        initWithBrowserState:GetBrowserState()];
    cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), kCertFileName);
    ASSERT_TRUE(cert_);

    base::ScopedCFTypeRef<CFMutableArrayRef> chain(
        net::x509_util::CreateSecCertificateArrayForX509Certificate(
            cert_.get()));
    ASSERT_TRUE(chain);
    valid_trust_ = web::CreateServerTrustFromChain(
        base::mac::CFToNSCast(chain.get()), kHostName);
    web::EnsureFutureTrustEvaluationSucceeds(valid_trust_.get());
    invalid_trust_ = web::CreateServerTrustFromChain(
        base::mac::CFToNSCast(chain.get()), kHostName);
  }

  // Synchronously returns result of
  // decideLoadPolicyForTrust:host:completionHandler: call.
  void DecidePolicy(const base::ScopedCFTypeRef<SecTrustRef>& trust,
                    NSString* host,
                    web::CertAcceptPolicy* policy,
                    net::CertStatus* status) {
    __block bool completion_handler_called = false;
    [controller_
        decideLoadPolicyForTrust:trust
                            host:host
               completionHandler:^(web::CertAcceptPolicy callback_policy,
                                   net::CertStatus callback_status) {
                 *policy = callback_policy;
                 *status = callback_status;
                 completion_handler_called = true;
               }];
    base::test::ios::WaitUntilCondition(
        ^{
          return completion_handler_called;
        },
        true, base::TimeDelta());
  }

  // Synchronously returns result of
  // querySSLStatusForTrust:host:completionHandler: call.
  void QueryStatus(const base::ScopedCFTypeRef<SecTrustRef>& trust,
                   NSString* host,
                   SecurityStyle* style,
                   net::CertStatus* status) {
    __block bool completion_handler_called = false;
    [controller_ querySSLStatusForTrust:trust
                                   host:host
                      completionHandler:^(SecurityStyle callback_style,
                                          net::CertStatus callback_status) {
                        *style = callback_style;
                        *status = callback_status;
                        completion_handler_called = true;
                      }];
    base::test::ios::WaitUntilCondition(
        ^{
          return completion_handler_called;
        },
        true, base::TimeDelta());
  }

  scoped_refptr<net::X509Certificate> cert_;
  base::ScopedCFTypeRef<SecTrustRef> valid_trust_;
  base::ScopedCFTypeRef<SecTrustRef> invalid_trust_;
  CRWCertVerificationController* controller_;
};

// Tests cert policy with a valid trust.
TEST_F(CRWCertVerificationControllerTest, PolicyForValidTrust) {
  web::CertAcceptPolicy policy = CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR;
  net::CertStatus status;
  DecidePolicy(valid_trust_, kHostName, &policy, &status);
  EXPECT_EQ(CERT_ACCEPT_POLICY_ALLOW, policy);
  EXPECT_FALSE(status);
}

// Tests cert policy with an invalid trust not accepted by user.
TEST_F(CRWCertVerificationControllerTest, PolicyForInvalidTrust) {
  web::CertAcceptPolicy policy = CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR;
  net::CertStatus status;
  DecidePolicy(invalid_trust_, kHostName, &policy, &status);
  EXPECT_EQ(CERT_ACCEPT_POLICY_RECOVERABLE_ERROR_UNDECIDED_BY_USER, policy);
  EXPECT_TRUE(net::CERT_STATUS_AUTHORITY_INVALID & status);
}

// Tests cert policy with an invalid trust accepted by user.
TEST_F(CRWCertVerificationControllerTest, PolicyForInvalidTrustAcceptedByUser) {
  [controller_ allowCert:cert_.get()
                 forHost:kHostName
                  status:net::CERT_STATUS_ALL_ERRORS];
  web::CertAcceptPolicy policy = CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR;
  net::CertStatus status;
  DecidePolicy(invalid_trust_, kHostName, &policy, &status);
  EXPECT_EQ(CERT_ACCEPT_POLICY_RECOVERABLE_ERROR_ACCEPTED_BY_USER, policy);
  EXPECT_TRUE(net::CERT_STATUS_AUTHORITY_INVALID & status);
}

// Tests that allowCert:forHost:status: strips all intermediate certs.
TEST_F(CRWCertVerificationControllerTest, AllowCertIgnoresIntermediateCerts) {
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  intermediates.push_back(bssl::UpRef(cert_->cert_buffer()));
  scoped_refptr<net::X509Certificate> cert(
      net::X509Certificate::CreateFromBuffer(bssl::UpRef(cert_->cert_buffer()),
                                             std::move(intermediates)));
  ASSERT_TRUE(cert);
  [controller_ allowCert:cert.get()
                 forHost:kHostName
                  status:net::CERT_STATUS_ALL_ERRORS];
  web::CertAcceptPolicy policy = CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR;
  net::CertStatus status;
  DecidePolicy(invalid_trust_, kHostName, &policy, &status);
  EXPECT_EQ(CERT_ACCEPT_POLICY_RECOVERABLE_ERROR_ACCEPTED_BY_USER, policy);
  EXPECT_TRUE(net::CERT_STATUS_AUTHORITY_INVALID & status);
}

// Tests cert policy with null trust.
TEST_F(CRWCertVerificationControllerTest, PolicyForNullTrust) {
  web::CertAcceptPolicy policy = CERT_ACCEPT_POLICY_ALLOW;
  net::CertStatus status;
  base::ScopedCFTypeRef<SecTrustRef> null_trust;
  DecidePolicy(null_trust, kHostName, &policy, &status);
  EXPECT_EQ(CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR, policy);
  EXPECT_EQ(net::CERT_STATUS_INVALID, status);
}

// Tests cert policy with invalid trust and null host.
TEST_F(CRWCertVerificationControllerTest, PolicyForNullHost) {
  web::CertAcceptPolicy policy = CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR;
  net::CertStatus status;
  DecidePolicy(invalid_trust_, nil, &policy, &status);
  EXPECT_EQ(CERT_ACCEPT_POLICY_RECOVERABLE_ERROR_UNDECIDED_BY_USER, policy);
  EXPECT_TRUE(net::CERT_STATUS_AUTHORITY_INVALID & status);
}

// Tests SSL status with valid trust.
TEST_F(CRWCertVerificationControllerTest, SSLStatusForValidTrust) {
  SecurityStyle style = SECURITY_STYLE_UNKNOWN;
  net::CertStatus status = net::CERT_STATUS_ALL_ERRORS;

  QueryStatus(valid_trust_, kHostName, &style, &status);
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED, style);
  EXPECT_FALSE(status);
}

// Tests SSL status with invalid host.
TEST_F(CRWCertVerificationControllerTest, SSLStatusForInvalidTrust) {
  SecurityStyle style = SECURITY_STYLE_UNKNOWN;
  net::CertStatus status = net::CERT_STATUS_ALL_ERRORS;

  QueryStatus(invalid_trust_, kHostName, &style, &status);
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN, style);
  EXPECT_TRUE(net::CERT_STATUS_AUTHORITY_INVALID & status);
}

}  // namespace web
