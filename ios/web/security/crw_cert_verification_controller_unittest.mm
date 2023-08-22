// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/security/crw_cert_verification_controller.h"

#import "base/apple/bridging.h"
#import "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/test_timeouts.h"
#import "ios/web/public/session/session_certificate_policy_cache.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/security/wk_web_view_security_util.h"
#import "net/cert/x509_certificate.h"
#import "net/cert/x509_util.h"
#import "net/cert/x509_util_apple.h"
#import "net/test/cert_test_util.h"
#import "net/test/test_data_directory.h"

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

    base::apple::ScopedCFTypeRef<CFMutableArrayRef> chain(
        net::x509_util::CreateSecCertificateArrayForX509Certificate(
            cert_.get()));
    ASSERT_TRUE(chain);
    valid_trust_ = web::CreateServerTrustFromChain(
        base::apple::CFToNSPtrCast(chain.get()), kHostName);
    web::EnsureFutureTrustEvaluationSucceeds(valid_trust_.get());
    invalid_trust_ = web::CreateServerTrustFromChain(
        base::apple::CFToNSPtrCast(chain.get()), kHostName);
  }

  // Synchronously returns result of
  // decideLoadPolicyForTrust:host:completionHandler: call.
  void DecidePolicy(const base::apple::ScopedCFTypeRef<SecTrustRef>& trust,
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
    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        TestTimeouts::action_timeout(), true, ^{
          return completion_handler_called;
        }));
  }

  // Synchronously returns result of
  // querySSLStatusForTrust:host:completionHandler: call.
  void QueryStatus(const base::apple::ScopedCFTypeRef<SecTrustRef>& trust,
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
    ASSERT_TRUE(base::test::ios::WaitUntilConditionOrTimeout(
        TestTimeouts::action_timeout(), true, ^{
          return completion_handler_called;
        }));
  }

  scoped_refptr<net::X509Certificate> cert_;
  base::apple::ScopedCFTypeRef<SecTrustRef> valid_trust_;
  base::apple::ScopedCFTypeRef<SecTrustRef> invalid_trust_;
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
  auto test_web_state =
      std::make_unique<web::FakeWebStateWithPolicyCache>(GetBrowserState());
  test_web_state->GetSessionCertificatePolicyCache()
      ->RegisterAllowedCertificate(cert_.get(),
                                   base::SysNSStringToUTF8(kHostName),
                                   net::CERT_STATUS_ALL_ERRORS);
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
  base::apple::ScopedCFTypeRef<SecTrustRef> null_trust;
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
