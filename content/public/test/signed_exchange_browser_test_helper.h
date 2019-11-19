// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SIGNED_EXCHANGE_BROWSER_TEST_HELPER_H_
#define CONTENT_PUBLIC_TEST_SIGNED_EXCHANGE_BROWSER_TEST_HELPER_H_

#include <map>
#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/cert/x509_certificate.h"
#include "url/gurl.h"

namespace content {

// SignedExchangeBrowserTestHelper helps writing browser tests for
// signed exchanges by:
// - Override signed exchange verification time epoch
//   to that of the test SXGs in content/test/data/sxg.
//   See kSignatureHeaderDate and kSignatureHeaderExpires for more details.
// - Setup URLLoaderInterceptor, which serves injected content via
//   InstallUrlInterceptor.
//
// Typical usage would look like following:
//
// Installs a test mock certificate to the |mock_cert_verifier|
// to work with the test certificate used by the helper:
//   SignedExchangeBrowserTestHlper helper;
//   helper.InstallMockCert(mock_cert_verifier());
//
// Then installs a HTTP request interceptor for the cert chain
// for the same test mock certificate so that it can be fetched at
// the cert URL embedded in the test SXGs in content/test/data/sxg.
//   helper.InstallMockCertChainInterceptor()
//
// Sets up HTTP request interceptors for the testing (e.g. SXGs,
// service worker resources).
//   helper.InstallUrlInterceptor(
//       GURL("https://test.example.org/scope/test.sxg"),
//       "content/test/data/sxg/test.example.org_test.sxg");
class SignedExchangeBrowserTestHelper {
 public:
  SignedExchangeBrowserTestHelper();
  ~SignedExchangeBrowserTestHelper();

  void SetUp();
  void TearDownOnMainThread();

  // Load the same certificate file as the one that is used by the helper.
  // This can be used to verify if the same cert is correctly set up or to
  // compute its fingerprint.
  static scoped_refptr<net::X509Certificate> LoadCertificate();

  // Make the |cert_verifier| trust the test certificate for "test.example.org".
  void InstallMockCert(
      content::ContentMockCertVerifier::CertVerifier* cert_verifier);

  void InstallUrlInterceptor(const GURL& url, const std::string& data_path);

  // Make the URL loader interceptor serve the test certchain cbor for
  // "test.example.org" at "https://cert.example.org/cert.msg".
  void InstallMockCertChainInterceptor();

  // 'Date' and 'Expire' timestamp of the SXGs in content/test/data/sxg in Unix
  // time.
  static const uint64_t kSignatureHeaderDate;
  static const uint64_t kSignatureHeaderExpires;

 private:
  bool OnInterceptCallback(URLLoaderInterceptor::RequestParams* params);

  std::unique_ptr<URLLoaderInterceptor> interceptor_;
  std::map<GURL, std::string> interceptor_data_path_map_;

  DISALLOW_COPY_AND_ASSIGN(SignedExchangeBrowserTestHelper);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SIGNED_EXCHANGE_BROWSER_TEST_HELPER_H_
