// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/signed_exchange_cert_fetcher_factory.h"

#include <utility>

#include "base/unguessable_token.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "url/origin.h"

namespace content {

SignedExchangeCertFetcherFactory::~SignedExchangeCertFetcherFactory() = default;

class SignedExchangeCertFetcherFactoryImpl
    : public SignedExchangeCertFetcherFactory {
 public:
  SignedExchangeCertFetcherFactoryImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      URLLoaderThrottlesGetter url_loader_throttles_getter,
      const base::Optional<base::UnguessableToken>& throttling_profile_id)
      : url_loader_factory_(std::move(url_loader_factory)),
        url_loader_throttles_getter_(std::move(url_loader_throttles_getter)),
        throttling_profile_id_(throttling_profile_id) {}

  std::unique_ptr<SignedExchangeCertFetcher> CreateFetcherAndStart(
      const GURL& cert_url,
      bool force_fetch,
      SignedExchangeCertFetcher::CertificateCallback callback,
      SignedExchangeDevToolsProxy* devtools_proxy,
      SignedExchangeReporter* reporter) override;

 private:
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  URLLoaderThrottlesGetter url_loader_throttles_getter_;
  const base::Optional<base::UnguessableToken> throttling_profile_id_;
};

std::unique_ptr<SignedExchangeCertFetcher>
SignedExchangeCertFetcherFactoryImpl::CreateFetcherAndStart(
    const GURL& cert_url,
    bool force_fetch,
    SignedExchangeCertFetcher::CertificateCallback callback,
    SignedExchangeDevToolsProxy* devtools_proxy,
    SignedExchangeReporter* reporter) {
  DCHECK(url_loader_factory_);
  DCHECK(url_loader_throttles_getter_);
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
      std::move(url_loader_throttles_getter_).Run();
  return SignedExchangeCertFetcher::CreateAndStart(
      std::move(url_loader_factory_), std::move(throttles), cert_url,
      force_fetch, std::move(callback), devtools_proxy, reporter,
      throttling_profile_id_);
}

// static
std::unique_ptr<SignedExchangeCertFetcherFactory>
SignedExchangeCertFetcherFactory::Create(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    URLLoaderThrottlesGetter url_loader_throttles_getter,
    const base::Optional<base::UnguessableToken>& throttling_profile_id) {
  return std::make_unique<SignedExchangeCertFetcherFactoryImpl>(
      std::move(url_loader_factory), std::move(url_loader_throttles_getter),
      throttling_profile_id);
}

}  // namespace content
