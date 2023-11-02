// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_errors.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

TEST(NetErrorsTest, IsCertificateError) {
  // Positive tests.
  EXPECT_TRUE(IsCertificateError(ERR_CERT_AUTHORITY_INVALID));
  EXPECT_TRUE(IsCertificateError(ERR_CERT_COMMON_NAME_INVALID));
  EXPECT_TRUE(IsCertificateError(ERR_CERT_CONTAINS_ERRORS));
  EXPECT_TRUE(IsCertificateError(ERR_CERT_DATE_INVALID));
  EXPECT_TRUE(IsCertificateError(ERR_CERTIFICATE_TRANSPARENCY_REQUIRED));
  EXPECT_TRUE(IsCertificateError(ERR_CERT_INVALID));
  EXPECT_TRUE(IsCertificateError(ERR_CERT_NAME_CONSTRAINT_VIOLATION));
  EXPECT_TRUE(IsCertificateError(ERR_CERT_NON_UNIQUE_NAME));
  EXPECT_TRUE(IsCertificateError(ERR_CERT_NO_REVOCATION_MECHANISM));
  EXPECT_TRUE(IsCertificateError(ERR_CERT_REVOKED));
  EXPECT_TRUE(IsCertificateError(ERR_CERT_SYMANTEC_LEGACY));
  EXPECT_TRUE(IsCertificateError(ERR_CERT_UNABLE_TO_CHECK_REVOCATION));
  EXPECT_TRUE(IsCertificateError(ERR_CERT_VALIDITY_TOO_LONG));
  EXPECT_TRUE(IsCertificateError(ERR_CERT_WEAK_KEY));
  EXPECT_TRUE(IsCertificateError(ERR_CERT_WEAK_SIGNATURE_ALGORITHM));
  EXPECT_TRUE(IsCertificateError(ERR_SSL_PINNED_KEY_NOT_IN_CERT_CHAIN));
  EXPECT_TRUE(IsCertificateError(ERR_CERT_KNOWN_INTERCEPTION_BLOCKED));

  // Negative tests.
  EXPECT_FALSE(IsCertificateError(ERR_SSL_PROTOCOL_ERROR));
  EXPECT_FALSE(IsCertificateError(ERR_SSL_KEY_USAGE_INCOMPATIBLE));
  EXPECT_FALSE(
      IsCertificateError(ERR_SSL_CLIENT_AUTH_PRIVATE_KEY_ACCESS_DENIED));
  EXPECT_FALSE(IsCertificateError(ERR_QUIC_CERT_ROOT_NOT_KNOWN));
  EXPECT_FALSE(IsCertificateError(ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY));
  EXPECT_FALSE(IsCertificateError(ERR_FAILED));
  EXPECT_FALSE(IsCertificateError(OK));

  // Trigger a failure whenever ERR_CERT_END is changed, forcing developers to
  // update this test.
  EXPECT_EQ(ERR_CERT_END, -219)
      << "It looks like you added a new certificate error code ("
      << ErrorToString(ERR_CERT_END + 1)
      << ").\n"
         "\n"
         "Because this code is between ERR_CERT_BEGIN and ERR_CERT_END, it "
         "will be matched by net::IsCertificateError().\n"
         "\n"
         " (1) Please add a new test case to "
         "NetErrorsTest.IsCertificateError()."
         "\n"
         " (2) Review the existing consumers of IsCertificateError(). "
         "//content for instance has specialized handling of "
         "IsCertificateError() that may need to be updated.";
}

TEST(NetErrorsTest, IsClientCertificateError) {
  // Positive tests.
  EXPECT_TRUE(IsClientCertificateError(ERR_BAD_SSL_CLIENT_AUTH_CERT));
  EXPECT_TRUE(
      IsClientCertificateError(ERR_SSL_CLIENT_AUTH_PRIVATE_KEY_ACCESS_DENIED));
  EXPECT_TRUE(
      IsClientCertificateError(ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY));
  EXPECT_TRUE(IsClientCertificateError(ERR_SSL_CLIENT_AUTH_SIGNATURE_FAILED));
  EXPECT_TRUE(
      IsClientCertificateError(ERR_SSL_CLIENT_AUTH_NO_COMMON_ALGORITHMS));

  // Negative tests.
  EXPECT_FALSE(IsClientCertificateError(ERR_CERT_REVOKED));
  EXPECT_FALSE(IsClientCertificateError(ERR_SSL_PROTOCOL_ERROR));
  EXPECT_FALSE(IsClientCertificateError(ERR_CERT_WEAK_KEY));
}

}  // namespace

}  // namespace net
