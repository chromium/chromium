// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/security/wk_web_view_security_util.h"

#import <Foundation/Foundation.h>
#include <Security/Security.h>

#include <memory>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_ios.h"
#include "net/ssl/ssl_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
namespace {
// Subject for testing self-signed certificate.
const char kTestSubject[] = "self-signed";
// Hostname for testing SecTrustRef objects.
NSString* const kTestHost = @"www.example.com";

// Returns an autoreleased certificate chain for testing. Chain will contain a
// single self-signed cert with |subject| as a subject.
NSArray* MakeTestCertChain(const std::string& subject) {
  std::unique_ptr<crypto::RSAPrivateKey> private_key;
  std::string der_cert;
  net::x509_util::CreateKeyAndSelfSignedCert(
      "CN=" + subject, 1, base::Time::Now(),
      base::Time::Now() + base::TimeDelta::FromDays(1), &private_key,
      &der_cert);

  base::ScopedCFTypeRef<SecCertificateRef> cert(
      net::x509_util::CreateSecCertificateFromBytes(
          reinterpret_cast<const uint8_t*>(der_cert.data()), der_cert.size()));
  if (!cert)
    return nullptr;
  return @[ (__bridge id)cert.get() ];
}

// Returns an autoreleased dictionary, which represents NSError's user info for
// testing.
NSDictionary* MakeTestSSLCertErrorUserInfo() {
  return @{
    web::kNSErrorPeerCertificateChainKey : MakeTestCertChain(kTestSubject),
  };
}

// Returns SecTrustRef object for testing.
base::ScopedCFTypeRef<SecTrustRef> CreateTestTrust(NSArray* cert_chain) {
  base::ScopedCFTypeRef<SecPolicyRef> policy(SecPolicyCreateBasicX509());
  SecTrustRef trust = nullptr;
  SecTrustCreateWithCertificates(base::mac::NSToCFCast(cert_chain), policy,
                                 &trust);
  return base::ScopedCFTypeRef<SecTrustRef>(trust);
}

}  // namespace

// Test class for wk_web_view_security_util functions.
typedef PlatformTest WKWebViewSecurityUtilTest;

// Tests CreateCertFromChain with self-signed cert.
TEST_F(WKWebViewSecurityUtilTest, CreationCertFromChain) {
  scoped_refptr<net::X509Certificate> cert =
      CreateCertFromChain(MakeTestCertChain(kTestSubject));
  ASSERT_TRUE(cert);
  EXPECT_TRUE(cert->subject().GetDisplayName() == kTestSubject);
}

// Tests CreateCertFromChain with nil chain.
TEST_F(WKWebViewSecurityUtilTest, CreationCertFromNilChain) {
  EXPECT_FALSE(CreateCertFromChain(nil));
}

// Tests CreateCertFromChain with empty chain.
TEST_F(WKWebViewSecurityUtilTest, CreationCertFromEmptyChain) {
  EXPECT_FALSE(CreateCertFromChain(@[]));
}

// Tests MakeTrustValid with self-signed cert.
TEST_F(WKWebViewSecurityUtilTest, MakingTrustValid) {
  // Create invalid trust object.
  base::ScopedCFTypeRef<SecTrustRef> trust =
      CreateTestTrust(MakeTestCertChain(kTestSubject));

  SecTrustResultType result = kSecTrustResultInvalid;
  SecTrustEvaluate(trust, &result);
  EXPECT_EQ(kSecTrustResultRecoverableTrustFailure, result);

  // Make sure that trust becomes valid after
  // |EnsureFutureTrustEvaluationSucceeds| call.
  EnsureFutureTrustEvaluationSucceeds(trust);
  SecTrustEvaluate(trust, &result);
  EXPECT_EQ(kSecTrustResultProceed, result);
}

// Tests CreateCertFromTrust.
TEST_F(WKWebViewSecurityUtilTest, CreationCertFromTrust) {
  base::ScopedCFTypeRef<SecTrustRef> trust =
      CreateTestTrust(MakeTestCertChain(kTestSubject));
  scoped_refptr<net::X509Certificate> cert = CreateCertFromTrust(trust);
  ASSERT_TRUE(cert);
  EXPECT_TRUE(cert->subject().GetDisplayName() == kTestSubject);
}

// Tests CreateCertFromTrust with nil trust.
TEST_F(WKWebViewSecurityUtilTest, CreationCertFromNilTrust) {
  EXPECT_FALSE(CreateCertFromTrust(nil));
}

// Tests CreateServerTrustFromChain with valid input.
TEST_F(WKWebViewSecurityUtilTest, CreationServerTrust) {
  // Create server trust.
  NSArray* chain = MakeTestCertChain(kTestSubject);
  base::ScopedCFTypeRef<SecTrustRef> server_trust(
      CreateServerTrustFromChain(chain, kTestHost));
  EXPECT_TRUE(server_trust);

  // Verify chain.
  EXPECT_EQ(static_cast<CFIndex>(chain.count),
            SecTrustGetCertificateCount(server_trust));
  [chain enumerateObjectsUsingBlock:^(id expected_cert, NSUInteger i, BOOL*) {
    id actual_cert = static_cast<id>(SecTrustGetCertificateAtIndex(
        server_trust.get(), static_cast<CFIndex>(i)));
    EXPECT_EQ(expected_cert, actual_cert);
  }];

  // Verify policies.
  CFArrayRef policies = nullptr;
  EXPECT_EQ(errSecSuccess, SecTrustCopyPolicies(server_trust.get(), &policies));
  EXPECT_EQ(1, CFArrayGetCount(policies));
  SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(policies, 0);
  base::ScopedCFTypeRef<CFDictionaryRef> properties(
      SecPolicyCopyProperties(policy));
  NSString* name = static_cast<NSString*>(
      CFDictionaryGetValue(properties.get(), kSecPolicyName));
  EXPECT_NSEQ(kTestHost, name);
  CFRelease(policies);
}

// Tests CreateServerTrustFromChain with nil chain.
TEST_F(WKWebViewSecurityUtilTest, CreationServerTrustFromNilChain) {
  EXPECT_FALSE(CreateServerTrustFromChain(nil, kTestHost));
}

// Tests CreateServerTrustFromChain with empty chain.
TEST_F(WKWebViewSecurityUtilTest, CreationServerTrustFromEmptyChain) {
  EXPECT_FALSE(CreateServerTrustFromChain(@[], kTestHost));
}

// Tests that IsWKWebViewSSLCertError returns YES for NSError with
// NSURLErrorDomain domain, NSURLErrorSecureConnectionFailed error code and
// certificate chain.
TEST_F(WKWebViewSecurityUtilTest, CheckSecureConnectionFailedWithCertError) {
  EXPECT_TRUE(IsWKWebViewSSLCertError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorSecureConnectionFailed
             userInfo:MakeTestSSLCertErrorUserInfo()]));
}

// Tests that IsWKWebViewSSLCertError returns NO for NSError with
// NSURLErrorDomain domain, NSURLErrorSecureConnectionFailed error code and no
// certificate chain.
TEST_F(WKWebViewSecurityUtilTest, CheckSecureConnectionFailedWithoutCertError) {
  EXPECT_FALSE(IsWKWebViewSSLCertError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorSecureConnectionFailed
             userInfo:nil]));
}

// Tests that IsWKWebViewSSLCertError returns YES for NSError with
// NSURLErrorDomain domain and certificates error codes.
TEST_F(WKWebViewSecurityUtilTest, CheckCertificateSSLError) {
  EXPECT_TRUE(IsWKWebViewSSLCertError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorServerCertificateHasBadDate
             userInfo:nil]));
  EXPECT_TRUE(IsWKWebViewSSLCertError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorServerCertificateUntrusted
             userInfo:nil]));
  EXPECT_TRUE(IsWKWebViewSSLCertError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorServerCertificateHasUnknownRoot
             userInfo:nil]));
  EXPECT_TRUE(IsWKWebViewSSLCertError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorServerCertificateNotYetValid
             userInfo:nil]));
}

// Tests that IsWKWebViewSSLCertError returns NO for NSError with
// NSURLErrorDomain domain and non cert SSL error codes.
TEST_F(WKWebViewSecurityUtilTest, CheckNonCertificateSSLError) {
  EXPECT_FALSE(IsWKWebViewSSLCertError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorClientCertificateRejected
             userInfo:nil]));
  EXPECT_FALSE(IsWKWebViewSSLCertError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorClientCertificateRequired
             userInfo:nil]));
}

// Tests that IsWKWebViewSSLCertError returns NO for NSError with
// NSURLErrorDomain domain and NSURLErrorDataLengthExceedsMaximum error code.
TEST_F(WKWebViewSecurityUtilTest, CheckDataLengthExceedsMaximumError) {
  EXPECT_FALSE(IsWKWebViewSSLCertError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorDataLengthExceedsMaximum
             userInfo:nil]));
}

// Tests that IsWKWebViewSSLCertError returns NO for NSError with
// NSURLErrorDomain domain and NSURLErrorCannotLoadFromNetwork error code.
TEST_F(WKWebViewSecurityUtilTest, CheckCannotLoadFromNetworkError) {
  EXPECT_FALSE(IsWKWebViewSSLCertError([NSError
      errorWithDomain:NSURLErrorDomain
                 code:NSURLErrorCannotLoadFromNetwork
             userInfo:nil]));
}

// Tests GetSSLInfoFromWKWebViewSSLCertError with NSError and self-signed cert.
TEST_F(WKWebViewSecurityUtilTest, SSLInfoFromErrorWithCert) {
  NSError* unknownCertError =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorServerCertificateHasUnknownRoot
                      userInfo:MakeTestSSLCertErrorUserInfo()];

  net::SSLInfo info;
  GetSSLInfoFromWKWebViewSSLCertError(unknownCertError, &info);
  EXPECT_TRUE(info.is_valid());
  EXPECT_EQ(net::CERT_STATUS_INVALID, info.cert_status);
  EXPECT_TRUE(info.cert->subject().GetDisplayName() == kTestSubject);
  EXPECT_TRUE(info.unverified_cert->subject().GetDisplayName() == kTestSubject);
}

// Tests GetSSLInfoFromWKWebViewSSLCertError with NSError and empty chain.
TEST_F(WKWebViewSecurityUtilTest, SSLInfoFromErrorWithoutCert) {
  NSError* noCertChainError =
      [NSError errorWithDomain:NSURLErrorDomain
                          code:NSURLErrorServerCertificateHasBadDate
                      userInfo:nil];

  net::SSLInfo info;
  GetSSLInfoFromWKWebViewSSLCertError(noCertChainError, &info);
  EXPECT_FALSE(info.is_valid());
  // If cert can not be parsed status should always be CERT_STATUS_INVALID,
  // regardless of iOS error code. This is consistent with other platforms.
  EXPECT_EQ(net::CERT_STATUS_INVALID, info.cert_status);
  EXPECT_FALSE(info.cert);
  EXPECT_FALSE(info.unverified_cert);
}

// Tests GetSecurityStyleFromTrustResult with bad SecTrustResultType result.
TEST_F(WKWebViewSecurityUtilTest, GetSecurityStyleFromBadResult) {
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN,
            GetSecurityStyleFromTrustResult(kSecTrustResultDeny));
  EXPECT_EQ(
      SECURITY_STYLE_AUTHENTICATION_BROKEN,
      GetSecurityStyleFromTrustResult(kSecTrustResultRecoverableTrustFailure));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN,
            GetSecurityStyleFromTrustResult(kSecTrustResultFatalTrustFailure));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATION_BROKEN,
            GetSecurityStyleFromTrustResult(kSecTrustResultOtherError));
}

// Tests GetSecurityStyleFromTrustResult with good SecTrustResultType result.
TEST_F(WKWebViewSecurityUtilTest, GetSecurityStyleFromGoodResult) {
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED,
            GetSecurityStyleFromTrustResult(kSecTrustResultProceed));
  EXPECT_EQ(SECURITY_STYLE_AUTHENTICATED,
            GetSecurityStyleFromTrustResult(kSecTrustResultUnspecified));
}

// Tests GetSecurityStyleFromTrustResult with invalid SecTrustResultType result.
TEST_F(WKWebViewSecurityUtilTest, GetSecurityStyleFromInvalidResult) {
  EXPECT_EQ(SECURITY_STYLE_UNKNOWN,
            GetSecurityStyleFromTrustResult(kSecTrustResultInvalid));
}

}  // namespace web
