// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/session/session_certificate_policy_cache_impl.h"

#import "base/functional/bind.h"
#import "base/test/ios/wait_util.h"
#import "base/time/time.h"
#import "ios/web/public/security/certificate_policy_cache.h"
#import "ios/web/public/session/proto/session.pb.h"
#import "ios/web/public/test/fakes/fake_browser_state.h"
#import "ios/web/public/test/web_task_environment.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/session/session_certificate.h"
#import "net/cert/x509_certificate.h"
#import "net/test/cert_test_util.h"
#import "net/test/test_data_directory.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"

using base::test::ios::WaitUntilConditionOrTimeout;

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

// Returns a test session certificate.
web::SessionCertificate CreateTestSessionCertificate(
    const std::string& filename,
    const std::string& host,
    net::CertStatus status) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(), filename);
  return web::SessionCertificate(cert, host, status);
}

}  // namespace

// Test fixture to test SessionCertificatePolicyCacheImpl class.
class SessionCertificatePolicyCacheImplTest : public PlatformTest {
 protected:
  SessionCertificatePolicyCacheImplTest() = default;

  web::WebTaskEnvironment task_environment_{
      web::WebTaskEnvironment::IOThreadType::REAL_THREAD};
  web::FakeBrowserState browser_state_;
};

// Tests that registering an allowed certificate will successfully create an
// accurate SessionCertificate in the allowed certs set.
TEST_F(SessionCertificatePolicyCacheImplTest, RegisterAllowedCertificate) {
  const web::SessionCertificate cert = CreateTestSessionCertificate(
      "ok_cert.pem", "test.com", net::CERT_STATUS_REVOKED);

  // Check that the default value for the CertPolicyJudgment in
  // web::CertificatePolicyCache is UNKNOWN before registering it.
  ASSERT_EQ(web::CertPolicy::Judgment::UNKNOWN,
            GetJudgmenet(
                web::BrowserState::GetCertificatePolicyCache(&browser_state_),
                cert.certificate(), cert.host(), cert.status()));

  // Check that creating the SessionCertificatePolicyCacheImpl does not
  // register the certificate.
  web::SessionCertificatePolicyCacheImpl cache(&browser_state_);
  ASSERT_EQ(web::CertPolicy::Judgment::UNKNOWN,
            GetJudgmenet(
                web::BrowserState::GetCertificatePolicyCache(&browser_state_),
                cert.certificate(), cert.host(), cert.status()));

  // Verify that calling RegisterAllowedCertificate() register the certificate.
  cache.RegisterAllowedCertificate(cert.certificate(), cert.host(),
                                   cert.status());
  EXPECT_EQ(web::CertPolicy::Judgment::ALLOWED,
            GetJudgmenet(
                web::BrowserState::GetCertificatePolicyCache(&browser_state_),
                cert.certificate(), cert.host(), cert.status()));

  // Verify that there is one certificate in the serialisation, and that it
  // corresponds to the registered certificate.
  web::proto::CertificatesCacheStorage storage;
  cache.SerializeToProto(storage);

  ASSERT_EQ(storage.certs_size(), 1);
  const web::SessionCertificate decoded(storage.certs()[0]);
  EXPECT_EQ(cert, decoded);
}

// Tests that UpdateCertificatePolicyCache() successfully transfers the allowed
// certificate information to a CertificatePolicyCache.
TEST_F(SessionCertificatePolicyCacheImplTest, UpdateCertificatePolicyCache) {
  const web::SessionCertificate cert = CreateTestSessionCertificate(
      "ok_cert.pem", "test.com", net::CERT_STATUS_REVOKED);

  // Check that the default value for the CertPolicyJudgment in
  // web::CertificatePolicyCache is UNKNOWN before registering it.
  ASSERT_EQ(web::CertPolicy::Judgment::UNKNOWN,
            GetJudgmenet(
                web::BrowserState::GetCertificatePolicyCache(&browser_state_),
                cert.certificate(), cert.host(), cert.status()));

  // Create a fake serialised state.
  web::proto::CertificatesCacheStorage storage;
  cert.SerializeToProto(*storage.add_certs());

  // Check that creating the SessionCertificatePolicyCacheImpl from
  // serialized state does not register the certificate.
  web::SessionCertificatePolicyCacheImpl cache(&browser_state_, storage);
  ASSERT_EQ(web::CertPolicy::Judgment::UNKNOWN,
            GetJudgmenet(
                web::BrowserState::GetCertificatePolicyCache(&browser_state_),
                cert.certificate(), cert.host(), cert.status()));

  // Verify that calling RegisterAllowedCertificate() register the certificate.
  cache.UpdateCertificatePolicyCache();
  EXPECT_EQ(web::CertPolicy::Judgment::ALLOWED,
            GetJudgmenet(
                web::BrowserState::GetCertificatePolicyCache(&browser_state_),
                cert.certificate(), cert.host(), cert.status()));
}
