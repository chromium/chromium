// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/security/certificate_policy_cache.h"

#include <type_traits>

#include "base/memory/ref_counted.h"
#include "ios/web/public/test/web_task_environment.h"
#include "ios/web/security/certificate_policy_cache_impl.h"
#include "net/test/test_certificate_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web {

static_assert(std::is_abstract_v<CertificatePolicyCache>);

class CertificatePolicyCacheTest : public testing::Test {
 protected:
  WebTaskEnvironment task_environment_;
  scoped_refptr<CertificatePolicyCache> cache_ =
      base::MakeRefCounted<CertificatePolicyCacheImpl>();
};

TEST_F(CertificatePolicyCacheTest, PoliciesAreScopedToHostAndCertificate) {
  auto cert = net::X509Certificate::CreateFromBytes(google_der);
  auto other_cert = net::X509Certificate::CreateFromBytes(webkit_der);
  ASSERT_TRUE(cert);
  ASSERT_TRUE(other_cert);
  EXPECT_EQ(CertificatePolicyCache::UNKNOWN,
            cache_->QueryPolicy(cert.get(), "example.test",
                                net::CERT_STATUS_DATE_INVALID));

  cache_->AllowCertForHost(cert.get(), "example.test",
                           net::CERT_STATUS_DATE_INVALID);
  EXPECT_EQ(CertificatePolicyCache::ALLOWED,
            cache_->QueryPolicy(cert.get(), "example.test",
                                net::CERT_STATUS_DATE_INVALID));
  EXPECT_EQ(CertificatePolicyCache::UNKNOWN,
            cache_->QueryPolicy(cert.get(), "other.test",
                                net::CERT_STATUS_DATE_INVALID));
  EXPECT_EQ(CertificatePolicyCache::UNKNOWN,
            cache_->QueryPolicy(other_cert.get(), "example.test",
                                net::CERT_STATUS_DATE_INVALID));
  EXPECT_EQ(CertificatePolicyCache::UNKNOWN,
            cache_->QueryPolicy(cert.get(), "example.test",
                                net::CERT_STATUS_DATE_INVALID |
                                    net::CERT_STATUS_AUTHORITY_INVALID));
}

TEST_F(CertificatePolicyCacheTest, ClearRemovesAllHostPolicies) {
  auto cert = net::X509Certificate::CreateFromBytes(google_der);
  ASSERT_TRUE(cert);
  for (const char* host : {"first.test", "second.test"}) {
    cache_->AllowCertForHost(cert.get(), host, net::CERT_STATUS_DATE_INVALID);
    EXPECT_EQ(
        CertificatePolicyCache::ALLOWED,
        cache_->QueryPolicy(cert.get(), host, net::CERT_STATUS_DATE_INVALID));
  }

  cache_->ClearCertificatePolicies();
  for (const char* host : {"first.test", "second.test"}) {
    EXPECT_EQ(
        CertificatePolicyCache::UNKNOWN,
        cache_->QueryPolicy(cert.get(), host, net::CERT_STATUS_DATE_INVALID));
  }
}

}  // namespace web
