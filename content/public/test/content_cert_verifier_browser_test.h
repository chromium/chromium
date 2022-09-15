// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_CONTENT_CERT_VERIFIER_BROWSER_TEST_H_
#define CONTENT_PUBLIC_TEST_CONTENT_CERT_VERIFIER_BROWSER_TEST_H_

#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_mock_cert_verifier.h"

namespace content {

// CertVerifierBrowserTest allows tests to force certificate verification
// results for requests made with any profile's main request context (such as
// navigations). To do so, tests can use the MockCertVerifier exposed via
// CertVerifierBrowserTest::mock_cert_verifier().
class CertVerifierBrowserTest : public ContentBrowserTest {
 public:
  CertVerifierBrowserTest();

  CertVerifierBrowserTest(const CertVerifierBrowserTest&) = delete;
  CertVerifierBrowserTest& operator=(const CertVerifierBrowserTest&) = delete;

  ~CertVerifierBrowserTest() override;

  // InProcessBrowserTest:
  void SetUpCommandLine(base::CommandLine* command_line) override;
  void SetUpInProcessBrowserTestFixture() override;
  void TearDownInProcessBrowserTestFixture() override;

  ContentMockCertVerifier::CertVerifier* mock_cert_verifier() {
    return mock_cert_verifier_.mock_cert_verifier();
  }

  void disable_mock_cert_verifier() { mock_cert_verifier_disabled_ = true; }

 private:
  bool mock_cert_verifier_disabled_ = false;
  ContentMockCertVerifier mock_cert_verifier_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_CONTENT_CERT_VERIFIER_BROWSER_TEST_H_
