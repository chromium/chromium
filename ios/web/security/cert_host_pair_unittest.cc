// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/security/cert_host_pair.h"

#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/platform_test.h"

namespace web {

namespace {

// Test cert filenames.
const char kCertFileName1[] = "ok_cert.pem";
const char kCertFileName2[] = "expired_cert.pem";

// Test hostnames.
const char kHostName1[] = "www.example.com";
const char kHostName2[] = "www.chromium.test";

// Loads cert with the given |file_name|.
scoped_refptr<net::X509Certificate> GetCert(const std::string& file_name) {
  return net::ImportCertFromFile(net::GetTestCertsDirectory(), file_name);
}

}  // namespace

// Test fixture to test CertHostPair struct.
typedef PlatformTest CertHostPairTest;

// Tests constructions.
TEST_F(CertHostPairTest, Construction) {
  scoped_refptr<net::X509Certificate> cert = GetCert(kCertFileName1);
  ASSERT_TRUE(cert);
  CertHostPair pair(cert, kHostName1);
  EXPECT_EQ(cert, pair.cert_);
  EXPECT_EQ(std::string(kHostName1), pair.host_);
}

// Tests comparison with different certs and hosts.
TEST_F(CertHostPairTest, ComparisonWithDifferentCertsAndHosts) {
  scoped_refptr<net::X509Certificate> cert1 = GetCert(kCertFileName1);
  ASSERT_TRUE(cert1);
  scoped_refptr<net::X509Certificate> cert2 = GetCert(kCertFileName2);
  ASSERT_TRUE(cert2);
  CertHostPair pair1(cert1, kHostName1);
  CertHostPair pair2(cert2, kHostName2);

  EXPECT_FALSE(pair1 < pair1);
  EXPECT_FALSE(pair2 < pair2);
  EXPECT_TRUE((pair1 < pair2 && !(pair2 < pair1)) ||
              (pair2 < pair1 && !(pair1 < pair2)));
}

// Tests comparison with same cert.
TEST_F(CertHostPairTest, ComparisonWithSameCert) {
  scoped_refptr<net::X509Certificate> cert1 = GetCert(kCertFileName1);
  ASSERT_TRUE(cert1);
  scoped_refptr<net::X509Certificate> cert2 = GetCert(kCertFileName1);
  ASSERT_TRUE(cert2);
  CertHostPair pair1(cert1, kHostName1);
  CertHostPair pair2(cert2, kHostName2);

  EXPECT_FALSE(pair1 < pair1);
  EXPECT_FALSE(pair2 < pair2);
  EXPECT_TRUE((pair1 < pair2 && !(pair2 < pair1)) ||
              (pair2 < pair1 && !(pair1 < pair2)));
}

// Tests comparison with same host.
TEST_F(CertHostPairTest, ComparisonWithSameHost) {
  scoped_refptr<net::X509Certificate> cert1 = GetCert(kCertFileName1);
  ASSERT_TRUE(cert1);
  scoped_refptr<net::X509Certificate> cert2 = GetCert(kCertFileName2);
  ASSERT_TRUE(cert2);
  CertHostPair pair1(cert1, kHostName1);
  CertHostPair pair2(cert2, kHostName1);

  EXPECT_FALSE(pair1 < pair1);
  EXPECT_FALSE(pair2 < pair2);
  EXPECT_TRUE((pair1 < pair2 && !(pair2 < pair1)) ||
              (pair2 < pair1 && !(pair1 < pair2)));
}

// Tests comparison with same cert and host.
TEST_F(CertHostPairTest, ComparisonWithSameCertAndHost) {
  scoped_refptr<net::X509Certificate> cert1 = GetCert(kCertFileName1);
  ASSERT_TRUE(cert1);
  scoped_refptr<net::X509Certificate> cert2 = GetCert(kCertFileName1);
  ASSERT_TRUE(cert2);
  CertHostPair pair1(cert1, kHostName1);
  CertHostPair pair2(cert2, kHostName1);

  EXPECT_FALSE(pair1 < pair1);
  EXPECT_FALSE(pair2 < pair2);
  EXPECT_FALSE(pair1 < pair2);
  EXPECT_FALSE(pair2 < pair1);
}

}  // namespace web
