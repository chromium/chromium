// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_x509_certificate_internal.h"

#include "base/strings/sys_string_conversions.h"
#include "net/cert/x509_certificate.h"
#include "net/test/test_certificate_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace ios_web_view {

class CWVX509CertificateTest : public PlatformTest {
 protected:
  CWVX509CertificateTest() {}
};

// Test CWVX509Certificate initialization.
TEST_F(CWVX509CertificateTest, Initialization) {
  scoped_refptr<net::X509Certificate> internal_cert(
      net::X509Certificate::CreateFromBytes(webkit_der));

  CWVX509Certificate* cwv_cert =
      [[CWVX509Certificate alloc] initWithInternalCertificate:internal_cert];
  EXPECT_EQ(internal_cert->issuer().GetDisplayName(),
            base::SysNSStringToUTF8([cwv_cert issuerDisplayName]));
  EXPECT_NSEQ(internal_cert->valid_expiry().ToNSDate(), [cwv_cert validExpiry]);
}

}  // namespace ios_web_view
