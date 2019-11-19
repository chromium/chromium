// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/signed_exchange_browser_test_helper.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/web_package/signed_exchange_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/cert_test_util.h"

namespace content {

constexpr uint64_t SignedExchangeBrowserTestHelper::kSignatureHeaderDate =
    1564272000;  // 2019-07-28T00:00:00Z
constexpr uint64_t SignedExchangeBrowserTestHelper::kSignatureHeaderExpires =
    1564876800;  // 2019-08-04T00:00:00Z

SignedExchangeBrowserTestHelper::SignedExchangeBrowserTestHelper() = default;

SignedExchangeBrowserTestHelper::~SignedExchangeBrowserTestHelper() = default;

void SignedExchangeBrowserTestHelper::SetUp() {
  signed_exchange_utils::SetVerificationTimeForTesting(
      base::Time::UnixEpoch() +
      base::TimeDelta::FromSeconds(kSignatureHeaderDate));
}

void SignedExchangeBrowserTestHelper::TearDownOnMainThread() {
  interceptor_.reset();
  signed_exchange_utils::SetVerificationTimeForTesting(
      base::Optional<base::Time>());
}

scoped_refptr<net::X509Certificate>
SignedExchangeBrowserTestHelper::LoadCertificate() {
  constexpr char kCertFileName[] = "prime256v1-sha256.public.pem";

  base::ScopedAllowBlockingForTesting allow_io;
  base::FilePath dir_path;
  base::PathService::Get(content::DIR_TEST_DATA, &dir_path);
  dir_path = dir_path.Append(FILE_PATH_LITERAL("sxg"));

  return net::CreateCertificateChainFromFile(
      dir_path, kCertFileName, net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
}

void SignedExchangeBrowserTestHelper::InstallUrlInterceptor(
    const GURL& url,
    const std::string& data_path) {
  if (!interceptor_) {
    interceptor_ = std::make_unique<URLLoaderInterceptor>(base::BindRepeating(
        &SignedExchangeBrowserTestHelper::OnInterceptCallback,
        base::Unretained(this)));
  }
  interceptor_data_path_map_[url] = data_path;
}

void SignedExchangeBrowserTestHelper::InstallMockCert(
    content::ContentMockCertVerifier::CertVerifier* cert_verifier) {
  // Make the MockCertVerifier treat the certificate
  // "prime256v1-sha256.public.pem" as valid for "test.example.org".
  scoped_refptr<net::X509Certificate> original_cert = LoadCertificate();
  net::CertVerifyResult dummy_result;
  dummy_result.verified_cert = original_cert;
  dummy_result.cert_status = net::OK;
  dummy_result.ocsp_result.response_status = net::OCSPVerifyResult::PROVIDED;
  dummy_result.ocsp_result.revocation_status = net::OCSPRevocationStatus::GOOD;
  cert_verifier->AddResultForCertAndHost(original_cert, "test.example.org",
                                         dummy_result, net::OK);
}

void SignedExchangeBrowserTestHelper::InstallMockCertChainInterceptor() {
  InstallUrlInterceptor(
      GURL("https://cert.example.org/cert.msg"),
      "content/test/data/sxg/test.example.org.public.pem.cbor");
}

bool SignedExchangeBrowserTestHelper::OnInterceptCallback(
    URLLoaderInterceptor::RequestParams* params) {
  const auto it = interceptor_data_path_map_.find(params->url_request.url);
  if (it == interceptor_data_path_map_.end())
    return false;
  URLLoaderInterceptor::WriteResponse(it->second, params->client.get());
  return true;
}

}  // namespace content
