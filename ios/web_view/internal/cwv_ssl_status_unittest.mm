// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_ssl_status_internal.h"

#include "net/test/test_certificate_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace ios_web_view {

using ::testing::IsNull;
using ::testing::NotNull;

class CWVSSLStatusTest : public PlatformTest {
 public:
  CWVSSLStatusTest(const CWVSSLStatusTest&) = delete;
  CWVSSLStatusTest& operator=(const CWVSSLStatusTest&) = delete;

 protected:
  CWVSSLStatusTest() {}
};

TEST_F(CWVSSLStatusTest, SecurityStyle) {
  web::SSLStatus internal_status;
  CWVSSLStatus* cwv_status;

  internal_status.security_style = web::SECURITY_STYLE_AUTHENTICATED;
  cwv_status = [[CWVSSLStatus alloc] initWithInternalStatus:internal_status];
  EXPECT_EQ(CWVSecurityStyleAuthenticated, cwv_status.securityStyle);

  internal_status.security_style = web::SECURITY_STYLE_AUTHENTICATION_BROKEN;
  cwv_status = [[CWVSSLStatus alloc] initWithInternalStatus:internal_status];
  EXPECT_EQ(CWVSecurityStyleAuthenticationBroken, cwv_status.securityStyle);
}

TEST_F(CWVSSLStatusTest, HasOnlySecureContent) {
  web::SSLStatus internal_status;
  CWVSSLStatus* cwv_status;

  // The entire page is in HTTPS.
  internal_status.security_style = web::SECURITY_STYLE_AUTHENTICATED;
  internal_status.content_status = web::SSLStatus::NORMAL_CONTENT;
  cwv_status = [[CWVSSLStatus alloc] initWithInternalStatus:internal_status];
  EXPECT_TRUE(cwv_status.hasOnlySecureContent);

  // The page is in HTTPS but it contains "displayed" HTTP resources.
  internal_status.security_style = web::SECURITY_STYLE_AUTHENTICATED;
  internal_status.content_status = web::SSLStatus::DISPLAYED_INSECURE_CONTENT;
  cwv_status = [[CWVSSLStatus alloc] initWithInternalStatus:internal_status];
  EXPECT_FALSE(cwv_status.hasOnlySecureContent);

  // The page is in HTTP.
  internal_status.security_style = web::SECURITY_STYLE_UNAUTHENTICATED;
  internal_status.content_status = web::SSLStatus::NORMAL_CONTENT;
  cwv_status = [[CWVSSLStatus alloc] initWithInternalStatus:internal_status];
  EXPECT_FALSE(cwv_status.hasOnlySecureContent);
}

TEST_F(CWVSSLStatusTest, CertStatusZero) {
  web::SSLStatus internal_status;
  internal_status.cert_status = 0;

  CWVSSLStatus* cwv_status =
      [[CWVSSLStatus alloc] initWithInternalStatus:internal_status];
  EXPECT_EQ(0, cwv_status.certStatus);
}

TEST_F(CWVSSLStatusTest, CertStatusHasSingleError) {
  web::SSLStatus internal_status;
  internal_status.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;

  CWVSSLStatus* cwv_status =
      [[CWVSSLStatus alloc] initWithInternalStatus:internal_status];
  EXPECT_EQ(CWVCertStatusCommonNameInvalid, cwv_status.certStatus);
}

TEST_F(CWVSSLStatusTest, CertStatusHasMultipleErrors) {
  web::SSLStatus internal_status;
  internal_status.cert_status =
      net::CERT_STATUS_COMMON_NAME_INVALID | net::CERT_STATUS_DATE_INVALID;

  CWVSSLStatus* cwv_status =
      [[CWVSSLStatus alloc] initWithInternalStatus:internal_status];
  EXPECT_EQ(CWVCertStatusCommonNameInvalid | CWVCertStatusDateInvalid,
            cwv_status.certStatus);
}

TEST_F(CWVSSLStatusTest, CertStatusHasNonErrorStatus) {
  web::SSLStatus internal_status;
  internal_status.cert_status = net::CERT_STATUS_IS_EV;  // Non-error status.

  CWVSSLStatus* cwv_status =
      [[CWVSSLStatus alloc] initWithInternalStatus:internal_status];
  // CWVCertStatus does not provide non-error statuses.
  EXPECT_EQ(0, cwv_status.certStatus);
}

TEST_F(CWVSSLStatusTest, CertificateNullForSSLStatusWithoutCertificate) {
  web::SSLStatus internal_status;

  CWVSSLStatus* cwv_status =
      [[CWVSSLStatus alloc] initWithInternalStatus:internal_status];
  EXPECT_THAT(cwv_status.certificate, IsNull());
}

TEST_F(CWVSSLStatusTest, CertificateNotNullForSSLStatusWithCertificate) {
  web::SSLStatus internal_status;
  internal_status.certificate =
      net::X509Certificate::CreateFromBytes(webkit_der);

  CWVSSLStatus* cwv_status =
      [[CWVSSLStatus alloc] initWithInternalStatus:internal_status];
  EXPECT_THAT(cwv_status.certificate, NotNull());
}

}  // namespace ios_web_view
