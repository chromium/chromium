// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/session/session_certificate_policy_cache_impl.h"

#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/web/public/security/certificate_policy_cache.h"
#import "ios/web/public/session/crw_session_certificate_policy_cache_storage.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "net/cert/x509_certificate.h"
#import "net/test/cert_test_util.h"
#import "net/test/test_data_directory.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::WaitUntilConditionOrTimeout;
using base::test::ios::kWaitForJSCompletionTimeout;

namespace {
// Synchronously checks `cache` for the specified cert and returns the judgment.
web::CertPolicy::Judgment GetJudgmenet(
    const scoped_refptr<web::CertificatePolicyCache>& cache,
    const scoped_refptr<net::X509Certificate>& cert,
    const std::string& host,
    const net::CertStatus status) {
  // Post a task to the IO thread and wait for a reply
  __block web::CertPolicy::Judgment judgement =
      web::CertPolicy::Judgment::UNKNOWN;
  __block bool completed = false;
  web::GetIOThreadTaskRunner({})->PostTask(FROM_HERE, base::BindOnce(^{
                                             completed = true;
                                             judgement = cache->QueryPolicy(
                                                 cert.get(), host, status);
                                           }));
  EXPECT_TRUE(WaitUntilConditionOrTimeout(base::Seconds(1), ^{
    return completed;
  }));
  return judgement;
}

}  // namespace

// Test fixture to test SessionCertificatePolicyCacheImpl class.
class SessionCertificatePolicyCacheImplTest : public PlatformTest {
 protected:
  SessionCertificatePolicyCacheImplTest()
      : task_environment_(web::WebTaskEnvironment::Options::REAL_IO_THREAD),
        cache_(&browser_state_),
        cert_(net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "ok_cert.pem")),
        host_("test.com"),
        status_(net::CERT_STATUS_REVOKED) {
    // Check that the default value for the CertPolicyJudgment in
    // web::CertificatePolicyCache is UNKNOWN before registering it.
    EXPECT_EQ(web::CertPolicy::Judgment::UNKNOWN,
              GetJudgmenet(
                  web::BrowserState::GetCertificatePolicyCache(&browser_state_),
                  cert_, host_, status_));
    cache_.RegisterAllowedCertificate(cert_, host_, status_);
  }

  web::WebTaskEnvironment task_environment_;
  web::FakeBrowserState browser_state_;
  web::SessionCertificatePolicyCacheImpl cache_;
  scoped_refptr<net::X509Certificate> cert_;
  std::string host_;
  net::CertStatus status_;
};

// Tests that registering an allowed certificate will successfully create an
// accurate CRWSessionCertificateStorage in the allowed certs set.
TEST_F(SessionCertificatePolicyCacheImplTest, RegisterAllowedCert) {
  // Verify that the cert information is added to the cache.
  EXPECT_EQ(1U, cache_.GetAllowedCerts().count);
  CRWSessionCertificateStorage* cert_storage =
      [cache_.GetAllowedCerts() anyObject];
  EXPECT_EQ(cert_.get(), cert_storage.certificate);
  EXPECT_EQ(host_, cert_storage.host);
  EXPECT_EQ(status_, cert_storage.status);
  // Verify that the CertificatePolicyCache gets updated.
  scoped_refptr<web::CertificatePolicyCache> cache =
      web::BrowserState::GetCertificatePolicyCache(&browser_state_);
  EXPECT_EQ(web::CertPolicy::Judgment::ALLOWED,
            GetJudgmenet(cache, cert_, host_, status_));
}

// Tests that UpdateCertificatePolicyCache() successfully transfers the allowed
// certificate information to a CertificatePolicyCache.
//
// TODO(crbug.com/1040566): Delete this test when UpdateCertificatePolicyCache
// is deleted. Currently disabled since RegisterAllowedCertificate already
// updates the CertificatePolicyCache.
TEST_F(SessionCertificatePolicyCacheImplTest,
       DISABLED_UpdateCertificatePolicyCache) {
  // Create a CertificatePolicyCache.
  scoped_refptr<web::CertificatePolicyCache> cache =
      web::BrowserState::GetCertificatePolicyCache(&browser_state_);
  EXPECT_EQ(web::CertPolicy::Judgment::UNKNOWN,
            GetJudgmenet(cache, cert_, host_, status_));
  // Add allowed certificates to the CertificatePolicyCache and verify that it's
  // now allowed by the CertificatePolicyCache.
  cache_.UpdateCertificatePolicyCache(cache);
  EXPECT_EQ(web::CertPolicy::Judgment::ALLOWED,
            GetJudgmenet(cache, cert_, host_, status_));
}
