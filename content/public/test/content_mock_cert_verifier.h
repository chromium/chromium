// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_CONTENT_MOCK_CERT_VERIFIER_H_
#define CONTENT_PUBLIC_TEST_CONTENT_MOCK_CERT_VERIFIER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/cert/mock_cert_verifier.h"
#include "services/network/public/mojom/network_service_test.mojom.h"

namespace base {
class CommandLine;
}

namespace net {
class MockCertVerifier;
}

namespace content {

// ContentMockCertVerifier allows tests to force certificate verification
// results for requests made with any BrowserContext's main request context
// (such as navigations). To do so, tests can use the MockCertVerifier exposed
// via ContentMockCertVerifier::mock_cert_verifier(). This class ensures the
// mocking works correctly regardless of whether the network service is enabled
// or disabled, or running in-process.
class ContentMockCertVerifier {
 public:
  ContentMockCertVerifier();

  ContentMockCertVerifier(const ContentMockCertVerifier&) = delete;
  ContentMockCertVerifier& operator=(const ContentMockCertVerifier&) = delete;

  virtual ~ContentMockCertVerifier();

  // Be sure to call these method from the relevant BrowserTestBase methods.
  virtual void SetUpCommandLine(base::CommandLine* command_line);
  virtual void SetUpInProcessBrowserTestFixture();
  virtual void TearDownInProcessBrowserTestFixture();

  // Has the same methods as net::MockCertVerifier and updates the network
  // service as well if it's in use. See the documentation of the net class
  // for documentation on the methods.
  // Once all requests use the NetworkContext, even when network service is not
  // enabled, we can stop also updating net::MockCertVerifier here and always
  // go through the NetworkServiceTest mojo interface.
  class CertVerifier {
   public:
    explicit CertVerifier(net::MockCertVerifier* verifier);

    CertVerifier(const CertVerifier&) = delete;
    CertVerifier& operator=(const CertVerifier&) = delete;

    ~CertVerifier();
    void set_default_result(int default_result);
    void AddResultForCert(scoped_refptr<net::X509Certificate> cert,
                          const net::CertVerifyResult& verify_result,
                          int rv);
    void AddResultForCertAndHost(scoped_refptr<net::X509Certificate> cert,
                                 const std::string& host_pattern,
                                 const net::CertVerifyResult& verify_result,
                                 int rv);

   private:
    void EnsureNetworkServiceTestInitialized();

    raw_ptr<net::MockCertVerifier> verifier_;
    mojo::Remote<network::mojom::NetworkServiceTest> network_service_test_;
  };

  // Returns a pointer to the MockCertVerifier used by all profiles in
  // this test. This is what test code should use to mock certificate
  // verification.
  CertVerifier* mock_cert_verifier();

  // An internal getter to be used by test harness that wraps this class.
  net::MockCertVerifier* mock_cert_verifier_internal() {
    return mock_cert_verifier_.get();
  }

 private:
  std::unique_ptr<net::MockCertVerifier> mock_cert_verifier_;

  CertVerifier cert_verifier_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_CONTENT_MOCK_CERT_VERIFIER_H_
