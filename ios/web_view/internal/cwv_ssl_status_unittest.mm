// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_ssl_status_internal.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

class CWVSSLStatusTest : public PlatformTest {
 protected:
  CWVSSLStatusTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CWVSSLStatusTest);
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

}  // namespace ios_web_view
